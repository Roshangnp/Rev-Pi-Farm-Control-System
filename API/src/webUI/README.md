# webUI

A small web page served by a farm machine, listing the devices its config registers (local or
remote) with their current values, plus controls for outputs:

- pump run, valve open, switched load on: a switch
- pump speed (if the pump has speed control): a number box and a Send button
- sensors: read only, with units; "read failed" in red if a read fails

The page refreshes every second.

## Using it

The config registers the devices and exposes two functions through `config.h`:

```cpp
startWebUI();                  // once at startup
while (true) {
    // control logic
    serviceWebUI(0);           // handle page requests
    serviceDeviceServer(20);   // the last call sleeps for the loop
}
```

On the host PC (no device server) just call `serviceWebUI(20)`. Then open
`http://<machine>:8080` (on the host PC: `http://localhost:8080`).

Like `device_server`, `web_ui` is single threaded: requests are only handled inside `poll()`,
so they never run at the same time as control code.

## HTTP API

| Request | Response |
|---|---|
| `GET /` | The page (embedded in `web_ui.cpp`) |
| `GET /api/devices` | JSON list of devices and their current values |
| `POST /api/command` with body `<device> <method> [arg]` | `ok`, `fail` or `err <reason>` |

Only commands are accepted on `/api/command` (`setOn`, `setOff`, `setSpeed`, `setOpen`,
`setClosed`); reads go through `/api/devices`. Example:

```sh
curl -s localhost:8080/api/devices
curl -s -X POST --data "ro_pump setSpeed 0.5" localhost:8080/api/command
```

`/api/devices` returns one entry per device, each with a list of fields:

```json
{"name":"ro_pump","fields":[
  {"label":"run","kind":"switch","value":false,"on":"setOn","off":"setOff"},
  {"label":"speed","kind":"setpoint","value":0,"method":"setSpeed","unit":"(0-1)"},
  {"label":"alarm","kind":"bool","value":false,"unit":""}]}
```

`kind` is `switch`, `setpoint`, `bool` or `value`; `value` is `null` when the read failed.

## Security

There is no login, and the page switches real loads.

- RevPis listen on all interfaces: limit port 8080 in firewalld to the machines that should see it.
- The host PC binds to `127.0.0.1` only (`web_ui ui(WEB_UI_PORT, "127.0.0.1")` in
  `config_host_pc.cpp`); use `"0.0.0.0"` to serve the LAN.
- Commands sent through a remote device are covered by the owner's watchdog: if the machine
  serving the page stops, what it switched on is turned off within about 2 s.

## Limitations

- Each refresh reads every device once. For remote devices that blocks the loop about 1 ms per
  read, or up to 300 ms per read while an owner is unreachable.
- Reading a flow sensor from the page shortens the averaging window the control code sees.
- No history, charts or confirmation dialogs yet.
