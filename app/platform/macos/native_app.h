#pragma once
#include <string>

namespace platform::macos {
// Embedded fallback for the standalone executable. Games may override it later
// through URGE.set_window_icon, just as on other platforms.
void SetDefaultApplicationIcon();
void InstallGameMenu();
// Empty for a standalone executable. A bundle uses an adjacent game, or a
// writable copy of its demo so logs/saves never alter the signed bundle.
std::string PrepareBundledGame(int argc, char* argv[], std::string& error);
// Called after engine shutdown. Starts a fresh app instance when a game was
// selected, avoiding a second initialization of CRuby/SDL in one process.
bool RelaunchSelectedGame();
}
