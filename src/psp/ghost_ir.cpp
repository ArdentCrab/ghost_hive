#include "ghost_ir.h"

GhostIR::GhostIR() {
    init();
}

void GhostIR::init() {
    irReady_ = false;
    /* PSPSDK: sceSircsSend only. Spec §13 ir_rx, never TX. */
}

void GhostIR::scan() {
    uint8_t cmd = 0;
    (void)takeRx(&cmd);
}

bool GhostIR::sendSignal(uint8_t signalId) {
    // §13 ir_rx: nie TX. SIRCS-Send wäre ein Honeypot-Pfad.
    (void)signalId;
    return false;
}

bool GhostIR::takeRx(uint8_t* cmd) {
    if (cmd == nullptr) return false;
    *cmd = 0;
    return false;
}

bool GhostIR::ready() const {
    return irReady_;
}
