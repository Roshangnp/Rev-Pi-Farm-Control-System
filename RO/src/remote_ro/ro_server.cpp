#include "ro_server.h"

#include <cstdlib>

#include "ro_config.h"

namespace {

const char* const RO_NAME = "ro";
const char* const PROCEDURE_NAME = "ro_procedure";

bool parseFloat(const std::string& text, float& value) {
    if (text.empty()) return false;
    char* end = nullptr;
    value = std::strtof(text.c_str(), &end);
    return *end == '\0';
}

}  // namespace

bool ro_server::registerWith(line_server& server) {
    return server.addName(RO_NAME, *this) && server.addName(PROCEDURE_NAME, *this);
}

void ro_server::snapshot(std::string& out) {
    for (const char* method : {"readState", "isSettling"}) {
        appendSnapshotEntry(out, std::string(RO_NAME) + "." + method, handleRO(method, ""));
    }
    appendSnapshotEntry(out, std::string(PROCEDURE_NAME) + ".readProcedure", handleProcedure("readProcedure", ""));
}

std::string ro_server::handleRequest(int client_id, const std::string& name,
                                     const std::string& method, const std::string& arg) {
    (void)client_id;
    if (name == PROCEDURE_NAME) return handleProcedure(method, arg);
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

std::string ro_server::handleProcedure(const std::string& method, const std::string& arg) {
    if (method == "run") {
        procedure_types p;
        if (!fromString(arg, p)) return "err bad argument";
        return runROProcedure(p) ? "ok" : "fail";
    }
    if (method == "stop") return stopROProcedure() ? "ok" : "fail";
    if (method == "readProcedure") {
        bool running = false;
        procedure_types p;
        if (!readROProcedure(running, p)) return "fail";
        return running ? std::string("ok ") + toString(p) : "ok NONE";
    }
    return "err unknown method";
}
