#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

#include "console_devices.h"
#include "console_ui.h"

// Farm console: a keyboard-only terminal UI for the whole farm. Connects only to
// this machine's own service on 127.0.0.1 (main_ro, main_uppertank or
// main_host_pc), which serves every device and the RO, so it's the same
// program on every machine, with no options. The header clock is local time:
// run it with TZ set to the farm's time zone on the RevPis (farm-console.service).
//
//   ro_console
//
// Log messages go to ~/.cache/ro_console.log, not the terminal, where they
// would draw over the full-screen UI.

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
        std::fprintf(stderr, "usage: %s   (no options: it connects to this machine's service on 127.0.0.1)\n", argv[0]);
        return 2;
    }

    redirectLogs();
    console_devices farm;
    return runConsole(farm);
}
