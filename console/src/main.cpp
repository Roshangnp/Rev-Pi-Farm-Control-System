#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

#include "console_devices.h"
#include "console_ui.h"

// Farm console: a keyboard-only terminal UI for the RO RevPi and the upper tank.
// A client of the RevPis' line servers, so it runs anywhere that can reach them:
// the RO RevPi's HDMI console (use --ro 127.0.0.1 so it works without the
// network), or over SSH on emerald-server or the dev PC.
// WiseConn irrigation zones are read from the WiseConn API directly (keys built
// in from wiseconn_keys.txt).
//
//   ro_console [--ro HOST] [--upper HOST]
//
// Log messages (WiseConn poll errors and the like) go to ~/.cache/ro_console.log,
// not the terminal, where they would draw over the full-screen UI.

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
    std::string ro_host = "192.168.1.20";
    std::string upper_host = "192.168.1.21";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ro" && i + 1 < argc) {
            ro_host = argv[++i];
        } else if (arg == "--upper" && i + 1 < argc) {
            upper_host = argv[++i];
        } else {
            std::fprintf(stderr, "usage: %s [--ro HOST] [--upper HOST]\n"
                                 "  defaults: --ro 192.168.1.20 --upper 192.168.1.21\n", argv[0]);
            return 2;
        }
    }

    redirectLogs();
    console_devices farm(ro_host, upper_host);
    return runConsole(farm, ro_host, upper_host);
}
