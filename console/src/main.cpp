#include <cstdio>
#include <string>

#include "console_devices.h"
#include "console_ui.h"

// Farm console: a keyboard-only terminal UI for the RO RevPi and the upper tank.
// A client of the RevPis' line servers, so it runs anywhere that can reach them:
// the RO RevPi's HDMI console (use --ro 127.0.0.1 so it works without the
// network), or over SSH on emerald-server or the dev PC.
//
//   ro_console [--ro HOST] [--upper HOST]
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

    console_devices farm(ro_host, upper_host);
    return runConsole(farm, ro_host, upper_host);
}
