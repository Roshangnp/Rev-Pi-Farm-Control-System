#include "config.h"

#include "HAL/electronics.h"
#include "devices/local_devices/local_pump.h"
#include "devices/local_devices/local_float_switch.h"
#include "devices/local_devices/local_valve.h"
#include "devices/local_devices/local_pressure_transducer.h"
#include "devices/local_devices/local_tds_sensor.h"
#include "devices/local_devices/local_flow_sensor.h"
#include "devices/remote_devices/device_server.h"
#include "remote_link.h"
#include "devices/remote_devices/remote_pump.h"

// ---------------------------------------------------------------------------
// Test bench RevPi (192.168.1.20). Example wiring - one device per type.
// Pin names are PiCtory variable names.
// ---------------------------------------------------------------------------

namespace {

// Electrical interfaces: (device id, interface type, PiCtory pin)
electrical_interface feed_pump_run(1, relay{}, "Output_Bit_1");                     // relay board, relay 1
electrical_interface feed_pump_speed(2, analog_current_out{}, "OutputValue_1");     // AIO analog out 1, 4-20 mA
electrical_interface feed_pump_alarm(3, digital_in{}, "I_1");                       // DIO input 1
electrical_interface feed_tank_float_input(4, digital_in{}, "I_2");                 // DIO input 2
electrical_interface inlet_valve_solenoid(5, digital_out{}, "O_1");                 // DIO output 1
electrical_interface feed_pressure_input(6, analog_current_in{}, "InputValue_1"); // AIO input 1, 4-20 mA
electrical_interface permeate_tds_input(7, analog_voltage_in{}, "InputValue_2");    // AIO input 2, 0-10 V
electrical_interface permeate_flow_pulses(8, pulse_in{}, "Counter_3");              // DIO input 3, counter mode

// Devices wired to this RevPi
local_pump local_feed_pump(&feed_pump_speed, &feed_pump_run, &feed_pump_alarm, nullptr);
// A pump without speed control or alarm, e.g. a peristaltic dosing pump:
//   local_pump local_antiscalant_pump(nullptr, &antiscalant_pump_run, nullptr, nullptr);
local_float_switch local_feed_tank_float(&feed_tank_float_input);
local_valve local_inlet_valve(&inlet_valve_solenoid);
local_pressure_transducer local_feed_pressure(&feed_pressure_input, 0.0f, 300.0f);  // 0-300 psi
local_tds_sensor local_permeate_tds(&permeate_tds_input, 0.0f, 2000.0f);                  // 0-2000 ppm
local_flow_sensor local_permeate_flow(&permeate_flow_pulses, 75.0f);                      // 75 pulses per gallon

// Devices owned by another RevPi: one link per RevPi, then one remote_* per device,
// using the name that RevPi gave the device in its device server. Example:
//   remote_link revpi_b("192.168.1.117");
//   remote_pump remote_transfer_pump(revpi_b, "transfer_pump");
// and below:  pump& transfer_pump = remote_transfer_pump;

// Shares the devices wired to this RevPi with the other RevPis
device_server devices;

}  // namespace

// Public devices declared in config.h
pump& feed_pump = local_feed_pump;
float_switch& feed_tank_float = local_feed_tank_float;
valve& inlet_valve = local_inlet_valve;
pressure_transducer& feed_pressure = local_feed_pressure;
tds_sensor& permeate_tds = local_permeate_tds;
flow_sensor& permeate_flow = local_permeate_flow;

bool attachAllInterfaces() {
    bool ok = true;
    for (electrical_interface* e : {&feed_pump_run, &feed_pump_speed, &feed_pump_alarm,
                                    &feed_tank_float_input, &inlet_valve_solenoid,
                                    &feed_pressure_input, &permeate_tds_input,
                                    &permeate_flow_pulses}) {
        ok = e->attach() && ok;
    }
    return ok;
}

bool addLocalDevices(line_server& server) {
    // Only devices wired to this RevPi; other RevPis reach them by these names
    bool ok = devices.addDevice("feed_pump", local_feed_pump)
           && devices.addDevice("feed_tank_float", local_feed_tank_float)
           && devices.addDevice("inlet_valve", local_inlet_valve)
           && devices.addDevice("feed_pressure", local_feed_pressure)
           && devices.addDevice("permeate_tds", local_permeate_tds)
           && devices.addDevice("permeate_flow", local_permeate_flow);
    return ok && devices.registerWith(server);
}
