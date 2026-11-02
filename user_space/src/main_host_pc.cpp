#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "config.h"
#include "ntfy.h"
#include "ntfy_config.h"
#include "web_ui.h"

// Host PC: serves the web UI for the devices it reaches on the RevPis, and sends
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

// Every device on the farm, in the order the page lists them
bool addAllDevices(web_ui& ui) {
    return ui.addDevice("upper_tank_high_float", upper_tank_high_float)
        && ui.addDevice("upper_tank_water_level", upper_tank_water_level)
        && ui.addDevice("permeate_flow", permeate_flow)
        && ui.addDevice("concentrate_flow", concentrate_flow)
        && ui.addDevice("feed_flow", feed_flow)
        && ui.addDevice("feed_tank_high_float", feed_tank_high_float)
        && ui.addDevice("high_ph_tank_float", high_ph_tank_float)
        && ui.addDevice("cip_tank_mid_float", cip_tank_mid_float)
        && ui.addDevice("cip_tank_low_float", cip_tank_low_float)
        && ui.addDevice("cip_tank_high_float", cip_tank_high_float)
        && ui.addDevice("low_ph_tank_float", low_ph_tank_float)
        && ui.addDevice("asa_tank_low_float", asa_tank_low_float)
        && ui.addDevice("permeate_tank_high_float", permeate_tank_high_float)
        && ui.addDevice("feed_tank_low_float", feed_tank_low_float)
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
        && ui.addDevice("media_backwash_2_valve", media_backwash_2_valve);
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
