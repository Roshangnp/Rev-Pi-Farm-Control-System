#pragma once
#include <string>

#include "procedure.h"
#include "remote_link.h"

// A procedure instance on another machine (served there by procedure_server
// under name), e.g. the repressure procedure on the RO RevPi seen from
// emerald-server or the console. Shares the remote_link to that machine with its
// remote devices. Every call returns false if the machine can't be reached or
// refuses the request.
class remote_procedure{
    public:
    remote_procedure(remote_link& link, std::string name)
    : link_(link), name_(std::move(name)){};

    // Starts procedure p there. False if one is still running or that instance
    // isn't allowed to run p
    bool run(procedure_types p);
    // Removes the remaining steps there (what it switched stays as it is). False
    // if none is running
    bool stop();
    // running = a procedure is running there, p = which one. False if it can't
    // be read (from the snapshot cache, never waits on the network)
    bool readProcedure(bool& running, procedure_types& p);

    private:
    remote_link& link_;
    std::string name_;
};
