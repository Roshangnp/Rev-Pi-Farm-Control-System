#include "procedure.h"

#include "config.h"
#include "ro_config.h"

#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

procedure::procedure(scheduler& sched)
    : sched_(sched) {}

procedure::~procedure() {
    stop();
}

bool procedure::isRunning() const {
    for (task_id id : task_ids_) {
        for (const auto& entry : sched_.tasks()) {
            if (entry.first == id && !entry.second.done) return true;
        }
    }
    return false;   // never run, stopped, or every task has run (a task removed from the scheduler counts as done)
}

bool procedure::run(procedure_types procedure_type) {
    if (isRunning()) {
        std::fprintf(stderr, "procedure: %s is already running, can't start %s\n",
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
    case procedure_types::EMPTY_FEED_TANK: ok = empty_feed_tank(); break;
    case procedure_types::OVERHEAD_PATTERN_1: ok = run_overhead1_pattern(); break;
    case procedure_types::OVERHEAD_6_IRRIGATION: ok = run_overhead6_irrigation(); break;
    }
    if (!ok) stop();   // don't leave half a procedure scheduled
    return ok;
}

bool procedure::readProcedure(bool& running, procedure_types& p) {
    running = isRunning();
    p = procedure_type_;
    return true;
}

bool procedure::stop() {
    // Tasks first: a condition still used by a task can't be removed
    for (task_id id : task_ids_) sched_.removeTask(id);
    bool ok = true;
    for (condition_id id : condition_ids_) ok = sched_.removeCondition(id) && ok;
    task_ids_.clear();
    condition_ids_.clear();
    return ok;
}

bool procedure::low_ph_cip_cycle() {

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
        [] { RO_state s; return readROState(s) && s == RO_state::CIP_REFILL; }));

    task_id start_dose_id;
    task_id start_flush_id;
    task_id start_low_flow_id;
    task_id start_soak_id;
    task_id start_high_flow_id;
    task_id start_final_flush_id;
    task_id start_refill_id;
    task_id stop_id;
    sched_.addTask({"start_low_ph_dose", [] {return setROState(RO_state::LOW_PH_DOSE);}, {}, task_mode::ONCE}, start_dose_id);
    sched_.addTask({"start_cip_flush", [] {return setROState(RO_state::CIP_FLUSH);}, {dose_done_id}, task_mode::ONCE}, start_flush_id);
    sched_.addTask({"start_cip_low_flow", [] {return setROState(RO_state::CIP_LOW_FLOW_CYCLE);}, {flush_done_id}, task_mode::ONCE}, start_low_flow_id);
    sched_.addTask({"start_cip_soak", [] {return setROState(RO_state::CIP_SOAK);}, {low_flow_done_id}, task_mode::ONCE}, start_soak_id);
    sched_.addTask({"start_cip_high_flow", [] {return setROState(RO_state::CIP_HIGH_FLOW_CYCLE);}, {soak_done_id}, task_mode::ONCE}, start_high_flow_id);
    sched_.addTask({"start_final_cip_flush", [] {return setROState(RO_state::CIP_FLUSH);}, {high_flow_done_id}, task_mode::ONCE}, start_final_flush_id);
    sched_.addTask({"start_cip_refill", [] {return setROState(RO_state::CIP_REFILL);}, {high_flow_done_id, cip_tank_empty_id}, task_mode::ONCE}, start_refill_id);
    sched_.addTask({"stop_low_ph_cip_cycle", [] {return setROState(RO_state::OFF);}, {refilling_id, cip_tank_full_id}, task_mode::ONCE}, stop_id);

    condition_ids_ = {dose_done_id, flush_done_id, low_flow_done_id, soak_done_id, high_flow_done_id,
                      cip_tank_empty_id, cip_tank_full_id, refilling_id};
    task_ids_ = {start_dose_id, start_flush_id, start_low_flow_id, start_soak_id,
                 start_high_flow_id, start_final_flush_id, start_refill_id, stop_id};
    return true;
}

bool procedure::high_ph_cip_cycle() {

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
        [] { RO_state s; return readROState(s) && s == RO_state::CIP_REFILL; }));

    task_id start_dose_id;
    task_id start_flush_id;
    task_id start_low_flow_id;
    task_id start_soak_id;
    task_id start_high_flow_id;
    task_id start_final_flush_id;
    task_id start_refill_id;
    task_id stop_id;
    sched_.addTask({"start_high_ph_dose", [] {return setROState(RO_state::HIGH_PH_DOSE);}, {}, task_mode::ONCE}, start_dose_id);
    sched_.addTask({"start_cip_flush", [] {return setROState(RO_state::CIP_FLUSH);}, {dose_done_id}, task_mode::ONCE}, start_flush_id);
    sched_.addTask({"start_cip_low_flow", [] {return setROState(RO_state::CIP_LOW_FLOW_CYCLE);}, {flush_done_id}, task_mode::ONCE}, start_low_flow_id);
    sched_.addTask({"start_cip_soak", [] {return setROState(RO_state::CIP_SOAK);}, {low_flow_done_id}, task_mode::ONCE}, start_soak_id);
    sched_.addTask({"start_cip_high_flow", [] {return setROState(RO_state::CIP_HIGH_FLOW_CYCLE);}, {soak_done_id}, task_mode::ONCE}, start_high_flow_id);
    sched_.addTask({"start_final_cip_flush", [] {return setROState(RO_state::CIP_FLUSH);}, {high_flow_done_id}, task_mode::ONCE}, start_final_flush_id);
    sched_.addTask({"start_cip_refill", [] {return setROState(RO_state::CIP_REFILL);}, {high_flow_done_id, cip_tank_empty_id}, task_mode::ONCE}, start_refill_id);
    sched_.addTask({"stop_high_ph_cip_cycle", [] {return setROState(RO_state::OFF);}, {refilling_id, cip_tank_full_id}, task_mode::ONCE}, stop_id);

    condition_ids_ = {dose_done_id, flush_done_id, low_flow_done_id, soak_done_id, high_flow_done_id,
                      cip_tank_empty_id, cip_tank_full_id, refilling_id};
    task_ids_ = {start_dose_id, start_flush_id, start_low_flow_id, start_soak_id,
                 start_high_flow_id, start_final_flush_id, start_refill_id, stop_id};
    return true;
}

bool procedure::media_backwash_cycle() {

    // Every afterDuration counts from now (when it's created), so each step's end
    // is the sum of the step times before it
    condition_id first_backwash_done_id = sched_.addCondition(condition::afterDuration("first_backwash_done",
        std::chrono::minutes(MEDIA_BACKWASH_TIME_MIN)));
    condition_id second_backwash_done_id = sched_.addCondition(condition::afterDuration("second_backwash_done",
        std::chrono::minutes(MEDIA_BACKWASH_TIME_MIN + MEDIA_BACKWASH_TIME_MIN)));
    
    task_id start_first_backwash_id;
    task_id start_second_backwash_id;
    task_id stop_id;
    sched_.addTask({"start_first_backwash", [] {return setROState(RO_state::MEDIA_BACKWASH_1);}, {}, task_mode::ONCE}, start_first_backwash_id);
    sched_.addTask({"start_second_backwash", [] {return setROState(RO_state::MEDIA_BACKWASH_2);}, {first_backwash_done_id}, task_mode::ONCE}, start_second_backwash_id);
    sched_.addTask({"stop_media_backwash_cycle", [] {return setROState(RO_state::OFF);}, {second_backwash_done_id}, task_mode::ONCE}, stop_id);

    condition_ids_ = {first_backwash_done_id, second_backwash_done_id};
    task_ids_ = {start_first_backwash_id, start_second_backwash_id, stop_id};
    return true;
}

bool procedure::empty_feed_tank() {

    // A float that can't be read counts as not met, so the step waits
    condition_id feed_tank_low_submerged_id = sched_.addCondition(condition::when("feed_tank_low_submerged",
        [] { bool submerged; return feed_tank_low_float.readSubmerged(submerged) && submerged; }));
    condition_id feed_tank_low_dry_id = sched_.addCondition(condition::when("feed_tank_low_dry",
        [] { bool submerged; return feed_tank_low_float.readSubmerged(submerged) && !submerged; }));
    condition_id permeate_tank_high_submerged_id = sched_.addCondition(condition::when("permeate_tank_high_submerged",
        [] { bool submerged; return permeate_tank_high_float.readSubmerged(submerged) && submerged; }));
    condition_id permeate_tank_high_dry_id = sched_.addCondition(condition::when("permeate_tank_high_dry",
        [] { bool submerged; return permeate_tank_high_float.readSubmerged(submerged) && !submerged; }));
    condition_id feed_tank_low_read_failed_id = sched_.addCondition(condition::when("feed_tank_low_read_failed",
        [] { bool submerged; return !feed_tank_low_float.readSubmerged(submerged); }));
    condition_id permeate_tank_high_read_failed_id = sched_.addCondition(condition::when("permeate_tank_high_read_failed",
        [] { bool submerged; return !permeate_tank_high_float.readSubmerged(submerged); }));
    condition_id ro_off_id = sched_.addCondition(condition::when("ro_off",
        [] { RO_state s; return readROState(s) && s == RO_state::OFF; }));
    condition_id ro_normal_op_id = sched_.addCondition(condition::when("ro_normal_op",
        [] { RO_state s; return readROState(s) && s == RO_state::NORMAL_OP; }));

    condition_ids_ = {feed_tank_low_submerged_id, feed_tank_low_dry_id,
                      permeate_tank_high_submerged_id, permeate_tank_high_dry_id,
                      feed_tank_low_read_failed_id, permeate_tank_high_read_failed_id, ro_off_id,
                      ro_normal_op_id};

    // The RO moves water from the feed tank to the permeate tank, so it runs
    // while the feed tank has water and the permeate tank has room. The procedure
    // ends (OFF, removes its tasks) when the permeate tank is full or the feed
    // tank low float is dry, whichever comes first. A float that can't be read
    // only pauses it (OFF); it resumes (ON_RISING) once both read again.
    task_id run_id;
    task_id stop_permeate_full_id;
    task_id pause_permeate_read_failed_id;
    task_id pause_feed_read_failed_id;
    task_id done_id;
    sched_.addTask({"empty_feed_tank_run", [] {return setROState(RO_state::NORMAL_OP);},
                    {feed_tank_low_submerged_id, permeate_tank_high_dry_id}, task_mode::ON_RISING}, run_id);
    // Done (permeate tank full): OFF, then remove every task and condition. In any
    // RO state, so a tank that is already full at the start ends it on the first pass
    sched_.addTask({"empty_feed_tank_stop_permeate_full", [this] {
                        bool ok = setROState(RO_state::OFF);
                        return stop() && ok;
                    }, {permeate_tank_high_submerged_id}, task_mode::ONCE}, stop_permeate_full_id);
    // A float that can't be read pauses the RO; it resumes once both read again
    sched_.addTask({"empty_feed_tank_pause_permeate_read_failed", [] {return setROState(RO_state::OFF);},
                    {permeate_tank_high_read_failed_id, ro_normal_op_id}, task_mode::ON_RISING}, pause_permeate_read_failed_id);
    sched_.addTask({"empty_feed_tank_pause_feed_read_failed", [] {return setROState(RO_state::OFF);},
                    {feed_tank_low_read_failed_id, ro_normal_op_id}, task_mode::ON_RISING}, pause_feed_read_failed_id);
    // Done (feed tank empty): OFF, then remove every task and condition (the
    // ON_RISING tasks never finish by themselves), so isRunning() goes false.
    // The scheduler allows a task to remove itself.
    sched_.addTask({"empty_feed_tank_done", [this] {
                        bool ok = setROState(RO_state::OFF);
                        return stop() && ok;
                    }, {feed_tank_low_dry_id}, task_mode::ONCE}, done_id);

    task_ids_ = {run_id, stop_permeate_full_id, pause_permeate_read_failed_id,
                 pause_feed_read_failed_id, done_id};
    return true;
}

bool procedure::run_overhead1_pattern() {

    // wiseconn_irrigation_tables/overhead_pattern_1.csv, one WiseConn irrigation per row
    // (today). Each scheduleIrrigation is one WiseConn request (about 1 s, blocking the
    // loop it runs in), so they go out 2 s apart
    condition_id step_2_id = sched_.addCondition(condition::afterDuration("irrigation_step_2", std::chrono::seconds(2)));
    condition_id step_3_id = sched_.addCondition(condition::afterDuration("irrigation_step_3", std::chrono::seconds(4)));
    condition_id step_4_id = sched_.addCondition(condition::afterDuration("irrigation_step_4", std::chrono::seconds(6)));
    condition_id step_5_id = sched_.addCondition(condition::afterDuration("irrigation_step_5", std::chrono::seconds(8)));
    condition_id step_6_id = sched_.addCondition(condition::afterDuration("irrigation_step_6", std::chrono::seconds(10)));
    condition_id step_7_id = sched_.addCondition(condition::afterDuration("irrigation_step_7", std::chrono::seconds(12)));
    condition_id step_8_id = sched_.addCondition(condition::afterDuration("irrigation_step_8", std::chrono::seconds(14)));
    condition_id step_9_id = sched_.addCondition(condition::afterDuration("irrigation_step_9", std::chrono::seconds(16)));
    condition_id step_10_id = sched_.addCondition(condition::afterDuration("irrigation_step_10", std::chrono::seconds(18)));
    condition_id step_11_id = sched_.addCondition(condition::afterDuration("irrigation_step_11", std::chrono::seconds(20)));
    condition_id step_12_id = sched_.addCondition(condition::afterDuration("irrigation_step_12", std::chrono::seconds(22)));
    condition_id step_13_id = sched_.addCondition(condition::afterDuration("irrigation_step_13", std::chrono::seconds(24)));
    condition_id step_14_id = sched_.addCondition(condition::afterDuration("irrigation_step_14", std::chrono::seconds(26)));
    condition_id step_15_id = sched_.addCondition(condition::afterDuration("irrigation_step_15", std::chrono::seconds(28)));
    condition_id step_16_id = sched_.addCondition(condition::afterDuration("irrigation_step_16", std::chrono::seconds(30)));
    condition_id step_17_id = sched_.addCondition(condition::afterDuration("irrigation_step_17", std::chrono::seconds(32)));
    condition_id step_18_id = sched_.addCondition(condition::afterDuration("irrigation_step_18", std::chrono::seconds(34)));

    task_id irrigation_1_id;
    task_id irrigation_2_id;
    task_id irrigation_3_id;
    task_id irrigation_4_id;
    task_id irrigation_5_id;
    task_id irrigation_6_id;
    task_id irrigation_7_id;
    task_id irrigation_8_id;
    task_id irrigation_9_id;
    task_id irrigation_10_id;
    task_id irrigation_11_id;
    task_id irrigation_12_id;
    task_id irrigation_13_id;
    task_id irrigation_14_id;
    task_id irrigation_15_id;
    task_id irrigation_16_id;
    task_id irrigation_17_id;
    task_id irrigation_18_id;
    sched_.addTask({"schedule_overhead_1_1400", [] {return overhead_1.scheduleIrrigation(14, 0, 14, 15);}, {}, task_mode::ONCE}, irrigation_1_id);
    sched_.addTask({"schedule_overhead_2_1400", [] {return overhead_2.scheduleIrrigation(14, 0, 14, 15);}, {step_2_id}, task_mode::ONCE}, irrigation_2_id);
    sched_.addTask({"schedule_overhead_3_1415", [] {return overhead_3.scheduleIrrigation(14, 15, 14, 30);}, {step_3_id}, task_mode::ONCE}, irrigation_3_id);
    sched_.addTask({"schedule_overhead_4_1415", [] {return overhead_4.scheduleIrrigation(14, 15, 14, 30);}, {step_4_id}, task_mode::ONCE}, irrigation_4_id);
    sched_.addTask({"schedule_overhead_5_1430", [] {return overhead_5.scheduleIrrigation(14, 30, 14, 45);}, {step_5_id}, task_mode::ONCE}, irrigation_5_id);
    sched_.addTask({"schedule_overhead_6_1430", [] {return overhead_6.scheduleIrrigation(14, 30, 14, 45);}, {step_6_id}, task_mode::ONCE}, irrigation_6_id);
    sched_.addTask({"schedule_overhead_1_1445", [] {return overhead_1.scheduleIrrigation(14, 45, 15, 0);}, {step_7_id}, task_mode::ONCE}, irrigation_7_id);
    sched_.addTask({"schedule_overhead_2_1445", [] {return overhead_2.scheduleIrrigation(14, 45, 15, 0);}, {step_8_id}, task_mode::ONCE}, irrigation_8_id);
    sched_.addTask({"schedule_overhead_3_1500", [] {return overhead_3.scheduleIrrigation(15, 0, 15, 15);}, {step_9_id}, task_mode::ONCE}, irrigation_9_id);
    sched_.addTask({"schedule_overhead_4_1500", [] {return overhead_4.scheduleIrrigation(15, 0, 15, 15);}, {step_10_id}, task_mode::ONCE}, irrigation_10_id);
    sched_.addTask({"schedule_overhead_5_1515", [] {return overhead_5.scheduleIrrigation(15, 15, 15, 30);}, {step_11_id}, task_mode::ONCE}, irrigation_11_id);
    sched_.addTask({"schedule_overhead_6_1515", [] {return overhead_6.scheduleIrrigation(15, 15, 15, 30);}, {step_12_id}, task_mode::ONCE}, irrigation_12_id);
    sched_.addTask({"schedule_overhead_1_1530", [] {return overhead_1.scheduleIrrigation(15, 30, 15, 45);}, {step_13_id}, task_mode::ONCE}, irrigation_13_id);
    sched_.addTask({"schedule_overhead_2_1530", [] {return overhead_2.scheduleIrrigation(15, 30, 15, 45);}, {step_14_id}, task_mode::ONCE}, irrigation_14_id);
    sched_.addTask({"schedule_overhead_3_1545", [] {return overhead_3.scheduleIrrigation(15, 45, 16, 0);}, {step_15_id}, task_mode::ONCE}, irrigation_15_id);
    sched_.addTask({"schedule_overhead_4_1545", [] {return overhead_4.scheduleIrrigation(15, 45, 16, 0);}, {step_16_id}, task_mode::ONCE}, irrigation_16_id);
    sched_.addTask({"schedule_overhead_6_1600", [] {return overhead_6.scheduleIrrigation(16, 0, 16, 15);}, {step_17_id}, task_mode::ONCE}, irrigation_17_id);
    sched_.addTask({"schedule_overhead_5_1600", [] {return overhead_5.scheduleIrrigation(16, 0, 16, 15);}, {step_18_id}, task_mode::ONCE}, irrigation_18_id);

    condition_ids_ = {step_2_id, step_3_id, step_4_id, step_5_id, step_6_id, step_7_id, step_8_id, step_9_id, step_10_id, step_11_id, step_12_id, step_13_id, step_14_id, step_15_id, step_16_id, step_17_id, step_18_id};
    task_ids_ = {irrigation_1_id, irrigation_2_id, irrigation_3_id, irrigation_4_id, irrigation_5_id, irrigation_6_id, irrigation_7_id, irrigation_8_id, irrigation_9_id, irrigation_10_id, irrigation_11_id, irrigation_12_id, irrigation_13_id, irrigation_14_id, irrigation_15_id, irrigation_16_id, irrigation_17_id, irrigation_18_id};
    return true;
}

bool procedure::run_overhead6_irrigation() {

    // One WiseConn irrigation on overhead 6, today 20:00-21:00
    task_id irrigation_id;
    sched_.addTask({"schedule_overhead_6_2000", [] {return overhead_6.scheduleIrrigation(20, 0, 21, 0);}, {}, task_mode::ONCE}, irrigation_id);

    condition_ids_ = {};
    task_ids_ = {irrigation_id};
    return true;
}
