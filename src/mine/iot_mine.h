#ifndef IOT_MINE_H
#define IOT_MINE_H

// =====================================================
// ghost-mine-iot — Mine-Geschwister
// Spec-Basis: §4, §11, §22, §38
// =====================================================

#include "mine.h"

class IotMine {
public:
    IotMine();

    void init(const char* mineId);
    bool setTotpSeed(const uint8_t* seed, uint8_t len);
    bool send(MinePayload* out, uint32_t now);
    void freezeEvents();
    bool recv(MinePayload* out) const;
    bool canReceive() const;

private:
    Mine mine_;
};

#endif
