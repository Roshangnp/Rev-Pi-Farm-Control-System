# notifications

Push notifications to a phone through [ntfy](https://ntfy.sh). Host PC only: it is built
with `-DDEVICE_CONFIG=host_pc` and never on the RevPis. Independent of `API/`.

## Setup

1. Put a long random topic name on the first line of `ntfy_topic.txt` in the repo root
   (git-ignored). Anyone who knows the topic can read and post to it. An optional second
   line sets the server (default `https://ntfy.sh`).
2. Rebuild: CMake hardcodes the topic into `build/generated/ntfy_config.h`.
3. On the phone, install the ntfy app and subscribe to the same topic.
4. Test: `./build/ntfy_send -p 4 -t warning "Test" "Hello from the farm"`

No topic: everything still builds and runs, notifications are just off.

## ntfy_notifier (`src/ntfy.h`)

```cpp
ntfy_notifier notifier(NTFY_TOPIC, NTFY_SERVER);
notifier.send("RO pump VFD alarm", "The drive has tripped.", 5, "rotating_light");
```

- `send()` only queues (max 50) and returns at once. A background thread posts each message
  as JSON with the `curl` program (HTTPS, 10 s timeout, 3 tries 5 s and 30 s apart). A slow
  or missing internet connection never blocks the caller; failures go to stderr.
- Priority 1-5 (5 bypasses Do Not Disturb). Tags are comma separated; emoji names such as
  `warning` show as icons.
- Needs `curl` installed (no libcurl).

## Alerts in main_host_pc

`user_space/src/main_host_pc.cpp` only reports the feed pump and RO pump VFD alarms. It checks
every 5 s; an alarm on for 10 s sends a priority 5 notification (bypasses Do Not Disturb), and
off again for 10 s sends "Cleared". `readAlarm()` also returns true when the RO RevPi can't be
reached, so the alarm only counts when a `readOn()` before and after it both succeed: an
outage sends nothing.

It also sends "LIGHTS RELAY SWITCHED" (priority 3) once each time the lights relay goes from
off to on, checked in the same 5 s loop (an on shorter than 5 s can be missed).
