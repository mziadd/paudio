#include <cstring>
#include <iostream>

#if defined(_WIN32)
#include <ixwebsocket/IXNetSystem.h>
#include "platform/win_service.hpp"
#endif

#include "server_loop.hpp"

int main(int argc, char **argv) {
#if defined(_WIN32)
  ix::initNetSystem();

  if (argc >= 2) {
    const char *cmd = argv[1];
    if (std::strcmp(cmd, "--service") == 0)
      return pocket_audio::platform::runServiceDispatcher();
    if (std::strcmp(cmd, "--install-service") == 0)
      return pocket_audio::platform::installService();
    if (std::strcmp(cmd, "--uninstall-service") == 0)
      return pocket_audio::platform::uninstallService();
    if (std::strcmp(cmd, "--help") == 0 || std::strcmp(cmd, "-h") == 0) {
      std::cout
          << "pocket-audio-server.exe\n"
          << "  (no args)            Run in console\n"
          << "  --install-service    Install Windows service (admin)\n"
          << "  --uninstall-service  Remove Windows service (admin)\n"
          << "  --service            Run as service (SCM only)\n";
      ix::uninitNetSystem();
      return 0;
    }
  }

  const int code = pocket_audio::runServer();
  ix::uninitNetSystem();
  return code;
#else
  (void)argc;
  (void)argv;
  return pocket_audio::runServer();
#endif
}
