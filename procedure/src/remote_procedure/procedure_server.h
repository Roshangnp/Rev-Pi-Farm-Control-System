#pragma once
#include <string>

#include "line_server.h"
#include "procedure.h"

// Serves one procedure instance to other machines (remote_procedure) as a
// handler on the line_server (comms/src), under one name (e.g. "repressure").
// Requests are "<name> <method> [arg]":
//   repressure run REPRESSURE_CONTROL  ->  ok | fail | err bad argument
//   repressure stop                    ->  ok | fail (fail = none running)
//   repressure readProcedure           ->  ok NONE | ok <the running one>
// readProcedure is also in the snapshot, so remote reads come from the cache.
// Requests are only handled inside the line_server's poll(), so they never run
// at the same time as the scheduler.
//
// stop() only removes the remaining steps; whatever the procedure last switched
// stays as it is. A client disconnecting changes nothing.
class procedure_server : public line_handler{
    public:
    // p must outlive the server
    procedure_server(procedure& p, std::string name)
    : procedure_(p), name_(std::move(name)){};

    procedure_server(const procedure_server&) = delete;
    procedure_server& operator=(const procedure_server&) = delete;

    // Registers the name on server. False if it is already taken.
    bool registerWith(line_server& server);

    std::string handleRequest(int client_id, const std::string& name,
                              const std::string& method, const std::string& arg) override;
    // "<name>.readProcedure", for remote_link's cache
    void snapshot(std::string& out) override;

    private:
    procedure& procedure_;
    std::string name_;
};
