#pragma once
#include <string>
#include <vector>

// Minimal JSON reader for WiseConn API replies (no JSON library on the RevPis
// or the host PC). Parses the whole document into a tree; numbers are doubles.
struct json_value{
    enum class kind {null_value, boolean, number, string, array, object};

    kind type = kind::null_value;
    bool boolean = false;
    double number = 0.0;
    std::string text;                  // string
    std::vector<json_value> items;     // array items, or object values
    std::vector<std::string> keys;     // object keys, same order as items

    // Object member, or nullptr if missing / not an object
    const json_value* find(const std::string& key) const;
};

// False (and error set) if the text is not one complete JSON value
bool parseJson(const std::string& text, json_value& out, std::string& error);
