#pragma once
#include "devices.h"
#include "HAL/electronics.h"


// Interfaces are not owned by the pump and must outlive it.
// Pass nullptr for anything the pump doesn't have (speed control, alarm, pressure control).
//   on_off_control:                     relay or digital_out
//   electrical_interface_speed_control: analog_voltage_out (0-10 V), analog_current_out (4-20 mA) or pwm_duty_out
//   alarm:                              digital_in, high = alarm
//   pressure_control:                   analog_voltage_out (0-10 V), analog_current_out (4-20 mA) or pwm_duty_out;
//                                       pressure_min..pressure_max maps to the bottom..top of the output
//                                       (default 0-1, i.e. the same as speed)
class local_pump : public pump{
    public:
    local_pump(electrical_interface* electrical_interface_speed_control, electrical_interface* on_off_control, electrical_interface* alarm,
               electrical_interface* pressure_control, float pressure_min = 0.0f, float pressure_max = 1.0f)
    : electrical_interface_speed_control_(electrical_interface_speed_control), on_off_control_(on_off_control), alarm_(alarm),
      pressure_control_(pressure_control), pressure_min_(pressure_min), pressure_max_(pressure_max){};

    bool hasSpeedControl() const override {return electrical_interface_speed_control_ != nullptr;}
    bool hasAlarm() const override {return alarm_ != nullptr;}
    bool hasPressureControl() const override {return pressure_control_ != nullptr;}

    bool readAlarm() override; // false if no alarm fitted, true if the alarm input can't be read
    bool setOn() override;
    bool setOff() override;
    bool setSpeed(float speed) override; // 0-1
    bool readOn(bool& on) override;
    bool readSpeed(float& speed) override;
    bool setPressure(float pressure) override;   // pressure_min..pressure_max, rejected outside it
    bool readPressure(float& pressure) override;

    private:
    electrical_interface* electrical_interface_speed_control_;
    electrical_interface* on_off_control_;
    electrical_interface* alarm_;
    electrical_interface* pressure_control_;
    float pressure_min_;
    float pressure_max_;
};
