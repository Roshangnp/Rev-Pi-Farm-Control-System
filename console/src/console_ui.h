#pragma once
#include "console_devices.h"

// Runs the full-screen terminal UI until Ctrl+C. Keyboard only:
//   Up/Down pick a tab or a control, Right/Left move between the tab list and
//   the tab, Tab/Shift+Tab also move, Enter presses a button or opens a list,
//   Esc cancels a confirmation.
// Everything that switches hardware asks Yes/No first.
// Returns the exit code.
int runConsole(console_devices& farm, const std::string& ro_host, const std::string& upper_host);
