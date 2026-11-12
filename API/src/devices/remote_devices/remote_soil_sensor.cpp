#include "remote_soil_sensor.h"

#include <cstdlib>

bool remote_soil_sensor::readMoisture(std::vector<float>& moisture) {
    return readDepths("readMoisture", moisture);
}

bool remote_soil_sensor::readETC(std::vector<float>& etc) {
    return readDepths("readETC", etc);
}

bool remote_soil_sensor::readTemperature(std::vector<float>& temperature) {
    return readDepths("readTemperature", temperature);
}

// "18,22.3,25.1" -> {18, 22.3, 25.1}; "" -> {}. values is only changed on success
bool remote_soil_sensor::readDepths(const char* method, std::vector<float>& values) {
    std::string text;
    if (!link_->query(device_name_, method, text)) return false;
    std::vector<float> parsed;
    size_t start = 0;
    while (start < text.size()) {
        size_t comma = text.find(',', start);
        if (comma == std::string::npos) comma = text.size();
        std::string item = text.substr(start, comma - start);
        char* end = nullptr;
        float value = std::strtof(item.c_str(), &end);
        if (item.empty() || *end != '\0') return false;
        parsed.push_back(value);
        start = comma + 1;
    }
    values = std::move(parsed);
    return true;
}
