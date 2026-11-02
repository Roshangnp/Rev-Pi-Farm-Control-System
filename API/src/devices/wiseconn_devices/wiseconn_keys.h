#pragma once
#include <string>
#include <vector>

// Contents of wiseconn_keys.txt (repo root, git-ignored), built into the
// program by CMake. Format: "key = value" lines, "#" starts a comment, and a
// "[name]" line starts a soil sensor section:
//
//   api_key = ...
//   farm_id = 3995
//   time_zone = America/Los_Angeles            (the farm's, see GET /farms timeZoneName)
//   base_url = https://apiv2.wiseconn.com     (optional, this is the default)
//   poll_seconds = 60                          (optional)
//   max_age_minutes = 45                       (optional)
//
//   [zone_7_soil]
//   zone_id = 163760
//   moisture = <id 4 in>, <id 8 in>, <id 12 in>
//   etc = ...
//   temperature = ...
struct wiseconn_soil_ids{
    std::string name;
    std::string zone_id;
    std::vector<std::string> moisture;     // WiseConn measure ids, shallowest first
    std::vector<std::string> etc;
    std::vector<std::string> temperature;
};

struct wiseconn_keys{
    std::string api_key;
    std::string farm_id;
    std::string time_zone;
    std::string base_url = "https://apiv2.wiseconn.com";
    int poll_seconds = 60;
    int max_age_minutes = 45;
    std::vector<wiseconn_soil_ids> soil_sensors;

    // The [name] section, or an empty entry (no ids) if it isn't there
    wiseconn_soil_ids soil(const std::string& name) const;
};

// False (and error set, with the line number) on a malformed file. Every soil
// sensor must have a zone_id and the same number of ids in each list.
bool parseWiseconnKeys(const std::string& text, wiseconn_keys& keys, std::string& error);

// parseWiseconnKeys() for config files: prints any problem to stderr and returns
// empty keys (no api_key, so the link stays off) instead of failing
wiseconn_keys loadWiseconnKeys(const std::string& text);
