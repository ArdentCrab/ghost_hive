#ifndef TRANSPORT_BIDI_H
#define TRANSPORT_BIDI_H

// =====================================================
// Bidirektionaler Peer-Link (§18 ACK, §22, §31)
// Worker / Sensor / Safe / Phone / Router.
// =====================================================

#include "transport_frame.h"
#include "medium_wlan.h"
#include "medium_ir.h"

class TransportBidi {
public:
    TransportBidi();

    void init(const char* deviceId, uint8_t role);
    void attach(MediumWlan* wlan, MediumIr* ir);

    bool send(const Event& event, uint32_t now);
    bool poll(Event& out);
    void tick(uint32_t now);
    bool lastAcked() const;
    bool recvAllowed() const;

private:
    char id_[32];
    uint8_t role_;
    MediumWlan* wlan_;
    MediumIr* ir_;
    Event pending_;
    uint32_t pending_at_;
    uint8_t retries_;
    bool pending_used_;
    bool last_acked_;

    bool pushKernel(const TransportFrame& frame);
};

#endif
