#ifndef GHOST_IR_H
#define GHOST_IR_H

// =====================================================
// Ghost Hive v1.7.1
// IR Toolkit — P2 FINAL
// Spec-Basis: §12.2, §13 ir_rx
// PSP-IR ist RX-only. Kein SIRCS-TX, kein Honeypot.
// =====================================================

#include "ghost_core.h"

class GhostIR {
public:
    GhostIR();

    void init();
    void scan();
    bool sendSignal(uint8_t signalId);
    bool takeRx(uint8_t* cmd);
    bool ready() const;

private:
    bool irReady_;
};

#endif // GHOST_IR_H
