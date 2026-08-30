#ifndef OS_MINE_H
#define OS_MINE_H

// =====================================================
// ghost-mine-os — Mine-Geschwister
// Spec-Basis: §4, §11, §22, §38
// Send-only. Prozesse / Dateien lösen MinePayload aus.
// =====================================================

#include "mine.h"

class OsMine {
public:
    OsMine();

    void init(const char* mineId);
    bool setTotpSeed(const uint8_t* seed, uint8_t len);
    bool onSuspiciousProcess(MinePayload* out, uint32_t now);
    bool onSuspiciousFile(MinePayload* out, uint32_t now);
    bool send(MinePayload* out, uint32_t now);
    void freezeEvents();
    bool recv(MinePayload* out) const;
    bool canReceive() const;

private:
    Mine mine_;
};

#endif
