#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

// Runs tasks when their conditions are met, checked against the system clock.
// Single threaded, like line_server: call update() from the main loop (next to
// server.poll()); tasks and conditions only ever run inside update(), so they
// never race the control code. Tasks and conditions must not block for long:
// update() runs them one after another on the caller's thread (a remote call
// can block up to 300 ms if the owner is down).

using schedule_clock = std::chrono::system_clock;
using schedule_time = schedule_clock::time_point;

using task_id = int;
using condition_id = int;

// Something that returns true or false. Checked on every update() of a task
// that uses it, with the time of that update().
struct condition {
    std::string name;                           // for logs and listings
    std::function<bool(schedule_time now)> met; // true when the condition holds; empty = never met
    schedule_time created_time = schedule_clock::now(); // when this condition was made

    // Any check, e.g. [] { bool on; return feed_tank_high_float.readSubmerged(on) && on; }
    // (a read that fails should return false, so a task never runs on a failed read)
    static condition when(std::string name, std::function<bool()> check);

    // True once now >= at (stays true after that)
    static condition after(std::string name, schedule_time at);

    // True between from and to (from <= now < to)
    static condition between(std::string name, schedule_time from, schedule_time to);

    // True once wait has passed since the condition was made (now >= created_time + wait),
    // e.g. afterDuration("10 min", std::chrono::minutes(10)). Counts from this call,
    // not from addCondition() or addTask(). System clock, so a clock change moves it.
    static condition afterDuration(std::string name, schedule_clock::duration wait);
};

// How often a task runs once its conditions are met
enum class task_mode {
    ONCE,       // runs the first time all its conditions are met, then is done
    ON_RISING,  // runs each time its conditions go from not all met to all met
                // (counts as not met before the first update(), so a task whose
                // conditions already hold runs on its first update())
};

// Something to do, e.g. [] { return setROState(RO_state::NORMAL_OP); }
// or a member of another module bound in a lambda. Returns false on failure,
// like the device methods.
struct task {
    std::string name;                   // for logs and listings
    std::function<bool()> run;
    std::vector<condition_id> conditions; // all must be met (AND); empty = run on the next update()
    task_mode mode = task_mode::ONCE;

    // Filled in by the scheduler
    bool done = false;          // ONCE task that has run; never runs again
    bool was_met = false;       // conditions at the last update(), for ON_RISING
    int run_count = 0;
    bool last_ok = false;       // what run() returned the last time it ran
    schedule_time last_run{};   // time of the last run (epoch if never run)
};

class scheduler {
    public:
    // Adds a condition tasks can refer to; one condition can be shared by several tasks.
    // Ids are never reused.
    condition_id addCondition(condition c);

    // Adds a task. False (and nothing added) if run is empty or a condition id
    // is unknown; id is filled in on success.
    bool addTask(task t, task_id& id);

    // Removes a task or condition. Removing a condition that a task still uses
    // fails (returns false).
    bool removeTask(task_id id);
    bool removeCondition(condition_id id);

    // Reads the clock into now_, then for every task that isn't done checks its
    // conditions and runs it as its mode says. A task's run() may add or remove
    // tasks; tasks added during a pass are first checked on the next update(). A task that fails is not retried
    // by itself (ONCE: still done; ON_RISING: waits for the next rising edge).
    // False if any task that ran this pass returned false.
    bool update();

    // Read-only views, e.g. for a UI listing
    const std::vector<std::pair<task_id, task>>& tasks() const { return tasks_; }
    const std::vector<std::pair<condition_id, condition>>& conditions() const { return conditions_; }

    // Time of the last update() (epoch before the first)
    schedule_time now() const { return now_; }

    private:
    std::vector<std::pair<task_id, task>> tasks_;
    std::vector<std::pair<condition_id, condition>> conditions_;
    task_id next_task_id_ = 1;
    condition_id next_condition_id_ = 1;
    schedule_time now_{};
};
