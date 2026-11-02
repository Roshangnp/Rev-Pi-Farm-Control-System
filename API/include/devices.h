#pragma once
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
};

class float_switch{
    public:
    virtual ~float_switch() = default;

    virtual bool readSubmerged(bool& submerged) = 0;
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

    // false if the sensor can't be read or there is no flow rate yet (e.g. first call)
    virtual bool readFlow(float& gpm) = 0;
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
};