#include "local_temperature_sensor.h"
#include "HAL/analog_input.h"

bool local_temperature_sensor::readTemperature(float& deg_f) {
    return readScaledAnalog(temperature_input_, min_deg_f_, max_deg_f_, deg_f);
}
