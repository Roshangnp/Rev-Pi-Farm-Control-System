#include <cstdio>
#include "config.h"
#include "line_server.h"
#include "ro_config.h"
#include "scheduler.h"

// This machine's one scheduler: procedures (ro) and user space schedule tasks on
// it. Global so it outlives the RO controller (a function-local static created
// later, in addRO, so destroyed first: its procedure removes tasks from here).
scheduler sched;

void schedule_RO_operation(){
    condition_id feed_tank_low_submerged = sched.addCondition(condition::when("feed tank low float submerged", [] { bool s; return feed_tank_low_float.readSubmerged(s) && s; }));
    condition_id product_tank_high_dry = sched.addCondition(condition::when("product tank high float not submerged", [] { bool s; return permeate_tank_high_float.readSubmerged(s) && !s; }));
    condition_id upper_tank_high_dry = sched.addCondition(condition::when("upper tank high float not submerged", [] { bool s; return upper_tank_high_float.readSubmerged(s) && !s; }));
    condition_id upper_tank_high_submerged = sched.addCondition(condition::when("upper tank high float submerged", [] { bool s; return upper_tank_high_float.readSubmerged(s) && s; }));
    condition_id upper_tank_high_read_failed = sched.addCondition(condition::when("upper tank high float read failed", [] { bool s; return !upper_tank_high_float.readSubmerged(s); }));
    condition_id repressure_toggle_on = sched.addCondition(condition::when("repressure toggle switch on", [] { bool on; return repressure_toggle_switch.readValue(on) && on; }));
    condition_id ro_toggle_on = sched.addCondition(condition::when("RO toggle switch on", [] { bool on; return ro_toggle_switch.readValue(on) && on; }));
    condition_id repressure_pump_off = sched.addCondition(condition::when("repressure pump off", [] { bool on; return repressure_pump.readOn(on) && !on; }));

    task_id id;
    sched.addTask({"repressure pump on: upper tank not full", [] { return repressure_pump.setOn(); }, {upper_tank_high_dry, repressure_pump_off}, task_mode::ON_RISING}, id);
    sched.addTask({"repressure pump off: upper tank full", [] { return repressure_pump.setOff(); }, {upper_tank_high_submerged}, task_mode::ON_RISING}, id);
    sched.addTask({"repressure pump off: upper tank float read failed", [] { return repressure_pump.setOff(); }, {upper_tank_high_read_failed}, task_mode::ON_RISING}, id);
}

// RO RevPi: attaches its I/O, shares its devices with the other machines, and
// runs the RO controller (state set remotely, e.g. from the web UI via remote_ro).
int main() {
    if (!attachAllInterfaces()) std::fprintf(stderr, "Some interfaces failed to attach (check PiCtory pin names)\n");

    line_server server;   // port 47100, drops clients idle 20 s
    if (!addLocalDevices(server)) {
        std::fprintf(stderr, "Device server failed to start\n");
        return 1;
    }
    // Not fatal: the device server keeps running so the outputs can still be read and switched
    if (!addRO(server, sched)) std::fprintf(stderr, "RO controller failed to start cleanly\n");
    schedule_RO_operation();   // after addRO: the RO is OFF before any scheduled task can run
    if (!server.start()) {
        std::fprintf(stderr, "Device server failed to start\n");
        return 1;
    }

    while (true) {
        // Rolling flow averages: readFlow() only returns what these compute
        permeate_flow.update();
        concentrate_flow.update();
        feed_flow.update();

        if (!sched.update()) std::fprintf(stderr, "Scheduler: a task failed\n");   // procedure steps
        if (!updateRO()) std::fprintf(stderr, "RO controller: starting a pump failed\n");
        server.poll(20);   // handles remote requests, doubles as the loop sleep
    }
}
