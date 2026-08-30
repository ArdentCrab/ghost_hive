#ifndef SAFE_TRANSPORT_H
#define SAFE_TRANSPORT_H

// =====================================================
// Safe-Link 1.0 — bidirektional, alarmiert nie
// Spec-Basis: §7, §18, §20, §28
// =====================================================

#include "transport_bidi.h"

class SafeTransport {
public:
    SafeTransport();

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
