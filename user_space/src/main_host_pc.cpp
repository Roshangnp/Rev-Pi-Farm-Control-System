#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "config.h"
#include "ntfy.h"
#include "ntfy_config.h"
#include "ro_config.h"
#include "web_ui.h"

// Host PC: serves the web UI for the devices it reaches on the RevPis and for the
// RO controller (state and procedure dropdowns), and sends
// an ntfy push notification when a pump VFD alarms (topic from ntfy_topic.txt).
namespace {

using clock_type = std::chrono::steady_clock;

constexpr auto CHECK_INTERVAL = std::chrono::seconds(5);
constexpr auto ALARM_HOLD = std::chrono::seconds(10);  // alarm on this long before notifying
constexpr auto CLEAR_HOLD = std::chrono::seconds(10);  // alarm off this long before "cleared"

// One pump's VFD alarm. The alarm has to read on for ALARM_HOLD before the
// notification goes out, and off for CLEAR_HOLD before "cleared" does, so a
// single bad read never notifies. A failed read changes nothing.
struct vfd_alarm{
    std::string name;
    pump& device;

    bool active = false;
    bool pending = false;
    clock_type::time_point pending_since{};
};

void logLine(const std::string& text) {
    std::printf("%s\n", text.c_str());
    std::fflush(stdout);
}

// readAlarm() also returns true when the RevPi can't be reached, so a read
// before and after it has to succeed for the alarm to count. False = unknown.
bool readVfdAlarm(pump& device, bool& alarm) {
    bool on;
    if (!device.readOn(on)) return false;
    alarm = device.readAlarm();
    return device.readOn(on);
}

void updateAlarm(vfd_alarm& a, clock_type::time_point now, ntfy_notifier& notifier) {
    bool alarm;
    if (!readVfdAlarm(a.device, alarm) || alarm == a.active) {
        a.pending = false;
        return;
    }
    if (!a.pending) {
        a.pending = true;
        a.pending_since = now;
    }
    if (now - a.pending_since < (alarm ? ALARM_HOLD : CLEAR_HOLD)) return;

    a.active = alarm;
    a.pending = false;
    std::string title = a.name + " VFD alarm";
    if (alarm) {
        logLine("ALERT: " + title);
        notifier.send(title, "The " + a.name + " VFD alarm input is on: the drive has tripped.", 5, "rotating_light");
    } else {
        logLine("cleared: " + title);
        notifier.send("Cleared: " + title, "The " + a.name + " VFD alarm is off.", 3, "white_check_mark");
    }
}

// The RO controller on the RO RevPi (ro_config.h, through remote_ro), as a
// dropdown on the page. Setting a state or running a procedure switches real
// pumps and valves there; the RevPi keeps running it if this program stops.
class ro_state_selector : public state_selector{
    public:
    std::vector<std::string> states() const override {
        std::vector<std::string> names;
        for (RO_state s : ALL_RO_STATES) names.push_back(toString(s));
        return names;
    }
    bool readState(std::string& state) override {
        RO_state s;
        if (!readROState(s)) return false;
        state = toString(s);
        return true;
    }
    bool setState(const std::string& state) override {
        RO_state s;
        return fromString(state, s) && setROState(s);
    }
    bool readSettling(bool& settling) override {
        return readROSettling(settling);
    }
    std::vector<std::string> procedures() const override {
        std::vector<std::string> names;
        for (procedure_types p : ALL_PROCEDURE_TYPES) names.push_back(toString(p));
        return names;
    }
    bool runProcedure(const std::string& procedure) override {
        procedure_types p;
        return fromString(procedure, p) && runROProcedure(p);
    }
    bool stopProcedure() override {
        return stopROProcedure();
    }
};

ro_state_selector ro_controller;

// Every device on the farm, in the order the page lists them
bool addAllDevices(web_ui& ui) {
    return ui.addDevice("ro", ro_controller)
        && ui.addDevice("upper_tank_high_float", upper_tank_high_float)
        && ui.addDevice("upper_tank_water_level", upper_tank_water_level)
        && ui.addDevice("irrigation_pump", irrigation_pump)
        && ui.addDevice("well_4_pump", well_4_pump)
        && ui.addDevice("permeate_flow", permeate_flow)
        && ui.addDevice("concentrate_flow", concentrate_flow)
        && ui.addDevice("feed_flow", feed_flow)
        && ui.addDevice("feed_tank_water_level", feed_tank_water_level)
        && ui.addDevice("high_ph_tank_float", high_ph_tank_float)
        && ui.addDevice("cip_tank_mid_float", cip_tank_mid_float)
        && ui.addDevice("cip_tank_low_float", cip_tank_low_float)
        && ui.addDevice("cip_tank_high_float", cip_tank_high_float)
        && ui.addDevice("low_ph_tank_float", low_ph_tank_float)
        && ui.addDevice("asa_tank_low_float", asa_tank_low_float)
        && ui.addDevice("permeate_tank_high_float", permeate_tank_high_float)
        && ui.addDevice("feed_tank_low_float", feed_tank_low_float)
        && ui.addDevice("repressure_toggle_switch", repressure_toggle_switch)
        && ui.addDevice("ro_toggle_switch", ro_toggle_switch)
        && ui.addDevice("media_outlet_pressure", media_outlet_pressure)
        && ui.addDevice("media_inlet_pressure", media_inlet_pressure)
        && ui.addDevice("concentrate_pressure", concentrate_pressure)
        && ui.addDevice("cip_inlet_pressure", cip_inlet_pressure)
        && ui.addDevice("interstage_pressure", interstage_pressure)
        && ui.addDevice("aqualine_outlet_pressure", aqualine_outlet_pressure)
        && ui.addDevice("cip_outlet_pressure", cip_outlet_pressure)
        && ui.addDevice("primary_pressure", primary_pressure)
        && ui.addDevice("aqualine_inlet_pressure", aqualine_inlet_pressure)
        && ui.addDevice("permeate_pressure", permeate_pressure)
        && ui.addDevice("air_pressure", air_pressure)
        && ui.addDevice("permeate_conductivity", permeate_conductivity)
        && ui.addDevice("feed_conductivity", feed_conductivity)
        && ui.addDevice("cip_temp", cip_temp)
        && ui.addDevice("feed_pump", feed_pump)
        && ui.addDevice("ro_pump", ro_pump)
        && ui.addDevice("repressure_pump", repressure_pump)
        && ui.addDevice("lights", lights)
        && ui.addDevice("cip_heater", cip_heater)
        && ui.addDevice("asa_pump", asa_pump)
        && ui.addDevice("high_ph_pump", high_ph_pump)
        && ui.addDevice("low_ph_pump", low_ph_pump)
        && ui.addDevice("flush_valve", flush_valve)
        && ui.addDevice("cip_inlet_valve", cip_inlet_valve)
        && ui.addDevice("inlet_valve", inlet_valve)
        && ui.addDevice("concentrate_cip_valve", concentrate_cip_valve)
        && ui.addDevice("media_backwash_1_valve", media_backwash_1_valve)
        && ui.addDevice("permeate_cip_valve", permeate_cip_valve)
        && ui.addDevice("media_backwash_2_valve", media_backwash_2_valve)
        && ui.addDevice("zone_7_soil", zone_7_soil, {"4 in", "8 in", "12 in"})
        && ui.addDevice("zone_8_soil", zone_8_soil, {"4 in", "8 in", "12 in"})
        && ui.addDevice("zone_10_3_soil", zone_10_3_soil, {"4 in", "8 in", "12 in"})
        // WiseConn irrigation zones: on/off indicator
        && ui.addDevice("valve_1_1", valve_1_1)
        && ui.addDevice("valve_1_2", valve_1_2)
        && ui.addDevice("valve_2_1", valve_2_1)
        && ui.addDevice("valve_3_1", valve_3_1)
        && ui.addDevice("valve_4_1", valve_4_1)
        && ui.addDevice("valve_5_1", valve_5_1)
        && ui.addDevice("valve_5_2", valve_5_2)
        && ui.addDevice("valve_5_3", valve_5_3)
        && ui.addDevice("valve_5_4", valve_5_4)
        && ui.addDevice("valve_6_1", valve_6_1)
        && ui.addDevice("valve_6_2", valve_6_2)
        && ui.addDevice("valve_7_1", valve_7_1)
        && ui.addDevice("valve_8_1", valve_8_1)
        && ui.addDevice("valve_9_1", valve_9_1)
        && ui.addDevice("valve_10_1", valve_10_1)
        && ui.addDevice("valve_10_2", valve_10_2)
        && ui.addDevice("valve_10_3", valve_10_3)
        && ui.addDevice("overhead_1", overhead_1)
        && ui.addDevice("overhead_2", overhead_2)
        && ui.addDevice("overhead_3", overhead_3)
        && ui.addDevice("overhead_4", overhead_4)
        && ui.addDevice("overhead_5", overhead_5)
        && ui.addDevice("overhead_6", overhead_6)
        && ui.addDevice("city_water_fill_valve", city_water_fill_valve);
}

}  // namespace

int main() {
    attachAllInterfaces();

    // Open to the LAN, password protected (webui_password.txt); "127.0.0.1" = this PC only
    web_ui ui(WEB_UI_PORT, "0.0.0.0");
    if (!addAllDevices(ui) || !ui.start()) {
        std::fprintf(stderr, "Web UI failed to start (no password built in, or port 8080 in use?)\n");
        return 1;
    }
    std::printf("Web UI on http://<this PC's IP>:8080\n");

    ntfy_notifier notifier(NTFY_TOPIC, NTFY_SERVER);
    if (notifier.enabled()) std::printf("VFD alarm notifications to %s/%s\n", NTFY_SERVER, NTFY_TOPIC);
    else std::printf("Notifications off (no topic in ntfy_topic.txt)\n");
    std::fflush(stdout);

    std::vector<vfd_alarm> alarms = {{"Feed pump", feed_pump}, {"RO pump", ro_pump}};
    bool lights_were_on = false;
    auto next_check = clock_type::now();
    while (true) {
        ui.poll(20);
        if (clock_type::now() >= next_check) {
            next_check = clock_type::now() + CHECK_INTERVAL;
            auto now = clock_type::now();
            for (vfd_alarm& a : alarms) updateAlarm(a, now, notifier);

            // Once when the lights relay goes off -> on; a failed read changes nothing
            bool lights_on;
            if (lights.readOn(lights_on)) {
                if (lights_on && !lights_were_on) notifier.send("LIGHTS RELAY SWITCHED", "", 3, "");
                lights_were_on = lights_on;
            }
        }
    }
}
