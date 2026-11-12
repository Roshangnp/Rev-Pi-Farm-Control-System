#pragma once
#include <chrono>
#include <cstdint>

// Line protocol between farm machines, shared by line_server and remote_link.
// One TCP connection per client; requests and replies are lines, replies come
// back in request order.
//   "<name> <method> [arg]"  ->  "ok [value]" | "fail" | "err <reason>"
//   "snapshot"               ->  "ok <key>=<value>;<key>;..."
//        every readable value of every handler, key = "<name>.<method>";
//        a key without "=" is a read that failed. remote_link polls this and
//        answers reads from it
//   "ping"                   ->  "ok"   (kept for old clients)
// No authentication: firewall the port to the other farm machines.
inline constexpr uint16_t LINE_SERVER_PORT = 47100;

namespace link_timing {
// remote_link asks for a snapshot this often (also keeps the connection from idling out)
inline constexpr auto SNAPSHOT_PERIOD = std::chrono::milliseconds(500);
// A cached value older than this is a failed read
inline constexpr auto STALE_TIMEOUT = std::chrono::milliseconds(3000);
// A command (setOn, setState, ...) fails if its reply takes longer than this
inline constexpr auto COMMAND_TIMEOUT = std::chrono::milliseconds(1500);
// Reads wait up to this long after the link is created for the first snapshot,
// so a program's first loop pass doesn't see every remote read fail
inline constexpr auto FIRST_SNAPSHOT_GRACE = std::chrono::milliseconds(2000);
inline constexpr auto CONNECT_TIMEOUT = std::chrono::milliseconds(1000);
inline constexpr auto RECONNECT_BACKOFF = std::chrono::milliseconds(1000);
// The link is dropped after this long with requests outstanding and nothing
// received (longer than the owner's 20 s idle timeout, so a stall is ridden out
// on both ends)
inline constexpr auto LINK_DEAD_TIMEOUT = std::chrono::seconds(25);
}  // namespace link_timing
