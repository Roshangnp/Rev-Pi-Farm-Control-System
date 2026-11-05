#pragma once

#include <vector>

#include "scheduler.h"
#include "types.h"

// Forward declaration, not #include "ro.h": ro.h includes this header, so
// including it back is circular (ro wouldn't be declared yet here).
// procedures.cpp includes ro.h itself.
class ro;

// Step times in minutes (a -D on the compile line overrides them, e.g. for tests).
// TODO: confirm the CIP times, they are placeholders
#ifndef LOW_PH_DOSING_TIME_MIN
#define LOW_PH_DOSING_TIME_MIN 1
#endif
#ifndef HIGH_PH_DOSING_TIME_MIN
#define HIGH_PH_DOSING_TIME_MIN 1
#endif
#ifndef CIP_FLUSH_TIME_MIN
#define CIP_FLUSH_TIME_MIN 1
#endif
#ifndef CIP_LOW_FLOW_TIME_MIN
#define CIP_LOW_FLOW_TIME_MIN 1
#endif
#ifndef CIP_SOAK_TIME_MIN
#define CIP_SOAK_TIME_MIN 1
#endif
#ifndef CIP_HIGH_FLOW_TIME_MIN
#define CIP_HIGH_FLOW_TIME_MIN 1
#endif
#ifndef MEDIA_BACKWASH_TIME_MIN
#define MEDIA_BACKWASH_TIME_MIN 2
#endif

// Runs multi-step procedures on a scheduler, one at a time (ro owns the one
// instance). Every step is a task scheduled up front (timed steps on afterDuration conditions with cumulative times, float
// steps on float conditions). Nothing blocks; sched.update() and ro.update()
// must be called every loop pass.
// Changing the RO state mid-cycle doesn't stop the remaining tasks; stop() or
// destroying the object removes them.
//
// Cycles:
//   LOW_PH_CIP_CYCLE: LOW_PH_DOSE (LOW_PH_DOSING_TIME_MIN) -> CIP_FLUSH (CIP_FLUSH_TIME_MIN)
//     -> CIP_LOW_FLOW_CYCLE (CIP_LOW_FLOW_TIME_MIN) -> CIP_SOAK (CIP_SOAK_TIME_MIN)
//     -> CIP_HIGH_FLOW_CYCLE (CIP_HIGH_FLOW_TIME_MIN)
//     -> CIP_FLUSH until cip_tank_low_float is not submerged
//     -> CIP_REFILL until cip_tank_high_float is submerged -> OFF.
//     No heater tasks: ro::setState() sets heater_active_ from HeaterFor(state).
//   HIGH_PH_CIP_CYCLE: the same with HIGH_PH_DOSE (HIGH_PH_DOSING_TIME_MIN) first.
//   MEDIA_BACKWASH_CYCLE: MEDIA_BACKWASH_1 (MEDIA_BACKWASH_TIME_MIN)
//     -> MEDIA_BACKWASH_2 (MEDIA_BACKWASH_TIME_MIN) -> OFF.
class ro_procedure {
    public:
    // Nothing is scheduled until run(). r and sched must outlive this object
    ro_procedure(ro& r, scheduler& sched);
    ~ro_procedure(); // removes all tasks and conditions from the scheduler

    // Holds ids in one scheduler: a copy would remove the same tasks twice
    ro_procedure(const ro_procedure&) = delete;
    ro_procedure& operator=(const ro_procedure&) = delete;

    // Schedules procedure_type (switch/case over the procedure helpers); the first
    // step runs on the next sched.update(). One at a time: false if one is still running
    bool run(procedure_types procedure_type);
    // True while any scheduled task hasn't run. False if none was started, it was
    // stopped, or every task has run
    bool isRunning() const;
    // Removes all tasks and conditions from the scheduler, so no further steps run.
    // Leaves the RO in its current state: setState(OFF) is up to the caller
    bool stop();

    private:
    // Procedure helpers: each schedules its steps and populates task_ids_ and condition_ids_
    bool low_ph_cip_cycle();
    bool high_ph_cip_cycle();
    bool media_backwash_cycle();

    ro& r_;
    scheduler& sched_;
    std::vector<task_id> task_ids_;
    std::vector<condition_id> condition_ids_;
    procedure_types procedure_type_ = procedure_types::LOW_PH_CIP_CYCLE;   // the one run() last started, for the log
};
