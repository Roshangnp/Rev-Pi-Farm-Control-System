#pragma once
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "protocol.h"

// Answers requests for the names it registered on a line_server
class line_handler{
    public:
    virtual ~line_handler() = default;

    // One request "<name> <method> [arg]" for a name this handler registered.
    // client_id identifies the connection it came from. Returns the reply
    // without the newline: "ok [value]", "fail" or "err <reason>"
    virtual std::string handleRequest(int client_id, const std::string& name,
                                      const std::string& method, const std::string& arg) = 0;

    // Appends every readable value of this handler to a "snapshot" reply, as
    // entries separated by ';': "<name>.<method>=<value>", or "<name>.<method>"
    // alone if the read failed. Values must not contain ';', '=' or whitespace.
    // Must only read (never command anything)
    virtual void snapshot(std::string& out) {(void)out;}
};

// Adds one snapshot entry from a handleRequest-style reply: "ok <value>" gives
// "key=value", anything else gives "key" (failed read)
void appendSnapshotEntry(std::string& out, const std::string& key, const std::string& reply);

// TCP server for the line protocol (protocol.h). Routes each request to the
// handler that registered its first word, so several modules (e.g. the device
// server and a controller) share one port, and answers "snapshot" with every
// handler's snapshot().
//
// Single threaded: requests are only handled inside poll(), so handlers never
// run at the same time as the code that calls poll().
//
// A client that sends nothing for idle_timeout_ms (default 20 s) is dropped, so
// dead connections (client powered off, network gone) don't pile up. Dropping a
// client changes nothing on the hardware. remote_link asks for a snapshot every
// 0.5 s, so a live client is never idle that long.
class line_server{
    public:
    // loopback_only: listen on 127.0.0.1 only, so only programs on this machine
    // can connect (e.g. the host PC, which has no firewall); default all interfaces
    line_server(uint16_t port = LINE_SERVER_PORT, int idle_timeout_ms = 20000, bool loopback_only = false)
    : port_(port), idle_timeout_(idle_timeout_ms), loopback_only_(loopback_only){};
    ~line_server();

    line_server(const line_server&) = delete;
    line_server& operator=(const line_server&) = delete;

    // Routes requests whose first word is name to handler. Names must be unique
    // across all handlers, non-empty, without whitespace, '.', ';' or '=', and
    // not "ping" or "snapshot". The handler must outlive the server.
    bool addName(const std::string& name, line_handler& handler);

    // Opens the listening socket. False if the port can't be opened.
    bool start();

    // Handles pending requests and drops idle clients. Waits up to timeout_ms
    // for activity, so it can double as the control loop's sleep.
    void poll(int timeout_ms);

    private:
    struct client {
        int fd;
        int id;
        std::string rx_buffer;
        std::string tx_buffer;  // replies the socket hasn't taken yet
        std::chrono::steady_clock::time_point last_seen;
    };

    void acceptClients();
    bool readClient(client& c);  // false if the client should be dropped
    bool flushClient(client& c); // false if the client should be dropped
    std::string handleLine(const client& c, const std::string& line);
    std::string snapshot();
    void dropClient(size_t index);

    uint16_t port_;
    std::chrono::milliseconds idle_timeout_;
    bool loopback_only_;
    int listen_fd_ = -1;
    int next_client_id_ = 1;
    std::vector<client> clients_;
    std::map<std::string, line_handler*> names_;
    std::vector<line_handler*> handlers_; // each once, for snapshot()
};
