#pragma once

#include "pocket_audio/capture/system_capture.hpp"

namespace pocket_audio::capture {

// macOS 15+: ScreenCaptureKit display audio (system mix).
class MacSystemCapture : public SystemCapture {
public:
    MacSystemCapture();
    ~MacSystemCapture() override;

    bool start(ChunkCallback on_chunk) override;
    void stop() override;
    bool isRunning() const override { return running_; }
    const std::string& lastError() const override { return last_error_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool running_ = false;
    std::string last_error_;
};

}  // namespace pocket_audio::capture
