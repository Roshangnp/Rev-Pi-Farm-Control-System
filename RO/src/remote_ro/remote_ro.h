#pragma once
#include <string>

#include "remote_link.h"
#include "types.h"

// The RO controller on the RO RevPi, seen from another machine (e.g.
// emerald-server's web UI). Talks to its ro_server over a remote_link to the
// RO RevPi. Every call returns false if the RO RevPi can't be reached or
// refuses the request.
class remote_ro{
    public:
    explicit remote_ro(remote_link& link, std::string name = "ro")
    : link_(link), name_(std::move(name)){};

    // Moves the RO to state s (non-blocking on the RO RevPi: pumps start
    // VALVE_SETTLE_MS later). False if any device failed; the RO is in state s
    // anyway and retries are up to the caller
    bool setState(RO_state s);
    bool readState(RO_state& s);
    bool isSettling(bool& settling);
    // RO pump VFD speed, 0-1
    bool setRoSpeed(float speed);
    // Starts procedure p on the RO RevPi (scheduled there, its first step runs
    // on the next loop pass). False if p isn't implemented yet
    bool runProcedure(procedure_types p);

    private:
    remote_link& link_;
    std::string name_;
};
