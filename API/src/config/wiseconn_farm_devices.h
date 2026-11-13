#pragma once

class device_server;

// Adds every WiseConn device bound in wiseconn_farm_devices.cpp to devices,
// under its config.h name, so the machine serves them like its other devices.
// Called by config_host_pc's addLocalDevices (the owner). False if a name is already taken.
bool addWiseconnDevices(device_server& devices);
