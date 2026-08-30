#ifndef MINE_TRANSPORT_H
#define MINE_TRANSPORT_H

// =====================================================
// Mine-Link 1.0 — send-only, kein RX, kein ACK
// Spec-Basis: §4, §15, §22, §44
// =====================================================

#include "transport_frame.h"
#include "medium_wlan.h"
#include "medium_ir.h"

class MineTransport {
public:
    MineTransport();

    void init(const char* mineId);
    void attach(MediumWlan* wlan, MediumIr* ir);
    bool send(const MinePayload& payload, uint32_t now);
    bool recv(MinePayload& payload);

private:
    char id_[32];
    MediumWlan* wlan_;
    MediumIr* ir_;
};

#endif
