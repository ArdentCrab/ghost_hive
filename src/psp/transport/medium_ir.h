#ifndef MEDIUM_IR_H
#define MEDIUM_IR_H

// =====================================================
// Ghost Hive v1.7.1 — IR-Medium Stub
// Spec-Basis: §12.2, §13 ir_tx/ir_rx
// Verdrahtet GhostIR RX. Kein SIRCS-TX.
// =====================================================

#include "transport_frame.h"

class GhostIR;

class MediumIr {
public:
    MediumIr();

    void init();
    void attach(GhostIR* ir);
    bool toKernel(const TransportFrame& frame);
    bool fromKernel(TransportFrame& frame);

private:
    GhostIR* ir_;
    TransportFrame rx_[TRANSPORT_IR_SLOTS];
    uint8_t head_;
    uint8_t tail_;
    uint8_t count_;
};

#endif
