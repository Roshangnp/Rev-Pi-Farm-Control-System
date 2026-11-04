#include "scheduler.h"

#include <algorithm>
#include <utility>

namespace {

// Entry with this id in a vector of (id, value) pairs, or nullptr
template <typename T>
T* findById(std::vector<std::pair<int, T>>& entries, int id) {
    for (auto& entry : entries) {
        if (entry.first == id) return &entry.second;
    }
    return nullptr;
}

}  // namespace

condition condition::when(std::string name, std::function<bool()> check) {
    condition c;
    c.name = std::move(name);
    if (check) {
        c.met = [check = std::move(check)](schedule_time) { return check(); };
    }
    return c;
}

condition condition::after(std::string name, schedule_time at) {
    condition c;
    c.name = std::move(name);
    c.met = [at](schedule_time now) { return now >= at; };
    return c;
}

condition condition::between(std::string name, schedule_time from, schedule_time to) {
    condition c;
    c.name = std::move(name);
    c.met = [from, to](schedule_time now) { return from <= now && now < to; };
    return c;
}

condition condition::afterDuration(std::string name, schedule_clock::duration wait) {
    condition c;
    c.name = std::move(name);
    schedule_time at = c.created_time + wait;
    c.met = [at](schedule_time now) { return now >= at; };
    return c;
}

condition_id scheduler::addCondition(condition c) {
    condition_id id = next_condition_id_++;
    conditions_.emplace_back(id, std::move(c));
    return id;
}

bool scheduler::addTask(task t, task_id& id) {
    if (!t.run) return false;
    for (condition_id c : t.conditions) {
        if (!findById(conditions_, c)) return false;
    }
    // Start clean, whatever the caller left in the scheduler's fields
    t.done = false;
    t.was_met = false;
    t.run_count = 0;
    t.last_ok = false;
    t.last_run = schedule_time{};

    id = next_task_id_++;
    tasks_.emplace_back(id, std::move(t));
    return true;
}

bool scheduler::removeTask(task_id id) {
    auto it = std::find_if(tasks_.begin(), tasks_.end(),
                           [id](const auto& entry) { return entry.first == id; });
    if (it == tasks_.end()) return false;
    tasks_.erase(it);
    return true;
}

bool scheduler::removeCondition(condition_id id) {
    for (const auto& entry : tasks_) {
        const auto& used = entry.second.conditions;
        if (std::find(used.begin(), used.end(), id) != used.end()) return false;
    }
    auto it = std::find_if(conditions_.begin(), conditions_.end(),
                           [id](const auto& entry) { return entry.first == id; });
    if (it == conditions_.end()) return false;
    conditions_.erase(it);
    return true;
}

bool scheduler::update() {
    now_ = schedule_clock::now();
    bool all_ok = true;

    // Walk a snapshot of the ids and look each task up again: a task's run() may
    // add or remove tasks (even itself), which would invalidate an iterator.
    // Tasks added during this pass are first checked on the next update().
    std::vector<task_id> ids;
    ids.reserve(tasks_.size());
    for (const auto& entry : tasks_) ids.push_back(entry.first);

    for (task_id id : ids) {
        task* t = findById(tasks_, id);
        if (!t || t->done) continue;

        // All conditions must hold; stops at the first one that doesn't
        // (conditions that read remote devices cost a network round trip)
        bool met = true;
        for (condition_id c_id : t->conditions) {
            condition* c = findById(conditions_, c_id);
            if (!c || !c->met || !c->met(now_)) {
                met = false;
                break;
            }
        }

        bool should_run = met && (t->mode == task_mode::ONCE || !t->was_met);
        t->was_met = met;
        if (!should_run) continue;

        // Copy so the function survives if run() removes this task
        std::function<bool()> run = t->run;
        bool ok = run();
        if (!ok) all_ok = false;

        t = findById(tasks_, id);  // run() may have changed tasks_
        if (!t) continue;
        t->run_count++;
        t->last_ok = ok;
        t->last_run = now_;
        if (t->mode == task_mode::ONCE) t->done = true;
    }
    return all_ok;
}
