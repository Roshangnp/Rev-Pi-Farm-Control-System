#include "procedure_server.h"

#include <algorithm>

bool procedure_server::registerWith(line_server& server) {
    return server.addName(name_, *this);
}

void procedure_server::snapshot(std::string& out) {
    appendSnapshotEntry(out, name_ + ".readProcedure", handleRequest(0, name_, "readProcedure", ""));
}

bool procedure_server::isAllowed(procedure_types p) const {
    return allowed_.empty() || std::find(allowed_.begin(), allowed_.end(), p) != allowed_.end();
}

std::string procedure_server::handleRequest(int client_id, const std::string& name,
                                            const std::string& method, const std::string& arg) {
    (void)client_id;
    (void)name;

    if (method == "run") {
        procedure_types p;
        if (!fromString(arg, p)) return "err bad argument";
        if (!isAllowed(p)) return "err not allowed";
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
