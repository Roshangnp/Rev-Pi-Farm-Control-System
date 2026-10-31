#include <cstdio>
#include "config.h"

// Host PC: serves the web UI for the devices it reaches on the RevPis.
int main() {
    attachAllInterfaces();
    if (!startWebUI()) {
        std::fprintf(stderr, "Web UI failed to start (port 8080 in use?)\n");
        return 1;
    }
    std::printf("Web UI on http://localhost:8080\n");

    while (true) {
        serviceWebUI(20);
    }
}
