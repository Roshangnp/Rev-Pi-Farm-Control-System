#pragma once
#include <string>

#include "line_server.h"

// Serves the RO (ro_config.h) to other machines as a handler on the
// line_server (comms/src), under two names, the same protocol as remote_ro and
// remote_procedure expect:
//   ro setState NORMAL_OP          ->  ok | fail | err bad argument
//   ro readState                   ->  ok NORMAL_OP | fail
//   ro isSettling                  ->  ok 0 | ok 1 | fail
//   ro setRoSpeed 0.5              ->  ok | fail | err bad argument
//   ro_procedure run EMPTY_FEED_TANK  ->  ok | fail | err bad argument
//   ro_procedure stop              ->  ok | fail (fail = none running)
//   ro_procedure readProcedure     ->  ok NONE | ok <the running one> | fail
// Built on the ro_config.h functions, not on the ro object, so it runs on every
// machine with an RO config: on the RO RevPi the calls drive the controller, on
// the others they go to the RO RevPi through remote_ro (registered by addRO).
// Requests are only handled inside the line_server's poll(), so they never run
// at the same time as the RO control loop. A client disconnecting changes nothing.
class ro_server : public line_handler{
    public:
    ro_server() = default;

    ro_server(const ro_server&) = delete;
    ro_server& operator=(const ro_server&) = delete;

    // Registers "ro" and "ro_procedure" on server. False if either is taken.
    bool registerWith(line_server& server);

    std::string handleRequest(int client_id, const std::string& name,
                              const std::string& method, const std::string& arg) override;
    // ro.readState, ro.isSettling, ro_procedure.readProcedure, for remote_link's cache
    void snapshot(std::string& out) override;

    private:
    std::string handleRO(const std::string& method, const std::string& arg);
    std::string handleProcedure(const std::string& method, const std::string& arg);
};
