// RO pump speed ramp, on the RO RevPi. Stop farm-ro first:
//   sudo systemctl stop farm-ro
//   tests/ro_ramp_test/build/ro_pump_speed_test [out.csv]
//   sudo systemctl start farm-ro
//
// Sets the RO pump speed to 0, moves the RO to NORMAL_OP, waits for the valves to
// settle and the pumps to start, ramps the speed to RAMP_END_SPEED at
// RAMP_RATE_PER_MIN, then sets the speed to 0 and the RO to OFF. Logs every RO
// pressure, flow and conductivity at 1 Hz. A read that fails is an empty CSV field
// (every flow's first read does).
//
// Stops early, the same way, on Ctrl-C, SIGTERM, the SSH session closing, a VFD
// alarm, a failed speed command, or the RO leaving NORMAL_OP (e.g. set to OFF from
// the web UI). A crash or kill -9 leaves the relays as they were: starting farm-ro
// drives them to OFF.

#include <chrono>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <string>

#include "config.h"
#include "line_server.h"
#include "ro_config.h"
#include "scheduler.h"

namespace {

using clock_type = std::chrono::steady_clock;

constexpr float RAMP_END_SPEED = 0.7f;      // 0-1
constexpr float RAMP_RATE_PER_MIN = 0.2f;   // speed per minute
constexpr auto LOG_PERIOD = std::chrono::seconds(1);

struct pressure_column { const char* name; pressure_transducer& sensor; };
struct flow_column { const char* name; flow_sensor& sensor; };
struct conductivity_column { const char* name; conductivity_sensor& sensor; };

// Global so it outlives the RO controller, like main_ro.cpp
scheduler sched;

volatile std::sig_atomic_t stop_requested = 0;

void requestStop(int) {
    stop_requested = 1;
}

void writeValue(std::FILE* csv, bool ok, float value) {
    if (ok) std::fprintf(csv, ",%.4f", static_cast<double>(value));
    else std::fputc(',', csv);
}

std::string defaultFileName() {
    char text[64];
    std::time_t now = std::time(nullptr);
    std::strftime(text, sizeof(text), "ro_pump_speed_test_%Y%m%d_%H%M%S.csv", std::localtime(&now));
    return text;
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
    std::fprintf(csv, "\n");

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
    std::printf("Logging to %s. Ctrl-C stops the test and moves the RO to OFF\n", file_name.c_str());
    std::fflush(stdout);

    const auto start_time = clock_type::now();
    auto next_log = start_time;
    auto ramp_start = start_time;
    bool ramping = false;
    bool finished = false;
    float speed = 0.0f;
    int exit_code = 0;

    while (!stop_requested && !finished) {
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

        // Speed comes from the time since the pumps started, so a late pass can't slow the ramp
        if (!ramping && !settling) {
            ramping = true;
            ramp_start = now;
            std::printf("Pumps started, ramping\n");
            std::fflush(stdout);
        }
        if (ramping) {
            float minutes = std::chrono::duration<float, std::ratio<60>>(now - ramp_start).count();
            speed = RAMP_RATE_PER_MIN * minutes;
            if (speed >= RAMP_END_SPEED) {
                speed = RAMP_END_SPEED;
                finished = true;
            }
            if (!setROSpeed(speed)) {
                std::fprintf(stderr, "Setting the RO pump speed to %.3f failed, stopping\n", static_cast<double>(speed));
                exit_code = 1;
                break;
            }
        }

        std::fprintf(csv, "%.2f,%s,%d,%.4f", std::chrono::duration<double>(now - start_time).count(),
                     toString(state), settling ? 1 : 0, static_cast<double>(speed));
        for (const auto& c : pressures) {
            float psi = 0.0f;
            bool ok = c.sensor.readPressure(psi);
            writeValue(csv, ok, psi);
        }
        for (const auto& c : flows) {
            float gpm = 0.0f;
            bool ok = c.sensor.readFlow(gpm);
            writeValue(csv, ok, gpm);
        }
        for (const auto& c : conductivities) {
            float us_per_cm = 0.0f;
            bool ok = c.sensor.readConductivity(us_per_cm);
            writeValue(csv, ok, us_per_cm);
        }
        std::fprintf(csv, "\n");
        std::fflush(csv);   // a crash keeps every row so far
    }

    if (stop_requested) std::fprintf(stderr, "Stop requested\n");
    shutDown();
    std::fclose(csv);
    std::printf("Done, RO is OFF. %s\n", file_name.c_str());
    return exit_code;
}
