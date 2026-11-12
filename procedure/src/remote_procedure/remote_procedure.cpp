#include "remote_procedure.h"

bool remote_procedure::run(procedure_types p) {
    return link_.command(name_, "run", toString(p));
}

bool remote_procedure::stop() {
    return link_.command(name_, "stop");
}

bool remote_procedure::readProcedure(bool& running, procedure_types& p) {
    std::string value;
    if (!link_.query(name_, "readProcedure", value)) return false;
    running = value != "NONE";
    return !running || fromString(value, p);
}
