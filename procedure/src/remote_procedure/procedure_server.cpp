#include "procedure_server.h"

bool procedure_server::registerWith(line_server& server) {
    return server.addName(name_, *this);
}

void procedure_server::snapshot(std::string& out) {
    appendSnapshotEntry(out, name_ + ".readProcedure", handleRequest(0, name_, "readProcedure", ""));
}

std::string procedure_server::handleRequest(int client_id, const std::string& name,
                                            const std::string& method, const std::string& arg) {
    (void)client_id;
    (void)name;

    if (method == "run") {
        procedure_types p;
        if (!fromString(arg, p)) return "err bad argument";
        return procedure_.run(p) ? "ok" : "fail";
    }
    if (method == "stop") {
        if (!procedure_.isRunning()) return "fail";
        return procedure_.stop() ? "ok" : "fail";
    }
    if (method == "readProcedure") {
        return procedure_.isRunning() ? std::string("ok ") + toString(procedure_.type()) : "ok NONE";
    }
    return "err unknown method";
}
