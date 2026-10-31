#include "local_level_sensor.h"
#include "HAL/analog_input.h"

bool local_level_sensor::readLevel(float& level) {
    return readScaledAnalog(level_input_, min_level_, max_level_, level);
}
