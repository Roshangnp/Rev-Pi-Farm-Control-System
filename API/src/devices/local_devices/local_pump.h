#pragma once
#include "devices.h"
#include "HAL/electronics.h"


// Interfaces are not owned by the pump and must outlive it.
// Pass nullptr for anything the pump doesn't have (speed control, alarm).
//   on_off_control:                     relay or digital_out
//   electrical_interface_speed_control: analog_voltage_out (0-10 V), analog_current_out (4-20 mA) or pwm_duty_out
//   alarm:                              digital_in, high = alarm
class local_pump : public pump{
    public:
    local_pump(electrical_interface* electrical_interface_speed_control, electrical_interface* on_off_control, electrical_interface* alarm)
    : electrical_interface_speed_control_(electrical_interface_speed_control), on_off_control_(on_off_control), alarm_(alarm){};

    bool hasSpeedControl() const override {return electrical_interface_speed_control_ != nullptr;}
    bool hasAlarm() const override {return alarm_ != nullptr;}

    bool readAlarm() override; // false if no alarm fitted, true if the alarm input can't be read
    bool setOn() override;
    bool setOff() override;
    bool setSpeed(float speed) override; // 0-1
    bool readOn(bool& on) override;
    bool readSpeed(float& speed) override;

    private:
    electrical_interface* electrical_interface_speed_control_;
    electrical_interface* on_off_control_;
    electrical_interface* alarm_;
};
