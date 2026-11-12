#include "procedure_config.h"

// RO RevPi (192.168.1.20): owns ro_procedure, the RO procedures, run on main's
// scheduler and served to the other machines as "ro_procedure" on main's
// line_server (procedure_server, only the RO procedure types).

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "line_server.h"
#include "scheduler.h"
#include "remote_procedure/procedure_server.h"

namespace {

// main's scheduler, set by addProcedures() before the instance is created
scheduler* main_scheduler = nullptr;

// Only the RO procedures may run on ro_procedure (not e.g. a future repressure one)
const std::vector<procedure_types> RO_PROCEDURE_TYPES = {
    procedure_types::LOW_PH_CIP_CYCLE, procedure_types::HIGH_PH_CIP_CYCLE,
    procedure_types::MEDIA_BACKWASH_CYCLE, procedure_types::EMPTY_FEED_TANK};

// Function-local statics: created on the addProcedures() call, after main's
// scheduler (a global), so they are destroyed before it (the procedure's
// destructor removes its tasks from the scheduler)
procedure& roProcedure() {
    if (!main_scheduler) {
        std::fprintf(stderr, "RO procedure: used before addProcedures()\n");
        std::abort();
    }
    static procedure instance(*main_scheduler);
    return instance;
}

procedure_server& roProcedureServer() {
    static procedure_server instance(roProcedure(), "ro_procedure", RO_PROCEDURE_TYPES);
    return instance;
}

// What ro_procedure is bound to: forwards to the instance created by
// addProcedures() (so the binding exists from the start, like every config.h device)
class owned_procedure : public procedure_control {
    public:
    bool run(procedure_types p) override { return roProcedure().run(p); }
    bool stop() override { return roProcedure().stop(); }
    bool readProcedure(bool& running, procedure_types& p) override { return roProcedure().readProcedure(running, p); }
};

owned_procedure ro_procedure_here;

}  // namespace

procedure_control& ro_procedure = ro_procedure_here;

bool addProcedures(line_server& server, scheduler& sched) {
    main_scheduler = &sched;
    roProcedure();
    if (!roProcedureServer().registerWith(server)) {
        std::fprintf(stderr, "RO procedure: name \"ro_procedure\" is already taken on the server\n");
        return false;
    }
    return true;
}
