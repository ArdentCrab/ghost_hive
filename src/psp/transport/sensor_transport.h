#ifndef SENSOR_TRANSPORT_H
#define SENSOR_TRANSPORT_H

// =====================================================
// Sensor-Link 1.0 — bidirektional, schreibt nicht
// Spec-Basis: §2.2, §7, §18, §22
// =====================================================

#include "transport_bidi.h"

class SensorTransport {
public:
    SensorTransport();

    void init(const char* deviceId);
    void attach(MediumWlan* wlan, MediumIr* ir);
    bool send(const Event& event, uint32_t now);
    bool poll(Event& out);
    void tick(uint32_t now);
    bool lastAcked() const;

private:
    TransportBidi link_;
};

#endif
