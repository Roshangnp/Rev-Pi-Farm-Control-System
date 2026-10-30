#include "remote_pressure_transducer.h"

bool remote_pressure_transducer::readPressure(float& psi) {
    return link_->queryFloat(device_name_, "readPressure", psi);
}
