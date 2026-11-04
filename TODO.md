# TODO

1. Update CIP cleaning times
2. Add heater bang-bang controller
3. Add pump PID controller
4. Add something to make sure all relays are off on start up of a revpi (The ro_state::OFF assertion does this but it needs to be done at a lower level too), so in main_ro.cpp, maybe add a helper to set all relays to off.
5. add a new procedure (new procedure instance, each instance can have one procedure at a time) that is a watchdog. If a vfd alarm trips, or if pressure or flow goes outside of its range
given by pressuresFor() and flowsFor. The task would just destroy reset the other procedure and set state to OFF.