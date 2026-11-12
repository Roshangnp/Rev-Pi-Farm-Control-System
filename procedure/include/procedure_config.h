#pragma once

#include "procedure.h"

// Every farm procedure instance, like config.h for the devices: one owner per
// instance (the main_* that runs it, on its scheduler, and serves it with a
// procedure_server under the same name); every other program on every machine
// binds it to a remote_procedure on its connection to the owner
// (procedure/src/config/procedure_remote_config.cpp). A program on a machine
// that doesn't bind an instance fails to link.

// The RO procedures (CIP cycles, media backwash, empty feed tank; only those
// types). Owner: main_ro (RO RevPi), served as "ro_procedure"; main_ro sets the
// RO OFF when it stops running (stopped or finished)
extern procedure_control& ro_procedure;
