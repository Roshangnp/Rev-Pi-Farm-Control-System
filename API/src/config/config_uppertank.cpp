#include "config.h"

#include "HAL/electronics.h"
#include "devices/local_devices/local_float_switch.h"
#include "devices/local_devices/local_level_sensor.h"
#include "devices/local_devices/local_pump.h"
#include "devices/remote_devices/device_server.h"
#include "wiseconn_farm_devices.h"
#include "remote_link.h"

// ---------------------------------------------------------------------------
// Upper tank RevPi (192.168.1.21). Wiring from API/hardware_setup.csv.
// ---------------------------------------------------------------------------

// The one connection to the RO RevPi, used by ro_config_uppertank.cpp (remote_ro),
// and by any RO RevPi devices bound here later, so they share one snapshot poll.
// Needs port 47100 on the RO RevPi open to this RevPi (192.168.1.21); until then
// it retries quietly every second and the RO calls fail.
remote_link ro_revpi("192.168.1.20");

namespace {

// Electrical interfaces: (device id, interface type, PiCtory pin)
electrical_interface upper_tank_high_float_input(1, digital_in{}, "DigitalInput_4");             // MIO digital input 4
electrical_interface upper_tank_water_level_input(2, analog_current_in{}, "InputValue_1");  // 4-20 mA
electrical_interface irrigation_pump_pressure_output(47, analog_voltage_out{}, "OutputValue_1"); // 0-10 V pressure setpoint
electrical_interface well_4_pump_run(49, relay{}, "RelayOutput_4");                             // RevPi RO relay module, relay 4

// Devices wired to this RevPi
local_float_switch local_upper_tank_high_float(&upper_tank_high_float_input);
local_level_sensor local_upper_tank_water_level(&upper_tank_water_level_input, 0.6797f, 12.1797f);
// Pressure setpoint only (no run relay, speed or alarm wired): 0-100 -> 0-10 V
local_pump local_irrigation_pump(nullptr, nullptr, nullptr, &irrigation_pump_pressure_output, 0.0f, 100.0f);
// On/off only (run relay, no speed, alarm or pressure wired)
local_pump local_well_4_pump(nullptr, &well_4_pump_run, nullptr, nullptr);

// Shares the devices wired to this RevPi, and the WiseConn devices, with the other machines
device_server devices;

}  // namespace

// Public devices declared in config.h
float_switch& upper_tank_high_float = local_upper_tank_high_float;
level_sensor& upper_tank_water_level = local_upper_tank_water_level;
pump& irrigation_pump = local_irrigation_pump;
pump& well_4_pump = local_well_4_pump;

bool attachAllInterfaces() {
    bool ok = true;
    for (electrical_interface* e : {&upper_tank_high_float_input, &upper_tank_water_level_input, &irrigation_pump_pressure_output,
                                     &well_4_pump_run}) {
        ok = e->attach() && ok;
    }
    return ok;
}

bool addLocalDevices(line_server& server) {
    bool ok = devices.addDevice("upper_tank_high_float", local_upper_tank_high_float)
           && devices.addDevice("upper_tank_water_level", local_upper_tank_water_level)
           && devices.addDevice("irrigation_pump", local_irrigation_pump)
           && devices.addDevice("well_4_pump", local_well_4_pump)
           && addWiseconnDevices(devices);
    return ok && devices.registerWith(server);
}
