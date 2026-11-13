#include "procedure.h"

#include "config.h"
#include "ro_config.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

procedure::procedure(scheduler& sched, std::function<bool()> on_user_stop)
    : sched_(sched), on_user_stop_(std::move(on_user_stop)) {}

procedure::~procedure() {
    clear();
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
    clear();   // a finished run's leftovers
    procedure_type_ = procedure_type;
    bool ok = false;
    switch (procedure_type_) {
    case procedure_types::LOW_PH_CIP_CYCLE: ok = low_ph_cip_cycle(); break;
    case procedure_types::HIGH_PH_CIP_CYCLE: ok = high_ph_cip_cycle(); break;
    case procedure_types::MEDIA_BACKWASH_CYCLE: ok = media_backwash_cycle(); break;
    case procedure_types::EMPTY_FEED_TANK: ok = empty_feed_tank(); break;
    case procedure_types::OVERHEAD_PATTERN_1: ok = run_overhead1_pattern(); break;
    case procedure_types::IRRIGATION_PATTERN_1: ok = run_irrigation_pattern_1(); break;
    case procedure_types::REPRESSURE_CONTROL: ok = repressure_control(); break;
    case procedure_types::CANCEL_OVERHEAD_PATTERN_1: ok = cancel_overhead1_pattern(); break;
    case procedure_types::CANCEL_IRRIGATION_PATTERN_1: ok = cancel_irrigation_pattern_1(); break;
    }
    if (!ok) clear();   // don't leave half a procedure scheduled
    return ok;
}

bool procedure::readProcedure(bool& running, procedure_types& p) {
    running = isRunning();
    p = procedure_type_;
    return true;
}

bool procedure::stop() {
    bool was_running = isRunning();
    bool ok = clear();
    if (was_running && on_user_stop_) ok = on_user_stop_() && ok;
    return ok;
}

bool procedure::clear() {
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
                        return clear() && ok;
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
                        return clear() && ok;
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


    // Stays running until the last irrigation ends (today 16:15), so a user stop
    // still cancels the bookings until then
    std::time_t now_time = std::time(nullptr);
    std::tm end_tm{};
    localtime_r(&now_time, &end_tm);
    end_tm.tm_hour = 16;
    end_tm.tm_min = 15;
    end_tm.tm_sec = 0;
    end_tm.tm_isdst = -1;
    condition_id pattern_end_id = sched_.addCondition(condition::after("overhead_pattern_end", schedule_clock::from_time_t(std::mktime(&end_tm))));
    task_id pattern_end_task_id;
    sched_.addTask({"overhead_pattern_end", [] { return true; }, {pattern_end_id}, task_mode::ONCE}, pattern_end_task_id);

    condition_ids_ = {step_2_id, step_3_id, step_4_id, step_5_id, step_6_id, step_7_id, step_8_id, step_9_id, step_10_id, step_11_id, step_12_id, step_13_id, step_14_id, step_15_id, step_16_id, step_17_id, step_18_id, pattern_end_id};
    task_ids_ = {irrigation_1_id, irrigation_2_id, irrigation_3_id, irrigation_4_id, irrigation_5_id, irrigation_6_id, irrigation_7_id, irrigation_8_id, irrigation_9_id, irrigation_10_id, irrigation_11_id, irrigation_12_id, irrigation_13_id, irrigation_14_id, irrigation_15_id, irrigation_16_id, irrigation_17_id, irrigation_18_id, pattern_end_task_id};
    return true;
}

bool procedure::run_irrigation_pattern_1() {

    // Every irrigation valve for 1 hour, back to back, the first starting 1 hour from
    // now (farm local time, today: a valve whose hour would run past 23:59 is refused
    // by the device). Each scheduleIrrigation is one WiseConn request (about 1 s,
    // blocking the loop it runs in), so they go out 2 s apart
    std::time_t now_time = std::time(nullptr);
    std::tm now{};
    localtime_r(&now_time, &now);
    int h = now.tm_hour + 1;   // start of the first irrigation: 1 hour from now
    int m = now.tm_min;

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
    sched_.addTask({"schedule_valve_1_1", [h, m] {return valve_1_1.scheduleIrrigation(h, m, h + 1, m);}, {}, task_mode::ONCE}, irrigation_1_id);
    sched_.addTask({"schedule_valve_1_2", [h, m] {return valve_1_2.scheduleIrrigation(h + 1, m, h + 2, m);}, {step_2_id}, task_mode::ONCE}, irrigation_2_id);
    sched_.addTask({"schedule_valve_2_1", [h, m] {return valve_2_1.scheduleIrrigation(h + 2, m, h + 3, m);}, {step_3_id}, task_mode::ONCE}, irrigation_3_id);
    sched_.addTask({"schedule_valve_3_1", [h, m] {return valve_3_1.scheduleIrrigation(h + 3, m, h + 4, m);}, {step_4_id}, task_mode::ONCE}, irrigation_4_id);
    sched_.addTask({"schedule_valve_4_1", [h, m] {return valve_4_1.scheduleIrrigation(h + 4, m, h + 5, m);}, {step_5_id}, task_mode::ONCE}, irrigation_5_id);
    sched_.addTask({"schedule_valve_5_1", [h, m] {return valve_5_1.scheduleIrrigation(h + 5, m, h + 6, m);}, {step_6_id}, task_mode::ONCE}, irrigation_6_id);
    sched_.addTask({"schedule_valve_5_2", [h, m] {return valve_5_2.scheduleIrrigation(h + 6, m, h + 7, m);}, {step_7_id}, task_mode::ONCE}, irrigation_7_id);
    sched_.addTask({"schedule_valve_5_3", [h, m] {return valve_5_3.scheduleIrrigation(h + 7, m, h + 8, m);}, {step_8_id}, task_mode::ONCE}, irrigation_8_id);
    sched_.addTask({"schedule_valve_5_4", [h, m] {return valve_5_4.scheduleIrrigation(h + 8, m, h + 9, m);}, {step_9_id}, task_mode::ONCE}, irrigation_9_id);
    sched_.addTask({"schedule_valve_6_1", [h, m] {return valve_6_1.scheduleIrrigation(h + 9, m, h + 10, m);}, {step_10_id}, task_mode::ONCE}, irrigation_10_id);
    sched_.addTask({"schedule_valve_6_2", [h, m] {return valve_6_2.scheduleIrrigation(h + 10, m, h + 11, m);}, {step_11_id}, task_mode::ONCE}, irrigation_11_id);
    sched_.addTask({"schedule_valve_7_1", [h, m] {return valve_7_1.scheduleIrrigation(h + 11, m, h + 12, m);}, {step_12_id}, task_mode::ONCE}, irrigation_12_id);
    sched_.addTask({"schedule_valve_8_1", [h, m] {return valve_8_1.scheduleIrrigation(h + 12, m, h + 13, m);}, {step_13_id}, task_mode::ONCE}, irrigation_13_id);
    sched_.addTask({"schedule_valve_9_1", [h, m] {return valve_9_1.scheduleIrrigation(h + 13, m, h + 14, m);}, {step_14_id}, task_mode::ONCE}, irrigation_14_id);
    sched_.addTask({"schedule_valve_10_1", [h, m] {return valve_10_1.scheduleIrrigation(h + 14, m, h + 15, m);}, {step_15_id}, task_mode::ONCE}, irrigation_15_id);
    sched_.addTask({"schedule_valve_10_2", [h, m] {return valve_10_2.scheduleIrrigation(h + 15, m, h + 16, m);}, {step_16_id}, task_mode::ONCE}, irrigation_16_id);
    sched_.addTask({"schedule_valve_10_3", [h, m] {return valve_10_3.scheduleIrrigation(h + 16, m, h + 17, m);}, {step_17_id}, task_mode::ONCE}, irrigation_17_id);


    // Stays running until the last irrigation that fits today ends (the device
    // refuses any ending after 23:59), so a user stop still cancels the bookings until then
    int booked = std::min(17, (23 * 60 + 59 - (h * 60 + m)) / 60);   // valves that fit today
    std::tm end_tm = now;
    end_tm.tm_hour = booked > 0 ? h : now.tm_hour;
    end_tm.tm_min = booked > 0 ? m + 60 * booked : now.tm_min;   // mktime carries minutes into hours
    end_tm.tm_sec = 0;
    end_tm.tm_isdst = -1;
    condition_id pattern_end_id = sched_.addCondition(condition::after("irrigation_pattern_end", schedule_clock::from_time_t(std::mktime(&end_tm))));
    task_id pattern_end_task_id;
    sched_.addTask({"irrigation_pattern_end", [] { return true; }, {pattern_end_id}, task_mode::ONCE}, pattern_end_task_id);

    condition_ids_ = {step_2_id, step_3_id, step_4_id, step_5_id, step_6_id, step_7_id, step_8_id, step_9_id, step_10_id, step_11_id, step_12_id, step_13_id, step_14_id, step_15_id, step_16_id, step_17_id, pattern_end_id};
    task_ids_ = {irrigation_1_id, irrigation_2_id, irrigation_3_id, irrigation_4_id, irrigation_5_id, irrigation_6_id, irrigation_7_id, irrigation_8_id, irrigation_9_id, irrigation_10_id, irrigation_11_id, irrigation_12_id, irrigation_13_id, irrigation_14_id, irrigation_15_id, irrigation_16_id, irrigation_17_id, pattern_end_task_id};
    return true;
}

bool procedure::cancel_overhead1_pattern() {

    // Cancels what each overhead booked (cancelIrrigation: every booking the zone made,
    // one WiseConn DELETE each, about 1 s, blocking the loop it runs in; zones with
    // nothing booked are skipped), one zone every 2 s
    condition_id step_2_id = sched_.addCondition(condition::afterDuration("cancel_step_2", std::chrono::seconds(2)));
    condition_id step_3_id = sched_.addCondition(condition::afterDuration("cancel_step_3", std::chrono::seconds(4)));
    condition_id step_4_id = sched_.addCondition(condition::afterDuration("cancel_step_4", std::chrono::seconds(6)));
    condition_id step_5_id = sched_.addCondition(condition::afterDuration("cancel_step_5", std::chrono::seconds(8)));
    condition_id step_6_id = sched_.addCondition(condition::afterDuration("cancel_step_6", std::chrono::seconds(10)));

    task_id cancel_1_id;
    task_id cancel_2_id;
    task_id cancel_3_id;
    task_id cancel_4_id;
    task_id cancel_5_id;
    task_id cancel_6_id;
    sched_.addTask({"cancel_overhead_1", [] {int id; return !overhead_1.readScheduledIrrigation(id) || overhead_1.cancelIrrigation();}, {}, task_mode::ONCE}, cancel_1_id);
    sched_.addTask({"cancel_overhead_2", [] {int id; return !overhead_2.readScheduledIrrigation(id) || overhead_2.cancelIrrigation();}, {step_2_id}, task_mode::ONCE}, cancel_2_id);
    sched_.addTask({"cancel_overhead_3", [] {int id; return !overhead_3.readScheduledIrrigation(id) || overhead_3.cancelIrrigation();}, {step_3_id}, task_mode::ONCE}, cancel_3_id);
    sched_.addTask({"cancel_overhead_4", [] {int id; return !overhead_4.readScheduledIrrigation(id) || overhead_4.cancelIrrigation();}, {step_4_id}, task_mode::ONCE}, cancel_4_id);
    sched_.addTask({"cancel_overhead_5", [] {int id; return !overhead_5.readScheduledIrrigation(id) || overhead_5.cancelIrrigation();}, {step_5_id}, task_mode::ONCE}, cancel_5_id);
    sched_.addTask({"cancel_overhead_6", [] {int id; return !overhead_6.readScheduledIrrigation(id) || overhead_6.cancelIrrigation();}, {step_6_id}, task_mode::ONCE}, cancel_6_id);

    condition_ids_ = {step_2_id, step_3_id, step_4_id, step_5_id, step_6_id};
    task_ids_ = {cancel_1_id, cancel_2_id, cancel_3_id, cancel_4_id, cancel_5_id, cancel_6_id};
    return true;
}

bool procedure::cancel_irrigation_pattern_1() {

    // Cancels what each irrigation valve booked (cancelIrrigation: every booking the
    // zone made, one WiseConn DELETE each, about 1 s, blocking the loop it runs in;
    // valves with nothing booked are skipped), one valve every 2 s
    condition_id step_2_id = sched_.addCondition(condition::afterDuration("cancel_step_2", std::chrono::seconds(2)));
    condition_id step_3_id = sched_.addCondition(condition::afterDuration("cancel_step_3", std::chrono::seconds(4)));
    condition_id step_4_id = sched_.addCondition(condition::afterDuration("cancel_step_4", std::chrono::seconds(6)));
    condition_id step_5_id = sched_.addCondition(condition::afterDuration("cancel_step_5", std::chrono::seconds(8)));
    condition_id step_6_id = sched_.addCondition(condition::afterDuration("cancel_step_6", std::chrono::seconds(10)));
    condition_id step_7_id = sched_.addCondition(condition::afterDuration("cancel_step_7", std::chrono::seconds(12)));
    condition_id step_8_id = sched_.addCondition(condition::afterDuration("cancel_step_8", std::chrono::seconds(14)));
    condition_id step_9_id = sched_.addCondition(condition::afterDuration("cancel_step_9", std::chrono::seconds(16)));
    condition_id step_10_id = sched_.addCondition(condition::afterDuration("cancel_step_10", std::chrono::seconds(18)));
    condition_id step_11_id = sched_.addCondition(condition::afterDuration("cancel_step_11", std::chrono::seconds(20)));
    condition_id step_12_id = sched_.addCondition(condition::afterDuration("cancel_step_12", std::chrono::seconds(22)));
    condition_id step_13_id = sched_.addCondition(condition::afterDuration("cancel_step_13", std::chrono::seconds(24)));
    condition_id step_14_id = sched_.addCondition(condition::afterDuration("cancel_step_14", std::chrono::seconds(26)));
    condition_id step_15_id = sched_.addCondition(condition::afterDuration("cancel_step_15", std::chrono::seconds(28)));
    condition_id step_16_id = sched_.addCondition(condition::afterDuration("cancel_step_16", std::chrono::seconds(30)));
    condition_id step_17_id = sched_.addCondition(condition::afterDuration("cancel_step_17", std::chrono::seconds(32)));

    task_id cancel_1_id;
    task_id cancel_2_id;
    task_id cancel_3_id;
    task_id cancel_4_id;
    task_id cancel_5_id;
    task_id cancel_6_id;
    task_id cancel_7_id;
    task_id cancel_8_id;
    task_id cancel_9_id;
    task_id cancel_10_id;
    task_id cancel_11_id;
    task_id cancel_12_id;
    task_id cancel_13_id;
    task_id cancel_14_id;
    task_id cancel_15_id;
    task_id cancel_16_id;
    task_id cancel_17_id;
    sched_.addTask({"cancel_valve_1_1", [] {int id; return !valve_1_1.readScheduledIrrigation(id) || valve_1_1.cancelIrrigation();}, {}, task_mode::ONCE}, cancel_1_id);
    sched_.addTask({"cancel_valve_1_2", [] {int id; return !valve_1_2.readScheduledIrrigation(id) || valve_1_2.cancelIrrigation();}, {step_2_id}, task_mode::ONCE}, cancel_2_id);
    sched_.addTask({"cancel_valve_2_1", [] {int id; return !valve_2_1.readScheduledIrrigation(id) || valve_2_1.cancelIrrigation();}, {step_3_id}, task_mode::ONCE}, cancel_3_id);
    sched_.addTask({"cancel_valve_3_1", [] {int id; return !valve_3_1.readScheduledIrrigation(id) || valve_3_1.cancelIrrigation();}, {step_4_id}, task_mode::ONCE}, cancel_4_id);
    sched_.addTask({"cancel_valve_4_1", [] {int id; return !valve_4_1.readScheduledIrrigation(id) || valve_4_1.cancelIrrigation();}, {step_5_id}, task_mode::ONCE}, cancel_5_id);
    sched_.addTask({"cancel_valve_5_1", [] {int id; return !valve_5_1.readScheduledIrrigation(id) || valve_5_1.cancelIrrigation();}, {step_6_id}, task_mode::ONCE}, cancel_6_id);
    sched_.addTask({"cancel_valve_5_2", [] {int id; return !valve_5_2.readScheduledIrrigation(id) || valve_5_2.cancelIrrigation();}, {step_7_id}, task_mode::ONCE}, cancel_7_id);
    sched_.addTask({"cancel_valve_5_3", [] {int id; return !valve_5_3.readScheduledIrrigation(id) || valve_5_3.cancelIrrigation();}, {step_8_id}, task_mode::ONCE}, cancel_8_id);
    sched_.addTask({"cancel_valve_5_4", [] {int id; return !valve_5_4.readScheduledIrrigation(id) || valve_5_4.cancelIrrigation();}, {step_9_id}, task_mode::ONCE}, cancel_9_id);
    sched_.addTask({"cancel_valve_6_1", [] {int id; return !valve_6_1.readScheduledIrrigation(id) || valve_6_1.cancelIrrigation();}, {step_10_id}, task_mode::ONCE}, cancel_10_id);
    sched_.addTask({"cancel_valve_6_2", [] {int id; return !valve_6_2.readScheduledIrrigation(id) || valve_6_2.cancelIrrigation();}, {step_11_id}, task_mode::ONCE}, cancel_11_id);
    sched_.addTask({"cancel_valve_7_1", [] {int id; return !valve_7_1.readScheduledIrrigation(id) || valve_7_1.cancelIrrigation();}, {step_12_id}, task_mode::ONCE}, cancel_12_id);
    sched_.addTask({"cancel_valve_8_1", [] {int id; return !valve_8_1.readScheduledIrrigation(id) || valve_8_1.cancelIrrigation();}, {step_13_id}, task_mode::ONCE}, cancel_13_id);
    sched_.addTask({"cancel_valve_9_1", [] {int id; return !valve_9_1.readScheduledIrrigation(id) || valve_9_1.cancelIrrigation();}, {step_14_id}, task_mode::ONCE}, cancel_14_id);
    sched_.addTask({"cancel_valve_10_1", [] {int id; return !valve_10_1.readScheduledIrrigation(id) || valve_10_1.cancelIrrigation();}, {step_15_id}, task_mode::ONCE}, cancel_15_id);
    sched_.addTask({"cancel_valve_10_2", [] {int id; return !valve_10_2.readScheduledIrrigation(id) || valve_10_2.cancelIrrigation();}, {step_16_id}, task_mode::ONCE}, cancel_16_id);
    sched_.addTask({"cancel_valve_10_3", [] {int id; return !valve_10_3.readScheduledIrrigation(id) || valve_10_3.cancelIrrigation();}, {step_17_id}, task_mode::ONCE}, cancel_17_id);

    condition_ids_ = {step_2_id, step_3_id, step_4_id, step_5_id, step_6_id, step_7_id, step_8_id, step_9_id, step_10_id, step_11_id, step_12_id, step_13_id, step_14_id, step_15_id, step_16_id, step_17_id};
    task_ids_ = {cancel_1_id, cancel_2_id, cancel_3_id, cancel_4_id, cancel_5_id, cancel_6_id, cancel_7_id, cancel_8_id, cancel_9_id, cancel_10_id, cancel_11_id, cancel_12_id, cancel_13_id, cancel_14_id, cancel_15_id, cancel_16_id, cancel_17_id};
    return true;
}

bool procedure::repressure_control() {

    // Moved from main_ro.cpp's schedule_RO_operation()
    condition_id feed_tank_low_submerged = sched_.addCondition(condition::when("feed tank low float submerged", [] { bool s; return feed_tank_low_float.readSubmerged(s) && s; }));
    condition_id product_tank_high_dry = sched_.addCondition(condition::when("product tank high float not submerged", [] { bool s; return permeate_tank_high_float.readSubmerged(s) && !s; }));
    condition_id upper_tank_high_dry = sched_.addCondition(condition::when("upper tank high float not submerged", [] { bool s; return upper_tank_high_float.readSubmerged(s) && !s; }));
    condition_id upper_tank_high_submerged = sched_.addCondition(condition::when("upper tank high float submerged", [] { bool s; return upper_tank_high_float.readSubmerged(s) && s; }));
    condition_id upper_tank_high_read_failed = sched_.addCondition(condition::when("upper tank high float read failed", [] { bool s; return !upper_tank_high_float.readSubmerged(s); }));
    condition_id repressure_toggle_on = sched_.addCondition(condition::when("repressure toggle switch on", [] { bool on; return repressure_toggle_switch.readValue(on) && on; }));
    condition_id ro_toggle_on = sched_.addCondition(condition::when("RO toggle switch on", [] { bool on; return ro_toggle_switch.readValue(on) && on; }));
    condition_id repressure_pump_off = sched_.addCondition(condition::when("repressure pump off", [] { bool on; return repressure_pump.readOn(on) && !on; }));
    condition_id repressure_pump_on = sched_.addCondition(condition::when("repressure pump on", [] { bool on; return repressure_pump.readOn(on) && on; }));

    task_id pump_on_id;
    task_id pump_off_full_id;
    task_id pump_off_read_failed_id;
    sched_.addTask({"repressure pump on: upper tank not full", [] { return repressure_pump.setOn(); }, {upper_tank_high_dry, repressure_pump_off}, task_mode::ON_RISING}, pump_on_id);
    sched_.addTask({"repressure pump off: upper tank full", [] { return repressure_pump.setOff(); }, {upper_tank_high_submerged, repressure_pump_on}, task_mode::ON_RISING}, pump_off_full_id);
    sched_.addTask({"repressure pump off: upper tank float read failed", [] { return repressure_pump.setOff(); }, {upper_tank_high_read_failed, repressure_pump_on}, task_mode::ON_RISING}, pump_off_read_failed_id);

    condition_ids_ = {feed_tank_low_submerged, product_tank_high_dry, upper_tank_high_dry, upper_tank_high_submerged,
                      upper_tank_high_read_failed, repressure_toggle_on, ro_toggle_on, repressure_pump_off,
                      repressure_pump_on};
    task_ids_ = {pump_on_id, pump_off_full_id, pump_off_read_failed_id};
    return true;
}
