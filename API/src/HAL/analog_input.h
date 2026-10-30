#pragma once
#include "electronics.h"

// Reads an analog input and scales it linearly to min_value..max_value.
//   analog_current_in: 4-20 mA, fails below 3.5 mA (broken wire)
//   analog_voltage_in: 0-10 V
// Returns false if input is nullptr, the wrong type, or can't be read.
bool readScaledAnalog(electrical_interface* input, float min_value, float max_value, float& value);
