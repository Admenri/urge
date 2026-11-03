#pragma once
#include <string>

namespace platform::macos {
// Embedded fallback for the standalone executable. Games may override it later
// through URGE.set_window_icon, just as on other platforms.
void SetDefaultApplicationIcon();
void InstallGameMenu();
// Empty for a standalone executable. A bundle uses --game, an adjacent game,
// or a folder chosen at startup. Canceling sets canceled without an error.
std::string PrepareBundledGame(int argc, char* argv[], bool& canceled,
                               std::string& error);
// Called after engine shutdown. Starts a fresh app instance when a game was
// selected, avoiding a second initialization of CRuby/SDL in one process.
bool RelaunchSelectedGame();
}
