#ifndef ROUTER_MINE_H
#define ROUTER_MINE_H

// =====================================================
// ghost-mine-router — Honigtopf auf dem Router
// Spec-Basis: §11, §38
// Nie auf der PSP. Send-only MinePayload.
// =====================================================

#include "mine.h"

class RouterMine {
public:
    RouterMine();

    void init(const char* mineId);
    bool setTotpSeed(const uint8_t* seed, uint8_t len);
    bool onDecoyHit(MinePayload* out, uint32_t now);
    bool send(MinePayload* out, uint32_t now);
    void freezeEvents();
    bool recv(MinePayload* out) const;
    bool canReceive() const;

private:
    Mine mine_;
};

#endif
