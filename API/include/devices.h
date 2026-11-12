#pragma once
#include <cstdint>
#include <vector>

// Hardware-agnostic device interfaces. Controller code uses only these, so a
// device works the same whether it is wired to this RevPi (local_*) or
// owned by another RevPi on the network.
// All methods return false on failure; reads fill the out parameter.

class pump{
    public:
    virtual ~pump() = default;

    virtual bool hasSpeedControl() const = 0;
    virtual bool hasAlarm() const = 0;

    virtual bool readAlarm() = 0; // false if no alarm fitted, true if the alarm can't be read
    virtual bool setOn() = 0;
    virtual bool setOff() = 0;
    virtual bool setSpeed(float speed) = 0; // 0-1

    // Read back the outputs as last written (not a running feedback signal)
    virtual bool readOn(bool& on) = 0;
    virtual bool readSpeed(float& speed) = 0; // 0-1, false if no speed control

    // Pressure setpoint output (e.g. a VFD running its own pressure loop)
    virtual bool hasPressureControl() const = 0;
    virtual bool setPressure(float pressure) = 0;     // in the pump's pressure range
    virtual bool readPressure(float& pressure) = 0;   // setpoint as last written, false if no pressure control
};

class float_switch{
    public:
    virtual ~float_switch() = default;

    virtual bool readSubmerged(bool& submerged) = 0;
};

class toggle_switch{
    public:
    virtual ~toggle_switch() = default;

    virtual bool readValue(bool& value) = 0;
};

class toggle_switch_double_pole{
    public:
    virtual ~toggle_switch_double_pole() = default;

    virtual bool readPole1(bool& on) = 0; // on = pole 1 input high
    virtual bool readPole2(bool& on) = 0; // on = pole 2 input high
};

class push_button{
    public:
    virtual ~push_button() = default;

    // The button's press counter (counts up, reads don't change it). To count
    // presses in your own code, keep the last total you saw and subtract
    virtual bool readPressTotal(uint32_t& total) = 0;

    // Presses since this object's previous call. Each push_button object keeps
    // its own count: a remote button on another machine doesn't take presses
    // from this one, but callers sharing this object share the count.
    // false on the first call, if the counter can't be read or was reset
    bool readPushButtonCount(uint32_t& presses) {
        uint32_t total = 0;
        if (!readPressTotal(total)) return false;
        bool had_last = has_last_total_;
        uint32_t last = last_total_;
        has_last_total_ = true;
        last_total_ = total;
        if (!had_last || total < last) return false;
        presses = total - last;
        return true;
    }

    private:
    bool has_last_total_ = false;
    uint32_t last_total_ = 0;
};

class valve{
    public:
    virtual ~valve() = default;

    virtual bool setOpen() = 0;
    virtual bool setClosed() = 0;

    // Reads back the output as last written (not a position feedback signal)
    virtual bool readOpen(bool& open) = 0;
};

class pressure_transducer{
    public:
    virtual ~pressure_transducer() = default;

    virtual bool readPressure(float& psi) = 0; // false if the sensor can't be read or the wire is broken
};

class tds_sensor{
    public:
    virtual ~tds_sensor() = default;

    virtual bool readTds(float& ppm) = 0; // false if the sensor can't be read or the wire is broken
};

class flow_sensor{
    public:
    virtual ~flow_sensor() = default;

    // Latest flow, a rolling average kept up to date by update() on the machine
    // that owns the sensor. Reads change nothing, so any number of readers see
    // the same value. false if the sensor can't be read, there is no full
    // averaging window yet, or update() hasn't run recently
    virtual bool readFlow(float& gpm) = 0;

    // Samples the sensor and recomputes the average. Call every main loop pass
    // on the RevPi that owns the sensor. Does nothing for a remote sensor
    virtual void update() {}
};

class level_sensor{
    public:
    virtual ~level_sensor() = default;

    // Level in the units of the sensor's configured range (see config).
    // false if the sensor can't be read or the wire is broken
    virtual bool readLevel(float& level) = 0;
};

class conductivity_sensor{
    public:
    virtual ~conductivity_sensor() = default;

    virtual bool readConductivity(float& us_per_cm) = 0; // false if the sensor can't be read or the wire is broken
};

class temperature_sensor{
    public:
    virtual ~temperature_sensor() = default;

    virtual bool readTemperature(float& deg_f) = 0; // false if the sensor can't be read or the wire is broken
};

// A load that is only switched on or off, e.g. lights or a heater
class switched_load{
    public:
    virtual ~switched_load() = default;

    virtual bool setOn() = 0;
    virtual bool setOff() = 0;

    // Reads back the output as last written (not a feedback signal)
    virtual bool readOn(bool& on) = 0;
};

class soil_sensor{
    public:
    virtual ~soil_sensor() = default;

    virtual bool readMoisture(std::vector<float>& moisture) = 0;
    virtual bool readETC(std::vector<float>& etc) = 0;
    virtual bool readTemperature(std::vector<float>& temperature) = 0;
};

class irrigation{
    public:
    virtual ~irrigation() = default;

    virtual bool readOn(bool& on) = 0;
    virtual bool scheduleIrrigation(int startHour, int startMinute, int endHour, int endMinute) = 0; // hours in 24hr format and day is infered to be today
    virtual bool scheduleIrrigation(int durationMinutes) = 0; // schedule irrigation for now for duration

    // The id of the irrigation the last successful scheduleIrrigation() created
    // (kept until cancelIrrigation() deletes it or the next one replaces it; lost
    // on a restart). False if there is none
    virtual bool readScheduledIrrigation(int& id) = 0;
    // Deletes that irrigation. Only works while it is still in the future and not
    // yet sent to the field (WiseConn status "Saved"); false otherwise, or if
    // there is none
    virtual bool cancelIrrigation() = 0;
};