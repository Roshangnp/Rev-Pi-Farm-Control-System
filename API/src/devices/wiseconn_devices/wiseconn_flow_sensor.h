#pragma once
#include "devices.h"
#include "wiseconn_keys.h"
#include "wiseconn_link.h"

// Flow meter read from the WiseConn cloud: the latest value of one "Flow" measure
// (gal/min), from the link's cache (its zone polled every poll interval; WiseConn
// updates these about every 5 min). A read fails unless the value is no older
// than the link's max age. update() is the base class's no-op: WiseConn computes
// the flow. Read only.
class wiseconn_flow_sensor : public flow_sensor{
    public:
    // ids: the line in wiseconn_keys.txt [flows]. Registers its zone with the link.
    wiseconn_flow_sensor(wiseconn_link& link, const wiseconn_flow_ids& ids);

    bool readFlow(float& gpm) override;

    private:
    wiseconn_link& link_;
    wiseconn_flow_ids ids_;
};
