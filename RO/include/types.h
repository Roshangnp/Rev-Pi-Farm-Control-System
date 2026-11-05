#pragma once

#include <limits>
#include <string>
#include "config.h"

enum class RO_state{
    CIP_FLUSH,
    CIP_SOAK,
    CIP_LOW_FLOW_CYCLE,
    CIP_HIGH_FLOW_CYCLE,
    CIP_REFILL,
    MEDIA_BACKWASH_1,
    MEDIA_BACKWASH_2,
    NORMAL_OP,
    OFF,
    HIGH_PH_DOSE,
    LOW_PH_DOSE
};

// Every state, in enum order: add new states here too, or fromString() can't parse them
inline constexpr RO_state ALL_RO_STATES[] = {
    RO_state::CIP_FLUSH, RO_state::CIP_SOAK, RO_state::CIP_LOW_FLOW_CYCLE,
    RO_state::CIP_HIGH_FLOW_CYCLE, RO_state::CIP_REFILL, RO_state::MEDIA_BACKWASH_1,
    RO_state::MEDIA_BACKWASH_2, RO_state::NORMAL_OP, RO_state::OFF,
    RO_state::HIGH_PH_DOSE, RO_state::LOW_PH_DOSE,
};

// The enum name, e.g. "NORMAL_OP". Used on the wire by ro_server / remote_ro
inline const char* toString(RO_state s) {
    switch (s) {
    case RO_state::CIP_FLUSH: return "CIP_FLUSH";
    case RO_state::CIP_SOAK: return "CIP_SOAK";
    case RO_state::CIP_LOW_FLOW_CYCLE: return "CIP_LOW_FLOW_CYCLE";
    case RO_state::CIP_HIGH_FLOW_CYCLE: return "CIP_HIGH_FLOW_CYCLE";
    case RO_state::CIP_REFILL: return "CIP_REFILL";
    case RO_state::MEDIA_BACKWASH_1: return "MEDIA_BACKWASH_1";
    case RO_state::MEDIA_BACKWASH_2: return "MEDIA_BACKWASH_2";
    case RO_state::NORMAL_OP: return "NORMAL_OP";
    case RO_state::OFF: return "OFF";
    case RO_state::HIGH_PH_DOSE: return "HIGH_PH_DOSE";
    case RO_state::LOW_PH_DOSE: return "LOW_PH_DOSE";
    }
    return "UNKNOWN";
}

// Inverse of toString (exact match). False if text names no state
inline bool fromString(const std::string& text, RO_state& s) {
    for (RO_state candidate : ALL_RO_STATES) {
        if (text == toString(candidate)) {
            s = candidate;
            return true;
        }
    }
    return false;
}

// Multi-step procedures the RO can run (RO/src/procedures.h, ro::runProcedure)
enum class procedure_types {
    LOW_PH_CIP_CYCLE,
    HIGH_PH_CIP_CYCLE,
    MEDIA_BACKWASH_CYCLE
};

// Every procedure, in enum order: add new ones here too, or fromString() can't parse them
inline constexpr procedure_types ALL_PROCEDURE_TYPES[] = {
    procedure_types::LOW_PH_CIP_CYCLE, procedure_types::HIGH_PH_CIP_CYCLE,
    procedure_types::MEDIA_BACKWASH_CYCLE,
};

// The enum name, e.g. "LOW_PH_CIP_CYCLE". Used on the wire by ro_server / remote_ro
inline const char* toString(procedure_types p) {
    switch (p) {
    case procedure_types::LOW_PH_CIP_CYCLE: return "LOW_PH_CIP_CYCLE";
    case procedure_types::HIGH_PH_CIP_CYCLE: return "HIGH_PH_CIP_CYCLE";
    case procedure_types::MEDIA_BACKWASH_CYCLE: return "MEDIA_BACKWASH_CYCLE";
    }
    return "UNKNOWN";
}

// Inverse of toString (exact match). False if text names no procedure
inline bool fromString(const std::string& text, procedure_types& p) {
    for (procedure_types candidate : ALL_PROCEDURE_TYPES) {
        if (text == toString(candidate)) {
            p = candidate;
            return true;
        }
    }
    return false;
}

#define NON_ZERO_PRESSURE 50
#define PRESSURE_TOL 5
#define NON_ZERO_FLOW 20
#define FLOW_TOL 5
#define VALVE_SETTLE_MS 10000   // setState waits this long after moving valves before starting pumps
#define CIP_TEMP_TARGET 80

struct range {
    //range is target +/- range
    float target = 0;
    float tol = std::numeric_limits<float>::infinity();

    bool contains(float x) const {return target-tol <= x && x <= target+tol;}
};


struct valve_state {
    bool flush_valve = false, inlet_valve = false, cip_inlet_valve = false,
         concentrate_cip_valve = false, permeate_cip_valve = false,
         media_backwash_1_valve = false, media_backwash_2_valve = false;
};
struct pump_state {
    bool feed_pump = false, ro_pump = false, asa_pump = false,
         low_ph_pump = false, high_ph_pump = false;
};
// T = range for targets, float for measurements (same field names for both)
template <class T>
struct pressure_state {
    T media_outlet_pressure{}, media_inlet_pressure{}, concentrate_pressure{},
      cip_inlet_pressure{}, cip_outlet_pressure{}, interstage_pressure{},
      aqualine_inlet_pressure{}, aqualine_outlet_pressure{}, primary_pressure{},
      permeate_pressure{}, air_pressure{};
};
template <class T>
struct flow_state {
    T permeate_flow{}, concentrate_flow{}, feed_flow{};
};
using heater_state = bool;
// Per-state targets. Each case only sets what differs from the defaults
// (valves closed, pumps off, pressures and flows don't care).
// No default: so -Wall warns when a new RO_state has no case.

inline valve_state valvesFor(RO_state s) {
    valve_state v;
    switch (s) {
    case RO_state::CIP_FLUSH:
        v.flush_valve = true;
        v.cip_inlet_valve = true;
        break;
    case RO_state::CIP_SOAK:
    case RO_state::CIP_LOW_FLOW_CYCLE:
    case RO_state::CIP_HIGH_FLOW_CYCLE:
        v.flush_valve = true;
        v.cip_inlet_valve = true;
        v.concentrate_cip_valve = true;
        v.permeate_cip_valve = true;
        break;
    case RO_state::CIP_REFILL:
        v.inlet_valve = true;
        v.permeate_cip_valve = true;
        break;
    case RO_state::MEDIA_BACKWASH_1:
        v.media_backwash_1_valve = true;
        break;
    case RO_state::MEDIA_BACKWASH_2:
        v.media_backwash_2_valve = true;
        break;
    case RO_state::NORMAL_OP:
        v.inlet_valve = true;
        break;
    case RO_state::OFF:
    case RO_state::LOW_PH_DOSE:
    case RO_state::HIGH_PH_DOSE:
        break;
    }
    return v;
}

inline pump_state pumpsFor(RO_state s) {
    pump_state p;
    switch (s) {
    case RO_state::CIP_FLUSH:
    case RO_state::CIP_LOW_FLOW_CYCLE:
    case RO_state::CIP_HIGH_FLOW_CYCLE:
        p.ro_pump = true;
        break;
    case RO_state::CIP_REFILL:
    case RO_state::NORMAL_OP:
        p.feed_pump = true;
        p.ro_pump = true;
        break;
    case RO_state::MEDIA_BACKWASH_1:
    case RO_state::MEDIA_BACKWASH_2:
        p.feed_pump = true;
        break;
    case RO_state::CIP_SOAK:
    case RO_state::OFF:
        break;
    case RO_state::LOW_PH_DOSE:
        p.low_ph_pump = true;
        break;
    case RO_state::HIGH_PH_DOSE:
        p.high_ph_pump = true;
        break;
    }
    return p;
}

inline pressure_state<range> pressuresFor(RO_state s) {
    pressure_state<range> t;
    switch (s) {
    case RO_state::CIP_REFILL:
    case RO_state::NORMAL_OP:
        t.concentrate_pressure = {NON_ZERO_PRESSURE, PRESSURE_TOL};
        t.permeate_pressure = {NON_ZERO_PRESSURE, PRESSURE_TOL};
        break;
    case RO_state::CIP_FLUSH:
    case RO_state::CIP_SOAK:
    case RO_state::CIP_LOW_FLOW_CYCLE:
    case RO_state::CIP_HIGH_FLOW_CYCLE:
    case RO_state::MEDIA_BACKWASH_1:
    case RO_state::MEDIA_BACKWASH_2:
    case RO_state::OFF:
    case RO_state::HIGH_PH_DOSE:
    case RO_state::LOW_PH_DOSE:
        break;
    }
    return t;
}

inline flow_state<range> flowsFor(RO_state s) {
    flow_state<range> t;
    switch (s) {
    case RO_state::CIP_FLUSH:
    case RO_state::CIP_LOW_FLOW_CYCLE:
    case RO_state::CIP_HIGH_FLOW_CYCLE:
        t.concentrate_flow = {NON_ZERO_FLOW, FLOW_TOL};
        break;
    case RO_state::CIP_REFILL:
    case RO_state::NORMAL_OP:
        t.permeate_flow = {NON_ZERO_FLOW, FLOW_TOL};
        t.feed_flow = {NON_ZERO_FLOW, FLOW_TOL};
        break;
    case RO_state::MEDIA_BACKWASH_1:
    case RO_state::MEDIA_BACKWASH_2:
        t.feed_flow = {NON_ZERO_FLOW, FLOW_TOL};
        break;
    case RO_state::CIP_SOAK:
    case RO_state::OFF:
    case RO_state::HIGH_PH_DOSE:
    case RO_state::LOW_PH_DOSE:
        break;
    }
    return t;
}

inline heater_state HeaterFor(RO_state s) {
    heater_state t = false;
    switch (s) {
    case RO_state::CIP_FLUSH:
        t = true;
        break;
    case RO_state::CIP_LOW_FLOW_CYCLE:
        t = true;
        break;
    case RO_state::CIP_HIGH_FLOW_CYCLE:
        t = true;
        break;
    case RO_state::CIP_REFILL:
    case RO_state::NORMAL_OP:
    case RO_state::MEDIA_BACKWASH_1:
    case RO_state::MEDIA_BACKWASH_2:
    case RO_state::CIP_SOAK:
        t = true;
        break;
    case RO_state::OFF:
    case RO_state::HIGH_PH_DOSE:
        t = true;
        break;
    case RO_state::LOW_PH_DOSE:
        t = true;
        break;
    }
    return t;
}
