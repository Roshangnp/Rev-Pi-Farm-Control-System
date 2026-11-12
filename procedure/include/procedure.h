#pragma once

#include <string>
#include <vector>

#include "scheduler.h"

// Every farm procedure, run by a procedure instance (the RO ones by ro's, served as "ro_procedure")
enum class procedure_types {
    LOW_PH_CIP_CYCLE,
    HIGH_PH_CIP_CYCLE,
    MEDIA_BACKWASH_CYCLE,
    EMPTY_FEED_TANK
};

// Every procedure, in enum order: add new ones here too, or fromString() can't parse them
inline constexpr procedure_types ALL_PROCEDURE_TYPES[] = {
    procedure_types::LOW_PH_CIP_CYCLE, procedure_types::HIGH_PH_CIP_CYCLE,
    procedure_types::MEDIA_BACKWASH_CYCLE, procedure_types::EMPTY_FEED_TANK,
};

// The enum name, e.g. "LOW_PH_CIP_CYCLE". Used on the wire by ro_server / remote_ro
inline const char* toString(procedure_types p) {
    switch (p) {
    case procedure_types::LOW_PH_CIP_CYCLE: return "LOW_PH_CIP_CYCLE";
    case procedure_types::HIGH_PH_CIP_CYCLE: return "HIGH_PH_CIP_CYCLE";
    case procedure_types::MEDIA_BACKWASH_CYCLE: return "MEDIA_BACKWASH_CYCLE";
    case procedure_types::EMPTY_FEED_TANK: return "EMPTY_FEED_TANK";
    }
    return "UNKNOWN";
}

// Inverse of toString (exact match). False if text names no procedure
inline bool fromString(const std::string& text, procedure_types& p) {
    for (procedure_types candidate : ALL_PROCEDURE_TYPES) {
        if (text == toString(candidate)) {
            p = candidate;
            return true;
        }
    }
    return false;
}


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

// Runs multi-step farm procedures on a scheduler, one at a time per instance
// (one instance per thing being controlled, e.g. the RO owns one). Every step is
// a task scheduled up front (timed steps on afterDuration conditions with
// cumulative times, float steps on float conditions). Steps use only the public
// interfaces: devices from config.h, the RO from ro_config.h (setROState,
// readROState), so procedure needs no reference to what it drives; a machine
// that lacks one of them fails to link. Run only on the machine that owns the
// devices (on the host PC, ro_config.h goes over the network). Nothing blocks;
// sched.update() (and updateRO() for the RO procedures) must be called every
// loop pass.
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
//   EMPTY_FEED_TANK: NORMAL_OP while feed_tank_low_float is submerged and
//     permeate_tank_high_float is dry; done (OFF, removes its tasks) when the
//     permeate tank is full or feed_tank_low_float is dry (at once if so at the start). A
//     float that can't be read pauses it (OFF) until both read again (ON_RISING).
class procedure {
    public:
    // Nothing is scheduled until run(). sched must outlive this object
    explicit procedure(scheduler& sched);
    ~procedure(); // removes all tasks and conditions from the scheduler

    // Holds ids in one scheduler: a copy would remove the same tasks twice
    procedure(const procedure&) = delete;
    procedure& operator=(const procedure&) = delete;

    // Schedules procedure_type (switch/case over the procedure helpers); the first
    // step runs on the next sched.update(). One at a time: false if one is still running
    bool run(procedure_types procedure_type);
    // True while any scheduled task hasn't run. False if none was started, it was
    // stopped, or every task has run
    bool isRunning() const;
    // The procedure run() last started (meaningful while isRunning())
    procedure_types type() const { return procedure_type_; }
    // Removes all tasks and conditions from the scheduler, so no further steps run.
    // Leaves the RO in its current state: setState(OFF) is up to the caller
    bool stop();

    private:
    // Procedure helpers: each schedules its steps and populates task_ids_ and condition_ids_
    bool low_ph_cip_cycle();
    bool high_ph_cip_cycle();
    bool media_backwash_cycle();
    bool empty_feed_tank();

    scheduler& sched_;
    std::vector<task_id> task_ids_;
    std::vector<condition_id> condition_ids_;
    procedure_types procedure_type_ = procedure_types::LOW_PH_CIP_CYCLE;   // the one run() last started
};
