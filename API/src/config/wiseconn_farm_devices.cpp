#include "config.h"

#include <cstdlib>
#include <ctime>

#include "devices/wiseconn_devices/wiseconn_keys.h"
#include "devices/wiseconn_devices/wiseconn_link.h"
#include "devices/wiseconn_devices/wiseconn_soil_sensor.h"
#include "devices/wiseconn_devices/wiseconn_irrigation.h"
#include "wiseconn_keys_text.h"

// ---------------------------------------------------------------------------
// WiseConn cloud devices, bound the same way on every machine (built into
// farm_api next to whichever config_<name>.cpp is chosen), so every program
// has the same WiseConn access. Read only by policy: never call
// scheduleIrrigation against the real API. Keys from wiseconn_keys.txt (built
// in by CMake); missing = the link is off and every read fails. Not registered
// on any device_server. To be replaced by more RevPis eventually.
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
