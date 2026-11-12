#pragma once

class device_server;

// Adds every WiseConn device bound in wiseconn_farm_devices.cpp to devices,
// under its config.h name, so the machine serves them like its other devices.
// Called by each config's addLocalDevices. False if a name is already taken.
bool addWiseconnDevices(device_server& devices);
