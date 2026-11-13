#include "config.h"

#include "devices/remote_devices/remote_flow_sensor.h"
#include "devices/remote_devices/remote_irrigation.h"
#include "devices/remote_devices/remote_soil_sensor.h"
#include "remote_link.h"

// ---------------------------------------------------------------------------
// WiseConn devices on every machine except their owner. emerald-server's
// main_host_pc owns them (wiseconn_farm_devices.cpp, built only into the host_pc
// farm): it alone polls WiseConn and holds every irrigation booking, so a cancel
// from any machine sees them all. Everywhere else (the RevPis, and every
// ro_console through farm_client) they are remote_* proxies on the connection to
// emerald-server, like the RevPis' devices on each other. Built next to
// config_ro.cpp, config_uppertank.cpp and client_config.cpp, which define host_pc.
// scheduleIrrigation / cancelIrrigation go to the owner's device, so they START /
// CANCEL REAL IRRIGATIONS like calling them there.
// ---------------------------------------------------------------------------

// The connection to emerald-server, defined by this machine's config and shared
// with its procedure config (the WiseConn procedures are owned there too)
extern remote_link host_pc;

namespace {

remote_soil_sensor remote_zone_7_soil(host_pc, "zone_7_soil");
remote_soil_sensor remote_zone_8_soil(host_pc, "zone_8_soil");
remote_soil_sensor remote_zone_10_3_soil(host_pc, "zone_10_3_soil");
remote_irrigation remote_valve_1_1(host_pc, "valve_1_1");
remote_irrigation remote_valve_1_2(host_pc, "valve_1_2");
remote_irrigation remote_valve_2_1(host_pc, "valve_2_1");
remote_irrigation remote_valve_3_1(host_pc, "valve_3_1");
remote_irrigation remote_valve_4_1(host_pc, "valve_4_1");
remote_irrigation remote_valve_5_1(host_pc, "valve_5_1");
remote_irrigation remote_valve_5_2(host_pc, "valve_5_2");
remote_irrigation remote_valve_5_3(host_pc, "valve_5_3");
remote_irrigation remote_valve_5_4(host_pc, "valve_5_4");
remote_irrigation remote_valve_6_1(host_pc, "valve_6_1");
remote_irrigation remote_valve_6_2(host_pc, "valve_6_2");
remote_irrigation remote_valve_7_1(host_pc, "valve_7_1");
remote_irrigation remote_valve_8_1(host_pc, "valve_8_1");
remote_irrigation remote_valve_9_1(host_pc, "valve_9_1");
remote_irrigation remote_valve_10_1(host_pc, "valve_10_1");
remote_irrigation remote_valve_10_2(host_pc, "valve_10_2");
remote_irrigation remote_valve_10_3(host_pc, "valve_10_3");
remote_irrigation remote_overhead_1(host_pc, "overhead_1");
remote_irrigation remote_overhead_2(host_pc, "overhead_2");
remote_irrigation remote_overhead_3(host_pc, "overhead_3");
remote_irrigation remote_overhead_4(host_pc, "overhead_4");
remote_irrigation remote_overhead_5(host_pc, "overhead_5");
remote_irrigation remote_overhead_6(host_pc, "overhead_6");
remote_irrigation remote_city_water_fill_valve(host_pc, "city_water_fill_valve");
remote_flow_sensor remote_main_flow(host_pc, "main_flow");
remote_flow_sensor remote_city_water_flow(host_pc, "city_water_flow");
remote_flow_sensor remote_well_1_flow(host_pc, "well_1_flow");
remote_flow_sensor remote_well_2_flow(host_pc, "well_2_flow");
remote_flow_sensor remote_well_3_flow(host_pc, "well_3_flow");
remote_flow_sensor remote_well_4_flow(host_pc, "well_4_flow");
remote_flow_sensor remote_well_5_flow(host_pc, "well_5_flow");

}  // namespace

// Public devices declared in config.h
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
