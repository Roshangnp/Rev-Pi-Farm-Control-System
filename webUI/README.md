# webUI

A small web page listing farm devices (local or remote) with their current values, plus
controls for outputs. Not part of the API: it only uses the public device interfaces in
`API/include/devices.h`, and is built only with `-DDEVICE_CONFIG=host_pc` (the RevPi builds
never compile it). `main_host_pc` serves it from emerald-server.

- pump run, valve open, switched load on: a switch
- pump speed (if the pump has speed control): a number box and a Send button
- sensors: read only, with units; "read failed" in red if a read fails
- irrigation zones (WiseConn): on/off, plus start and end time boxes and a Schedule button that
  calls `scheduleIrrigation(start, end)` for today after a browser confirmation. This starts a
  REAL irrigation. Command line: `<zone> scheduleIrrigation HH:MM-HH:MM` (anything else is
  `err bad argument`); `fail (see the log)` if the device or WiseConn refused it
- soil sensors: read only, one row per depth for moisture, ETC and temperature, labelled with the
  depth names passed to `addDevice` (`{"4 in", "8 in", "12 in"}` in `main_host_pc`)
- state selectors (`state_selector`, declared in `web_ui.h`): the current state, a dropdown of
  `states()` with a Set button (browser confirmation), and a read only "settling" row. The
  dropdown follows the current state until you pick another one. Command line:
  `<name> setState <STATE>`, only names from `states()` (anything else is `err bad argument`).
  `main_host_pc` adds the RO controller as `ro`: an adapter over `RO/include/ro_config.h`
  (`setROState` etc), so the web UI itself never depends on `RO/`. Setting a state switches
  real pumps and valves on the RO RevPi, which keeps running it if the web UI stops

The page refreshes every second.

## Using it

The program creates the `web_ui` and adds the devices to show, using the references from
`config.h` (see `addAllDevices()` in `user_space/src/main_host_pc.cpp`):

```cpp
#include "config.h"
#include "web_ui.h"

web_ui ui(WEB_UI_PORT, "0.0.0.0");     // "127.0.0.1" = this machine only
ui.addDevice("feed_pump", feed_pump);  // listed in the order added
ui.start();                            // false: no password built in, or port in use
while (true) {
    // control logic
    ui.poll(20);                       // handles page requests, doubles as the loop sleep
}
```

Then open `http://<machine>:8080` from any device on the LAN and enter the password.
In CMake, link the `web_ui` target.

Like `device_server`, `web_ui` is single threaded: requests are only handled inside `poll()`,
so they never run at the same time as control code.

## HTTP API

| Request | Response |
|---|---|
| `GET /` | The page (embedded in `web_ui.cpp`) |
| `GET /api/devices` | JSON list of devices and their current values |
| `POST /api/command` with body `<device> <method> [arg]` | `ok`, `fail` or `err <reason>` |

Every `/api` request needs the password (URL-encoded) in an `X-Password` header, otherwise
`401`; after a wrong password that IP gets `429` for 1 s. `GET /` needs no password.

Only commands are accepted on `/api/command` (`setOn`, `setOff`, `setSpeed`, `setOpen`,
`setClosed`, `scheduleIrrigation`, `setState`); reads go through `/api/devices`. Example:

```sh
curl -s -H "X-Password: $(cat webui_password.txt)" localhost:8080/api/devices
curl -s -H "X-Password: $(cat webui_password.txt)" -X POST --data "ro_pump setSpeed 0.5" localhost:8080/api/command
```

`/api/devices` returns one entry per device, each with a list of fields:

```json
{"name":"ro_pump","fields":[
  {"label":"run","kind":"switch","value":false,"on":"setOn","off":"setOff"},
  {"label":"speed","kind":"setpoint","value":0,"method":"setSpeed","unit":"(0-1)"},
  {"label":"alarm","kind":"bool","value":false,"unit":""}]}
```

`kind` is `switch`, `setpoint`, `schedule` (irrigation: on/off value plus `method`), `bool` or `value`;
`value` is `null` when the read failed.

## Security

The page switches real loads, so it is password protected.

- The password is the first line of `webui_password.txt` in the repo root (git-ignored, surrounding
  whitespace ignored). CMake reads it at configure time into `build/generated/web_ui/web_ui_password.h`,
  so it is hardcoded in the binary; editing the file re-runs CMake on the next build. Each machine
  needs its own copy of the file. No file or an empty one: CMake warns and `start()` fails
- Builds that skip CMake (the plain `g++` test command) have no password, so their web UI won't start
- Plain HTTP: the password crosses the LAN unencrypted and is stored in the binary. It keeps out
  casual access, not someone sniffing the network or holding a copy of the program
- `main_host_pc` listens on all interfaces (`web_ui ui(WEB_UI_PORT, "0.0.0.0")`; `"127.0.0.1"` =
  that machine only). Still limit port 8080 in the firewall to the LAN
- Commands sent through a remote device are covered by the owner's watchdog: if the program
  serving the page stops (or its service restarts), what it switched on is turned off at once; if
  the network to the owner goes silent, after 20 s.

## Limitations

- Each refresh reads every device once. For remote devices that blocks the loop about 1 ms per
  read, or up to 300 ms per read while an owner is unreachable.
- Reading a flow sensor from the page shortens the averaging window the control code sees.
- No history, charts or confirmation dialogs yet.
