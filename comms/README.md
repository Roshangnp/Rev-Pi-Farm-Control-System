# comms

Line protocol between the farm machines, independent of the API (it knows nothing about
devices). Built as the `comms` library; `farm_api` links it.

| File | Purpose |
|---|---|
| `protocol.h` | Port (47100) and the protocol description |
| `line_server.h/.cpp` | Server: one port, routes each request to the handler that registered its first word, client watchdog |
| `remote_link.h/.cpp` | Client: one connection to another machine, reconnects, heartbeat (see `API/src/devices/README.md`) |

## Protocol

One request line, one reply line:

```
<name> <method> [arg]    ->    ok [value] | fail | err <reason>
ping                     ->    ok          (remote_link's heartbeat)
```

`err unknown device` = no handler registered that name, `err too many arguments` = more than
three words. There is no authentication: firewall port 47100 to the other farm machines.

## line_server

`main` owns one `line_server` per machine. Modules register names on it with a
`line_handler` (`handleRequest`, optional `clientGone`), e.g. the API's `device_server`
registers every local device via `addLocalDevices(server)`. Names are unique across handlers.

```cpp
line_server server;            // port 47100, 20 s watchdog
addLocalDevices(server);       // device_server's names
server.start();
while (true) server.poll(20);  // requests only handled here; doubles as the loop sleep
```

Single threaded: handlers only run inside `poll()`, so they never race the main loop.

Watchdog: a client that disconnects (at once) or sends nothing for `watchdog_ms` (20 s) is
dropped and every handler's `clientGone(id)` is called. Each handler decides what that means
(the device server turns off what that client switched on; a handler can also ignore it).
