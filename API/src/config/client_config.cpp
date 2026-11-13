#include "config.h"

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
// Client config (farm_client, linked by ro_console): every config.h device is a
// remote_* on a connection to its owner, like config_host_pc.cpp, and nothing is
// local or served. Built for the machine in DEVICE_CONFIG, which only decides
// the addresses (set by API/api.cmake):
//   FARM_RO_HOST, FARM_UPPER_HOST : 127.0.0.1 for this machine's own RevPi (so its
//                                   console keeps working with the network down),
//                                   the LAN address for the other one
//   FARM_HOST_PC_HOST : emerald-server, which owns the WiseConn devices and the
//                       WiseConn procedures (127.0.0.1 in its own build)
// WiseConn devices are bound in wiseconn_remote_devices.cpp, on host_pc, so a
// client needs no keys or internet. Device names are the config.h names, as each
// owner serves them.
// ---------------------------------------------------------------------------

#ifndef FARM_RO_HOST
#error "FARM_RO_HOST not set (API/api.cmake sets it for farm_client)"
#endif
#ifndef FARM_UPPER_HOST
#error "FARM_UPPER_HOST not set (API/api.cmake sets it for farm_client)"
#endif
#ifndef FARM_HOST_PC_HOST
#error "FARM_HOST_PC_HOST not set (API/api.cmake sets it for farm_client)"
#endif

// The RO RevPi connection, also used by ro_client_config.cpp (remote_ro), so its
// devices and the RO share one snapshot poll
remote_link ro_revpi(FARM_RO_HOST);

// The emerald-server connection, for the WiseConn devices
// (wiseconn_remote_devices.cpp) and the WiseConn procedures
// (procedure_client_config.cpp), so they share one snapshot poll
remote_link host_pc(FARM_HOST_PC_HOST);

namespace {

remote_link upper_tank_revpi(FARM_UPPER_HOST);

remote_float_switch remote_upper_tank_high_float(upper_tank_revpi, "upper_tank_high_float");
remote_level_sensor remote_upper_tank_water_level(upper_tank_revpi, "upper_tank_water_level");
remote_pump remote_irrigation_pump(upper_tank_revpi, "irrigation_pump");
remote_pump remote_well_4_pump(upper_tank_revpi, "well_4_pump");
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

}  // namespace

// Public devices declared in config.h
float_switch& upper_tank_high_float = remote_upper_tank_high_float;
level_sensor& upper_tank_water_level = remote_upper_tank_water_level;
pump& irrigation_pump = remote_irrigation_pump;
pump& well_4_pump = remote_well_4_pump;
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

// Nothing wired here
bool attachAllInterfaces() {
    return true;
}

// A client: shares nothing
bool addLocalDevices(line_server&) {
    return true;
}
