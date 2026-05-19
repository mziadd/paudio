#pragma once

namespace pocket_audio::platform {

// Install/remove PocketAudio Windows service (administrator).
int installService();
int uninstallService();

// Called by Service Control Manager (do not run manually).
int runServiceDispatcher();

} // namespace pocket_audio::platform
