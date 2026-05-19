#pragma once

namespace pocket_audio {

// Run capture + WebSocket until stop is requested. Returns process exit code.
int runServer();

// Console Ctrl+C or Windows service stop.
void requestServerStop();

} // namespace pocket_audio
