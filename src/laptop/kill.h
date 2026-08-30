#ifndef HIVE_KILL_H
#define HIVE_KILL_H

// =====================================================
// hive-kill — Laptop Noah
// Spec-Basis: §7, §11, §33, §43
// Signiert Kill. Schreibt kein kill.flag. Ohne PSP-Root kein Kill.
// =====================================================

#include "ghost_core.h"
#include "ghost_keys.h"

class HiveKill {
public:
    HiveKill();

    void attach(GhostKeys* keys);
    bool fill(Event* out, const char* sourceId, uint32_t now) const;
    bool fill(Event* out, const char* sourceId, uint32_t now,
              uint32_t snapshotRef) const;
    bool sign(Event& event) const;
    bool canWriteFlag() const;

private:
    GhostKeys* keys_;
};

#endif
