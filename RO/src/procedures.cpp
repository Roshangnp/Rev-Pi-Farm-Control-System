#include "procedures.h"

#include "config.h"
#include "ro.h"

#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

ro_procedure::ro_procedure(procedure_types procedure_type)
    : procedure_type_(procedure_type) {}

ro_procedure::~ro_procedure() {
    resetProcedure();
}

bool ro_procedure::getDone() {
    if (!sched_ || task_ids_.empty()) return false;   // never scheduled
    for (task_id id : task_ids_) {
        for (const auto& entry : sched_->tasks()) {
            if (entry.first == id && !entry.second.done) return false;
        }
    }
    return true;   // every task has run (a task removed from the scheduler counts as done)
}

bool ro_procedure::scheduleProcedure(ro& r, scheduler& sched) {
    if (!task_ids_.empty() && !getDone()) {
        std::fprintf(stderr, "ro_procedure: %s is already running\n", toString(procedure_type_));
        return false;
    }
    resetProcedure();   // a finished run's leftovers
    sched_ = &sched;
    bool ok = false;
    switch (procedure_type_) {
    case procedure_types::LOW_PH_CIP_CYCLE: ok = low_ph_cip_cycle(r, sched); break;
    case procedure_types::HIGH_PH_CIP_CYCLE: ok = high_ph_cip_cycle(r, sched); break;
    case procedure_types::MEDIA_BACKWASH_CYCLE: ok = media_backwash_cycle(r, sched); break;
    }
    if (!ok) resetProcedure();   // don't leave half a procedure scheduled
    return ok;
}

bool ro_procedure::resetProcedure() {
    bool ok = true;
    if (sched_) {
        // Tasks first: a condition still used by a task can't be removed
        for (task_id id : task_ids_) sched_->removeTask(id);
        for (condition_id id : condition_ids_) ok = sched_->removeCondition(id) && ok;
    }
    task_ids_.clear();
    condition_ids_.clear();
    sched_ = nullptr;
    return ok;
}

bool ro_procedure::low_ph_cip_cycle(ro& r, scheduler& sched) {

    // Every afterDuration counts from now (when it's created), so each step's end
    // is the sum of the step times before it
    condition_id dose_done_id = sched.addCondition(condition::afterDuration("low_ph_dose_done",
        std::chrono::minutes(LOW_PH_DOSING_TIME_MIN)));
    condition_id flush_done_id = sched.addCondition(condition::afterDuration("cip_flush_done",
        std::chrono::minutes(LOW_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN)));
    condition_id low_flow_done_id = sched.addCondition(condition::afterDuration("cip_low_flow_done",
        std::chrono::minutes(LOW_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN + CIP_LOW_FLOW_TIME_MIN)));
    condition_id soak_done_id = sched.addCondition(condition::afterDuration("cip_soak_done",
        std::chrono::minutes(LOW_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN + CIP_LOW_FLOW_TIME_MIN
                             + CIP_SOAK_TIME_MIN)));
    condition_id high_flow_done_id = sched.addCondition(condition::afterDuration("cip_high_flow_done",
        std::chrono::minutes(LOW_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN + CIP_LOW_FLOW_TIME_MIN
                             + CIP_SOAK_TIME_MIN + CIP_HIGH_FLOW_TIME_MIN)));
    // A float that can't be read counts as not met, so the step waits
    condition_id cip_tank_empty_id = sched.addCondition(condition::when("cip_tank_empty",
        [] { bool submerged; return cip_tank_low_float.readSubmerged(submerged) && !submerged; }));
    condition_id cip_tank_full_id = sched.addCondition(condition::when("cip_tank_full",
        [] { bool submerged; return cip_tank_high_float.readSubmerged(submerged) && submerged; }));
    // The tank is full before the cycle starts, so OFF also waits for the refill to be running
    condition_id refilling_id = sched.addCondition(condition::when("cip_refilling",
        [&r] { return r.readState() == RO_state::CIP_REFILL; }));

    task_id start_dose_id;
    task_id heater_on_id;
    task_id start_flush_id;
    task_id start_low_flow_id;
    task_id start_soak_id;
    task_id start_high_flow_id;
    task_id start_final_flush_id;
    task_id heater_off_id;
    task_id start_refill_id;
    task_id stop_id;
    sched.addTask({"start_low_ph_dose", [&r] {return r.setState(RO_state::LOW_PH_DOSE);}, {}, task_mode::ONCE}, start_dose_id);
    sched.addTask({"cip_heater_on", [] {return cip_heater.setOn();}, {}, task_mode::ONCE}, heater_on_id);
    sched.addTask({"start_cip_flush", [&r] {return r.setState(RO_state::CIP_FLUSH);}, {dose_done_id}, task_mode::ONCE}, start_flush_id);
    sched.addTask({"start_cip_low_flow", [&r] {return r.setState(RO_state::CIP_LOW_FLOW_CYCLE);}, {flush_done_id}, task_mode::ONCE}, start_low_flow_id);
    sched.addTask({"start_cip_soak", [&r] {return r.setState(RO_state::CIP_SOAK);}, {low_flow_done_id}, task_mode::ONCE}, start_soak_id);
    sched.addTask({"start_cip_high_flow", [&r] {return r.setState(RO_state::CIP_HIGH_FLOW_CYCLE);}, {soak_done_id}, task_mode::ONCE}, start_high_flow_id);
    sched.addTask({"start_final_cip_flush", [&r] {return r.setState(RO_state::CIP_FLUSH);}, {high_flow_done_id}, task_mode::ONCE}, start_final_flush_id);
    sched.addTask({"cip_heater_off", [] {return cip_heater.setOff();}, {high_flow_done_id}, task_mode::ONCE}, heater_off_id);
    sched.addTask({"start_cip_refill", [&r] {return r.setState(RO_state::CIP_REFILL);}, {high_flow_done_id, cip_tank_empty_id}, task_mode::ONCE}, start_refill_id);
    sched.addTask({"stop_low_ph_cip_cycle", [&r] {return r.setState(RO_state::OFF);}, {refilling_id, cip_tank_full_id}, task_mode::ONCE}, stop_id);

    condition_ids_ = {dose_done_id, flush_done_id, low_flow_done_id, soak_done_id, high_flow_done_id,
                      cip_tank_empty_id, cip_tank_full_id, refilling_id};
    task_ids_ = {start_dose_id, heater_on_id, start_flush_id, start_low_flow_id, start_soak_id,
                 start_high_flow_id, start_final_flush_id, heater_off_id, start_refill_id, stop_id};
    return true;
}

bool ro_procedure::high_ph_cip_cycle(ro& r, scheduler& sched) {

    // Every afterDuration counts from now (when it's created), so each step's end
    // is the sum of the step times before it
    condition_id dose_done_id = sched.addCondition(condition::afterDuration("high_ph_dose_done",
        std::chrono::minutes(HIGH_PH_DOSING_TIME_MIN)));
    condition_id flush_done_id = sched.addCondition(condition::afterDuration("cip_flush_done",
        std::chrono::minutes(HIGH_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN)));
    condition_id low_flow_done_id = sched.addCondition(condition::afterDuration("cip_low_flow_done",
        std::chrono::minutes(HIGH_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN + CIP_LOW_FLOW_TIME_MIN)));
    condition_id soak_done_id = sched.addCondition(condition::afterDuration("cip_soak_done",
        std::chrono::minutes(HIGH_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN + CIP_LOW_FLOW_TIME_MIN
                             + CIP_SOAK_TIME_MIN)));
    condition_id high_flow_done_id = sched.addCondition(condition::afterDuration("cip_high_flow_done",
        std::chrono::minutes(HIGH_PH_DOSING_TIME_MIN + CIP_FLUSH_TIME_MIN + CIP_LOW_FLOW_TIME_MIN
                             + CIP_SOAK_TIME_MIN + CIP_HIGH_FLOW_TIME_MIN)));
    // A float that can't be read counts as not met, so the step waits
    condition_id cip_tank_empty_id = sched.addCondition(condition::when("cip_tank_empty",
        [] { bool submerged; return cip_tank_low_float.readSubmerged(submerged) && !submerged; }));
    condition_id cip_tank_full_id = sched.addCondition(condition::when("cip_tank_full",
        [] { bool submerged; return cip_tank_high_float.readSubmerged(submerged) && submerged; }));
    // The tank is full before the cycle starts, so OFF also waits for the refill to be running
    condition_id refilling_id = sched.addCondition(condition::when("cip_refilling",
        [&r] { return r.readState() == RO_state::CIP_REFILL; }));

    task_id start_dose_id;
    task_id heater_on_id;
    task_id start_flush_id;
    task_id start_low_flow_id;
    task_id start_soak_id;
    task_id start_high_flow_id;
    task_id start_final_flush_id;
    task_id heater_off_id;
    task_id start_refill_id;
    task_id stop_id;
    sched.addTask({"start_high_ph_dose", [&r] {return r.setState(RO_state::HIGH_PH_DOSE);}, {}, task_mode::ONCE}, start_dose_id);
    sched.addTask({"cip_heater_on", [] {return cip_heater.setOn();}, {}, task_mode::ONCE}, heater_on_id);
    sched.addTask({"start_cip_flush", [&r] {return r.setState(RO_state::CIP_FLUSH);}, {dose_done_id}, task_mode::ONCE}, start_flush_id);
    sched.addTask({"start_cip_low_flow", [&r] {return r.setState(RO_state::CIP_LOW_FLOW_CYCLE);}, {flush_done_id}, task_mode::ONCE}, start_low_flow_id);
    sched.addTask({"start_cip_soak", [&r] {return r.setState(RO_state::CIP_SOAK);}, {low_flow_done_id}, task_mode::ONCE}, start_soak_id);
    sched.addTask({"start_cip_high_flow", [&r] {return r.setState(RO_state::CIP_HIGH_FLOW_CYCLE);}, {soak_done_id}, task_mode::ONCE}, start_high_flow_id);
    sched.addTask({"start_final_cip_flush", [&r] {return r.setState(RO_state::CIP_FLUSH);}, {high_flow_done_id}, task_mode::ONCE}, start_final_flush_id);
    sched.addTask({"cip_heater_off", [] {return cip_heater.setOff();}, {high_flow_done_id}, task_mode::ONCE}, heater_off_id);
    sched.addTask({"start_cip_refill", [&r] {return r.setState(RO_state::CIP_REFILL);}, {high_flow_done_id, cip_tank_empty_id}, task_mode::ONCE}, start_refill_id);
    sched.addTask({"stop_high_ph_cip_cycle", [&r] {return r.setState(RO_state::OFF);}, {refilling_id, cip_tank_full_id}, task_mode::ONCE}, stop_id);

    condition_ids_ = {dose_done_id, flush_done_id, low_flow_done_id, soak_done_id, high_flow_done_id,
                      cip_tank_empty_id, cip_tank_full_id, refilling_id};
    task_ids_ = {start_dose_id, heater_on_id, start_flush_id, start_low_flow_id, start_soak_id,
                 start_high_flow_id, start_final_flush_id, heater_off_id, start_refill_id, stop_id};
    return true;
}

bool ro_procedure::media_backwash_cycle(ro& r, scheduler& sched) {
    (void)r;
    (void)sched;
    std::fprintf(stderr, "ro_procedure: MEDIA_BACKWASH_CYCLE is not implemented yet\n");
    return false;
}
