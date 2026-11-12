#include "config.h"

#include "devices/remote_devices/remote_conductivity_sensor.h"
#include "devices/remote_devices/remote_float_switch.h"
#include "devices/remote_devices/remote_flow_sensor.h"
#include "devices/remote_devices/remote_irrigation.h"
#include "devices/remote_devices/remote_level_sensor.h"
#include "devices/remote_devices/remote_pressure_transducer.h"
#include "devices/remote_devices/remote_pump.h"
#include "devices/remote_devices/remote_soil_sensor.h"
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
//   FARM_CLIENT_LOCAL_RO / FARM_CLIENT_LOCAL_UPPER : this machine is that RevPi
// WiseConn devices are read from this machine's own main (127.0.0.1), which
// serves them, so a client needs no keys or internet and each machine polls
// WiseConn once. Device names are the config.h names, as each owner serves them.
// ---------------------------------------------------------------------------

#ifndef FARM_RO_HOST
#error "FARM_RO_HOST not set (API/api.cmake sets it for farm_client)"
#endif
#ifndef FARM_UPPER_HOST
#error "FARM_UPPER_HOST not set (API/api.cmake sets it for farm_client)"
#endif

// The RO RevPi connection, also used by ro_client_config.cpp (remote_ro), so its
// devices and the RO share one snapshot poll
remote_link ro_revpi(FARM_RO_HOST);

namespace {

remote_link upper_tank_revpi(FARM_UPPER_HOST);

// This machine's own main, for WiseConn: the RevPi connection when it is local,
// so the same main isn't polled twice
#if defined(FARM_CLIENT_LOCAL_RO)
remote_link& wiseconn_main = ro_revpi;
#elif defined(FARM_CLIENT_LOCAL_UPPER)
remote_link& wiseconn_main = upper_tank_revpi;
#else
remote_link local_main("127.0.0.1");
remote_link& wiseconn_main = local_main;
#endif

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

// WiseConn, through this machine's main
remote_soil_sensor remote_zone_7_soil(wiseconn_main, "zone_7_soil");
remote_soil_sensor remote_zone_8_soil(wiseconn_main, "zone_8_soil");
remote_soil_sensor remote_zone_10_3_soil(wiseconn_main, "zone_10_3_soil");
remote_irrigation remote_valve_1_1(wiseconn_main, "valve_1_1");
remote_irrigation remote_valve_1_2(wiseconn_main, "valve_1_2");
remote_irrigation remote_valve_2_1(wiseconn_main, "valve_2_1");
remote_irrigation remote_valve_3_1(wiseconn_main, "valve_3_1");
remote_irrigation remote_valve_4_1(wiseconn_main, "valve_4_1");
remote_irrigation remote_valve_5_1(wiseconn_main, "valve_5_1");
remote_irrigation remote_valve_5_2(wiseconn_main, "valve_5_2");
remote_irrigation remote_valve_5_3(wiseconn_main, "valve_5_3");
remote_irrigation remote_valve_5_4(wiseconn_main, "valve_5_4");
remote_irrigation remote_valve_6_1(wiseconn_main, "valve_6_1");
remote_irrigation remote_valve_6_2(wiseconn_main, "valve_6_2");
remote_irrigation remote_valve_7_1(wiseconn_main, "valve_7_1");
remote_irrigation remote_valve_8_1(wiseconn_main, "valve_8_1");
remote_irrigation remote_valve_9_1(wiseconn_main, "valve_9_1");
remote_irrigation remote_valve_10_1(wiseconn_main, "valve_10_1");
remote_irrigation remote_valve_10_2(wiseconn_main, "valve_10_2");
remote_irrigation remote_valve_10_3(wiseconn_main, "valve_10_3");
remote_irrigation remote_overhead_1(wiseconn_main, "overhead_1");
remote_irrigation remote_overhead_2(wiseconn_main, "overhead_2");
remote_irrigation remote_overhead_3(wiseconn_main, "overhead_3");
remote_irrigation remote_overhead_4(wiseconn_main, "overhead_4");
remote_irrigation remote_overhead_5(wiseconn_main, "overhead_5");
remote_irrigation remote_overhead_6(wiseconn_main, "overhead_6");
remote_irrigation remote_city_water_fill_valve(wiseconn_main, "city_water_fill_valve");
remote_flow_sensor remote_main_flow(wiseconn_main, "main_flow");
remote_flow_sensor remote_city_water_flow(wiseconn_main, "city_water_flow");
remote_flow_sensor remote_well_1_flow(wiseconn_main, "well_1_flow");
remote_flow_sensor remote_well_2_flow(wiseconn_main, "well_2_flow");
remote_flow_sensor remote_well_3_flow(wiseconn_main, "well_3_flow");
remote_flow_sensor remote_well_4_flow(wiseconn_main, "well_4_flow");
remote_flow_sensor remote_well_5_flow(wiseconn_main, "well_5_flow");

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
soil_sensor& zone_7_soil = remote_zone_7_soil;
soil_sensor& zone_8_soil = remote_zone_8_soil;
soil_sensor& zone_10_3_soil = remote_zone_10_3_soil;
irrigation& valve_1_1 = remote_valve_1_1;
irrigation& valve_1_2 = remote_valve_1_2;
irrigation& valve_2_1 = remote_valve_2_1;
irrigation& valve_3_1 = remote_valve_3_1;
irrigation& valve_4_1 = remote_valve_4_1;
irrigation& valve_5_1 = remote_valve_5_1;
irrigation& valve_5_2 = remote_valve_5_2;
irrigation& valve_5_3 = remote_valve_5_3;
irrigation& valve_5_4 = remote_valve_5_4;
irrigation& valve_6_1 = remote_valve_6_1;
irrigation& valve_6_2 = remote_valve_6_2;
irrigation& valve_7_1 = remote_valve_7_1;
irrigation& valve_8_1 = remote_valve_8_1;
irrigation& valve_9_1 = remote_valve_9_1;
irrigation& valve_10_1 = remote_valve_10_1;
irrigation& valve_10_2 = remote_valve_10_2;
irrigation& valve_10_3 = remote_valve_10_3;
irrigation& overhead_1 = remote_overhead_1;
irrigation& overhead_2 = remote_overhead_2;
irrigation& overhead_3 = remote_overhead_3;
irrigation& overhead_4 = remote_overhead_4;
irrigation& overhead_5 = remote_overhead_5;
irrigation& overhead_6 = remote_overhead_6;
irrigation& city_water_fill_valve = remote_city_water_fill_valve;
flow_sensor& main_flow = remote_main_flow;
flow_sensor& city_water_flow = remote_city_water_flow;
flow_sensor& well_1_flow = remote_well_1_flow;
flow_sensor& well_2_flow = remote_well_2_flow;
flow_sensor& well_3_flow = remote_well_3_flow;
flow_sensor& well_4_flow = remote_well_4_flow;
flow_sensor& well_5_flow = remote_well_5_flow;

// Nothing wired here
bool attachAllInterfaces() {
    return true;
}

// A client: shares nothing
bool addLocalDevices(line_server&) {
    return true;
}
