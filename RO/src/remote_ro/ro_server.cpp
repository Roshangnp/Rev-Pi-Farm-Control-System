#include "ro_server.h"

#include <cstdlib>

#include "ro_config.h"

namespace {

const char* const RO_NAME = "ro";

bool parseFloat(const std::string& text, float& value) {
    if (text.empty()) return false;
    char* end = nullptr;
    value = std::strtof(text.c_str(), &end);
    return *end == '\0';
}

}  // namespace

bool ro_server::registerWith(line_server& server) {
    return server.addName(RO_NAME, *this);
}

void ro_server::snapshot(std::string& out) {
    for (const char* method : {"readState", "isSettling"}) {
        appendSnapshotEntry(out, std::string(RO_NAME) + "." + method, handleRO(method, ""));
    }
}

std::string ro_server::handleRequest(int client_id, const std::string& name,
                                     const std::string& method, const std::string& arg) {
    (void)client_id;
    (void)name;
    return handleRO(method, arg);
}

std::string ro_server::handleRO(const std::string& method, const std::string& arg) {
    if (method == "setState") {
        RO_state s;
        if (!fromString(arg, s)) return "err bad argument";
        return setROState(s) ? "ok" : "fail";
    }
    if (method == "readState") {
        RO_state s;
        return readROState(s) ? std::string("ok ") + toString(s) : "fail";
    }
    if (method == "isSettling") {
        bool settling = false;
        if (!readROSettling(settling)) return "fail";
        return settling ? "ok 1" : "ok 0";
    }
    if (method == "setRoSpeed") {
        float speed;
        if (!parseFloat(arg, speed)) return "err bad argument";
        return setROSpeed(speed) ? "ok" : "fail";
    }
    return "err unknown method";
}
