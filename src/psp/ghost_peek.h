#ifndef GHOST_PEEK_H
#define GHOST_PEEK_H

// =====================================================
// Ghost Hive v1.7.1
// Ghost Peek
// Spec-Basis: §12.7, §41
// Kein Kaltstart, low_power_mode
// =====================================================

#include "ghost_core.h"
#include "ghost_data.h"
#include "replay_guard.h"

class GhostTransport;

const uint8_t MAX_MINES = 8;

class GhostPeek {
public:
    GhostPeek();

    void init();
    void perform();
    void perform(ReplayGuard& guard);
    void ingestGuard(const ReplayGuard& guard);
    void ingestMine(const MinePayload& payload);
    void attachTransport(GhostTransport* transport);

    uint8_t getMineCount() const;
    const MineInfo* getMine(uint8_t index) const;
    bool coldStart() const;

private:
    MineInfo mines_[MAX_MINES];
    uint8_t mineCount_;
    GhostTransport* transport_;
};

#endif // GHOST_PEEK_H
