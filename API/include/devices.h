#pragma once

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
