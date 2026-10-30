# Rev-Pi-Farm-Control-System

Farm control system running on Revolution Pi Connect 4 units.

## Layout

- `API/` - hardware API, the layer between the RevPi I/O and the controllers
  - Include roots are `-IAPI/include -IAPI/src`; `API/include` headers are included by bare name (`"devices.h"`), cross-folder `API/src` includes are written relative to `API/src` (e.g. `"HAL/electronics.h"`), same-folder includes use the bare name
  - `API/include/` - what external programs (e.g. the controller) include from the API
    - `devices.h` - public device interface: abstract base classes `pump`, `float_switch`, `valve`, `pressure_transducer`, `tds_sensor`, `flow_sensor` (pure virtual, no hardware includes). Controller code uses only these, so local and remote devices are interchangeable
    - `config.h` - device counts, the devices as `extern` base-class references (`extern pump& feed_pump;`), `attachAllInterfaces()`. Includes only `devices.h`
  - `API/src/config.cpp` - the wiring: `electrical_interface`s and `local_*` devices (anonymous namespace), bound to the references declared in config.h. Swap a device to `remote_*` here without touching controller code
  - `API/src/HAL/` - hardware abstraction layer
    - `electronics.h/.cpp` - `electrical_interface`: one I/O point bound to a PiCtory variable
    - `analog_input.h/.cpp` - shared 4-20 mA / 0-10 V scaling for pressure and TDS; < 3.5 mA = broken wire
    - `piControl.h` - vendored KUNBUS driver header (MIT), do not edit
  - `API/src/devices/local_devices/local_*.h/.cpp` - devices wired to this RevPi, each inherits its base from `devices.h`
    - `local_pump`: non-owning pointers to on/off, speed and alarm interfaces; nullptr = not fitted. Speed 0-1 maps to 0-10 V, 4-20 mA or 0-100 % PWM
    - `local_float_switch`, `local_valve`, `local_pressure_transducer`, `local_tds_sensor`, `local_flow_sensor` - one interface each; reads return false on failure and fill an out param
  - `API/src/devices/remote_devices/` - devices owned by another RevPi on the LAN. One owner per device
    - `remote_link` - one TCP connection per remote RevPi (port 47100), shared by its `remote_*` devices; auto-reconnect (1 s backoff), 300 ms connect/reply timeouts, heartbeat thread pings every 0.5 s
    - `remote_*` - one proxy per base class, `(remote_link&, device_name)`; network failure returns false (`readAlarm` returns true)
    - `device_server` - on the owner; single threaded, requests only handled inside `poll()` so it never races local control code. Watchdog: if a client disconnects or is silent 2 s, pumps it last commanded go off and valves it last commanded close
    - Protocol: text lines `"<device> <method> [arg]"` -> `"ok [value]"` / `"fail"` / `"err <reason>"`; method names = C++ method names. No authentication: restrict the firewall to the other RevPi
  - `config.h` also exposes `startDeviceServer()` / `serviceDeviceServer(timeout_ms)`; config.cpp registers the local devices under their public names
- `controllers/` - control applications that use the public header
- `Math Notes/` - personal notes, do not read unless asked

## Hardware

- RevPi Connect 4 at `192.168.1.116`, user `pi` (password: ask the user), Debian 12 Bookworm, aarch64
- Modules in PiCtory: 2x DIO (pos 30, 31, left of the Connect; 31 is the one next to it), 4x AIO (pos 32-35, right of the Connect)
- Real loads are wired to the relay board: never switch relays without asking
- KinCony E16v3 16-relay board on the Connect 4 RS485 port via PiCtory "ModbusRTU Master"
  - Slave ID 10, 38400 8N1
  - Action 1: FC15 write coils, Register Address 1 (PiCtory is 1-based), qty 16, 100 ms, `Output_Bit_1..16` -> relays 1-16
  - Action 2: FC01 read coils, qty 16, 200 ms, `Input_Bit_1..16` = relay readback
  - `Modbus_Action_Status_1` = 0 means OK
  - Board has no comm timeout: relays hold last state if RS485 drops
  - After saving in PiCtory, `sudo systemctl restart pimodbus-master` (it can come up stale after a save)

## electrical_interface

- Interface types: `electrical_interface_type` enum + matching structs + `interface_value` variant, all in the same order (static_assert checks the count)
- Pins are PiCtory variable names, e.g. `I_1`, `O_1`, `Counter_1`, `PWM_1`, `InputValue_1`, `OutputValue_1`, `Output_Bit_1`
- `attach()` resolves the name and rejects a size mismatch; `getHardwareState()` / `setHardwareState()` return false on failure; set is refused for input types
- Process image types: DIO bits = 1 bit, DIO `Counter_N` = uint32, DIO `PWM_N` = uint8 0-100, AIO `InputValue`/`OutputValue` = int16 (mV / uA)
- Analog struct values are in V / mA; conversion assumes PiCtory AIO multiplier = divisor = 1
- DIO PWM frequency is per module, not per channel

## Device behaviour and PiCtory setup still needed

- Polarity assumptions (add an "inverted" option if wiring differs): float input on = submerged; valve output on = open (normally closed valve); pump alarm input on = alarm (a normally-closed Danfoss alarm relay would need inverting)
- `local_pump::readAlarm()` is fail-safe: false if no alarm fitted, true if the alarm input can't be read or is the wrong type
- `setSpeed()` rejects values outside 0-1 (no clamping); analog reads are not clamped, so unwired 0-10 V inputs can read slightly negative (e.g. -0.2 ppm)
- Flow: first `readFlow()` returns false (no previous sample); a counter reset makes that one read return false
- PiCtory, before real use: DIO inputs for flow sensors must be in counter mode (`InputMode_N`); AIO input ranges must match each sensor (all currently ±10 V); AIO output ranges must be set for VFD speed (currently 0 = off)
- On the current RevPi, DIO `I_1` and `I_2` read on with nothing known to be wired (example config uses them as feed pump alarm and feed tank float)

## Remote devices (two RevPis)

- Setup on each RevPi before they can talk:
  1. Open port 47100 only to the other RevPi (the protocol has no authentication):
     `sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="<other RevPi IP>" port port="47100" protocol="tcp" accept'`
     then `sudo firewall-cmd --reload`
  2. Build with `-pthread` (remote_link runs a heartbeat thread)
- Main loop on each RevPi:
  ```cpp
  attachAllInterfaces();
  startDeviceServer();
  while (true) {
      // control logic
      serviceDeviceServer(20);   // handles remote requests, doubles as the loop sleep
  }
  ```
- Using a device owned by the other RevPi, in config.cpp:
  ```cpp
  remote_link revpi_b("192.168.1.117");
  remote_pump remote_transfer_pump(revpi_b, "transfer_pump");   // name the owner registered in its device_server
  pump& transfer_pump = remote_transfer_pump;
  ```
  Device names must match on both RevPis (no discovery yet)
- Watchdog safe state is fixed: pumps off, valves closed. Make it configurable per device if a valve must fail open
- If both RevPis read the same flow sensor, each sees flow since whichever read came last (owner averages between reads)
- Remote call cost: ~20 us on the same machine, expect ~1 ms between RevPis. Owner unreachable: calls fail within the 300 ms connect timeout, then instantly during the 1 s backoff
- Tested: 25/25 end-to-end on the PC (owner in a separate process, fake hardware); on the RevPi with real hardware over loopback (relay 1 switched remotely, watchdog turned it off after disconnect). Not yet tested between two physical RevPis
- Gotcha fixed once: don't pass a value and the call that fills it in the same expression (`f(read(x), x)`); argument evaluation order is unspecified, GCC passed the stale value

## Build and test

- Builds on the RevPi with g++ 12 (C++17); no cross compiler on the dev PC
- Test copy lives in `~/ro_api_test` on the RevPi:
  (`API/` mirrored, test programs in `tests/`):
  `g++ -std=c++17 -pthread -Wall -Wextra -IAPI/include -IAPI/src tests/<test>.cpp $(find API/src -name "*.cpp") -o <test>`
- firewalld is active on the RevPi: port 47100/tcp must be opened (only for the other RevPi's IP) before RevPis can reach each other, see Remote devices
- Tests in `~/ro_api_test/tests`: `config_test` + `other` (read-only), `pump_test` and `sensors_hw_test` (switch relay 1), `owner_main` + `controller_main` (remote over loopback, switch relay 1). PC-side logic tests use a fake `electrical_interface` (attach/get/set just succeed) to feed exact signals
- Controller code needs only `-IAPI/include` (`#include "config.h"` / `"devices.h"`), then links against the API objects (built with `-IAPI/include -IAPI/src`)
- Inspect I/O with `piTest -r <var>`, `piTest -w <var>,<value>`, `piTest -v <var>`
- Don't switch relays or outputs that may have loads wired without asking first

## Style

- snake_case types, camelCase methods, trailing underscore on members
- `#pragma once`, 4-space indent
