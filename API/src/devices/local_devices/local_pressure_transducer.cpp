#include "local_pressure_transducer.h"
#include "HAL/analog_input.h"

bool local_pressure_transducer::readPressure(float& psi) {
    return readScaledAnalog(pressure_input_, min_psi_, max_psi_, psi);
}
