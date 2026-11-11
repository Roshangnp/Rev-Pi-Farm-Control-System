#include "ro_server.h"

#include <cstdlib>

namespace {

bool parseFloat(const std::string& text, float& value) {
    if (text.empty()) return false;
    char* end = nullptr;
    value = std::strtof(text.c_str(), &end);
    return *end == '\0';
}

}  // namespace

bool ro_server::registerWith(line_server& server) {
    return server.addName(name_, *this);
}

void ro_server::snapshot(std::string& out) {
    for (const char* method : {"readState", "isSettling"}) {
        appendSnapshotEntry(out, name_ + "." + method, handleRequest(0, name_, method, ""));
    }
}

std::string ro_server::handleRequest(int client_id, const std::string& name,
                                     const std::string& method, const std::string& arg) {
    (void)client_id;
    (void)name;

    if (method == "setState") {
        RO_state s;
        if (!fromString(arg, s)) return "err bad argument";
        return plant_.setState(s) ? "ok" : "fail";
    }
    if (method == "readState") return std::string("ok ") + toString(plant_.readState());
    if (method == "isSettling") return plant_.isSettling() ? "ok 1" : "ok 0";
    if (method == "setRoSpeed") {
        float speed;
        if (!parseFloat(arg, speed)) return "err bad argument";
        return plant_.setRoSpeed(speed) ? "ok" : "fail";
    }
    if (method == "runProcedure") {
        procedure_types p;
        if (!fromString(arg, p)) return "err bad argument";
        return plant_.runProcedure(p) ? "ok" : "fail";
    }
    if (method == "stopProcedure") return plant_.stopProcedure() ? "ok" : "fail";
    return "err unknown method";
}
