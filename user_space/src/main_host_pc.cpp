#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "config.h"
#include "line_server.h"
#include "ntfy.h"
#include "ntfy_config.h"
#include "procedure_config.h"
#include "ro_config.h"
#include "scheduler.h"

// Host PC (emerald-server): serves every device it binds (the RevPis' devices,
// passed on to their owners, and the WiseConn devices) and the RO ("ro",
// "ro_procedure", passed on to the RO RevPi) on port 47100, like the RevPis,
// but on 127.0.0.1 only: this machine has no firewall, and its clients (the
// console) run here. Also sends an ntfy push notification when a pump VFD
// alarms or the lights relay switches on (topic from ntfy_topic.txt).

// This machine's one scheduler (user space schedules tasks on it). A global, so
// it outlives anything that holds a reference to it.
scheduler sched;

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

    line_server server(LINE_SERVER_PORT, 20000, /*loopback_only=*/true);   // drops clients idle 20 s
    if (!addLocalDevices(server)) {
        std::fprintf(stderr, "Device server failed to start\n");
        return 1;
    }
    if (!addRO(server, sched)) std::fprintf(stderr, "RO server failed to register\n");
    addLocalProcedures(server);   // owns none here (ro_procedure is the RO RevPi's)
    if (!server.start()) {
        std::fprintf(stderr, "Device server failed to start (port %u taken?)\n", static_cast<unsigned>(LINE_SERVER_PORT));
        return 1;
    }

    ntfy_notifier notifier(NTFY_TOPIC, NTFY_SERVER);
    if (notifier.enabled()) std::printf("VFD alarm notifications to %s/%s\n", NTFY_SERVER, NTFY_TOPIC);
    else std::printf("Notifications off (no topic in ntfy_topic.txt)\n");
    std::fflush(stdout);

    std::vector<vfd_alarm> alarms = {{"Feed pump", feed_pump}, {"RO pump", ro_pump}};
    bool lights_were_on = false;
    auto next_check = clock_type::now();
    while (true) {
        auto now = clock_type::now();
        if (now >= next_check) {
            next_check = now + CHECK_INTERVAL;
            for (vfd_alarm& a : alarms) updateAlarm(a, now, notifier);

            // Once when the lights relay goes off -> on; a failed read changes nothing
            bool lights_on;
            if (lights.readOn(lights_on)) {
                if (lights_on && !lights_were_on) notifier.send("LIGHTS RELAY SWITCHED", "", 3, "");
                lights_were_on = lights_on;
            }
        }
        // Remote reads are cache lookups (remote_link polls in the background)

        if (!sched.update()) std::fprintf(stderr, "Scheduler: a task failed\n");
        updateRO();
        server.poll(20);   // handles requests, doubles as the loop sleep
    }
}
