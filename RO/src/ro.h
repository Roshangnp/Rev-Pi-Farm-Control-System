#pragma once

#include <chrono>
#include <memory>

#include "config.h"
#include "procedures.h"
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
    // false if any of them failed. While settling, setPumpState() only stops
    // pumps; starting them is left to update()
    bool setValveState();
    bool setPumpState();
    // Sets heater_active_ to HeaterFor(current state). Doesn't switch cip_heater:
    // the heater itself is operated elsewhere. Always true (nothing to fail)
    bool setHeaterState();

    // Moves to state s without blocking: stops the pumps that must be off, opens
    // valves, then closes valves, and returns. The pumps the state needs are
    // started by update() once VALVE_SETTLE_MS has passed, so a running pump
    // always has an open path. If any valve failed, no pumps are scheduled.
    // current_state_ becomes s even on failure, so setValveState() /
    // setPumpState() retry it. Calling it again while settling replaces the
    // pending start (and restarts the wait). False if any device failed
    bool setState(RO_state s);

    // Call every loop pass. Starts the pending pumps once the valves have had
    // VALVE_SETTLE_MS to move. False only if starting a pump failed. (Procedure
    // steps run in the scheduler's update(), called by main.)
    bool update();

    // Schedules procedure p (RO/src/procedures.h) on the scheduler; its first
    // step runs on the next scheduler update(). One procedure at a time: false if
    // one is still running, or p isn't implemented yet
    bool runProcedure(procedure_types p);

    // True between setState() and update() starting the pumps
    bool isSettling() const;

    // The state last passed to setState() (OFF until then)
    RO_state readState() const;

    // RO pump VFD speed, 0-1 (4-20 mA). Values outside 0-1 are rejected
    bool setRoSpeed(float speed);

    private:
    scheduler& scheduler_;                                // the machine's scheduler (owned by main)
    std::unique_ptr<ro_procedure> procedure_;             // last procedure started (its destructor removes its tasks from scheduler_)
    RO_state current_state_;
    bool pumps_pending_ = false;                          // pumps of current_state_ still to start
    std::chrono::steady_clock::time_point valves_set_at_; // when setState() moved the valves
    heater_state heater_active_ = false;
};
