#ifndef WORKER_TRANSPORT_H
#define WORKER_TRANSPORT_H

// =====================================================
// Worker-Link 1.0 — bidirektional, blockiert nicht
// Spec-Basis: §3, §7, §18, §22
// =====================================================

#include "transport_bidi.h"

class WorkerTransport {
public:
    WorkerTransport();

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
