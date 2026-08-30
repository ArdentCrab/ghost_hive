#include "mine.h"

Mine::Mine() : counter_(0), hasSeed_(false), lowPower_(false), peekAllowed_(false), freezeEvents_(false) {
    id_[0] = '\0';
    for (uint8_t i = 0; i < TOTP_SEED_LEN; ++i) seed_[i] = 0;
}

void Mine::init(const char* mineId) {
    setId(mineId);
}

void Mine::setId(const char* id) {
    uint8_t i = 0;
    if (id == nullptr) {
        id_[0] = '\0';
        return;
    }
    while (id[i] != '\0' && i < 31) {
        id_[i] = id[i];
        ++i;
    }
    id_[i] = '\0';
}

bool Mine::setTotpSeed(const uint8_t* seed, uint8_t len) {
    if (seed == nullptr || len == 0 || len > TOTP_SEED_LEN) return false;
    for (uint8_t i = 0; i < TOTP_SEED_LEN; ++i) seed_[i] = 0;
    for (uint8_t i = 0; i < len; ++i) seed_[i] = seed[i];
    hasSeed_ = true;
    return true;
}

bool Mine::send(MinePayload* out, uint32_t now) {
    if (out == nullptr) return false;
    return craftPayload(*out, now, false);
}

bool Mine::sendTrip(MinePayload* out, uint32_t now) {
    if (out == nullptr) return false;
    return craftPayload(*out, now, true);
}

bool Mine::craftPayload(MinePayload& payload, uint32_t now) {
    return craftPayload(payload, now, false);
}

bool Mine::craftPayload(MinePayload& payload, uint32_t now, bool trip) {
    if (freezeEvents_) return false;
    if (id_[0] == '\0' || !hasSeed_) return false;

    ++counter_;
    uint8_t i = 0;
    while (id_[i] != '\0' && i < 31) {
        payload.mine_id[i] = id_[i];
        ++i;
    }
    payload.mine_id[i] = '\0';
    payload.counter = counter_;
    payload.totp = ghost_totp(seed_, TOTP_SEED_LEN, now);
    if (trip) {
        payload.event = EventType::AnomalyDetected;
    } else {
        payload.event = peekAllowed_ ? EventType::PeekScan : EventType::MineEvent;
    }
    payload.timestamp = now;
    payload.hash[0] = '\0';
    return true;
}

bool Mine::canReceive() const {
    return false;
}

void Mine::freezeEvents() {
    freezeEvents_ = true;
}

bool Mine::eventsFrozen() const {
    return freezeEvents_;
}

void Mine::setLowPower(bool on) {
    lowPower_ = on;
}

bool Mine::lowPower() const {
    return lowPower_;
}

void Mine::setPeekAllowed(bool on) {
    peekAllowed_ = on;
}

bool Mine::peekAllowed() const {
    return peekAllowed_;
}

uint32_t Mine::intervalSec() const {
    return lowPower_ ? MINE_LOWPOWER_SEC : MINE_INTERVAL_SEC;
}

uint32_t Mine::counter() const {
    return counter_;
}

bool Mine::hasTotpSeed() const {
    return hasSeed_;
}

uint8_t Mine::versionMajor() const { return MINE_VERSION_MAJOR; }
uint8_t Mine::versionMinor() const { return MINE_VERSION_MINOR; }

bool mine_copy_id(char* dst, const char* src) {
    if (dst == nullptr) return false;
    uint8_t i = 0;
    if (src == nullptr || src[0] == '\0') {
        dst[0] = '\0';
        return false;
    }
    while (src[i] != '\0' && i < 31) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
    return dst[0] != '\0';
}

bool mine_arg_id(const char* arg, char* dst) {
    if (arg == nullptr || dst == nullptr) return false;
    if (arg[0] == 'm' && arg[1] == 'i' && arg[2] == 'n' && arg[3] == 'e' &&
        arg[4] == '=') {
        return mine_copy_id(dst, arg + 5);
    }
    if (arg[0] == 'i' && arg[1] == 'd' && arg[2] == '=') {
        return mine_copy_id(dst, arg + 3);
    }
    return false;
}

bool mine_push_id(char ids[][32], uint8_t* count, uint8_t max, const char* id) {
    if (ids == nullptr || count == nullptr || id == nullptr || id[0] == '\0') {
        return false;
    }
    if (*count >= max) return false;
    for (uint8_t i = 0; i < *count; ++i) {
        uint8_t j = 0;
        bool same = true;
        while (j < 32) {
            if (ids[i][j] != id[j]) {
                same = false;
                break;
            }
            if (id[j] == '\0') break;
            ++j;
        }
        if (same) return true;
    }
    mine_copy_id(ids[*count], id);
    ++(*count);
    return true;
}
