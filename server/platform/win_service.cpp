#if !defined(_WIN32)
#error "win_service.cpp is Windows only"
#endif

#include "platform/win_service.hpp"

#include "server_loop.hpp"

#include <windows.h>

#include <iostream>
#include <string>

namespace pocket_audio::platform {
namespace {

constexpr const char *kServiceName = "PocketAudio";
constexpr const char *kDisplayName = "PocketAudio Server";

SERVICE_STATUS g_status{};
SERVICE_STATUS_HANDLE g_statusHandle = nullptr;

std::wstring utf8ToWide(const std::string &s) {
  if (s.empty())
    return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  std::wstring out(static_cast<std::size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
  if (!out.empty() && out.back() == L'\0')
    out.pop_back();
  return out;
}

std::wstring exePath() {
  std::wstring buf(MAX_PATH, L'\0');
  for (;;) {
    const DWORD n = GetModuleFileNameW(nullptr, buf.data(),
                                       static_cast<DWORD>(buf.size()));
    if (n == 0)
      return {};
    if (n < buf.size()) {
      buf.resize(n);
      return buf;
    }
    buf.resize(buf.size() * 2);
  }
}

void setServiceState(DWORD state, DWORD win32Exit = NO_ERROR) {
  g_status.dwCurrentState = state;
  g_status.dwWin32ExitCode = win32Exit;
  if (g_statusHandle)
    SetServiceStatus(g_statusHandle, &g_status);
}

void WINAPI serviceCtrlHandler(DWORD control) {
  switch (control) {
  case SERVICE_CONTROL_STOP:
  case SERVICE_CONTROL_SHUTDOWN:
    setServiceState(SERVICE_STOP_PENDING);
    pocket_audio::requestServerStop();
    break;
  default:
    break;
  }
}

void WINAPI serviceMain(DWORD, LPWSTR *) {
  g_statusHandle = RegisterServiceCtrlHandlerA(kServiceName, serviceCtrlHandler);
  if (!g_statusHandle)
    return;

  g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  g_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
  g_status.dwWin32ExitCode = NO_ERROR;
  g_status.dwServiceSpecificExitCode = 0;

  setServiceState(SERVICE_START_PENDING);
  setServiceState(SERVICE_RUNNING);

  const int code = pocket_audio::runServer();

  setServiceState(SERVICE_STOPPED, code == 0 ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR);
}

} // namespace

int runServiceDispatcher() {
  SERVICE_TABLE_ENTRYA table[] = {
      {const_cast<LPSTR>(kServiceName), serviceMain}, {nullptr, nullptr}};
  if (!StartServiceCtrlDispatcherA(table)) {
    std::cerr << "StartServiceCtrlDispatcher failed: " << GetLastError() << "\n";
    return 1;
  }
  return 0;
}

int installService() {
  const std::wstring path = exePath();
  if (path.empty()) {
    std::cerr << "Could not get executable path\n";
    return 1;
  }

  const std::wstring binPath = L"\"" + path + L"\" --service";

  SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
  if (!scm) {
    std::cerr << "OpenSCManager failed (run as Administrator): "
              << GetLastError() << "\n";
    return 1;
  }

  SC_HANDLE svc = CreateServiceW(
      scm, utf8ToWide(kServiceName).c_str(), utf8ToWide(kDisplayName).c_str(),
      SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
      SERVICE_ERROR_NORMAL, binPath.c_str(), nullptr, nullptr, nullptr, nullptr,
      nullptr);

  if (!svc) {
    const DWORD err = GetLastError();
    if (err == ERROR_SERVICE_EXISTS) {
      std::cerr << "Service already installed. Use --uninstall-service first.\n";
    } else {
      std::cerr << "CreateService failed: " << err << "\n";
    }
    CloseServiceHandle(scm);
    return 1;
  }

  std::wcout << L"Installed service \"" << utf8ToWide(kServiceName)
             << L"\" (starts automatically at boot).\n";
  std::wcout << L"Start now: sc start " << utf8ToWide(kServiceName) << L"\n";
  std::wcout
      << L"\nNote: Windows services often cannot capture your speaker audio\n"
         L"(Session 0). If Listen fails, use scripts\\install-logon-task.ps1\n"
         L"instead (starts when you sign in).\n";

  CloseServiceHandle(svc);
  CloseServiceHandle(scm);
  return 0;
}

int uninstallService() {
  SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
  if (!scm) {
    std::cerr << "OpenSCManager failed (run as Administrator): "
              << GetLastError() << "\n";
    return 1;
  }

  SC_HANDLE svc =
      OpenServiceW(scm, utf8ToWide(kServiceName).c_str(), SERVICE_STOP | DELETE);
  if (!svc) {
    std::cerr << "OpenService failed: " << GetLastError() << "\n";
    CloseServiceHandle(scm);
    return 1;
  }

  SERVICE_STATUS st{};
  ControlService(svc, SERVICE_CONTROL_STOP, &st);

  if (!DeleteService(svc)) {
    std::cerr << "DeleteService failed: " << GetLastError() << "\n";
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return 1;
  }

  std::cout << "Service removed.\n";
  CloseServiceHandle(svc);
  CloseServiceHandle(scm);
  return 0;
}

} // namespace pocket_audio::platform
