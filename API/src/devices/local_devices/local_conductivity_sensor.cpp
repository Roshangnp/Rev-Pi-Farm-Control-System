#include "local_conductivity_sensor.h"
#include "HAL/analog_input.h"

bool local_conductivity_sensor::readConductivity(float& us_per_cm) {
    return readScaledAnalog(conductivity_input_, min_us_per_cm_, max_us_per_cm_, us_per_cm);
}
