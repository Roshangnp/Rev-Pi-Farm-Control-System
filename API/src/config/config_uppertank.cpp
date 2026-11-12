#include "config.h"

#include "HAL/electronics.h"
#include "devices/local_devices/local_float_switch.h"
#include "devices/local_devices/local_level_sensor.h"
#include "devices/local_devices/local_pump.h"
#include "devices/remote_devices/device_server.h"
#include "wiseconn_farm_devices.h"
#include "devices/remote_devices/remote_conductivity_sensor.h"
#include "devices/remote_devices/remote_float_switch.h"
#include "devices/remote_devices/remote_flow_sensor.h"
#include "devices/remote_devices/remote_level_sensor.h"
#include "devices/remote_devices/remote_pressure_transducer.h"
#include "devices/remote_devices/remote_pump.h"
#include "devices/remote_devices/remote_switched_load.h"
#include "devices/remote_devices/remote_temperature_sensor.h"
#include "devices/remote_devices/remote_toggle_switch.h"
#include "devices/remote_devices/remote_valve.h"
#include "remote_link.h"

// ---------------------------------------------------------------------------
// Upper tank RevPi (192.168.1.21). Wiring from API/hardware_setup.csv.
// ---------------------------------------------------------------------------

// The one connection to the RO RevPi, used by its devices below and by
// ro_config_uppertank.cpp (remote_ro), so they share one snapshot poll.
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

// Devices owned by the RO RevPi
remote_flow_sensor remote_permeate_flow(ro_revpi, "permeate_flow");
remote_flow_sensor remote_concentrate_flow(ro_revpi, "concentrate_flow");
remote_flow_sensor remote_feed_flow(ro_revpi, "feed_flow");
remote_level_sensor remote_feed_tank_water_level(ro_revpi, "feed_tank_water_level");
remote_float_switch remote_high_ph_tank_float(ro_revpi, "high_ph_tank_float");
remote_float_switch remote_cip_tank_mid_float(ro_revpi, "cip_tank_mid_float");
remote_float_switch remote_cip_tank_low_float(ro_revpi, "cip_tank_low_float");
remote_float_switch remote_cip_tank_high_float(ro_revpi, "cip_tank_high_float");
remote_float_switch remote_low_ph_tank_float(ro_revpi, "low_ph_tank_float");
remote_float_switch remote_asa_tank_low_float(ro_revpi, "asa_tank_low_float");
remote_float_switch remote_permeate_tank_high_float(ro_revpi, "permeate_tank_high_float");
remote_float_switch remote_feed_tank_low_float(ro_revpi, "feed_tank_low_float");
remote_toggle_switch remote_repressure_toggle_switch(ro_revpi, "repressure_toggle_switch");
remote_toggle_switch remote_ro_toggle_switch(ro_revpi, "ro_toggle_switch");
remote_pressure_transducer remote_media_outlet_pressure(ro_revpi, "media_outlet_pressure");
remote_pressure_transducer remote_media_inlet_pressure(ro_revpi, "media_inlet_pressure");
remote_pressure_transducer remote_concentrate_pressure(ro_revpi, "concentrate_pressure");
remote_pressure_transducer remote_cip_inlet_pressure(ro_revpi, "cip_inlet_pressure");
remote_pressure_transducer remote_interstage_pressure(ro_revpi, "interstage_pressure");
remote_pressure_transducer remote_aqualine_outlet_pressure(ro_revpi, "aqualine_outlet_pressure");
remote_pressure_transducer remote_cip_outlet_pressure(ro_revpi, "cip_outlet_pressure");
remote_pressure_transducer remote_primary_pressure(ro_revpi, "primary_pressure");
remote_pressure_transducer remote_aqualine_inlet_pressure(ro_revpi, "aqualine_inlet_pressure");
remote_pressure_transducer remote_permeate_pressure(ro_revpi, "permeate_pressure");
remote_pressure_transducer remote_air_pressure(ro_revpi, "air_pressure");
remote_conductivity_sensor remote_permeate_conductivity(ro_revpi, "permeate_conductivity");
remote_conductivity_sensor remote_feed_conductivity(ro_revpi, "feed_conductivity");
remote_temperature_sensor remote_cip_temp(ro_revpi, "cip_temp");
remote_pump remote_feed_pump(ro_revpi, "feed_pump");
remote_pump remote_ro_pump(ro_revpi, "ro_pump");
remote_pump remote_repressure_pump(ro_revpi, "repressure_pump");
remote_switched_load remote_lights(ro_revpi, "lights");
remote_switched_load remote_cip_heater(ro_revpi, "cip_heater");
remote_pump remote_asa_pump(ro_revpi, "asa_pump");
remote_pump remote_high_ph_pump(ro_revpi, "high_ph_pump");
remote_pump remote_low_ph_pump(ro_revpi, "low_ph_pump");
remote_valve remote_flush_valve(ro_revpi, "flush_valve");
remote_valve remote_cip_inlet_valve(ro_revpi, "cip_inlet_valve");
remote_valve remote_inlet_valve(ro_revpi, "inlet_valve");
remote_valve remote_concentrate_cip_valve(ro_revpi, "concentrate_cip_valve");
remote_valve remote_media_backwash_1_valve(ro_revpi, "media_backwash_1_valve");
remote_valve remote_permeate_cip_valve(ro_revpi, "permeate_cip_valve");
remote_valve remote_media_backwash_2_valve(ro_revpi, "media_backwash_2_valve");

// Shares every device bound here (wired to this RevPi, the RO RevPi's, WiseConn)
// with the other machines and local clients
device_server devices;

}  // namespace

// Public devices declared in config.h
float_switch& upper_tank_high_float = local_upper_tank_high_float;
level_sensor& upper_tank_water_level = local_upper_tank_water_level;
pump& irrigation_pump = local_irrigation_pump;
pump& well_4_pump = local_well_4_pump;
flow_sensor& permeate_flow = remote_permeate_flow;
flow_sensor& concentrate_flow = remote_concentrate_flow;
flow_sensor& feed_flow = remote_feed_flow;
level_sensor& feed_tank_water_level = remote_feed_tank_water_level;
float_switch& high_ph_tank_float = remote_high_ph_tank_float;
float_switch& cip_tank_mid_float = remote_cip_tank_mid_float;
float_switch& cip_tank_low_float = remote_cip_tank_low_float;
float_switch& cip_tank_high_float = remote_cip_tank_high_float;
float_switch& low_ph_tank_float = remote_low_ph_tank_float;
float_switch& asa_tank_low_float = remote_asa_tank_low_float;
float_switch& permeate_tank_high_float = remote_permeate_tank_high_float;
float_switch& feed_tank_low_float = remote_feed_tank_low_float;
toggle_switch& repressure_toggle_switch = remote_repressure_toggle_switch;
toggle_switch& ro_toggle_switch = remote_ro_toggle_switch;
pressure_transducer& media_outlet_pressure = remote_media_outlet_pressure;
pressure_transducer& media_inlet_pressure = remote_media_inlet_pressure;
pressure_transducer& concentrate_pressure = remote_concentrate_pressure;
pressure_transducer& cip_inlet_pressure = remote_cip_inlet_pressure;
pressure_transducer& interstage_pressure = remote_interstage_pressure;
pressure_transducer& aqualine_outlet_pressure = remote_aqualine_outlet_pressure;
pressure_transducer& cip_outlet_pressure = remote_cip_outlet_pressure;
pressure_transducer& primary_pressure = remote_primary_pressure;
pressure_transducer& aqualine_inlet_pressure = remote_aqualine_inlet_pressure;
pressure_transducer& permeate_pressure = remote_permeate_pressure;
pressure_transducer& air_pressure = remote_air_pressure;
conductivity_sensor& permeate_conductivity = remote_permeate_conductivity;
conductivity_sensor& feed_conductivity = remote_feed_conductivity;
temperature_sensor& cip_temp = remote_cip_temp;
pump& feed_pump = remote_feed_pump;
pump& ro_pump = remote_ro_pump;
pump& repressure_pump = remote_repressure_pump;
switched_load& lights = remote_lights;
switched_load& cip_heater = remote_cip_heater;
pump& asa_pump = remote_asa_pump;
pump& high_ph_pump = remote_high_ph_pump;
pump& low_ph_pump = remote_low_ph_pump;
valve& flush_valve = remote_flush_valve;
valve& cip_inlet_valve = remote_cip_inlet_valve;
valve& inlet_valve = remote_inlet_valve;
valve& concentrate_cip_valve = remote_concentrate_cip_valve;
valve& media_backwash_1_valve = remote_media_backwash_1_valve;
valve& permeate_cip_valve = remote_permeate_cip_valve;
valve& media_backwash_2_valve = remote_media_backwash_2_valve;

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
           && devices.addDevice("permeate_flow", permeate_flow)
           && devices.addDevice("concentrate_flow", concentrate_flow)
           && devices.addDevice("feed_flow", feed_flow)
           && devices.addDevice("feed_tank_water_level", feed_tank_water_level)
           && devices.addDevice("high_ph_tank_float", high_ph_tank_float)
           && devices.addDevice("cip_tank_mid_float", cip_tank_mid_float)
           && devices.addDevice("cip_tank_low_float", cip_tank_low_float)
           && devices.addDevice("cip_tank_high_float", cip_tank_high_float)
           && devices.addDevice("low_ph_tank_float", low_ph_tank_float)
           && devices.addDevice("asa_tank_low_float", asa_tank_low_float)
           && devices.addDevice("permeate_tank_high_float", permeate_tank_high_float)
           && devices.addDevice("feed_tank_low_float", feed_tank_low_float)
           && devices.addDevice("repressure_toggle_switch", repressure_toggle_switch)
           && devices.addDevice("ro_toggle_switch", ro_toggle_switch)
           && devices.addDevice("media_outlet_pressure", media_outlet_pressure)
           && devices.addDevice("media_inlet_pressure", media_inlet_pressure)
           && devices.addDevice("concentrate_pressure", concentrate_pressure)
           && devices.addDevice("cip_inlet_pressure", cip_inlet_pressure)
           && devices.addDevice("interstage_pressure", interstage_pressure)
           && devices.addDevice("aqualine_outlet_pressure", aqualine_outlet_pressure)
           && devices.addDevice("cip_outlet_pressure", cip_outlet_pressure)
           && devices.addDevice("primary_pressure", primary_pressure)
           && devices.addDevice("aqualine_inlet_pressure", aqualine_inlet_pressure)
           && devices.addDevice("permeate_pressure", permeate_pressure)
           && devices.addDevice("air_pressure", air_pressure)
           && devices.addDevice("permeate_conductivity", permeate_conductivity)
           && devices.addDevice("feed_conductivity", feed_conductivity)
           && devices.addDevice("cip_temp", cip_temp)
           && devices.addDevice("feed_pump", feed_pump)
           && devices.addDevice("ro_pump", ro_pump)
           && devices.addDevice("repressure_pump", repressure_pump)
           && devices.addDevice("lights", lights)
           && devices.addDevice("cip_heater", cip_heater)
           && devices.addDevice("asa_pump", asa_pump)
           && devices.addDevice("high_ph_pump", high_ph_pump)
           && devices.addDevice("low_ph_pump", low_ph_pump)
           && devices.addDevice("flush_valve", flush_valve)
           && devices.addDevice("cip_inlet_valve", cip_inlet_valve)
           && devices.addDevice("inlet_valve", inlet_valve)
           && devices.addDevice("concentrate_cip_valve", concentrate_cip_valve)
           && devices.addDevice("media_backwash_1_valve", media_backwash_1_valve)
           && devices.addDevice("permeate_cip_valve", permeate_cip_valve)
           && devices.addDevice("media_backwash_2_valve", media_backwash_2_valve)
           && addWiseconnDevices(devices);
    return ok && devices.registerWith(server);
}
