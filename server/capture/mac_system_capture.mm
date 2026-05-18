#include "capture/mac_system_capture.hpp"

#import <CoreMedia/CoreMedia.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <vector>

#include "core/format.hpp"

using pocket_audio::AudioChunk;
using pocket_audio::capture::ChunkCallback;

namespace {

constexpr std::size_t kPendingReserve = 8192;
constexpr std::size_t kPendingCompactThreshold = 8192;

} // namespace

struct CaptureState {
  ChunkCallback callback;
  std::mutex mutex;
  std::vector<float> pending;
  std::size_t head = 0;  // consumed samples at front — avoid erase in hot path
  SCStream *stream = nil;
};

namespace {

void flushChunks(CaptureState *state) {
  const std::size_t chunkLen = static_cast<std::size_t>(
      pocket_audio::kChunkSamplesPerChannel * pocket_audio::kChannelCount);

  // Emit fixed 8192-byte wire chunks as soon as we have enough floats queued.
  while (state->pending.size() - state->head >= chunkLen) {
    AudioChunk chunk;
    std::memcpy(chunk.data(), state->pending.data() + state->head,
                chunkLen * sizeof(float));  // interleaved L,R → AudioChunk
    state->head += chunkLen;
    if (state->callback)
      state->callback(chunk);
  }

  // Compact occasionally: slide unplayed samples to index 0 (not every callback).
  if (state->head >= kPendingCompactThreshold) {
    const std::size_t left = state->pending.size() - state->head;
    if (left > 0) {
      std::memmove(state->pending.data(), state->pending.data() + state->head,
                   left * sizeof(float));
    }
    state->pending.resize(left);
    state->head = 0;
  }
}

void appendSampleBuffer(CaptureState *state, CMSampleBufferRef sample) {
  CMFormatDescriptionRef fmt = CMSampleBufferGetFormatDescription(sample);
  const AudioStreamBasicDescription *asbd =
      CMAudioFormatDescriptionGetStreamBasicDescription(fmt);
  if (!asbd || !(asbd->mFormatFlags & kAudioFormatFlagIsFloat))
    return;

  std::size_t listSize = 0;
  if (CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
          sample, &listSize, nullptr, 0, nullptr, nullptr, 0, nullptr) != noErr)
    return;

  auto *list = static_cast<AudioBufferList *>(std::malloc(listSize));
  if (!list)
    return;

  CMBlockBufferRef block = nullptr;
  OSStatus st = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
      sample, nullptr, list, listSize, nullptr, nullptr, 0, &block);
  if (st != noErr) {
    std::free(list);
    return;
  }

  const int channels = asbd->mChannelsPerFrame > 0
                           ? static_cast<int>(asbd->mChannelsPerFrame)
                           : 2;
  const bool nonInterleaved =
      (asbd->mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;

  // SCK may send planar (separate L/R buffers) — we need interleaved for the wire.
  if (nonInterleaved && list->mNumberBuffers >= 1) {
    const auto *L = static_cast<const float *>(list->mBuffers[0].mData);
    const auto *R = (list->mNumberBuffers > 1)
                        ? static_cast<const float *>(list->mBuffers[1].mData)
                        : L;
    const std::size_t frames = list->mBuffers[0].mDataByteSize / sizeof(float);

    const std::size_t old = state->pending.size();
    state->pending.resize(old + frames * 2);
    float *dst = state->pending.data() + old;
    for (std::size_t i = 0; i < frames; ++i) {
      dst[i * 2] = L[i];
      dst[i * 2 + 1] = R[i];
    }
  } else if (list->mNumberBuffers >= 1) {
    const auto *s = static_cast<const float *>(list->mBuffers[0].mData);
    const std::size_t frames =
        list->mBuffers[0].mDataByteSize /
        (sizeof(float) * static_cast<std::size_t>(channels));

    const std::size_t old = state->pending.size();
    state->pending.resize(old + frames * 2);
    float *dst = state->pending.data() + old;

    if (channels == 2) {
      std::memcpy(dst, s, frames * 2 * sizeof(float));  // already L,R interleaved
    } else if (channels == 1) {
      for (std::size_t i = 0; i < frames; ++i) {
        dst[i * 2] = s[i];
        dst[i * 2 + 1] = s[i];
      }
    } else {
            for (std::size_t i = 0; i < frames; ++i) {
        dst[i * 2] = s[i * channels];
        dst[i * 2 + 1] = s[i * channels + 1];
      }
    }
  }

  if (block)
    CFRelease(block);
  std::free(list);
}

} // namespace

@interface StreamReceiver : NSObject <SCStreamOutput>
@property(nonatomic, assign) CaptureState *state;
@property(nonatomic, assign) bool loggedFirstAudio;
@end

@implementation StreamReceiver

- (void)stream:(SCStream *)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type {
  (void)stream;
  if (!_state || type != SCStreamOutputTypeAudio)
    return;

  if (!self.loggedFirstAudio) {
    self.loggedFirstAudio = true;
    std::cerr << "  audio capture active\n";
  }

  std::lock_guard<std::mutex> lock(_state->mutex);
  appendSampleBuffer(_state, sampleBuffer);
  flushChunks(_state);
}

@end

namespace pocket_audio::capture {

struct MacSystemCapture::Impl {
  CaptureState state;
  StreamReceiver *receiver = nil;
};

MacSystemCapture::MacSystemCapture() : impl_(std::make_unique<Impl>()) {}
MacSystemCapture::~MacSystemCapture() { stop(); }

bool MacSystemCapture::start(ChunkCallback on_chunk) {
  if (running_) {
    last_error_ = "already running";
    return false;
  }
  if (!on_chunk) {
    last_error_ = "callback required";
    return false;
  }

  if (@available(macOS 15.0, *)) {
    impl_->state.callback = std::move(on_chunk);
    impl_->state.pending.clear();
    impl_->state.pending.reserve(kPendingReserve);
    impl_->state.head = 0;

    MacSystemCapture *self = this;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block bool ok = false;
    __block NSString *errMsg = nil;

    [SCShareableContent
        getShareableContentExcludingDesktopWindows:NO
                               onScreenWindowsOnly:NO
                                 completionHandler:^(
                                     SCShareableContent *content,
                                     NSError *err) {
                                   if (err || content.displays.count == 0) {
                                     errMsg = err.localizedDescription
                                                  ?: @"no display available";
                                     dispatch_semaphore_signal(sem);
                                     return;
                                   }

                                   SCDisplay *display =
                                       content.displays.firstObject;
                                   SCContentFilter *filter =
                                       [[SCContentFilter alloc]
                                            initWithDisplay:display
                                           excludingWindows:@[]];

                                   SCStreamConfiguration *cfg =
                                       [[SCStreamConfiguration alloc] init];
                                   // SCK only outputs audio if a video stream exists too.
                                   cfg.width = 2;
                                   cfg.height = 2;
                                   cfg.minimumFrameInterval = CMTimeMake(1, 30);
                                   cfg.showsCursor = NO;
                                   cfg.capturesAudio = YES;
                                   cfg.sampleRate = kSampleRateHz;
                                   cfg.channelCount = kChannelCount;

                                   StreamReceiver *receiver =
                                       [[StreamReceiver alloc] init];
                                   receiver.state = &self->impl_->state;
                                   self->impl_->receiver = receiver;

                                   SCStream *stream =
                                       [[SCStream alloc] initWithFilter:filter
                                                          configuration:cfg
                                                               delegate:nil];
                                   self->impl_->state.stream = stream;

                                   dispatch_queue_t queue =
                                       dispatch_get_global_queue(
                                           QOS_CLASS_USER_INTERACTIVE, 0);

                                   NSError *addErr = nil;
                                   if (![stream
                                              addStreamOutput:receiver
                                                         type:
                                                             SCStreamOutputTypeScreen
                                           sampleHandlerQueue:queue
                                                        error:&addErr]) {
                                     errMsg = addErr.localizedDescription;
                                     dispatch_semaphore_signal(sem);
                                     return;
                                   }
                                   if (![stream
                                              addStreamOutput:receiver
                                                         type:
                                                             SCStreamOutputTypeAudio
                                           sampleHandlerQueue:queue
                                                        error:&addErr]) {
                                     errMsg = addErr.localizedDescription;
                                     dispatch_semaphore_signal(sem);
                                     return;
                                   }

                                   [stream startCaptureWithCompletionHandler:^(
                                               NSError *startErr) {
                                     if (startErr) {
                                       errMsg = startErr.localizedDescription;
                                     } else {
                                       ok = true;
                                       self->running_ = true;
                                     }
                                     dispatch_semaphore_signal(sem);
                                   }];
                                 }];

    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

    if (!ok) {
      last_error_ =
          errMsg ? [errMsg UTF8String] : "ScreenCaptureKit failed to start";
      last_error_ += " — check Screen Recording permission";
      impl_->state.callback = nullptr;
      return false;
    }

    last_error_.clear();
    return true;
  }

  last_error_ = "requires macOS 15 or newer";
  return false;
}

void MacSystemCapture::stop() {
  if (impl_->state.stream) {
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    [impl_->state.stream stopCaptureWithCompletionHandler:^(NSError *) {
      dispatch_semaphore_signal(sem);
    }];
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    impl_->state.stream = nil;
  }
  impl_->receiver = nil;
  running_ = false;
  std::lock_guard<std::mutex> lock(impl_->state.mutex);
  impl_->state.callback = nullptr;
  impl_->state.pending.clear();
  impl_->state.head = 0;
}

} // namespace pocket_audio::capture
