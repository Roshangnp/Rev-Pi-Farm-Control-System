#include "procedures.h"

#include "config.h"
#include "ro.h"

#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

ro_procedure::ro_procedure(ro& r, scheduler& sched)
    : r_(r), sched_(sched) {}

ro_procedure::~ro_procedure() {
    stop();
}

bool ro_procedure::isRunning() const {
    for (task_id id : task_ids_) {
        for (const auto& entry : sched_.tasks()) {
            if (entry.first == id && !entry.second.done) return true;
        }
    }
    return false;   // never run, stopped, or every task has run (a task removed from the scheduler counts as done)
}

bool ro_procedure::run(procedure_types procedure_type) {
    if (isRunning()) {
        std::fprintf(stderr, "ro_procedure: %s is already running, can't start %s\n",
                     toString(procedure_type_), toString(procedure_type));
        return false;
    }
    stop();   // a finished run's leftovers
    procedure_type_ = procedure_type;
    bool ok = false;
    switch (procedure_type_) {
    case procedure_types::LOW_PH_CIP_CYCLE: ok = low_ph_cip_cycle(); break;
    case procedure_types::HIGH_PH_CIP_CYCLE: ok = high_ph_cip_cycle(); break;
    case procedure_types::MEDIA_BACKWASH_CYCLE: ok = media_backwash_cycle(); break;
    }
    if (!ok) stop();   // don't leave half a procedure scheduled
    return ok;
}

bool ro_procedure::stop() {
    // Tasks first: a condition still used by a task can't be removed
    for (task_id id : task_ids_) sched_.removeTask(id);
    bool ok = true;
    for (condition_id id : condition_ids_) ok = sched_.removeCondition(id) && ok;
    task_ids_.clear();
    condition_ids_.clear();
    return ok;
}

bool ro_procedure::low_ph_cip_cycle() {

    // Every afterDuration counts from now (when it's created), so each step's end
    // is the sum of the step times before it
    condition_id dose_done_id = sched_.addCondition(condition::afterDuration("low_ph_dose_done",
        std::chrono::minutes(LOW_PH_DOSING_TIME_MIN)));
    condition_id flush_done_id = sched_.addCondition(condition::afterDuration("cip_flush_done",
        std::chrono::minutes(LOW_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN)));
    condition_id low_flow_done_id = sched_.addCondition(condition::afterDuration("cip_low_flow_done",
        std::chrono::minutes(LOW_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN + CIP_LOW_FLOW_TIME_MIN)));
    condition_id soak_done_id = sched_.addCondition(condition::afterDuration("cip_soak_done",
        std::chrono::minutes(LOW_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN + CIP_LOW_FLOW_TIME_MIN
                             + CIP_SOAK_TIME_MIN)));
    condition_id high_flow_done_id = sched_.addCondition(condition::afterDuration("cip_high_flow_done",
        std::chrono::minutes(LOW_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN + CIP_LOW_FLOW_TIME_MIN
                             + CIP_SOAK_TIME_MIN + CIP_HIGH_FLOW_TIME_MIN)));
    // A float that can't be read counts as not met, so the step waits
    condition_id cip_tank_empty_id = sched_.addCondition(condition::when("cip_tank_empty",
        [] { bool submerged; return cip_tank_low_float.readSubmerged(submerged) && !submerged; }));
    condition_id cip_tank_full_id = sched_.addCondition(condition::when("cip_tank_full",
        [] { bool submerged; return cip_tank_high_float.readSubmerged(submerged) && submerged; }));
    // The tank is full before the cycle starts, so OFF also waits for the refill to be running
    condition_id refilling_id = sched_.addCondition(condition::when("cip_refilling",
        [this] { return r_.readState() == RO_state::CIP_REFILL; }));

    task_id start_dose_id;
    task_id start_flush_id;
    task_id start_low_flow_id;
    task_id start_soak_id;
    task_id start_high_flow_id;
    task_id start_final_flush_id;
    task_id start_refill_id;
    task_id stop_id;
    sched_.addTask({"start_low_ph_dose", [this] {return r_.setState(RO_state::LOW_PH_DOSE);}, {}, task_mode::ONCE}, start_dose_id);
    sched_.addTask({"start_cip_flush", [this] {return r_.setState(RO_state::CIP_FLUSH);}, {dose_done_id}, task_mode::ONCE}, start_flush_id);
    sched_.addTask({"start_cip_low_flow", [this] {return r_.setState(RO_state::CIP_LOW_FLOW_CYCLE);}, {flush_done_id}, task_mode::ONCE}, start_low_flow_id);
    sched_.addTask({"start_cip_soak", [this] {return r_.setState(RO_state::CIP_SOAK);}, {low_flow_done_id}, task_mode::ONCE}, start_soak_id);
    sched_.addTask({"start_cip_high_flow", [this] {return r_.setState(RO_state::CIP_HIGH_FLOW_CYCLE);}, {soak_done_id}, task_mode::ONCE}, start_high_flow_id);
    sched_.addTask({"start_final_cip_flush", [this] {return r_.setState(RO_state::CIP_FLUSH);}, {high_flow_done_id}, task_mode::ONCE}, start_final_flush_id);
    sched_.addTask({"start_cip_refill", [this] {return r_.setState(RO_state::CIP_REFILL);}, {high_flow_done_id, cip_tank_empty_id}, task_mode::ONCE}, start_refill_id);
    sched_.addTask({"stop_low_ph_cip_cycle", [this] {return r_.setState(RO_state::OFF);}, {refilling_id, cip_tank_full_id}, task_mode::ONCE}, stop_id);

    condition_ids_ = {dose_done_id, flush_done_id, low_flow_done_id, soak_done_id, high_flow_done_id,
                      cip_tank_empty_id, cip_tank_full_id, refilling_id};
    task_ids_ = {start_dose_id, start_flush_id, start_low_flow_id, start_soak_id,
                 start_high_flow_id, start_final_flush_id, start_refill_id, stop_id};
    return true;
}

bool ro_procedure::high_ph_cip_cycle() {

    // Every afterDuration counts from now (when it's created), so each step's end
    // is the sum of the step times before it
    condition_id dose_done_id = sched_.addCondition(condition::afterDuration("high_ph_dose_done",
        std::chrono::minutes(HIGH_PH_DOSING_TIME_MIN)));
    condition_id flush_done_id = sched_.addCondition(condition::afterDuration("cip_flush_done",
        std::chrono::minutes(HIGH_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN)));
    condition_id low_flow_done_id = sched_.addCondition(condition::afterDuration("cip_low_flow_done",
        std::chrono::minutes(HIGH_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN + CIP_LOW_FLOW_TIME_MIN)));
    condition_id soak_done_id = sched_.addCondition(condition::afterDuration("cip_soak_done",
        std::chrono::minutes(HIGH_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN + CIP_LOW_FLOW_TIME_MIN
                             + CIP_SOAK_TIME_MIN)));
    condition_id high_flow_done_id = sched_.addCondition(condition::afterDuration("cip_high_flow_done",
        std::chrono::minutes(HIGH_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN + CIP_LOW_FLOW_TIME_MIN
                             + CIP_SOAK_TIME_MIN + CIP_HIGH_FLOW_TIME_MIN)));
    // A float that can't be read counts as not met, so the step waits
    condition_id cip_tank_empty_id = sched_.addCondition(condition::when("cip_tank_empty",
        [] { bool submerged; return cip_tank_low_float.readSubmerged(submerged) && !submerged; }));
    condition_id cip_tank_full_id = sched_.addCondition(condition::when("cip_tank_full",
        [] { bool submerged; return cip_tank_high_float.readSubmerged(submerged) && submerged; }));
    // The tank is full before the cycle starts, so OFF also waits for the refill to be running
    condition_id refilling_id = sched_.addCondition(condition::when("cip_refilling",
        [this] { return r_.readState() == RO_state::CIP_REFILL; }));

    task_id start_dose_id;
    task_id start_flush_id;
    task_id start_low_flow_id;
    task_id start_soak_id;
    task_id start_high_flow_id;
    task_id start_final_flush_id;
    task_id start_refill_id;
    task_id stop_id;
    sched_.addTask({"start_high_ph_dose", [this] {return r_.setState(RO_state::HIGH_PH_DOSE);}, {}, task_mode::ONCE}, start_dose_id);
    sched_.addTask({"start_cip_flush", [this] {return r_.setState(RO_state::CIP_FLUSH);}, {dose_done_id}, task_mode::ONCE}, start_flush_id);
    sched_.addTask({"start_cip_low_flow", [this] {return r_.setState(RO_state::CIP_LOW_FLOW_CYCLE);}, {flush_done_id}, task_mode::ONCE}, start_low_flow_id);
    sched_.addTask({"start_cip_soak", [this] {return r_.setState(RO_state::CIP_SOAK);}, {low_flow_done_id}, task_mode::ONCE}, start_soak_id);
    sched_.addTask({"start_cip_high_flow", [this] {return r_.setState(RO_state::CIP_HIGH_FLOW_CYCLE);}, {soak_done_id}, task_mode::ONCE}, start_high_flow_id);
    sched_.addTask({"start_final_cip_flush", [this] {return r_.setState(RO_state::CIP_FLUSH);}, {high_flow_done_id}, task_mode::ONCE}, start_final_flush_id);
    sched_.addTask({"start_cip_refill", [this] {return r_.setState(RO_state::CIP_REFILL);}, {high_flow_done_id, cip_tank_empty_id}, task_mode::ONCE}, start_refill_id);
    sched_.addTask({"stop_high_ph_cip_cycle", [this] {return r_.setState(RO_state::OFF);}, {refilling_id, cip_tank_full_id}, task_mode::ONCE}, stop_id);

    condition_ids_ = {dose_done_id, flush_done_id, low_flow_done_id, soak_done_id, high_flow_done_id,
                      cip_tank_empty_id, cip_tank_full_id, refilling_id};
    task_ids_ = {start_dose_id, start_flush_id, start_low_flow_id, start_soak_id,
                 start_high_flow_id, start_final_flush_id, start_refill_id, stop_id};
    return true;
}

bool ro_procedure::media_backwash_cycle() {

    // Every afterDuration counts from now (when it's created), so each step's end
    // is the sum of the step times before it
    condition_id first_backwash_done_id = sched_.addCondition(condition::afterDuration("first_backwash_done",
        std::chrono::minutes(MEDIA_BACKWASH_TIME_MIN)));
    condition_id second_backwash_done_id = sched_.addCondition(condition::afterDuration("second_backwash_done",
        std::chrono::minutes(MEDIA_BACKWASH_TIME_MIN + MEDIA_BACKWASH_TIME_MIN)));
    
    task_id start_first_backwash_id;
    task_id start_second_backwash_id;
    task_id stop_id;
    sched_.addTask({"start_first_backwash", [this] {return r_.setState(RO_state::MEDIA_BACKWASH_1);}, {}, task_mode::ONCE}, start_first_backwash_id);
    sched_.addTask({"start_second_backwash", [this] {return r_.setState(RO_state::MEDIA_BACKWASH_2);}, {first_backwash_done_id}, task_mode::ONCE}, start_second_backwash_id);
    sched_.addTask({"start_cip_low_flow", [this] {return r_.setState(RO_state::OFF);}, {second_backwash_done_id}, task_mode::ONCE}, stop_id);

    condition_ids_ = {first_backwash_done_id, second_backwash_done_id};
    task_ids_ = {start_first_backwash_id, start_second_backwash_id, stop_id};
    return true;
}
