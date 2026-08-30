#ifndef NAS_HONEYPOT_H
#define NAS_HONEYPOT_H

// =====================================================
// ghost-mine-nas — Honigtopf auf der NAS
// Spec-Basis: §11, §38
// Nur bei Zugriff auf Lockvogel-Shares. Nie auf der PSP.
// =====================================================

#include "mine.h"

class NasHoneypot {
public:
    NasHoneypot();

    void init(const char* mineId);
    bool setTotpSeed(const uint8_t* seed, uint8_t len);
    bool onLockvogel(MinePayload* out, uint32_t now);
    bool send(MinePayload* out, uint32_t now);
    void freezeEvents();
    bool recv(MinePayload* out) const;
    bool canReceive() const;
    bool canAlert() const;

private:
    Mine mine_;
};

#endif
