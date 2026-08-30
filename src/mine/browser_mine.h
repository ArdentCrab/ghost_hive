#ifndef BROWSER_MINE_H
#define BROWSER_MINE_H

// =====================================================
// ghost-mine-browser — Mine-Geschwister
// Spec-Basis: §4, §11, §22, §38
// Send-only. Gleiche MinePayload. Nie recv().
// =====================================================

#include "mine.h"

class BrowserMine {
public:
    BrowserMine();

    void init(const char* mineId);
    bool setTotpSeed(const uint8_t* seed, uint8_t len);
    bool onSuspiciousUrl(MinePayload* out, uint32_t now);
    bool send(MinePayload* out, uint32_t now);
    void freezeEvents();
    bool recv(MinePayload* out) const;
    bool canReceive() const;

private:
    Mine mine_;
};

#endif
