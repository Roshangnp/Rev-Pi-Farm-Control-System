#pragma once
#include <string>

#include "line_server.h"
#include "ro.h"

// Serves the RO controller to other machines (remote_ro) as a handler on the
// line_server (comms/src), under one name ("ro" by default). Requests are
// "<name> <method> [arg]" with method = the ro method name:
//   ro setState NORMAL_OP   ->  ok | fail | err bad argument
//   ro readState            ->  ok NORMAL_OP
//   ro isSettling           ->  ok 0 | ok 1
//   ro setRoSpeed 0.5       ->  ok | fail | err bad argument
//   ro runProcedure LOW_PH_CIP_CYCLE  ->  ok | fail | err bad argument
// Requests are only handled inside the line_server's poll(), so they never run
// at the same time as the RO control loop.
//
// No watchdog: a client disconnecting does not change the RO state, so the RO
// keeps running if emerald-server or the network goes down.
class ro_server : public line_handler{
    public:
    explicit ro_server(ro& plant, std::string name = "ro")
    : plant_(plant), name_(std::move(name)){};

    ro_server(const ro_server&) = delete;
    ro_server& operator=(const ro_server&) = delete;

    // Registers the name on server. False if it is already taken.
    bool registerWith(line_server& server);

    std::string handleRequest(int client_id, const std::string& name,
                              const std::string& method, const std::string& arg) override;

    private:
    ro& plant_;
    std::string name_;
};
