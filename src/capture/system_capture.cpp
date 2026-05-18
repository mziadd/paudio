#include "pocket_audio/capture/system_capture.hpp"

#if defined(__APPLE__)
#include "pocket_audio/capture/mac_system_capture.hpp"
#elif defined(_WIN32)
#include "pocket_audio/capture/win_system_capture.hpp"
#endif

namespace pocket_audio::capture {

#if defined(__APPLE__)

std::unique_ptr<SystemCapture> SystemCapture::create(MacBackend backend) {
    (void)backend;
    return std::make_unique<MacSystemCapture>();
}

#elif defined(_WIN32)

std::unique_ptr<SystemCapture> SystemCapture::create() {
    return std::make_unique<WinSystemCapture>();
}

#else

std::unique_ptr<SystemCapture> SystemCapture::create() {
    return nullptr;
}

#endif

} // namespace pocket_audio::capture
