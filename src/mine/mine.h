#ifndef MINE_H
#define MINE_H

// =====================================================
// Ghost Hive v1.7.1 — Mine 1.0
// Spec-Basis: §4, §15, §22, §25, §33
// Send-only. Counter monoton. TOTP lokal.
// =====================================================

#include "ghost_core.h"
#include "ghost_crypto.h"

const uint8_t MINE_VERSION_MAJOR = 1;
const uint8_t MINE_VERSION_MINOR = 0;
// §44 TOTP-Fenster 60–120s — nicht schneller. Mehr Sicht = mehr Minen-IDs, nicht kürzeres Tick.
const uint32_t MINE_INTERVAL_SEC = 90;
const uint32_t MINE_LOWPOWER_SEC = 90;
const uint8_t HOST_MINE_SLOTS = 8;

bool mine_copy_id(char* dst, const char* src);
bool mine_arg_id(const char* arg, char* dst);
bool mine_push_id(char ids[][32], uint8_t* count, uint8_t max, const char* id);

class Mine {
public:
    Mine();

    void init(const char* mineId);
    void setId(const char* id);
    bool setTotpSeed(const uint8_t* seed, uint8_t len);
    bool send(MinePayload* out, uint32_t now);
    bool sendTrip(MinePayload* out, uint32_t now);
    bool craftPayload(MinePayload& payload, uint32_t now);
    bool craftPayload(MinePayload& payload, uint32_t now, bool trip);

    bool canReceive() const;
    void freezeEvents();
    bool eventsFrozen() const;
    void setLowPower(bool on);
    bool lowPower() const;
    void setPeekAllowed(bool on);
    bool peekAllowed() const;
    uint32_t intervalSec() const;
    uint32_t counter() const;
    bool hasTotpSeed() const;

    uint8_t versionMajor() const;
    uint8_t versionMinor() const;

private:
    char id_[32];
    uint32_t counter_;
    uint8_t seed_[TOTP_SEED_LEN];
    bool hasSeed_;
    bool lowPower_;
    bool peekAllowed_;
    bool freezeEvents_;
};

#endif // MINE_H
