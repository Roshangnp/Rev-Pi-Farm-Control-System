#include "ro.h"

#include <chrono>
#include <cstdio>
#include <limits>


namespace {

constexpr float no_reading = std::numeric_limits<float>::quiet_NaN();

bool readBool(valve& v) {
    bool open = false;
    return v.readOpen(open) && open;
}

bool readBool(pump& p) {
    bool on = false;
    return p.readOn(on) && on;
}

float readFloat(pressure_transducer& p) {
    float psi = 0;
    return p.readPressure(psi) ? psi : no_reading;
}

float readFloat(flow_sensor& f) {
    float gpm = 0;
    return f.readFlow(gpm) ? gpm : no_reading;
}

bool apply(valve& v, bool open) {
    return open ? v.setOpen() : v.setClosed();
}

bool apply(pump& p, bool on) {
    return on ? p.setOn() : p.setOff();
}

// Only touches devices whose target equals `pass`, so a caller can do all the
// opens (pass = true) and all the closes (pass = false) as separate steps
template <class device>
bool applyIf(device& d, bool target, bool pass) {
    return target != pass || apply(d, target);
}

bool applyValves(const valve_state& v, bool pass) {
    bool ok = true;
    ok = applyIf(flush_valve, v.flush_valve, pass) && ok;
    ok = applyIf(inlet_valve, v.inlet_valve, pass) && ok;
    ok = applyIf(cip_inlet_valve, v.cip_inlet_valve, pass) && ok;
    ok = applyIf(concentrate_cip_valve, v.concentrate_cip_valve, pass) && ok;
    ok = applyIf(permeate_cip_valve, v.permeate_cip_valve, pass) && ok;
    ok = applyIf(media_backwash_1_valve, v.media_backwash_1_valve, pass) && ok;
    ok = applyIf(media_backwash_2_valve, v.media_backwash_2_valve, pass) && ok;
    return ok;
}

bool applyPumps(const pump_state& p, bool pass) {
    bool ok = true;
    ok = applyIf(feed_pump, p.feed_pump, pass) && ok;
    ok = applyIf(ro_pump, p.ro_pump, pass) && ok;
    ok = applyIf(asa_pump, p.asa_pump, pass) && ok;
    ok = applyIf(low_ph_pump, p.low_ph_pump, pass) && ok;
    ok = applyIf(high_ph_pump, p.high_ph_pump, pass) && ok;
    return ok;
}

}  // namespace

ro::ro(scheduler& sched) : scheduler_(sched), procedure_(sched), current_state_(RO_state::OFF) {}

ro::~ro() {
    setState(RO_state::OFF);   // pumps stop before valves close
}

bool ro::checkAllDevices() {
    // Naming every RO device here means a missing or misspelled one fails to
    // compile (not declared in config.h) or link (not bound in config_ro.cpp).
    // If this builds, they are all there.
    const void* devices[] = {
        &permeate_flow, &concentrate_flow, &feed_flow,
        &feed_tank_water_level, &feed_tank_low_float, &high_ph_tank_float,
        &low_ph_tank_float, &cip_tank_low_float, &cip_tank_mid_float,
        &cip_tank_high_float, &asa_tank_low_float, &permeate_tank_high_float,
        &media_outlet_pressure, &media_inlet_pressure, &concentrate_pressure,
        &cip_inlet_pressure, &cip_outlet_pressure, &interstage_pressure,
        &aqualine_inlet_pressure, &aqualine_outlet_pressure, &primary_pressure,
        &permeate_pressure, &air_pressure,
        &permeate_conductivity, &feed_conductivity, &cip_temp,
        &feed_pump, &ro_pump, &repressure_pump, &asa_pump, &high_ph_pump, &low_ph_pump,
        &lights, &cip_heater,
        &flush_valve, &inlet_valve, &cip_inlet_valve, &concentrate_cip_valve,
        &permeate_cip_valve, &media_backwash_1_valve, &media_backwash_2_valve,
    };
    for (const void* d : devices) {
        if (d == nullptr) {
            return false;
        }
    }
    return true;
}

valve_state ro::readValveState() {
    valve_state v;
    v.flush_valve = readBool(flush_valve);
    v.inlet_valve = readBool(inlet_valve);
    v.cip_inlet_valve = readBool(cip_inlet_valve);
    v.concentrate_cip_valve = readBool(concentrate_cip_valve);
    v.permeate_cip_valve = readBool(permeate_cip_valve);
    v.media_backwash_1_valve = readBool(media_backwash_1_valve);
    v.media_backwash_2_valve = readBool(media_backwash_2_valve);
    return v;
}

pump_state ro::readPumpState() {
    pump_state p;
    p.feed_pump = readBool(feed_pump);
    p.ro_pump = readBool(ro_pump);
    p.asa_pump = readBool(asa_pump);
    p.low_ph_pump = readBool(low_ph_pump);
    p.high_ph_pump = readBool(high_ph_pump);
    return p;
}

pressure_state<float> ro::readPressureState() {
    pressure_state<float> p;
    p.media_outlet_pressure = readFloat(media_outlet_pressure);
    p.media_inlet_pressure = readFloat(media_inlet_pressure);
    p.concentrate_pressure = readFloat(concentrate_pressure);
    p.cip_inlet_pressure = readFloat(cip_inlet_pressure);
    p.cip_outlet_pressure = readFloat(cip_outlet_pressure);
    p.interstage_pressure = readFloat(interstage_pressure);
    p.aqualine_inlet_pressure = readFloat(aqualine_inlet_pressure);
    p.aqualine_outlet_pressure = readFloat(aqualine_outlet_pressure);
    p.primary_pressure = readFloat(primary_pressure);
    p.permeate_pressure = readFloat(permeate_pressure);
    p.air_pressure = readFloat(air_pressure);
    return p;
}

flow_state<float> ro::readFlowState() {
    flow_state<float> f;
    f.permeate_flow = readFloat(permeate_flow);
    f.concentrate_flow = readFloat(concentrate_flow);
    f.feed_flow = readFloat(feed_flow);
    return f;
}

bool ro::setValveState() {
    const valve_state v = valvesFor(current_state_);
    bool ok = applyValves(v, true);      // open first
    ok = applyValves(v, false) && ok;    // then close
    return ok;
}

bool ro::setPumpState() {
    const pump_state p = pumpsFor(current_state_);
    bool ok = applyPumps(p, false);      // stop first
    if (!pumps_pending_) {               // valves still settling: update() starts them
        ok = applyPumps(p, true) && ok;  // then start
    }
    return ok;
}

bool ro::setHeaterState() {
    heater_active_ = HeaterFor(current_state_);
    return true;
}

bool ro::setState(RO_state s) {
    current_state_ = s;
    pumps_pending_ = false;              // drop any start pending from the previous state
    const pump_state p = pumpsFor(s);

    setHeaterState();                    // heater_active_ follows the state
    bool ok = applyPumps(p, false);      // 1. stop pumps that must be off
    ok = setValveState() && ok;          // 2+3. open valves, then close valves
    if (!ok) {
        return false;                    // don't start pumps without their flow path
    }

    const bool any_pump_on = p.feed_pump || p.ro_pump || p.asa_pump
                          || p.low_ph_pump || p.high_ph_pump;
    if (any_pump_on) {                   // nothing to start (e.g. OFF): no wait
        pumps_pending_ = true;           // 4. update() starts them after VALVE_SETTLE_MS
        valves_set_at_ = std::chrono::steady_clock::now();
    }
    return true;
}

bool ro::update() {
    bool ok = offWhenProcedureEnds();
    if (!pumps_pending_) {
        return ok;
    }
    if (std::chrono::steady_clock::now() - valves_set_at_
            < std::chrono::milliseconds(VALVE_SETTLE_MS)) {
        return ok;                       // still settling
    }
    pumps_pending_ = false;
    return applyPumps(pumpsFor(current_state_), true) && ok;
}

bool ro::offWhenProcedureEnds() {
    bool running = procedure_.isRunning();
    bool ended = procedure_was_running_ && !running;
    procedure_was_running_ = running;
    if (!ended) {
        return true;
    }
    std::fprintf(stderr, "RO controller: %s ended, moving to OFF\n", toString(procedure_.type()));
    return setState(RO_state::OFF);
}

bool ro::isSettling() const {
    return pumps_pending_;
}

RO_state ro::readState() const {
    return current_state_;
}

bool ro::setRoSpeed(float speed) {
    return ro_pump.setSpeed(speed);
}
