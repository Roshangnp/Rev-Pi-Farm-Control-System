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
#define LOW_PH_DOSING_TIME_MIN 10
#endif
#ifndef HIGH_PH_DOSING_TIME_MIN
#define HIGH_PH_DOSING_TIME_MIN 10
#endif
#ifndef CIP_FLUSH_TIME_MIN
#define CIP_FLUSH_TIME_MIN 10
#endif
#ifndef CIP_LOW_FLOW_TIME_MIN
#define CIP_LOW_FLOW_TIME_MIN 10
#endif
#ifndef CIP_SOAK_TIME_MIN
#define CIP_SOAK_TIME_MIN 10
#endif
#ifndef CIP_HIGH_FLOW_TIME_MIN
#define CIP_HIGH_FLOW_TIME_MIN 10
#endif

// One multi-step procedure on a scheduler: every step is a task scheduled up
// front (timed steps on afterDuration conditions with cumulative times, float
// steps on float conditions). Nothing blocks; sched.update() and ro.update()
// must be called every loop pass, and ro and sched must outlive this object.
// Changing the RO state mid-cycle doesn't stop the remaining tasks; destroying
// the object or resetProcedure() removes them.
//
// Cycles:
//   LOW_PH_CIP_CYCLE: LOW_PH_DOSE (LOW_PH_DOSING_TIME_MIN) -> CIP_FLUSH (CIP_FLUSH_TIME_MIN)
//     -> CIP_LOW_FLOW_CYCLE (CIP_LOW_FLOW_TIME_MIN) -> CIP_SOAK (CIP_SOAK_TIME_MIN)
//     -> CIP_HIGH_FLOW_CYCLE (CIP_HIGH_FLOW_TIME_MIN)
//     -> CIP_FLUSH until cip_tank_low_float is not submerged
//     -> CIP_REFILL until cip_tank_high_float is submerged -> OFF.
//     cip_heater is on from LOW_PH_DOSE to the end of CIP_HIGH_FLOW_CYCLE.
//   HIGH_PH_CIP_CYCLE: the same with HIGH_PH_DOSE (HIGH_PH_DOSING_TIME_MIN) first.
//   MEDIA_BACKWASH_CYCLE: not implemented yet (scheduleProcedure returns false).
class ro_procedure {
    public:
    ro_procedure(procedure_types procedure_type); // creates object, assigns procedure_type
    ~ro_procedure(); // frees memory AND removes all tasks and conditions from the scheduler

    // Holds ids in one scheduler: a copy would remove the same tasks twice
    ro_procedure(const ro_procedure&) = delete;
    ro_procedure& operator=(const ro_procedure&) = delete;

    bool getDone(); // returns true if all tasks have been completed
    // uses switch/case on procedure types and runs one of the 3 procedure helper functions;
    // the first step runs on the next sched.update(). False if it is still running,
    // or the procedure isn't implemented (nothing is left scheduled then)
    bool scheduleProcedure(ro& ro, scheduler& sched);
    bool resetProcedure(); // removes all tasks and conditions from schedular and resets private variables

    // procedure helper functions. responsible for populating task_ids_ and condition_ids_
    bool low_ph_cip_cycle(ro& ro, scheduler& sched);
    bool high_ph_cip_cycle(ro& ro, scheduler& sched);
    bool media_backwash_cycle(ro& ro, scheduler& sched);

    private:
    std::vector<task_id> task_ids_;
    std::vector<condition_id> condition_ids_;
    procedure_types procedure_type_;
    scheduler* sched_ = nullptr;   // where task_ids_ / condition_ids_ live (nullptr = nothing scheduled)
};
