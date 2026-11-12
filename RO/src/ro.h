#pragma once

#include <chrono>

#include "config.h"
#include "procedure.h"   // procedure/include
#include "scheduler.h"
#include "types.h"

class ro{
    public:
    // Starts in RO_state::OFF without touching any outputs. sched is the
    // machine's one scheduler (owned by main, which runs its update()); procedures
    // are scheduled on it, so it must outlive this ro
    explicit ro(scheduler& sched);
    // Drives the outputs to the OFF state (pumps off, then valves closed)
    ~ro();

    // checks in the API/include/config.h header and makes sure all RO devices are declared (everything on the RO revpi)
    // not a physical ping test, more of a build/compiling level test to make sure the headers were included. because this class
    // assumes all the deives are included and named correctly
    bool checkAllDevices();

    // A device that can't be read reads as false (valves, pumps) or NaN (pressures, flows)
    valve_state readValveState();
    pump_state readPumpState();
    pressure_state<float> readPressureState();
    flow_state<float> readFlowState();

    // Apply valvesFor / pumpsFor of the current state. Every device is attempted;
    // false if any of them failed. While pumps are stopping, setValveState() does
    // nothing (update() moves the valves); while stopping or settling,
    // setPumpState() only stops pumps (starting them is left to update())
    bool setValveState();
    bool setPumpState();
    // Sets heater_active_ to HeaterFor(current state). Doesn't switch cip_heater:
    // the heater itself is operated elsewhere. Always true (nothing to fail)
    bool setHeaterState();

    // Moves to state s without blocking, in phases:
    //   1. stops the pumps that must be off
    //   2. pumps stopping: if one of them was running, update() waits
    //      PUMP_STOP_MS (spin-down) before touching the valves; otherwise no wait
    //   3. opens valves, then closes valves
    //   4. valves settling: update() starts the pumps the state needs once
    //      VALVE_SETTLE_MS has passed (no wait if it needs none)
    // So a pump never runs against a valve that is moving. If any valve failed,
    // no pumps are scheduled. current_state_ becomes s even on failure, so
    // setValveState() / setPumpState() retry it. Calling it again mid-change
    // replaces the pending steps; a pump still spinning down from an earlier
    // call keeps holding the valves until its PUMP_STOP_MS is up. False if
    // stopping a pump or (without a stop wait) moving a valve failed; later
    // failures show in update()
    bool setState(RO_state s);

    // Call every loop pass. Moves to OFF when a procedure has just ended (see
    // offWhenProcedureEnds), moves the valves once stopped pumps have had
    // PUMP_STOP_MS, and starts the pending pumps once the valves have had
    // VALVE_SETTLE_MS. False if moving to OFF, a valve or starting a pump
    // failed. (Procedure steps run in the scheduler's update(), called by main.)
    bool update();

    // The RO's procedure instance (CIP cycles, media backwash, empty feed tank):
    // run / stop / isRunning / type. Served to other machines by a
    // procedure_server ("ro_procedure", ro_config_ro.cpp). When it stops or
    // finishes, update() moves the RO to OFF
    procedure& procedures() { return procedure_; }

    // True while a state change is in progress: pumps stopping (phase 2) or
    // valves settling (phase 4)
    bool isSettling() const;

    // The state last passed to setState() (OFF until then)
    RO_state readState() const;

    // RO pump VFD speed, 0-1 (4-20 mA). Values outside 0-1 are rejected
    bool setRoSpeed(float speed);

    private:
    // Moves to OFF on the pass a procedure stops running (stopped, or its last
    // step ran). Only on that edge: a state set by hand while nothing is running
    // (the manual override) is left alone. True if nothing ended or OFF succeeded
    bool offWhenProcedureEnds();
    // Phases 3+4: opens then closes the valves of current_state_, then schedules
    // its pumps after VALVE_SETTLE_MS (none if a valve failed). False if a valve failed
    bool moveValves();
    bool applyValveState();   // valvesFor(current_state_): open first, then close

    scheduler& scheduler_;                                // the machine's scheduler (owned by main)
    procedure procedure_;                                 // runs the RO procedures, one at a time (its destructor removes its tasks from scheduler_). Its steps go through ro_config.h (setROState), i.e. the ro created by addRO, which is this one
    RO_state current_state_;
    bool valves_pending_ = false;                         // phase 2: valves wait for stopped pumps to spin down
    std::chrono::steady_clock::time_point pumps_stopped_at_; // when setState() last stopped a running pump
    bool pumps_pending_ = false;                          // phase 4: pumps of current_state_ still to start
    std::chrono::steady_clock::time_point valves_set_at_; // when setState() moved the valves
    heater_state heater_active_ = false;
    bool procedure_was_running_ = false;                  // procedure_.isRunning() on the previous update()
};
