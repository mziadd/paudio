#include "capture/system_capture.hpp"

#if defined(__APPLE__)
#include "capture/mac_system_capture.hpp"
#elif defined(_WIN32)
#include "capture/win_system_capture.hpp"
#endif

namespace pocket_audio::capture {

std::unique_ptr<SystemCapture> SystemCapture::create() {
#if defined(__APPLE__)
  return std::make_unique<MacSystemCapture>();
#elif defined(_WIN32)
  return std::make_unique<WinSystemCapture>();
#else
  return nullptr;
#endif
}

} // namespace pocket_audio::capture
