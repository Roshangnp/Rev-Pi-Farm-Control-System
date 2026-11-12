#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

#include "config.h"
#include "console_devices.h"
#include "console_ui.h"

// Farm console: a keyboard-only terminal UI for the whole farm, on emerald-server
// (or the dev PC) only; the RevPis are self-contained and run no console. A
// user-space program of the host_pc config: devices from config.h (RevPi devices
// remote, WiseConn zones and probes from the WiseConn API), the RO from
// ro_config.h. Run it inside tmux over SSH.
//
//   ro_console
//
// Log messages (WiseConn poll errors, link drops and the like) go to
// ~/.cache/ro_console.log, not the terminal, where they would draw over the UI.

namespace {

void redirectLogs() {
    const char* home = std::getenv("HOME");
    std::string path = "/dev/null";
    if (home && *home) {
        std::string dir = std::string(home) + "/.cache";
        mkdir(dir.c_str(), 0755);   // fine if it already exists
        path = dir + "/ro_console.log";
    }
    if (!std::freopen(path.c_str(), "a", stderr)) std::freopen("/dev/null", "a", stderr);
    std::setvbuf(stderr, nullptr, _IOLBF, 0);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        std::fprintf(stderr, "usage: %s   (no options: the machines are set in config_host_pc.cpp)\n", argv[0]);
        return 2;
    }

    redirectLogs();
    attachAllInterfaces();   // host_pc has no local I/O; kept so the config is used like in every main
    console_devices farm;
    return runConsole(farm);
}
