# Rev-Pi-Farm-Control-System

Farm control system running on Revolution Pi Connect 4 units.

## Layout

- `API/` - hardware API, the layer between the RevPi I/O and the controllers
  - Include roots are `-IAPI/include -IAPI/src`; `API/include` headers are included by bare name (`"devices.h"`), cross-folder `API/src` includes are written relative to `API/src` (e.g. `"HAL/electronics.h"`), same-folder includes use the bare name
  - `API/include/` - what external programs (e.g. the controller) include from the API
    - `devices.h` - public device interface: abstract base classes `pump`, `float_switch`, `valve`, `pressure_transducer`, `tds_sensor`, `flow_sensor`, `level_sensor`, `conductivity_sensor` (uS/cm), `temperature_sensor` (deg F), `switched_load` (on/off only: lights, heater), `soil_sensor` (per-depth vectors) (pure virtual, no hardware includes). Controller code uses only these, so local and remote devices are interchangeable
    - `config.h` - shared by every machine: every farm device as an `extern` base-class reference (`extern pump& feed_pump;`), `attachAllInterfaces()`. Includes only `devices.h`
  - `API/src/config/config_<name>.cpp` - one per machine, exactly one is built (`cmake -DDEVICE_CONFIG=<name>`, no default). The wiring: `electrical_interface`s and `local_*`/`remote_*` devices (anonymous namespace), bound to the references declared in config.h. A config only binds the devices that machine uses; a program using an unbound device fails to link
    - `config_test` - test bench RevPi (192.168.1.20), example devices. Same RevPi as `config_ro`; shares the names `feed_pump`, `inlet_valve`, `permeate_flow`
    - `config_ro` - RO RevPi (192.168.1.20), written from the CSV: 3 flow, 9 float, 11 pressure, 2 conductivity, 1 temperature, 6 pumps (feed/RO pump = run relay + VFD alarm; RO pump speed = AIO pos 32 `OutputValue_1`, 4-20 mA, `Output1Range` = 9), 2 switched loads, 7 valves on the RS485 relays; no web UI
    - `config_uppertank` - upper tank RevPi (192.168.1.21, MIO + AIO): `upper_tank_high_float` (MIO `DigitalInput_4`), `upper_tank_water_level` (AIO `InputValue_1`, 4-20 mA)
    - `config_host_pc` - Ubuntu dev PC, all devices remote, nothing shared (`addLocalDevices` registers nothing)
  - `API/hardware_setup.csv` - user's device list (name, id, host IP, interface type, pin, analog range); configs are written by hand from it
  - `API/src/HAL/` - hardware abstraction layer
    - `electronics.h/.cpp` - `electrical_interface`: one I/O point bound to a PiCtory variable
    - `analog_input.h/.cpp` - shared 4-20 mA / 0-10 V scaling for pressure and TDS; < 3.5 mA = broken wire
    - `piControl.h` - vendored KUNBUS driver header (MIT), do not edit
  - `API/src/devices/local_devices/local_*.h/.cpp` - devices wired to this RevPi, each inherits its base from `devices.h`
    - `local_pump`: non-owning pointers to on/off, speed and alarm interfaces; nullptr = not fitted. Speed 0-1 maps to 0-10 V, 4-20 mA or 0-100 % PWM
    - `local_float_switch`, `local_valve`, `local_pressure_transducer`, `local_tds_sensor`, `local_flow_sensor`, `local_level_sensor`, `local_conductivity_sensor`, `local_temperature_sensor`, `local_switched_load` - one interface each; reads return false on failure and fill an out param
  - `API/src/devices/remote_devices/` - devices owned by another RevPi on the LAN. One owner per device
    - `remote_link` (lives in `comms/src/`) - one TCP connection per remote RevPi (port 47100), shared by its `remote_*` devices; auto-reconnect (1 s backoff), 300 ms connect/reply timeouts, heartbeat thread pings every 0.5 s. A late reply fails only that call (it's skipped when it arrives); with 2 replies overdue (network stall) calls fail at once without sending, so a stall blocks a caller at most 2 x 300 ms. The link is only dropped after 25 s with replies outstanding and nothing from the owner (longer than the owner's 20 s watchdog, so the owner alone decides when a stall resets outputs; closing the socket would reset them at once)
    - `remote_*` - one proxy per base class, `(remote_link&, device_name)`; network failure returns false (`readAlarm` returns true)
    - `device_server` - on the owner; a `line_handler` registered on main's `line_server` (`comms/`), so requests are only handled inside `server.poll()` and never race local control code. Watchdog (`line_server` drops the client, `device_server::clientGone` applies the safe state): if a client disconnects (at once) or is silent 20 s (default `watchdog_ms` = 20000), pumps and switched loads it last commanded go off and valves it last commanded close. Restarting `main_host_pc` (`rev-pi-control`) is a disconnect, so it resets what the web UI switched on. Was 2 s until 2026-11-01: Wi-Fi stalls of ~1.5 s between emerald-server and the RO RevPi reset relays set from the web UI
    - Protocol: text lines `"<device> <method> [arg]"` -> `"ok [value]"` / `"fail"` / `"err <reason>"`; method names = C++ method names. No authentication: restrict the firewall to the other RevPi
  - `config.h` also exposes `addLocalDevices(line_server&)` (forward-declared `line_server`); each config registers its local devices on its `device_server devices` under their public names (snake_case of the CSV name), then `devices.registerWith(server)`
  - `API/src/devices/wiseconn_devices/` - devices read from the WiseConn cloud API (farm irrigation system), host PC only (CMake excludes the folder from other configs). Read only, GET requests only: the WiseConn valves are live, never send anything that changes them. Bound in `config_host_pc`: soil probes `zone_7_soil`, `zone_8_soil`, `zone_10_3_soil` (3 depths 4/8/12 in; moisture %, ETC = EC in raw VIC units, temperature deg F) and 24 irrigation zones (`irrigation`: `valve_1_1`..`valve_10_3`, `overhead_1`..`overhead_6`, `city_water_fill_valve`; zone ids in `[irrigations]`): `readOn` from `GET /farms/3995/hydraulics` (zone's valve On/Off); `scheduleIrrigation(sh, sm, eh, em)` (today) sends `POST /irrigations {zoneId, initTime, endTime, directSchedule: true}` (farm local times, no Z) and `scheduleIrrigation(minutes)` sends `{zoneId, duration}`; both START REAL IRRIGATIONS, block ~1 s, never retry. Never test them against the real API without the user choosing the zone and time; test against a fake local server via `base_url`. Web UI shows on/off plus start/end time boxes and a Schedule button (browser confirm, then `<zone> scheduleIrrigation HH:MM-HH:MM` to `/api/command`; starts a real irrigation)
    - `wiseconn_link`: background thread polls `GET /zones/<id>/measures` every 60 s with the `curl` program, caches values; reads are instant, fail when `lastDataDate` is older than 45 min
    - Keys: `wiseconn_keys.txt` (repo root, git-ignored: api_key, farm_id 3995, time_zone, zone and measure ids), built in as text by CMake into `build/generated/wiseconn/`; missing = devices off
    - Gotcha: WiseConn `lastDataDate` ends in Z but is farm local time (America/Los_Angeles); parsed as local time, so the host must be in that time zone (emerald-server and the dev PC are)
    - Details and how to find ids: `API/src/devices/README.md`
  - Output readback: `pump::readOn/readSpeed`, `valve::readOpen` read the output process image as last written (not run/position feedback)
- `comms/src/` - line protocol between machines, independent of the API (`comms` library, `farm_api` links it). `protocol.h` (port 47100), `line_server` (one per machine, owned by `main_*`: routes each request by its first word to the `line_handler` that registered it, names unique across handlers, single threaded `poll()`, client watchdog calls every handler's `clientGone(id)`), `remote_link` (client). Lets other modules (e.g. a future RO controller) share port 47100 with the device server
- `RO/` - RO controller (state machine over the config.h devices), outside the API. `RO/include/`: `types.h` (`RO_state`, `ALL_RO_STATES`, `toString`/`fromString`, `range`, `valve_state`/`pump_state`/`pressure_state<T>`/`flow_state<T>`, `valvesFor`/`pumpsFor`/`pressuresFor`/`flowsFor`, `VALVE_SETTLE_MS` = 10 s, `procedure_types` + `ALL_PROCEDURE_TYPES` + `toString`/`fromString`), `ro_config.h` (like config.h: one header for every machine, implemented per machine in `RO/src/config/ro_config_<DEVICE_CONFIG>.cpp`; `addRO(line_server&, scheduler&)`, `updateRO()`, `setROState`, `readROState`, `readROSettling`, `setROSpeed`, `runROProcedure(procedure_types)`, `stopROProcedure()`). `ro_config_ro`: the controller runs here (`ro`/`ro_server` are function-local statics so they're destroyed before config_ro's devices and main's scheduler; using them before `addRO` aborts); `addRO` stores the scheduler, drives OFF at startup and registers "ro", `updateRO` every loop pass. `ro_config_host_pc`: the same functions go to the RO RevPi through `remote_ro` on its own `remote_link("192.168.1.20")`; `addRO`/`updateRO` return true. Other configs have no RO config. `RO/src/`: `ro` (non-blocking `setState`: stop pumps, open then close valves, pumps start in `update()` after `VALVE_SETTLE_MS`; holds `scheduler& scheduler_` (the machine's one scheduler, owned by main and passed through `addRO`; `ro(scheduler&)`); `setHeaterState()` (called in `setState`) only sets the private `heater_active_` = `HeaterFor(state)` (types.h); `ro` never switches `cip_heater`, the heater is operated elsewhere; `runProcedure(procedure_types)` = `procedure_.run(p)`; `stopProcedure()` = `procedure_.stop()` then OFF, false and state left alone if none is running; owns the one `ro_procedure procedure_` (declared after `scheduler_`, so destroyed first)), `ro_server` (`line_handler` "ro": `setState <STATE>`, `readState`, `isSettling`, `setRoSpeed <0-1>`, `runProcedure <NAME>`, `stopProcedure`; ignores `clientGone`, so the RO keeps its state when emerald-server disconnects), `remote_ro` (client). `ro_server` and `remote_ro` live in `RO/src/remote_ro/` (included as `"remote_ro/ro_server.h"`). CMake: `ro_control` lib built only when `ro_config_<DEVICE_CONFIG>.cpp` exists (ro: ro + ro_server + procedures, host_pc: remote_ro only, so no second controller can run; links `scheduler`), linked into `main_<DEVICE_CONFIG>`
  - `RO/src/procedures.h/.cpp` (built into `ro_control` for the ro config only): class `ro_procedure(ro&, scheduler&)`, one instance owned by `ro`. `run(procedure_types)` (one at a time, false if `isRunning()`) switches to the private `low_ph_cip_cycle` / `high_ph_cip_cycle` / `media_backwash_cycle`, which schedule every step up front as ONCE tasks and record their ids in `task_ids_`/`condition_ids_`: timed steps on `afterDuration` conditions with cumulative times (each counts from the call); CIP cycles then do a final CIP_FLUSH until `cip_tank_low_float` dry, CIP_REFILL until `cip_tank_high_float` submerged (OFF also needs state == CIP_REFILL, since the tank starts full); no heater tasks (`ro::setState` sets `heater_active_` from `HeaterFor`) (high pH: same cycle, HIGH_PH_DOSE first). Media backwash: MEDIA_BACKWASH_1, MEDIA_BACKWASH_2 (`MEDIA_BACKWASH_TIME_MIN` each), OFF. `isRunning()` = a scheduled task hasn't run (false before the first run and after `stop()`); `stop()` and the destructor remove its tasks and conditions but leave the RO in its current state; not copyable. Changing the RO state mid-cycle doesn't stop it. Step times are `#define`s in procedures.h (`-D` overrides, CIP times are placeholders). procedures.h forward-declares `ro` (ro.h includes it, `procedure_` is a value member)
- `scheduler/` - `scheduler` (`include/scheduler.h`, `src/scheduler.cpp`), independent of the API, CMake lib `scheduler` (every config, linked into every `main_*`). One instance per machine, owned by `main_*` (a global, so it outlives the `ro` static), which calls `sched.update()` every loop pass and passes it to `addRO`; `ro` and `ro_procedure` only hold references. Tasks (`bool()` function + condition ids, all must hold; `task_mode::ONCE` or `ON_RISING`) and shared conditions (`bool(now)`; `condition::when`, `after`, `between`, `afterDuration` (since `created_time`), system clock). Single threaded: `update()` from the main loop runs due tasks; failed tasks aren't retried
- `user_space/src/` - programs that use the public header. `main_<config>.cpp` is only built with that config (`main_uppertank`: attach + device server; `main_ro`: attach + device server + global `scheduler sched` + RO controller (`addRO(server, sched)`, `sched.update()` + `updateRO()` each pass); `main_host_pc`: web UI over the remote devices + ntfy VFD alarm notifications)
- `webUI/src/web_ui` - minimal HTTP server (port 8080), outside the API: uses only `devices.h`. Built only with `-DDEVICE_CONFIG=host_pc` (target `web_ui`), never on the RevPis; no RevPi config has web UI code. Single threaded, only handles requests inside `poll()`. Lists the devices the program adds (local, remote or WiseConn) with values; soil sensors get one read-only row per depth for moisture/ETC/temp (depth labels passed to `addDevice`); password field; switch for pump run / valve open, number box + Send for pump speed; `state_selector` (interface in web_ui.h: `states`, `readState`, `setState`, `readSettling`, `procedures`, `runProcedure`, `stopProcedure`) = current state + dropdown + Set (browser confirm, `<name> setState <STATE>`, only listed names) + settling row + procedure dropdown + Run (browser confirm, `<name> runProcedure <PROCEDURE>`, only listed names) + Stop (browser confirm, `<name> stopProcedure`; the UI doesn't show whether one is running); `main_host_pc` adds `ro` (adapter `ro_state_selector` over ro_config.h) as the first row. `GET /`, `GET /api/devices` (JSON), `POST /api/command` (body = device protocol line, commands only). Page is embedded in web_ui.cpp and polls every 1 s
  - `main_host_pc` creates `web_ui ui(WEB_UI_PORT, "0.0.0.0")` (LAN access), adds every device in `addAllDevices()` from the config.h references, calls `ui.start()` then `ui.poll(20)` in its loop
  - Password: first line of `webui_password.txt` (repo root, git-ignored), read by CMake into `build/generated/web_ui/web_ui_password.h` (only web_ui.cpp sees it; hardcoded at build time; editing the file re-runs cmake). `/api/*` needs it URL-encoded in `X-Password` (401 if wrong, then that IP gets 429 for 1 s); `GET /` is open. No password built in (missing file, or plain g++ build): `start()` fails. Plain HTTP, so still firewall port 8080 to the LAN
  - Each page refresh does one read per device signal; for remote devices that blocks the loop ~1 ms per read, up to 300 ms per read if the owner is down. Reading a flow sensor from the UI shortens the averaging window the control code sees
- `notifications/` - ntfy push notifications (`notifications/src/ntfy.h`: `ntfy_notifier`, `ntfy_send` CLI), independent of `API/`. Built only with `-DDEVICE_CONFIG=host_pc`, never on the RevPis. Topic from `ntfy_topic.txt` (repo root, git-ignored, line 1 topic, optional line 2 server), hardcoded at build time into `build/generated/ntfy_config.h`; missing = notifications off. `send()` queues, a background thread posts with the `curl` program (no libcurl). `main_host_pc` reports the feed/RO pump VFD alarms (10 s on to notify, 10 s off to clear) and the lights relay switching on, see `notifications/README.md`
- `Math Notes/` - personal notes, do not read unless asked

## Hardware

- RevPi Connect 4 at `192.168.1.20`, user `pi` (password: ask the user), Debian 12 Bookworm, aarch64
- Static IPs (set 2026-11-01 with NetworkManager on eth0, "Wired connection 1": `ipv4.method manual`, gateway and DNS 192.168.1.1): RO RevPi `192.168.1.20` (was DHCP .116), upper tank RevPi `192.168.1.21` (hostname RevPi200606, was DHCP .195 then .72). Change with `nmcli con mod "Wired connection 1" ipv4.addresses <ip>/24`, then `config_host_pc.cpp` and the CSV. The upper tank sits behind a bridge that answers ARP for every address, so check for conflicts from the RO RevPi's side
- Modules in PiCtory: 2x DIO (pos 30, 31, left of the Connect; 31 is the one next to it), 4x AIO (pos 32-35, right of the Connect)
- Real loads are wired to the relay board: never switch relays without asking
- KinCony E16v3 16-relay board on the Connect 4 RS485 port via PiCtory "ModbusRTU Master"
  - Slave ID 10, 38400 8N1
  - Action 1: FC15 write coils, Register Address 1 (PiCtory is 1-based), qty 16, 100 ms, `Output_Bit_1..16` -> relays 1-16
  - Action 2: FC01 read coils, qty 16, 200 ms, `Input_Bit_1..16` = relay readback
  - `Modbus_Action_Status_1` = 0 means OK
  - Board has no comm timeout: relays hold last state if RS485 drops
  - After saving in PiCtory, `sudo systemctl restart pimodbus-master` (it can come up stale after a save)
    - Stale symptom (seen 2026-11-01): `Output_Bit_N` sets and `Modbus_Action_Status_1/2` stay 0, but `Input_Bit_N` (board readback) never follows and relays don't move. Restart `pimodbus-master` then `farm-ro`

## electrical_interface

- Interface types: `electrical_interface_type` enum + matching structs + `interface_value` variant, all in the same order (static_assert checks the count)
- Pins are PiCtory variable names, e.g. `I_1`, `O_1`, `Counter_1`, `PWM_1`, `InputValue_1`, `OutputValue_1`, `Output_Bit_1`
- `attach()` resolves the name and rejects a size mismatch; `getHardwareState()` / `setHardwareState()` return false on failure; set is refused for input types
- Process image types: DIO bits = 1 bit, DIO `Counter_N` = uint32, DIO `PWM_N` = uint8 0-100, AIO `InputValue`/`OutputValue` = int16 (mV / uA)
- Analog struct values are in V / mA; conversion assumes PiCtory AIO multiplier = divisor = 1
- DIO PWM frequency is per module, not per channel

## Device behaviour and PiCtory setup still needed

- Polarity (add an "inverted" option if wiring differs): float input on = `readSubmerged` true = submerged (confirmed 2026-11-03), off/false = dry; valve output on = open (normally closed valve); pump alarm input on = alarm (a normally-closed Danfoss alarm relay would need inverting)
- `local_pump::readAlarm()` is fail-safe: false if no alarm fitted, true if the alarm input can't be read or is the wrong type
- `setSpeed()` rejects values outside 0-1 (no clamping); analog reads are not clamped, so unwired 0-10 V inputs can read slightly negative (e.g. -0.2 ppm)
- Flow: first `readFlow()` returns false (no previous sample); a counter reset makes that one read return false
- PiCtory, before real use: DIO inputs for flow sensors must be in counter mode (`InputMode_N`); AIO input ranges must match each sensor (all currently ±10 V); AIO output ranges must be set for VFD speed (currently 0 = off)
- RO RevPi (192.168.1.20, hostname RevPi212936): DIO pos 31 = unsuffixed `I_N` (floats `I_1..9`, VFD alarms `I_10..11`), DIO pos 30 = `_i03` (flow counters `Counter_1..3_i03`, already `InputMode` = 1 counter). On 2026-10-31 floats `I_1..5,8,9` read on (submerged), `I_6,7` off (dry); polarity on = submerged confirmed 2026-11-03

## Remote devices (two RevPis)

- Setup on each RevPi before they can talk:
  1. Open port 47100 only to the other RevPi (the protocol has no authentication):
     `sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="<other RevPi IP>" port port="47100" protocol="tcp" accept'`
     then `sudo firewall-cmd --reload`
  2. Build with `-pthread` (remote_link runs a heartbeat thread)
- Main loop on each RevPi:
  ```cpp
  attachAllInterfaces();
  line_server server;              // comms/src/line_server.h, port 47100, 20 s watchdog
  addLocalDevices(server);
  server.start();
  while (true) {
      // control logic
      server.poll(20);             // handles remote requests, doubles as the loop sleep
  }
  ```
- Using a device owned by the other RevPi, in that machine's config:
  ```cpp
  remote_link revpi_b("192.168.1.117");
  remote_pump remote_transfer_pump(revpi_b, "transfer_pump");   // name the owner registered in its device_server
  pump& transfer_pump = remote_transfer_pump;
  ```
  Device names must match on both RevPis (no discovery yet)
- Watchdog safe state is fixed: pumps and switched loads off, valves closed. Make it configurable per device if a valve must fail open
- If both RevPis read the same flow sensor, each sees flow since whichever read came last (owner averages between reads)
- Remote call cost: ~20 us on the same machine, expect ~1 ms between RevPis. Owner unreachable: calls fail within the 300 ms connect timeout, then instantly during the 1 s backoff
- Tested: 25/25 end-to-end on the PC (owner in a separate process, fake hardware); on the RevPi with real hardware over loopback (relay 1 switched remotely, watchdog turned it off after disconnect). Not yet tested between two physical RevPis
- Gotcha fixed once: don't pass a value and the call that fills it in the same expression (`f(read(x), x)`); argument evaluation order is unspecified, GCC passed the stale value

## Build and test

- CMake (top-level `CMakeLists.txt`): `cmake -B build -DDEVICE_CONFIG=<test|uppertank|ro|host_pc> && cmake --build build`. Builds the `farm_api` library (API/src minus the configs, plus the chosen config) and one executable per `user_space/src/*.cpp` (`main_<other config>.cpp` skipped)
- Builds natively on the RevPi (g++ 12, C++17) and on the Ubuntu PC (all configs compile there; piControl just fails to open at runtime); no cross compiler
- Test copy lives in `~/ro_api_test` on the RevPi:
  (`API/` mirrored, test programs in `tests/`):
  `g++ -std=c++17 -pthread -Wall -Wextra -IAPI/include -IAPI/src -Icomms/src tests/<test>.cpp $(find API/src comms/src -name "*.cpp" -not -path "*/config/*" -not -path "*/wiseconn_devices/*") API/src/config/config_test.cpp -o <test>` (exactly one config, or symbols are defined twice; `comms/` must be mirrored too since 2026-11-02, and test programs that called `startDeviceServer`/`serviceDeviceServer` need the `line_server` loop above)
- Both RevPis run their program as a systemd service: `farm-uppertank.service` (192.168.1.21) and `farm-ro.service` (192.168.1.20, After=pimodbus-master). User `pi` (in group `picontrol`), runs `~/Rev-Pi-Farm-Control-System/build/main_<config>`, Restart=always every 2 s, enabled at boot. After rebuilding: `sudo systemctl restart farm-<config>`; logs: `journalctl -u farm-<config> -f`; stop it before running `main_*` by hand (port 47100 is taken)
- Host PC web UI runs on `emerald-server.local` (192.168.1.194) as `rev-pi-control.service`, user `emerald` (password: ask the user). Source in `~/tools/Rev-Pi-Farm-Control-System` (`host_pc` config, has `webui_password.txt`); the service runs a copy at `~/tools/bin/Rev-Pi-Control/main_host_pc`, so after rebuilding copy the binary there, then `sudo systemctl restart rev-pi-control`
- Port 47100 is open on both RevPis to 192.168.1.95 (Ubuntu dev PC) and 192.168.1.194 (emerald-server)
- firewalld is active on the RevPi: port 47100/tcp must be opened (only for the other RevPi's IP) before RevPis can reach each other, see Remote devices
- Tests in `~/ro_api_test/tests`: `config_test` + `other` (read-only), `pump_test` and `sensors_hw_test` (switch relay 1), `owner_main` + `controller_main` (remote over loopback, switch relay 1). PC-side logic tests use a fake `electrical_interface` (attach/get/set just succeed) to feed exact signals
- Controller code needs only `-IAPI/include` (`#include "config.h"` / `"devices.h"`), plus `-Icomms/src` if it owns the `line_server`, then links against the API and comms objects (built with `-IAPI/include -IAPI/src -Icomms/src`)
- Inspect I/O with `piTest -r <var>`, `piTest -w <var>,<value>`, `piTest -v <var>`
- Don't switch relays or outputs that may have loads wired without asking first

## Style

- snake_case types, camelCase methods, trailing underscore on members
- `#pragma once`, 4-space indent
