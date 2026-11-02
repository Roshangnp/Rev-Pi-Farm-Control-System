#include "config.h"

#include "HAL/electronics.h"
#include "devices/local_devices/local_float_switch.h"
#include "devices/local_devices/local_level_sensor.h"
#include "devices/remote_devices/device_server.h"

// ---------------------------------------------------------------------------
// Upper tank RevPi (192.168.1.195). Wiring from API/hardware_setup.csv.
// ---------------------------------------------------------------------------

namespace {

// Electrical interfaces: (device id, interface type, PiCtory pin)
electrical_interface upper_tank_high_float_input(1, digital_in{}, "DigitalInput_4");             // MIO digital input 4
electrical_interface upper_tank_water_level_input(2, analog_current_in{}, "InputValue_1");  // 4-20 mA

// Devices wired to this RevPi
local_float_switch local_upper_tank_high_float(&upper_tank_high_float_input);
local_level_sensor local_upper_tank_water_level(&upper_tank_water_level_input, 0.843f, 11.5f);

// Shares the devices wired to this RevPi with the other machines
device_server server;

}  // namespace

// Public devices declared in config.h
float_switch& upper_tank_high_float = local_upper_tank_high_float;
level_sensor& upper_tank_water_level = local_upper_tank_water_level;

bool attachAllInterfaces() {
    bool ok = true;
    for (electrical_interface* e : {&upper_tank_high_float_input, &upper_tank_water_level_input}) {
        ok = e->attach() && ok;
    }
    return ok;
}

bool startDeviceServer() {
    bool ok = server.addDevice("upper_tank_high_float", local_upper_tank_high_float)
           && server.addDevice("upper_tank_water_level", local_upper_tank_water_level);
    return ok && server.start();
}

void serviceDeviceServer(int timeout_ms) {
    server.poll(timeout_ms);
}
