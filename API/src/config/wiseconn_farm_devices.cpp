#include "config.h"
#include "wiseconn_farm_devices.h"

#include <cstdlib>
#include <ctime>

#include "devices/wiseconn_devices/wiseconn_keys.h"
#include "devices/wiseconn_devices/wiseconn_link.h"
#include "devices/wiseconn_devices/wiseconn_soil_sensor.h"
#include "devices/wiseconn_devices/wiseconn_irrigation.h"
#include "wiseconn_keys_text.h"
#include "devices/remote_devices/device_server.h"

// ---------------------------------------------------------------------------
// WiseConn cloud devices, bound the same way on every machine (built into
// farm_api next to whichever config_<name>.cpp is chosen), so every program
// has the same WiseConn access. Read only by policy: never call
// scheduleIrrigation against the real API. Keys from wiseconn_keys.txt (built
// in by CMake); missing = the link is off and every read fails. Every config
// serves them on its device_server (addWiseconnDevices, from addLocalDevices).
// To be replaced by more RevPis eventually.
// ---------------------------------------------------------------------------

namespace {

// Keys built in at build time. Also puts this process in the farm's time zone
// (keys time_zone): wiseconn_link reads WiseConn's times as local time, and the
// RevPis run on UTC. Nothing else in the farm programs uses local time (the
// scheduler works on system_clock time points), so this changes no control timing.
wiseconn_keys loadKeysInFarmTimeZone() {
    wiseconn_keys keys = loadWiseconnKeys(WISECONN_KEYS_TEXT);
    if (!keys.time_zone.empty()) {
        setenv("TZ", keys.time_zone.c_str(), 1);
        tzset();
    }
    return keys;
}

// Defined in this order on purpose: the keys are parsed (and the time zone set)
// before the link and sensors use them.
const wiseconn_keys wiseconn_config = loadKeysInFarmTimeZone();
wiseconn_link wiseconn(wiseconn_config);
wiseconn_soil_sensor wiseconn_zone_7_soil(wiseconn, wiseconn_config.soil("zone_7_soil"));
wiseconn_soil_sensor wiseconn_zone_8_soil(wiseconn, wiseconn_config.soil("zone_8_soil"));
wiseconn_soil_sensor wiseconn_zone_10_3_soil(wiseconn, wiseconn_config.soil("zone_10_3_soil"));
// Irrigation zones: on/off from the WiseConn hydraulics list
wiseconn_irrigation wiseconn_valve_1_1(wiseconn, "valve_1_1", wiseconn_config.irrigation("valve_1_1"));
wiseconn_irrigation wiseconn_valve_1_2(wiseconn, "valve_1_2", wiseconn_config.irrigation("valve_1_2"));
wiseconn_irrigation wiseconn_valve_2_1(wiseconn, "valve_2_1", wiseconn_config.irrigation("valve_2_1"));
wiseconn_irrigation wiseconn_valve_3_1(wiseconn, "valve_3_1", wiseconn_config.irrigation("valve_3_1"));
wiseconn_irrigation wiseconn_valve_4_1(wiseconn, "valve_4_1", wiseconn_config.irrigation("valve_4_1"));
wiseconn_irrigation wiseconn_valve_5_1(wiseconn, "valve_5_1", wiseconn_config.irrigation("valve_5_1"));
wiseconn_irrigation wiseconn_valve_5_2(wiseconn, "valve_5_2", wiseconn_config.irrigation("valve_5_2"));
wiseconn_irrigation wiseconn_valve_5_3(wiseconn, "valve_5_3", wiseconn_config.irrigation("valve_5_3"));
wiseconn_irrigation wiseconn_valve_5_4(wiseconn, "valve_5_4", wiseconn_config.irrigation("valve_5_4"));
wiseconn_irrigation wiseconn_valve_6_1(wiseconn, "valve_6_1", wiseconn_config.irrigation("valve_6_1"));
wiseconn_irrigation wiseconn_valve_6_2(wiseconn, "valve_6_2", wiseconn_config.irrigation("valve_6_2"));
wiseconn_irrigation wiseconn_valve_7_1(wiseconn, "valve_7_1", wiseconn_config.irrigation("valve_7_1"));
wiseconn_irrigation wiseconn_valve_8_1(wiseconn, "valve_8_1", wiseconn_config.irrigation("valve_8_1"));
wiseconn_irrigation wiseconn_valve_9_1(wiseconn, "valve_9_1", wiseconn_config.irrigation("valve_9_1"));
wiseconn_irrigation wiseconn_valve_10_1(wiseconn, "valve_10_1", wiseconn_config.irrigation("valve_10_1"));
wiseconn_irrigation wiseconn_valve_10_2(wiseconn, "valve_10_2", wiseconn_config.irrigation("valve_10_2"));
wiseconn_irrigation wiseconn_valve_10_3(wiseconn, "valve_10_3", wiseconn_config.irrigation("valve_10_3"));
wiseconn_irrigation wiseconn_overhead_1(wiseconn, "overhead_1", wiseconn_config.irrigation("overhead_1"));
wiseconn_irrigation wiseconn_overhead_2(wiseconn, "overhead_2", wiseconn_config.irrigation("overhead_2"));
wiseconn_irrigation wiseconn_overhead_3(wiseconn, "overhead_3", wiseconn_config.irrigation("overhead_3"));
wiseconn_irrigation wiseconn_overhead_4(wiseconn, "overhead_4", wiseconn_config.irrigation("overhead_4"));
wiseconn_irrigation wiseconn_overhead_5(wiseconn, "overhead_5", wiseconn_config.irrigation("overhead_5"));
wiseconn_irrigation wiseconn_overhead_6(wiseconn, "overhead_6", wiseconn_config.irrigation("overhead_6"));
wiseconn_irrigation wiseconn_city_water_fill_valve(wiseconn, "city_water_fill_valve", wiseconn_config.irrigation("city_water_fill_valve"));

}  // namespace

// Public devices declared in config.h
soil_sensor& zone_7_soil = wiseconn_zone_7_soil;
soil_sensor& zone_8_soil = wiseconn_zone_8_soil;
soil_sensor& zone_10_3_soil = wiseconn_zone_10_3_soil;
irrigation& valve_1_1 = wiseconn_valve_1_1;
irrigation& valve_1_2 = wiseconn_valve_1_2;
irrigation& valve_2_1 = wiseconn_valve_2_1;
irrigation& valve_3_1 = wiseconn_valve_3_1;
irrigation& valve_4_1 = wiseconn_valve_4_1;
irrigation& valve_5_1 = wiseconn_valve_5_1;
irrigation& valve_5_2 = wiseconn_valve_5_2;
irrigation& valve_5_3 = wiseconn_valve_5_3;
irrigation& valve_5_4 = wiseconn_valve_5_4;
irrigation& valve_6_1 = wiseconn_valve_6_1;
irrigation& valve_6_2 = wiseconn_valve_6_2;
irrigation& valve_7_1 = wiseconn_valve_7_1;
irrigation& valve_8_1 = wiseconn_valve_8_1;
irrigation& valve_9_1 = wiseconn_valve_9_1;
irrigation& valve_10_1 = wiseconn_valve_10_1;
irrigation& valve_10_2 = wiseconn_valve_10_2;
irrigation& valve_10_3 = wiseconn_valve_10_3;
irrigation& overhead_1 = wiseconn_overhead_1;
irrigation& overhead_2 = wiseconn_overhead_2;
irrigation& overhead_3 = wiseconn_overhead_3;
irrigation& overhead_4 = wiseconn_overhead_4;
irrigation& overhead_5 = wiseconn_overhead_5;
irrigation& overhead_6 = wiseconn_overhead_6;
irrigation& city_water_fill_valve = wiseconn_city_water_fill_valve;

bool addWiseconnDevices(device_server& devices) {
    bool ok = true;
    ok = devices.addDevice("zone_7_soil", zone_7_soil) && ok;
    ok = devices.addDevice("zone_8_soil", zone_8_soil) && ok;
    ok = devices.addDevice("zone_10_3_soil", zone_10_3_soil) && ok;
    ok = devices.addDevice("valve_1_1", valve_1_1) && ok;
    ok = devices.addDevice("valve_1_2", valve_1_2) && ok;
    ok = devices.addDevice("valve_2_1", valve_2_1) && ok;
    ok = devices.addDevice("valve_3_1", valve_3_1) && ok;
    ok = devices.addDevice("valve_4_1", valve_4_1) && ok;
    ok = devices.addDevice("valve_5_1", valve_5_1) && ok;
    ok = devices.addDevice("valve_5_2", valve_5_2) && ok;
    ok = devices.addDevice("valve_5_3", valve_5_3) && ok;
    ok = devices.addDevice("valve_5_4", valve_5_4) && ok;
    ok = devices.addDevice("valve_6_1", valve_6_1) && ok;
    ok = devices.addDevice("valve_6_2", valve_6_2) && ok;
    ok = devices.addDevice("valve_7_1", valve_7_1) && ok;
    ok = devices.addDevice("valve_8_1", valve_8_1) && ok;
    ok = devices.addDevice("valve_9_1", valve_9_1) && ok;
    ok = devices.addDevice("valve_10_1", valve_10_1) && ok;
    ok = devices.addDevice("valve_10_2", valve_10_2) && ok;
    ok = devices.addDevice("valve_10_3", valve_10_3) && ok;
    ok = devices.addDevice("overhead_1", overhead_1) && ok;
    ok = devices.addDevice("overhead_2", overhead_2) && ok;
    ok = devices.addDevice("overhead_3", overhead_3) && ok;
    ok = devices.addDevice("overhead_4", overhead_4) && ok;
    ok = devices.addDevice("overhead_5", overhead_5) && ok;
    ok = devices.addDevice("overhead_6", overhead_6) && ok;
    ok = devices.addDevice("city_water_fill_valve", city_water_fill_valve) && ok;
    return ok;
}
