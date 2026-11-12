#pragma once
#include <string>

#include "line_server.h"

// Serves the RO (ro_config.h) to other machines (remote_ro) as a handler on the
// line_server (comms/src), under the name "ro":
//   ro setState NORMAL_OP          ->  ok | fail | err bad argument
//   ro readState                   ->  ok NORMAL_OP | fail
//   ro isSettling                  ->  ok 0 | ok 1 | fail
//   ro setRoSpeed 0.5              ->  ok | fail | err bad argument
// Built on the ro_config.h functions; registered by addRO on the RO RevPi.
// The RO procedures are served separately by main_ro (procedure_server "ro_procedure").
// Requests are only handled inside the line_server's poll(), so they never run
// at the same time as the RO control loop. A client disconnecting changes nothing.
class ro_server : public line_handler{
    public:
    ro_server() = default;

    ro_server(const ro_server&) = delete;
    ro_server& operator=(const ro_server&) = delete;

    // Registers "ro" on server. False if it is taken.
    bool registerWith(line_server& server);

    std::string handleRequest(int client_id, const std::string& name,
                              const std::string& method, const std::string& arg) override;
    // ro.readState, ro.isSettling, for remote_link's cache
    void snapshot(std::string& out) override;

    private:
    std::string handleRO(const std::string& method, const std::string& arg);
};
