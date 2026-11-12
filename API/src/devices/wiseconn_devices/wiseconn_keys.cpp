#include "wiseconn_keys.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace {

std::string trim(const std::string& text) {
    size_t start = text.find_first_not_of(" \t\r");
    if (start == std::string::npos) return "";
    size_t end = text.find_last_not_of(" \t\r");
    return text.substr(start, end - start + 1);
}

std::vector<std::string> splitList(const std::string& text) {
    std::vector<std::string> items;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, ',')) {
        item = trim(item);
        if (!item.empty()) items.push_back(item);
    }
    return items;
}

bool parseInt(const std::string& text, int& value) {
    char* end = nullptr;
    long parsed = std::strtol(text.c_str(), &end, 10);
    if (text.empty() || *end != '\0' || parsed <= 0 || parsed > 100000) return false;
    value = static_cast<int>(parsed);
    return true;
}

}  // namespace

std::string wiseconn_keys::irrigation(const std::string& name) const {
    for (const auto& v : irrigations) {
        if (v.first == name) return v.second;
    }
    return "";
}

wiseconn_soil_ids wiseconn_keys::soil(const std::string& name) const {
    for (const wiseconn_soil_ids& s : soil_sensors) {
        if (s.name == name) return s;
    }
    wiseconn_soil_ids missing;
    missing.name = name;
    return missing;
}

bool parseWiseconnKeys(const std::string& text, wiseconn_keys& keys, std::string& error) {
    keys = wiseconn_keys{};
    std::stringstream stream(text);
    std::string line;
    int number = 0;
    bool in_irrigations = false;
    auto bad = [&](const std::string& why) {
        error = "wiseconn_keys.txt line " + std::to_string(number) + ": " + why;
        return false;
    };

    while (std::getline(stream, line)) {
        ++number;
        line = trim(line.substr(0, line.find('#')));
        if (line.empty()) continue;

        if (line.front() == '[') {
            if (line.back() != ']' || line.size() < 3) return bad("bad section name");
            std::string name = trim(line.substr(1, line.size() - 2));
            if (name.find_first_of(" \t") != std::string::npos) return bad("section name has spaces");
            if (name == "irrigations") {
                in_irrigations = true;
                continue;
            }
            in_irrigations = false;
            for (const wiseconn_soil_ids& s : keys.soil_sensors) {
                if (s.name == name) return bad("duplicate section [" + name + "]");
            }
            keys.soil_sensors.push_back({name, "", {}, {}, {}});
            continue;
        }

        size_t equals = line.find('=');
        if (equals == std::string::npos) return bad("expected key = value");
        std::string key = trim(line.substr(0, equals));
        std::string value = trim(line.substr(equals + 1));

        if (in_irrigations) {
            if (key.empty() || key.find_first_of(" \t") != std::string::npos) return bad("bad irrigation name");
            if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) {
                return bad("irrigation " + key + ": zone id must be a number");
            }
            if (!keys.irrigation(key).empty()) return bad("duplicate irrigation " + key);
            keys.irrigations.emplace_back(key, value);
        } else if (keys.soil_sensors.empty()) {
            if (key == "api_key") keys.api_key = value;
            else if (key == "farm_id") keys.farm_id = value;
            else if (key == "time_zone") keys.time_zone = value;
            else if (key == "base_url") keys.base_url = value;
            else if (key == "poll_seconds") {
                if (!parseInt(value, keys.poll_seconds)) return bad("poll_seconds must be a positive number");
            } else if (key == "hydraulics_poll_seconds") {
                if (!parseInt(value, keys.hydraulics_poll_seconds)) return bad("hydraulics_poll_seconds must be a positive number");
            } else if (key == "max_age_minutes") {
                if (!parseInt(value, keys.max_age_minutes)) return bad("max_age_minutes must be a positive number");
            } else return bad("unknown key " + key);
        } else {
            wiseconn_soil_ids& s = keys.soil_sensors.back();
            if (key == "zone_id") s.zone_id = value;
            else if (key == "moisture") s.moisture = splitList(value);
            else if (key == "etc") s.etc = splitList(value);
            else if (key == "temperature") s.temperature = splitList(value);
            else return bad("unknown key " + key + " in [" + s.name + "]");
        }
    }

    while (!keys.base_url.empty() && keys.base_url.back() == '/') keys.base_url.pop_back();
    for (const wiseconn_soil_ids& s : keys.soil_sensors) {
        if (s.zone_id.empty()) {
            error = "wiseconn_keys.txt [" + s.name + "]: no zone_id";
            return false;
        }
        if (s.moisture.empty() || s.moisture.size() != s.etc.size() || s.moisture.size() != s.temperature.size()) {
            error = "wiseconn_keys.txt [" + s.name + "]: moisture, etc and temperature need the same number of ids";
            return false;
        }
    }
    return true;
}

wiseconn_keys loadWiseconnKeys(const std::string& text) {
    wiseconn_keys keys;
    std::string error;
    if (!parseWiseconnKeys(text, keys, error)) {
        std::fprintf(stderr, "wiseconn: %s; WiseConn devices are off\n", error.c_str());
        return wiseconn_keys{};
    }
    if (keys.api_key.empty()) std::fprintf(stderr, "wiseconn: no api_key in wiseconn_keys.txt; WiseConn devices are off\n");
    return keys;
}
