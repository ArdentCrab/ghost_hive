#ifndef TETACT_H
#define TETACT_H

// =====================================================
// TETACT — §46
// Peer-Awareness. Kein PSP-Modul. Entscheidet nie.
// =====================================================

#include "ghost_core.h"

const uint32_t TETACT_INTERVAL_SEC = 60;
const uint32_t TETACT_TIME_WARP_SEC = 300;

enum TetactKind : uint8_t {
    TETACT_NONE = 0,
    TETACT_SCAN = 1,
    TETACT_SEEN = 2,
    TETACT_TAMPER = 3
};

struct TetactState {
    uint32_t lastNow;
    uint16_t arpCount;
    uint16_t lastArpCount;
    uint32_t arpHash;
    uint8_t scanPhase;
    bool ready;
};

void tetact_init(TetactState& st);
void tetact_set_source(Event* out, const char* id);
TetactKind tetact_watch(TetactState& st, uint32_t now, Event* out);
TetactKind tetact_poll(TetactState& st, uint32_t now, Event* out);

#endif
