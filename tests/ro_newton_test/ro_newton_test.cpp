// RO permeate flow control by Newton iteration, on the RO RevPi. Stop farm-ro first:
//   sudo systemctl stop farm-ro
//   tests/ro_newton_test/build/ro_newton_test [out.csv]
//   sudo systemctl start farm-ro
//
// Uses the step test's model, y = a*u + b, with y the permeate flow in pulses/min
// and u the RO pump speed. Starts NORMAL_OP at speed 0, waits for the startup flow,
// moves to the feedforward speed (r - b)/a, then repeats: wait SETTLE_TIME, average
// the flow over MEASURE_SAMPLES seconds, apply u += (r - y)/a. Holds for RUN_TIME
// from the feedforward move, then sets the speed to 0 and the RO to OFF.
//
// Logs at 1 Hz with the same columns as the other RO tests, plus speed_target and
// window_pulses_per_min on the end. The loop calls each flow sensor's update()
// every pass, and readFlow() returns its rolling 1 s average, so the once-a-second
// reads each cover the last second.
//
// Stops early, the same way, on Ctrl-C, SIGTERM, the SSH session closing, a VFD
// alarm, a failed speed command, the RO leaving NORMAL_OP, primary pressure above
// MAX_PRIMARY_PSI or unreadable. A crash or kill -9 leaves the relays as they were:
// starting farm-ro drives them to OFF.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "config.h"
#include "line_server.h"
#include "ro_config.h"
#include "scheduler.h"

namespace {

using clock_type = std::chrono::steady_clock;

constexpr float TARGET_GPM = 30.0f;
constexpr float PERMEATE_PULSES_PER_GALLON = 133.17f;  // same as config_ro.cpp, only converts units
constexpr float TARGET_PULSES_PER_MIN = TARGET_GPM * PERMEATE_PULSES_PER_GALLON;

constexpr float MODEL_A = 2210.0f;              // pulses/min per unit speed, from the step test
constexpr float MODEL_B = 2678.0f;              // pulses/min at speed 0, from the step test
constexpr float MAX_SPEED_STEP = 0.1f;          // per LOG_PERIOD, so 0.1 speed/s
constexpr float MAX_SPEED = 0.7f;               // highest speed run so far
constexpr float DEADBAND_PULSES_PER_MIN = 100.0f;  // smaller errors are left alone
constexpr float MIN_VALID_PULSES_PER_MIN = 1500.0f;  // below this the reading is held, not corrected
constexpr float MAX_PRIMARY_PSI = 250.0f;       // stops the test

constexpr auto STARTUP_TIME = std::chrono::seconds(40);  // from the pumps starting
constexpr auto SETTLE_TIME = std::chrono::seconds(5);    // from the speed reaching its target
constexpr int MEASURE_SAMPLES = 5;                       // one per LOG_PERIOD
constexpr auto RUN_TIME = std::chrono::minutes(5);       // from the feedforward move
constexpr auto LOG_PERIOD = std::chrono::seconds(1);

enum class phase { WAIT_PUMPS, STARTUP, MOVING, SETTLING, MEASURING };

struct pressure_column { const char* name; pressure_transducer& sensor; };
struct flow_column { const char* name; flow_sensor& sensor; };
struct conductivity_column { const char* name; conductivity_sensor& sensor; };
struct reading { bool ok; float value; };

// Global so it outlives the RO controller, like main_ro.cpp
scheduler sched;

volatile std::sig_atomic_t stop_requested = 0;

void requestStop(int) {
    stop_requested = 1;
}

void writeValue(std::FILE* csv, const reading& r) {
    if (r.ok) std::fprintf(csv, ",%.4f", static_cast<double>(r.value));
    else std::fputc(',', csv);
}

std::string defaultFileName() {
    char text[64];
    std::time_t now = std::time(nullptr);
    std::strftime(text, sizeof(text), "ro_newton_test_%Y%m%d_%H%M%S.csv", std::localtime(&now));
    return text;
}

float clampSpeed(float speed) {
    return std::clamp(speed, 0.0f, MAX_SPEED);
}

// Speed 0 before the pumps stop, so the VFD doesn't restart at speed next time
void shutDown() {
    if (!setROSpeed(0.0f)) std::fprintf(stderr, "Setting the RO pump speed to 0 failed\n");
    if (!setROState(RO_state::OFF)) std::fprintf(stderr, "Moving to OFF failed: check the relays\n");
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, requestStop);
    std::signal(SIGTERM, requestStop);
    std::signal(SIGHUP, requestStop);
    std::signal(SIGPIPE, SIG_IGN);

    const pressure_column pressures[] = {
        {"media_inlet_pressure", media_inlet_pressure},
        {"media_outlet_pressure", media_outlet_pressure},
        {"primary_pressure", primary_pressure},
        {"interstage_pressure", interstage_pressure},
        {"concentrate_pressure", concentrate_pressure},
        {"permeate_pressure", permeate_pressure},
        {"aqualine_inlet_pressure", aqualine_inlet_pressure},
        {"aqualine_outlet_pressure", aqualine_outlet_pressure},
        {"cip_inlet_pressure", cip_inlet_pressure},
        {"cip_outlet_pressure", cip_outlet_pressure},
        {"air_pressure", air_pressure},
    };
    const flow_column flows[] = {
        {"feed_flow", feed_flow},
        {"permeate_flow", permeate_flow},
        {"concentrate_flow", concentrate_flow},
    };
    const conductivity_column conductivities[] = {
        {"feed_conductivity", feed_conductivity},
        {"permeate_conductivity", permeate_conductivity},
    };

    if (!attachAllInterfaces()) {
        std::fprintf(stderr, "Some interfaces failed to attach, nothing was switched\n");
        return 1;
    }

    std::string file_name = argc > 1 ? argv[1] : defaultFileName();
    std::FILE* csv = std::fopen(file_name.c_str(), "w");
    if (!csv) {
        std::fprintf(stderr, "Can't write %s, nothing was switched\n", file_name.c_str());
        return 1;
    }
    std::fprintf(csv, "time_s,ro_state,settling,ro_speed_command");
    for (const auto& c : pressures) std::fprintf(csv, ",%s_psi", c.name);
    for (const auto& c : flows) std::fprintf(csv, ",%s_gpm", c.name);
    for (const auto& c : conductivities) std::fprintf(csv, ",%s_us_cm", c.name);
    std::fprintf(csv, ",speed_target,window_pulses_per_min\n");

    // Started before addRO, which drives the outputs to OFF: if farm-ro is running
    // the port is taken and this exits without touching them
    line_server server;
    if (!server.start()) {
        std::fprintf(stderr, "Port 47100 is taken: stop farm-ro first. Nothing was switched\n");
        std::fclose(csv);
        return 1;
    }
    if (!addRO(server, sched)) {
        std::fprintf(stderr, "RO controller failed to start\n");
        shutDown();
        std::fclose(csv);
        return 1;
    }

    if (!setROSpeed(0.0f) || !setROState(RO_state::NORMAL_OP)) {
        std::fprintf(stderr, "Moving to NORMAL_OP at speed 0 failed\n");
        shutDown();
        std::fclose(csv);
        return 1;
    }
    std::printf("Logging to %s. Target %.1f gpm = %.0f pulses/min. Ctrl-C stops the test and moves the RO to OFF\n",
                file_name.c_str(), static_cast<double>(TARGET_GPM), static_cast<double>(TARGET_PULSES_PER_MIN));
    std::fflush(stdout);

    const auto start_time = clock_type::now();
    auto next_log = start_time;
    auto phase_start = start_time;
    auto control_start = start_time;
    phase current = phase::WAIT_PUMPS;
    float speed = 0.0f;
    float speed_target = 0.0f;
    float window_sum = 0.0f;
    int window_samples = 0;
    int window_valid = 0;
    bool finished = false;
    int exit_code = 0;

    while (!stop_requested && !finished) {
        // Rolling 1 s flow averages: readFlow() only returns what these compute
        permeate_flow.update();
        concentrate_flow.update();
        feed_flow.update();

        if (!sched.update()) std::fprintf(stderr, "Scheduler: a task failed\n");
        if (!updateRO()) {
            std::fprintf(stderr, "Starting a pump failed, stopping\n");
            exit_code = 1;
            break;
        }
        server.poll(20);   // answers emerald-server's "ro" requests, doubles as the loop sleep

        auto now = clock_type::now();
        if (now < next_log) continue;
        next_log += LOG_PERIOD;
        const double time_s = std::chrono::duration<double>(now - start_time).count();

        RO_state state = RO_state::OFF;
        bool settling = false;
        readROState(state);
        readROSettling(settling);
        if (state != RO_state::NORMAL_OP) {
            std::fprintf(stderr, "The RO left NORMAL_OP (now %s), stopping\n", toString(state));
            exit_code = 1;
            break;
        }
        if (feed_pump.readAlarm() || ro_pump.readAlarm()) {
            std::fprintf(stderr, "VFD alarm, stopping\n");
            exit_code = 1;
            break;
        }

        // Every sensor first: each flow is read exactly once a second, whatever the phase
        std::vector<reading> pressure_row;
        for (const auto& c : pressures) {
            float psi = 0.0f;
            bool ok = c.sensor.readPressure(psi);
            pressure_row.push_back({ok, psi});
        }
        std::vector<reading> flow_row;
        reading permeate = {false, 0.0f};
        for (const auto& c : flows) {
            float gpm = 0.0f;
            bool ok = c.sensor.readFlow(gpm);
            flow_row.push_back({ok, gpm});
            if (&c.sensor == &permeate_flow) permeate = {ok, gpm};
        }
        std::vector<reading> conductivity_row;
        for (const auto& c : conductivities) {
            float us_per_cm = 0.0f;
            bool ok = c.sensor.readConductivity(us_per_cm);
            conductivity_row.push_back({ok, us_per_cm});
        }

        float primary_psi = 0.0f;
        if (!primary_pressure.readPressure(primary_psi) || primary_psi > MAX_PRIMARY_PSI) {
            std::fprintf(stderr, "Primary pressure unreadable or above %.0f psi, stopping\n",
                         static_cast<double>(MAX_PRIMARY_PSI));
            exit_code = 1;
            break;
        }

        reading window = {false, 0.0f};
        switch (current) {
            case phase::WAIT_PUMPS:
                if (!settling) {
                    current = phase::STARTUP;
                    phase_start = now;
                    std::printf("Pumps started at time_s = %.2f, holding speed 0\n", time_s);
                }
                break;
            case phase::STARTUP:
                if (now - phase_start >= STARTUP_TIME) {
                    speed_target = clampSpeed((TARGET_PULSES_PER_MIN - MODEL_B) / MODEL_A);
                    control_start = now;
                    current = phase::MOVING;
                    std::printf("Feedforward to speed %.3f at time_s = %.2f\n",
                                static_cast<double>(speed_target), time_s);
                }
                break;
            case phase::MOVING: {
                float difference = speed_target - speed;
                speed = std::fabs(difference) <= MAX_SPEED_STEP
                            ? speed_target
                            : speed + std::copysign(MAX_SPEED_STEP, difference);
                if (!setROSpeed(speed)) {
                    std::fprintf(stderr, "Setting the RO pump speed to %.3f failed, stopping\n", static_cast<double>(speed));
                    exit_code = 1;
                    finished = true;
                    break;
                }
                if (speed == speed_target) {
                    current = phase::SETTLING;
                    phase_start = now;
                }
                break;
            }
            case phase::SETTLING:
                if (now - phase_start >= SETTLE_TIME) {
                    current = phase::MEASURING;
                    window_sum = 0.0f;
                    window_samples = 0;
                    window_valid = 0;
                }
                break;
            case phase::MEASURING:
                ++window_samples;
                if (permeate.ok) {
                    window_sum += permeate.value * PERMEATE_PULSES_PER_GALLON;
                    ++window_valid;
                }
                if (window_samples >= MEASURE_SAMPLES) {
                    current = phase::MOVING;
                    float flow = window_valid > 0 ? window_sum / static_cast<float>(window_valid) : 0.0f;
                    if (window_valid < MEASURE_SAMPLES || flow < MIN_VALID_PULSES_PER_MIN) {
                        std::printf("time_s = %.2f: bad flow window (%d/%d reads, %.0f pulses/min), holding speed %.3f\n",
                                    time_s, window_valid, MEASURE_SAMPLES, static_cast<double>(flow),
                                    static_cast<double>(speed));
                        break;
                    }
                    window = {true, flow};
                    float error = TARGET_PULSES_PER_MIN - flow;
                    if (std::fabs(error) > DEADBAND_PULSES_PER_MIN) {
                        speed_target = clampSpeed(speed + error / MODEL_A);
                    }
                    std::printf("time_s = %.2f: %.0f pulses/min (%.2f gpm), error %.0f, speed target %.3f\n",
                                time_s, static_cast<double>(flow),
                                static_cast<double>(flow / PERMEATE_PULSES_PER_GALLON),
                                static_cast<double>(error), static_cast<double>(speed_target));
                }
                break;
        }
        std::fflush(stdout);
        if (current != phase::WAIT_PUMPS && current != phase::STARTUP && now - control_start >= RUN_TIME) {
            finished = true;
        }

        std::fprintf(csv, "%.2f,%s,%d,%.4f", time_s, toString(state), settling ? 1 : 0, static_cast<double>(speed));
        for (const auto& r : pressure_row) writeValue(csv, r);
        for (const auto& r : flow_row) writeValue(csv, r);
        for (const auto& r : conductivity_row) writeValue(csv, r);
        writeValue(csv, {true, speed_target});
        writeValue(csv, window);
        std::fprintf(csv, "\n");
        std::fflush(csv);   // a crash keeps every row so far
    }

    if (stop_requested) std::fprintf(stderr, "Stop requested\n");
    shutDown();
    std::fclose(csv);
    std::printf("Done, RO is OFF. %s\n", file_name.c_str());
    return exit_code;
}
