#include "config.h"

#include "HAL/electronics.h"
#include "devices/local_devices/local_pump.h"
#include "devices/local_devices/local_float_switch.h"
#include "devices/local_devices/local_valve.h"
#include "devices/local_devices/local_pressure_transducer.h"
#include "devices/local_devices/local_flow_sensor.h"
#include "devices/local_devices/local_conductivity_sensor.h"
#include "devices/local_devices/local_temperature_sensor.h"
#include "devices/local_devices/local_switched_load.h"
#include "devices/local_devices/local_switch.h"
#include "devices/remote_devices/device_server.h"
#include "devices/remote_devices/remote_float_switch.h"
#include "remote_link.h"

// ---------------------------------------------------------------------------
// RO RevPi (192.168.1.20). Wiring from API/hardware_setup.csv.
// ---------------------------------------------------------------------------

namespace {

// Electrical interfaces: (device id, interface type, PiCtory pin)
electrical_interface permeate_flow_pulses(3, pulse_in{}, "Counter_1_i03");
electrical_interface concentrate_flow_pulses(4, pulse_in{}, "Counter_2_i03");
electrical_interface feed_flow_pulses(5, pulse_in{}, "Counter_3_i03");
electrical_interface feed_tank_high_float_input(6, digital_in{}, "I_1");
electrical_interface high_ph_tank_float_input(7, digital_in{}, "I_2");
electrical_interface cip_tank_mid_float_input(8, digital_in{}, "I_3");
electrical_interface cip_tank_low_float_input(9, digital_in{}, "I_4");
electrical_interface cip_tank_high_float_input(10, digital_in{}, "I_5");
electrical_interface low_ph_tank_float_input(11, digital_in{}, "I_6");
electrical_interface asa_tank_low_float_input(12, digital_in{}, "I_7");
electrical_interface permeate_tank_high_float_input(13, digital_in{}, "I_8");
electrical_interface feed_tank_low_float_input(14, digital_in{}, "I_9");
electrical_interface feed_pump_alarm(15, digital_in{}, "I_10");
electrical_interface ro_pump_alarm(16, digital_in{}, "I_11");
electrical_interface media_outlet_pressure_input(17, analog_current_in{}, "InputValue_1");
electrical_interface media_inlet_pressure_input(18, analog_current_in{}, "InputValue_2");
electrical_interface concentrate_pressure_input(19, analog_current_in{}, "InputValue_3");
electrical_interface cip_inlet_pressure_input(20, analog_current_in{}, "InputValue_4");
electrical_interface interstage_pressure_input(21, analog_current_in{}, "InputValue_1_i07");
electrical_interface aqualine_outlet_pressure_input(22, analog_current_in{}, "InputValue_2_i07");
electrical_interface cip_outlet_pressure_input(23, analog_current_in{}, "InputValue_3_i07");
electrical_interface primary_pressure_input(24, analog_current_in{}, "InputValue_4_i07");
electrical_interface aqualine_inlet_pressure_input(25, analog_current_in{}, "InputValue_1_i08");
electrical_interface permeate_pressure_input(26, analog_current_in{}, "InputValue_2_i08");
electrical_interface air_pressure_input(27, analog_current_in{}, "InputValue_3_i08");
electrical_interface permeate_conductivity_input(28, analog_current_in{}, "InputValue_4_i08");     // REVIEW RANGE
electrical_interface feed_conductivity_input(29, analog_current_in{}, "InputValue_1_i09");         // REVIEW RANGE
electrical_interface cip_temp_input(30, analog_current_in{}, "InputValue_2_i09");                  // REVIEW RANGE
electrical_interface feed_pump_run(31, relay{}, "Output_Bit_1");
electrical_interface ro_pump_run(32, relay{}, "Output_Bit_2");
electrical_interface repressure_pump_run(33, relay{}, "Output_Bit_3");
electrical_interface lights_relay(34, relay{}, "Output_Bit_4");
electrical_interface cip_heater_relay(35, relay{}, "Output_Bit_5");
electrical_interface asa_pump_run(36, relay{}, "Output_Bit_6");
electrical_interface high_ph_pump_run(37, relay{}, "Output_Bit_8");
electrical_interface low_ph_pump_run(38, relay{}, "Output_Bit_9");
electrical_interface flush_valve_solenoid(39, relay{}, "Output_Bit_10");
electrical_interface cip_inlet_valve_solenoid(40, relay{}, "Output_Bit_11");
electrical_interface inlet_valve_solenoid(41, relay{}, "Output_Bit_12");
electrical_interface concentrate_cip_valve_solenoid(42, relay{}, "Output_Bit_13");
electrical_interface media_backwash_1_valve_solenoid(43, relay{}, "Output_Bit_14");
electrical_interface permeate_cip_valve_solenoid(44, relay{}, "Output_Bit_15");
electrical_interface media_backwash_2_valve_solenoid(45, relay{}, "Output_Bit_16");
electrical_interface ro_pump_speed(46, analog_current_out{}, "OutputValue_1");
electrical_interface repressure_toggle_switch_input(47, digital_in{}, "I_4_i03");
electrical_interface ro_toggle_switch_input(48, digital_in{}, "I_5_i03");

// Devices wired to this RevPi. Feed and RO pump alarms are the VFD alarm inputs;
// only the RO pump has speed control (4-20 mA to its VFD).
local_flow_sensor local_permeate_flow(&permeate_flow_pulses, 133.17f);
local_flow_sensor local_concentrate_flow(&concentrate_flow_pulses, 133.17f);
local_flow_sensor local_feed_flow(&feed_flow_pulses, 10.19f);
local_float_switch local_feed_tank_high_float(&feed_tank_high_float_input);
local_float_switch local_high_ph_tank_float(&high_ph_tank_float_input);
local_float_switch local_cip_tank_mid_float(&cip_tank_mid_float_input);
local_float_switch local_cip_tank_low_float(&cip_tank_low_float_input);
local_float_switch local_cip_tank_high_float(&cip_tank_high_float_input);
local_float_switch local_low_ph_tank_float(&low_ph_tank_float_input);
local_float_switch local_asa_tank_low_float(&asa_tank_low_float_input);
local_float_switch local_permeate_tank_high_float(&permeate_tank_high_float_input);
local_float_switch local_feed_tank_low_float(&feed_tank_low_float_input);
local_pressure_transducer local_media_outlet_pressure(&media_outlet_pressure_input, 0.0f, 100.0f);
local_pressure_transducer local_media_inlet_pressure(&media_inlet_pressure_input, 0.0f, 100.0f);
local_pressure_transducer local_concentrate_pressure(&concentrate_pressure_input, 0.0f, 500.0f);
local_pressure_transducer local_cip_inlet_pressure(&cip_inlet_pressure_input, 0.0f, 100.0f);
local_pressure_transducer local_interstage_pressure(&interstage_pressure_input, 0.0f, 500.0f);
local_pressure_transducer local_aqualine_outlet_pressure(&aqualine_outlet_pressure_input, 0.0f, 100.0f);
local_pressure_transducer local_cip_outlet_pressure(&cip_outlet_pressure_input, 0.0f, 100.0f);
local_pressure_transducer local_primary_pressure(&primary_pressure_input, 0.0f, 500.0f);
local_pressure_transducer local_aqualine_inlet_pressure(&aqualine_inlet_pressure_input, 0.0f, 100.0f);
local_pressure_transducer local_permeate_pressure(&permeate_pressure_input, 0.0f, 100.0f);
local_pressure_transducer local_air_pressure(&air_pressure_input, 0.0f, 100.0f);
local_conductivity_sensor local_permeate_conductivity(&permeate_conductivity_input, 0.0f, 50.0f);
local_conductivity_sensor local_feed_conductivity(&feed_conductivity_input, 0.0f, 5000.0f);
local_temperature_sensor local_cip_temp(&cip_temp_input, 32.0f, 212.0f);
local_pump local_feed_pump(nullptr, &feed_pump_run, &feed_pump_alarm, nullptr);
local_pump local_ro_pump(&ro_pump_speed, &ro_pump_run, &ro_pump_alarm, nullptr);
local_pump local_repressure_pump(nullptr, &repressure_pump_run, nullptr, nullptr);
local_switched_load local_lights(&lights_relay);
local_switched_load local_cip_heater(&cip_heater_relay);
local_pump local_asa_pump(nullptr, &asa_pump_run, nullptr, nullptr);
local_pump local_high_ph_pump(nullptr, &high_ph_pump_run, nullptr, nullptr);
local_pump local_low_ph_pump(nullptr, &low_ph_pump_run, nullptr, nullptr);
local_valve local_flush_valve(&flush_valve_solenoid);
local_valve local_cip_inlet_valve(&cip_inlet_valve_solenoid);
local_valve local_inlet_valve(&inlet_valve_solenoid);
local_valve local_concentrate_cip_valve(&concentrate_cip_valve_solenoid);
local_valve local_media_backwash_1_valve(&media_backwash_1_valve_solenoid);
local_valve local_permeate_cip_valve(&permeate_cip_valve_solenoid);
local_valve local_media_backwash_2_valve(&media_backwash_2_valve_solenoid);
local_switch local_repressure_toggle_switch(&repressure_toggle_switch_input);
local_switch local_ro_toggle_switch(&ro_toggle_switch_input);

// Devices owned by the upper tank RevPi
remote_link upper_tank_revpi("192.168.1.21");
remote_float_switch remote_upper_tank_high_float(upper_tank_revpi, "upper_tank_high_float");

// Shares the devices wired to this RevPi with the other machines
device_server devices;

}  // namespace

// Public devices declared in config.h
flow_sensor& permeate_flow = local_permeate_flow;
flow_sensor& concentrate_flow = local_concentrate_flow;
flow_sensor& feed_flow = local_feed_flow;
float_switch& feed_tank_high_float = local_feed_tank_high_float;
float_switch& high_ph_tank_float = local_high_ph_tank_float;
float_switch& cip_tank_mid_float = local_cip_tank_mid_float;
float_switch& cip_tank_low_float = local_cip_tank_low_float;
float_switch& cip_tank_high_float = local_cip_tank_high_float;
float_switch& low_ph_tank_float = local_low_ph_tank_float;
float_switch& asa_tank_low_float = local_asa_tank_low_float;
float_switch& permeate_tank_high_float = local_permeate_tank_high_float;
float_switch& feed_tank_low_float = local_feed_tank_low_float;
float_switch& upper_tank_high_float = remote_upper_tank_high_float;
pressure_transducer& media_outlet_pressure = local_media_outlet_pressure;
pressure_transducer& media_inlet_pressure = local_media_inlet_pressure;
pressure_transducer& concentrate_pressure = local_concentrate_pressure;
pressure_transducer& cip_inlet_pressure = local_cip_inlet_pressure;
pressure_transducer& interstage_pressure = local_interstage_pressure;
pressure_transducer& aqualine_outlet_pressure = local_aqualine_outlet_pressure;
pressure_transducer& cip_outlet_pressure = local_cip_outlet_pressure;
pressure_transducer& primary_pressure = local_primary_pressure;
pressure_transducer& aqualine_inlet_pressure = local_aqualine_inlet_pressure;
pressure_transducer& permeate_pressure = local_permeate_pressure;
pressure_transducer& air_pressure = local_air_pressure;
conductivity_sensor& permeate_conductivity = local_permeate_conductivity;
conductivity_sensor& feed_conductivity = local_feed_conductivity;
temperature_sensor& cip_temp = local_cip_temp;
pump& feed_pump = local_feed_pump;
pump& ro_pump = local_ro_pump;
pump& repressure_pump = local_repressure_pump;
switched_load& lights = local_lights;
switched_load& cip_heater = local_cip_heater;
pump& asa_pump = local_asa_pump;
pump& high_ph_pump = local_high_ph_pump;
pump& low_ph_pump = local_low_ph_pump;
valve& flush_valve = local_flush_valve;
valve& cip_inlet_valve = local_cip_inlet_valve;
valve& inlet_valve = local_inlet_valve;
valve& concentrate_cip_valve = local_concentrate_cip_valve;
valve& media_backwash_1_valve = local_media_backwash_1_valve;
valve& permeate_cip_valve = local_permeate_cip_valve;
valve& media_backwash_2_valve = local_media_backwash_2_valve;
toggle_switch& repressure_toggle_switch = local_repressure_toggle_switch;
toggle_switch& ro_toggle_switch = local_ro_toggle_switch;

bool attachAllInterfaces() {
    bool ok = true;
    for (electrical_interface* e : {&permeate_flow_pulses, &concentrate_flow_pulses, &feed_flow_pulses,
                                    &feed_tank_high_float_input, &high_ph_tank_float_input, &cip_tank_mid_float_input,
                                    &cip_tank_low_float_input, &cip_tank_high_float_input, &low_ph_tank_float_input,
                                    &asa_tank_low_float_input, &permeate_tank_high_float_input, &feed_tank_low_float_input,
                                    &feed_pump_alarm, &ro_pump_alarm, &media_outlet_pressure_input,
                                    &media_inlet_pressure_input, &concentrate_pressure_input, &cip_inlet_pressure_input,
                                    &interstage_pressure_input, &aqualine_outlet_pressure_input, &cip_outlet_pressure_input,
                                    &primary_pressure_input, &aqualine_inlet_pressure_input, &permeate_pressure_input,
                                    &air_pressure_input, &permeate_conductivity_input, &feed_conductivity_input,
                                    &cip_temp_input, &feed_pump_run, &ro_pump_run,
                                    &repressure_pump_run, &lights_relay, &cip_heater_relay,
                                    &asa_pump_run, &high_ph_pump_run, &low_ph_pump_run,
                                    &flush_valve_solenoid, &cip_inlet_valve_solenoid, &inlet_valve_solenoid,
                                    &concentrate_cip_valve_solenoid, &media_backwash_1_valve_solenoid, &permeate_cip_valve_solenoid,
                                    &media_backwash_2_valve_solenoid, &ro_pump_speed, &repressure_toggle_switch_input,
                                    &ro_toggle_switch_input}) {
        ok = e->attach() && ok;
    }
    return ok;
}

bool addLocalDevices(line_server& server) {
    bool ok = devices.addDevice("permeate_flow", local_permeate_flow)
           && devices.addDevice("concentrate_flow", local_concentrate_flow)
           && devices.addDevice("feed_flow", local_feed_flow)
           && devices.addDevice("feed_tank_high_float", local_feed_tank_high_float)
           && devices.addDevice("high_ph_tank_float", local_high_ph_tank_float)
           && devices.addDevice("cip_tank_mid_float", local_cip_tank_mid_float)
           && devices.addDevice("cip_tank_low_float", local_cip_tank_low_float)
           && devices.addDevice("cip_tank_high_float", local_cip_tank_high_float)
           && devices.addDevice("low_ph_tank_float", local_low_ph_tank_float)
           && devices.addDevice("asa_tank_low_float", local_asa_tank_low_float)
           && devices.addDevice("permeate_tank_high_float", local_permeate_tank_high_float)
           && devices.addDevice("feed_tank_low_float", local_feed_tank_low_float)
           && devices.addDevice("media_outlet_pressure", local_media_outlet_pressure)
           && devices.addDevice("media_inlet_pressure", local_media_inlet_pressure)
           && devices.addDevice("concentrate_pressure", local_concentrate_pressure)
           && devices.addDevice("cip_inlet_pressure", local_cip_inlet_pressure)
           && devices.addDevice("interstage_pressure", local_interstage_pressure)
           && devices.addDevice("aqualine_outlet_pressure", local_aqualine_outlet_pressure)
           && devices.addDevice("cip_outlet_pressure", local_cip_outlet_pressure)
           && devices.addDevice("primary_pressure", local_primary_pressure)
           && devices.addDevice("aqualine_inlet_pressure", local_aqualine_inlet_pressure)
           && devices.addDevice("permeate_pressure", local_permeate_pressure)
           && devices.addDevice("air_pressure", local_air_pressure)
           && devices.addDevice("permeate_conductivity", local_permeate_conductivity)
           && devices.addDevice("feed_conductivity", local_feed_conductivity)
           && devices.addDevice("cip_temp", local_cip_temp)
           && devices.addDevice("feed_pump", local_feed_pump)
           && devices.addDevice("ro_pump", local_ro_pump)
           && devices.addDevice("repressure_pump", local_repressure_pump)
           && devices.addDevice("lights", local_lights)
           && devices.addDevice("cip_heater", local_cip_heater)
           && devices.addDevice("asa_pump", local_asa_pump)
           && devices.addDevice("high_ph_pump", local_high_ph_pump)
           && devices.addDevice("low_ph_pump", local_low_ph_pump)
           && devices.addDevice("flush_valve", local_flush_valve)
           && devices.addDevice("cip_inlet_valve", local_cip_inlet_valve)
           && devices.addDevice("inlet_valve", local_inlet_valve)
           && devices.addDevice("concentrate_cip_valve", local_concentrate_cip_valve)
           && devices.addDevice("media_backwash_1_valve", local_media_backwash_1_valve)
           && devices.addDevice("permeate_cip_valve", local_permeate_cip_valve)
           && devices.addDevice("media_backwash_2_valve", local_media_backwash_2_valve)
           && devices.addDevice("repressure_toggle_switch", local_repressure_toggle_switch)
           && devices.addDevice("ro_toggle_switch", local_ro_toggle_switch);
    return ok && devices.registerWith(server);
}
