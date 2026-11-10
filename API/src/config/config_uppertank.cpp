#include "config.h"

#include "HAL/electronics.h"
#include "devices/local_devices/local_float_switch.h"
#include "devices/local_devices/local_level_sensor.h"
#include "devices/local_devices/local_pump.h"
#include "devices/remote_devices/device_server.h"

// ---------------------------------------------------------------------------
// Upper tank RevPi (192.168.1.21). Wiring from API/hardware_setup.csv.
// ---------------------------------------------------------------------------

namespace {

// Electrical interfaces: (device id, interface type, PiCtory pin)
electrical_interface upper_tank_high_float_input(1, digital_in{}, "DigitalInput_4");             // MIO digital input 4
electrical_interface upper_tank_water_level_input(2, analog_current_in{}, "InputValue_1");  // 4-20 mA
electrical_interface irrigation_pump_pressure_output(47, analog_voltage_out{}, "OutputValue_1"); // 0-10 V pressure setpoint

// Devices wired to this RevPi
local_float_switch local_upper_tank_high_float(&upper_tank_high_float_input);
local_level_sensor local_upper_tank_water_level(&upper_tank_water_level_input, 0.6797f, 12.1797f);
// Pressure setpoint only (no run relay, speed or alarm wired): 0-100 -> 0-10 V
local_pump local_irrigation_pump(nullptr, nullptr, nullptr, &irrigation_pump_pressure_output, 0.0f, 100.0f);

// Shares the devices wired to this RevPi with the other machines
device_server devices;

}  // namespace

// Public devices declared in config.h
float_switch& upper_tank_high_float = local_upper_tank_high_float;
level_sensor& upper_tank_water_level = local_upper_tank_water_level;
pump& irrigation_pump = local_irrigation_pump;

bool attachAllInterfaces() {
    bool ok = true;
    for (electrical_interface* e : {&upper_tank_high_float_input, &upper_tank_water_level_input, &irrigation_pump_pressure_output}) {
        ok = e->attach() && ok;
    }
    return ok;
}

bool addLocalDevices(line_server& server) {
    bool ok = devices.addDevice("upper_tank_high_float", local_upper_tank_high_float)
           && devices.addDevice("upper_tank_water_level", local_upper_tank_water_level)
           && devices.addDevice("irrigation_pump", local_irrigation_pump);
    return ok && devices.registerWith(server);
}
