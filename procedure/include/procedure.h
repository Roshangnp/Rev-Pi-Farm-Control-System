#pragma once

#include <string>
#include <vector>

#include "scheduler.h"

// Every farm procedure, run by a procedure instance (the RO ones by main_ro's ro_procedure)
enum class procedure_types {
    LOW_PH_CIP_CYCLE,
    HIGH_PH_CIP_CYCLE,
    MEDIA_BACKWASH_CYCLE,
    EMPTY_FEED_TANK,
    OVERHEAD_PATTERN_1,
    OVERHEAD_6_IRRIGATION
};

// Every procedure, in enum order: add new ones here too, or fromString() can't parse them
inline constexpr procedure_types ALL_PROCEDURE_TYPES[] = {
    procedure_types::LOW_PH_CIP_CYCLE, procedure_types::HIGH_PH_CIP_CYCLE,
    procedure_types::MEDIA_BACKWASH_CYCLE, procedure_types::EMPTY_FEED_TANK,
    procedure_types::OVERHEAD_PATTERN_1, procedure_types::OVERHEAD_6_IRRIGATION,
};

// The enum name, e.g. "LOW_PH_CIP_CYCLE". Used on the wire by ro_server / remote_ro
inline const char* toString(procedure_types p) {
    switch (p) {
    case procedure_types::LOW_PH_CIP_CYCLE: return "LOW_PH_CIP_CYCLE";
    case procedure_types::HIGH_PH_CIP_CYCLE: return "HIGH_PH_CIP_CYCLE";
    case procedure_types::MEDIA_BACKWASH_CYCLE: return "MEDIA_BACKWASH_CYCLE";
    case procedure_types::EMPTY_FEED_TANK: return "EMPTY_FEED_TANK";
    case procedure_types::OVERHEAD_PATTERN_1: return "OVERHEAD_PATTERN_1";
    case procedure_types::OVERHEAD_6_IRRIGATION: return "OVERHEAD_6_IRRIGATION";
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

// A procedure instance as every program sees it, like the device base classes in
// devices.h: procedure on the machine that owns it, remote_procedure
// (remote_procedure/remote_procedure.h) everywhere else. Instances are declared
// in procedure_config.h.
class procedure_control {
    public:
    virtual ~procedure_control() = default;

    // Starts procedure p. False if one is still running (or the owner can't be reached)
    virtual bool run(procedure_types p) = 0;
    // Removes the remaining steps; whatever the procedure last switched stays as
    // it is (the owner decides what happens next, e.g. main_ro sets the RO OFF)
    virtual bool stop() = 0;
    // running = a procedure is running, p = which one (meaningful while running).
    // False if it can't be read
    virtual bool readProcedure(bool& running, procedure_types& p) = 0;
};

// Runs multi-step farm procedures on a scheduler, one at a time per instance
// (one instance per thing being controlled, owned by a main_*, e.g. main_ro's
// ro_procedure). Every step is
// a task scheduled up front (timed steps on afterDuration conditions with
// cumulative times, float steps on float conditions). Steps use only the public
// interfaces: devices from config.h, the RO from ro_config.h (setROState,
// readROState), so procedure needs no reference to what it drives; a machine
// that lacks one of them fails to link. Run only on the machine that owns the
// devices (on the host PC, ro_config.h goes over the network). Nothing blocks;
// sched.update() (and updateRO() for the RO procedures) must be called every
// loop pass. The owner does whatever should follow the end of a procedure (main_ro
// sets the RO OFF when ro_procedure stops running).
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
//   OVERHEAD_PATTERN_1: schedules the 18 WiseConn overhead irrigations of
//     wiseconn_irrigation_tables/overhead_pattern_1.csv (hard coded, today, 14:00-16:15),
//     one every 2 s; WiseConn runs them at those times.
//   OVERHEAD_6_IRRIGATION: schedules one WiseConn irrigation on overhead 6, today
//     20:00-21:00.
class procedure : public procedure_control {
    public:
    // Nothing is scheduled until run(). sched must outlive this object
    explicit procedure(scheduler& sched);
    ~procedure(); // removes all tasks and conditions from the scheduler

    // Holds ids in one scheduler: a copy would remove the same tasks twice
    procedure(const procedure&) = delete;
    procedure& operator=(const procedure&) = delete;

    // Schedules procedure_type (switch/case over the procedure helpers); the first
    // step runs on the next sched.update(). One at a time: false if one is still running
    bool run(procedure_types procedure_type) override;
    // True while any scheduled task hasn't run. False if none was started, it was
    // stopped, or every task has run
    bool isRunning() const;
    // The procedure run() last started (meaningful while isRunning())
    procedure_types type() const { return procedure_type_; }
    // Removes all tasks and conditions from the scheduler, so no further steps run.
    // Leaves the RO in its current state: setState(OFF) is up to the caller
    bool stop() override;
    // running = isRunning(), p = type()
    bool readProcedure(bool& running, procedure_types& p) override;

    private:
    // Procedure helpers: each schedules its steps and populates task_ids_ and condition_ids_
    bool low_ph_cip_cycle();
    bool high_ph_cip_cycle();
    bool media_backwash_cycle();
    bool empty_feed_tank();
    bool run_overhead1_pattern();
    bool run_overhead6_irrigation();

    scheduler& sched_;
    std::vector<task_id> task_ids_;
    std::vector<condition_id> condition_ids_;
    procedure_types procedure_type_ = procedure_types::LOW_PH_CIP_CYCLE;   // the one run() last started
};
