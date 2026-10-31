#include "config.h"

#include <chrono>
#include <thread>

#include "devices/remote_devices/remote_link.h"
#include "devices/remote_devices/remote_float_switch.h"
#include "devices/remote_devices/remote_level_sensor.h"

// ---------------------------------------------------------------------------
// Ubuntu host PC: no local I/O, every device is owned by a RevPi.
// Each RevPi must open port 47100 to this PC's IP (see CLAUDE.md).
// ---------------------------------------------------------------------------

namespace {

remote_link upper_tank_revpi("192.168.1.195");

remote_float_switch remote_upper_tank_high_float(upper_tank_revpi, "upper_tank_high_float");
remote_level_sensor remote_upper_tank_water_level(upper_tank_revpi, "upper_tank_water_level");

}  // namespace

// Public devices declared in config.h
float_switch& upper_tank_high_float = remote_upper_tank_high_float;
level_sensor& upper_tank_water_level = remote_upper_tank_water_level;

// Nothing wired to this machine
bool attachAllInterfaces() {
    return true;
}

// Nothing to share, so no server is opened
bool startDeviceServer() {
    return true;
}

// Still sleeps so the main loop paces the same as on a RevPi
void serviceDeviceServer(int timeout_ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
}
