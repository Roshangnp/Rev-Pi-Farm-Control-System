#include "local_tds_sensor.h"
#include "HAL/analog_input.h"

bool local_tds_sensor::readTds(float& ppm) {
    return readScaledAnalog(tds_input_, min_ppm_, max_ppm_, ppm);
}
