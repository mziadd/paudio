#pragma once

#include "pocket_audio/capture/system_capture.hpp"

namespace pocket_audio::capture {

// Windows 11: WASAPI loopback on default speakers.
class WinSystemCapture : public SystemCapture {
public:
    WinSystemCapture();
    ~WinSystemCapture() override;

    bool start(ChunkCallback on_chunk) override;
    void stop() override;
    bool isRunning() const override { return running_; }
    const std::string& lastError() const override { return last_error_; }

private:
    void captureThread();

    ChunkCallback on_chunk_;
    bool running_ = false;
    std::string last_error_;
};

}  // namespace pocket_audio::capture
