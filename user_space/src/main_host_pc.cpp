#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "config.h"
#include "ntfy.h"
#include "ntfy_config.h"

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

}  // namespace

int main() {
    attachAllInterfaces();
    if (!startWebUI()) {
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
        serviceWebUI(20);
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
