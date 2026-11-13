#include "procedure.h"

#include "config.h"
#include "ro_config.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
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
    return scheduleIrrigationTable("overhead_pattern_1.csv");
}

namespace {

// "overhead-1" or "overhead_1" -> the config.h irrigation zone, nullptr if unknown
irrigation* irrigationZone(std::string name) {
    for (char& c : name) {
        if (c == '-') c = '_';
    }
    const std::pair<const char*, irrigation*> zones[] = {
        {"valve_1_1", &valve_1_1}, {"valve_1_2", &valve_1_2}, {"valve_2_1", &valve_2_1},
        {"valve_3_1", &valve_3_1}, {"valve_4_1", &valve_4_1}, {"valve_5_1", &valve_5_1},
        {"valve_5_2", &valve_5_2}, {"valve_5_3", &valve_5_3}, {"valve_5_4", &valve_5_4},
        {"valve_6_1", &valve_6_1}, {"valve_6_2", &valve_6_2}, {"valve_7_1", &valve_7_1},
        {"valve_8_1", &valve_8_1}, {"valve_9_1", &valve_9_1}, {"valve_10_1", &valve_10_1},
        {"valve_10_2", &valve_10_2}, {"valve_10_3", &valve_10_3}, {"overhead_1", &overhead_1},
        {"overhead_2", &overhead_2}, {"overhead_3", &overhead_3}, {"overhead_4", &overhead_4},
        {"overhead_5", &overhead_5}, {"overhead_6", &overhead_6},
        {"city_water_fill_valve", &city_water_fill_valve}};
    for (const auto& zone : zones) {
        if (name == zone.first) return zone.second;
    }
    return nullptr;
}

}  // namespace

// Irrigation tables live in wiseconn_irrigation_tables/ (IRRIGATION_TABLES_DIR, set
// by CMake), read when the procedure starts, so editing a table needs no rebuild.
// Row: zone,HH:MM:SS,minutes (e.g. overhead-1,14:00:00,15): that zone, today, from
// the start time for that many minutes. Each row is a ONCE task that sends it to
// WiseConn (scheduleIrrigation: one request, about 1 s, which blocks the loop
// it runs in), 2 s apart so main_ro's loop is never held up for long. WiseConn
// refuses a row whose start has passed (logged; not retried). Nothing is sent if a
// row can't be parsed.
bool procedure::scheduleIrrigationTable(const std::string& file_name) {
    std::string path = std::string(IRRIGATION_TABLES_DIR) + "/" + file_name;
    std::ifstream file(path);
    if (!file) {
        std::fprintf(stderr, "procedure: can't open %s\n", path.c_str());
        return false;
    }

    struct row {
        irrigation* zone;
        std::string zone_name;
        int start_hour, start_minute, end_hour, end_minute;
    };
    std::vector<row> rows;
    std::string line;
    int number = 0;
    while (std::getline(file, line)) {
        ++number;
        if (line.find_first_not_of(" \t\r,") == std::string::npos) continue;   // blank line
        std::stringstream fields(line);
        std::string zone_name, start, minutes;
        std::getline(fields, zone_name, ',');
        std::getline(fields, start, ',');
        std::getline(fields, minutes, ',');
        int h = 0, m = 0, s = 0;
        int duration = std::atoi(minutes.c_str());
        irrigation* zone = irrigationZone(zone_name);
        if (!zone || std::sscanf(start.c_str(), "%d:%d:%d", &h, &m, &s) < 2 || duration <= 0) {
            std::fprintf(stderr, "procedure: %s line %d: expected zone,HH:MM:SS,minutes: %s\n",
                         file_name.c_str(), number, line.c_str());
            return false;
        }
        int end = h * 60 + m + duration;
        if (end >= 24 * 60) {
            std::fprintf(stderr, "procedure: %s line %d: ends after midnight\n", file_name.c_str(), number);
            return false;
        }
        rows.push_back({zone, zone_name, h, m, end / 60, end % 60});
    }
    if (rows.empty()) {
        std::fprintf(stderr, "procedure: %s has no rows\n", path.c_str());
        return false;
    }

    for (size_t i = 0; i < rows.size(); ++i) {
        const row r = rows[i];
        condition_id due = sched_.addCondition(condition::afterDuration(
            "irrigation_row_" + std::to_string(i + 1), std::chrono::seconds(2 * i)));
        task_id id;
        sched_.addTask({"schedule_" + r.zone_name, [r] {
            return r.zone->scheduleIrrigation(r.start_hour, r.start_minute, r.end_hour, r.end_minute);
        }, {due}, task_mode::ONCE}, id);
        condition_ids_.push_back(due);
        task_ids_.push_back(id);
    }
    std::fprintf(stderr, "procedure: scheduling %zu irrigations from %s\n", rows.size(), file_name.c_str());
    return true;
}
