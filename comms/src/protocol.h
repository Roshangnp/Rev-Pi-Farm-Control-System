#pragma once
#include <cstdint>

// Line protocol between farm machines, shared by line_server and remote_link.
// One TCP connection per client, one request line, one reply line:
//   "<name> <method> [arg]"  ->  "ok [value]" | "fail" | "err <reason>"
//   "ping"                   ->  "ok"   (remote_link's heartbeat)
// No authentication: firewall the port to the other farm machines.
inline constexpr uint16_t LINE_SERVER_PORT = 47100;
