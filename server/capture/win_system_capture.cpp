#include "capture/win_system_capture.hpp"

#if !defined(_WIN32)
#error "This file is Windows only"
#endif

#include <audioclient.h>
#include <iostream>
#include <mmdeviceapi.h>
#include <cstring>
#include <thread>
#include <vector>
#include <windows.h>

#include "core/chunk_emit.hpp"
#include "core/format.hpp"

namespace pocket_audio::capture {

WinSystemCapture::WinSystemCapture() = default;
WinSystemCapture::~WinSystemCapture() { stop(); }

bool WinSystemCapture::start(ChunkCallback on_chunk) {
  if (running_) {
    last_error_ = "already running";
    return false;
  }
  if (!on_chunk) {
    last_error_ = "callback required";
    return false;
  }

  on_chunk_ = std::move(on_chunk);
  running_ = true;
  std::thread([this] { captureThread(); }).detach();
  return true;
}

void WinSystemCapture::stop() {
  running_ = false;
  on_chunk_ = nullptr;
}

void WinSystemCapture::captureThread() {
  if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
    last_error_ = "CoInitializeEx failed";
    running_ = false;
    return;
  }

  IMMDeviceEnumerator *enumerator = nullptr;
  IMMDevice *device = nullptr;
  IAudioClient *client = nullptr;
  IAudioCaptureClient *capture = nullptr;
  WAVEFORMATEX *mixFormat = nullptr;
  HRESULT hr;

  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                        __uuidof(IMMDeviceEnumerator),
                        reinterpret_cast<void **>(&enumerator));
  if (FAILED(hr)) {
    last_error_ = "no device enumerator";
    goto cleanup;
  }

  hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
  if (FAILED(hr)) {
    last_error_ = "no default audio output";
    goto cleanup;
  }

  hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                        reinterpret_cast<void **>(&client));
  if (FAILED(hr)) {
    last_error_ = "activate audio client failed";
    goto cleanup;
  }

  hr = client->GetMixFormat(&mixFormat);
  if (FAILED(hr)) {
    last_error_ = "GetMixFormat failed";
    goto cleanup;
  }

  hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                          10000000, 0, mixFormat, nullptr);
  if (FAILED(hr)) {
    last_error_ = "IAudioClient::Initialize failed";
    goto cleanup;
  }

  hr = client->GetService(__uuidof(IAudioCaptureClient),
                          reinterpret_cast<void **>(&capture));
  if (FAILED(hr)) {
    last_error_ = "GetService failed";
    goto cleanup;
  }

  hr = client->Start();
  if (FAILED(hr)) {
    last_error_ = "Start failed";
    goto cleanup;
  }

  {
    const int srcChannels = mixFormat->nChannels;
    const int srcRate = static_cast<int>(mixFormat->nSamplesPerSec);
    const WORD bits = mixFormat->wBitsPerSample;
    const bool srcIsFloat =
        (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) || bits == 32;

    if (srcRate != kSampleRateHz) {
      std::cerr << "  warning: speaker mix is " << srcRate << " Hz, expected "
                << kSampleRateHz
                << " Hz — set speakers to 48000 Hz in Sound settings.\n";
    }

    std::vector<float> pending;
    std::size_t head = 0;
    pending.reserve(static_cast<std::size_t>(kChunkSamplesPerChannel) *
                    kChannelCount * 4);

    while (running_) {
      UINT32 packet = 0;
      if (FAILED(capture->GetNextPacketSize(&packet)))
        break;

      while (packet > 0) {
        BYTE *data = nullptr;
        UINT32 frames = 0;
        DWORD flags = 0;
        if (FAILED(
                capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr)))
          break;

        const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
        if (data && frames > 0 && !silent && srcIsFloat) {
          const auto *src = reinterpret_cast<const float *>(data);
          for (UINT32 i = 0; i < frames; ++i) {
            pending.push_back(src[i * srcChannels]);
            pending.push_back(srcChannels > 1 ? src[i * srcChannels + 1]
                                              : src[i * srcChannels]);
          }
        }

        capture->ReleaseBuffer(frames);

        const int chunkLen = kChunkSamplesPerChannel * kChannelCount;
        while (static_cast<int>(pending.size()) - static_cast<int>(head) >=
               chunkLen) {
          AudioChunk chunk;
          std::memcpy(chunk.data(), pending.data() + head,
                      static_cast<std::size_t>(chunkLen) * sizeof(float));
          head += static_cast<std::size_t>(chunkLen);
          if (on_chunk_)
            on_chunk_(chunk);
        }

        if (head >= 8192) {
          const std::size_t left = pending.size() - head;
          if (left > 0) {
            std::memmove(pending.data(), pending.data() + head,
                         left * sizeof(float));
          }
          pending.resize(left);
          head = 0;
        }

        if (FAILED(capture->GetNextPacketSize(&packet)))
          break;
      }

      Sleep(2);
    }
  }

cleanup:
  if (client)
    client->Stop();
  if (mixFormat)
    CoTaskMemFree(mixFormat);
  if (capture)
    capture->Release();
  if (client)
    client->Release();
  if (device)
    device->Release();
  if (enumerator)
    enumerator->Release();
  CoUninitialize();
  running_ = false;
}

} // namespace pocket_audio::capture
