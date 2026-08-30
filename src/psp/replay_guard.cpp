#include "replay_guard.h"

ReplayGuard::ReplayGuard() {
    init();
}

void ReplayGuard::init() {
    windowCount_ = 0;
    for (uint8_t i = 0; i < MAX_TRACKED_MINES; ++i) {
        windows_[i].mine_id[0] = '\0';
        windows_[i].index = 0;
        windows_[i].last_timestamp = 0;
        windows_[i].last_counter = 0;
        windows_[i].has_seed = false;
        windows_[i].blocked = false;
        for (uint8_t j = 0; j < REPLAY_WINDOW_PER_MINE; ++j) {
            windows_[i].counters[j] = 0;
        }
        for (uint8_t j = 0; j < TOTP_SEED_LEN; ++j) {
            windows_[i].totp_seed[j] = 0;
        }
    }
}

bool ReplayGuard::sameId(const char* a, const char* b) {
    for (uint8_t j = 0; j < 32; ++j) {
        if (a[j] != b[j]) return false;
        if (a[j] == '\0') return true;
    }
    return true;
}

int16_t ReplayGuard::findWindow(const char* mineId) const {
    for (uint8_t i = 0; i < windowCount_; ++i) {
        if (sameId(windows_[i].mine_id, mineId)) return static_cast<int16_t>(i);
    }
    return -1;
}

bool ReplayGuard::hashOk(const MinePayload& payload) {
    if (payload.hash[0] == '\0') return true;

    uint8_t raw[48];
    for (uint8_t i = 0; i < 48; ++i) raw[i] = 0;
    for (uint8_t i = 0; i < 32; ++i) raw[i] = static_cast<uint8_t>(payload.mine_id[i]);
    raw[32] = static_cast<uint8_t>(payload.counter);
    raw[33] = static_cast<uint8_t>(payload.counter >> 8);
    raw[34] = static_cast<uint8_t>(payload.counter >> 16);
    raw[35] = static_cast<uint8_t>(payload.counter >> 24);
    raw[36] = static_cast<uint8_t>(payload.totp);
    raw[37] = static_cast<uint8_t>(payload.totp >> 8);
    raw[38] = static_cast<uint8_t>(payload.totp >> 16);
    raw[39] = static_cast<uint8_t>(payload.totp >> 24);

    uint8_t digest[20];
    ghost_sha1(raw, 40, digest);

    char hex[41];
    const char* digits = "0123456789abcdef";
    for (uint8_t i = 0; i < 20; ++i) {
        hex[i * 2] = digits[digest[i] >> 4];
        hex[i * 2 + 1] = digits[digest[i] & 0x0f];
    }
    hex[40] = '\0';

    for (uint8_t i = 0; i < 40; ++i) {
        if (payload.hash[i] == '\0') break;
        if (payload.hash[i] != hex[i]) return false;
    }
    return true;
}

bool ReplayGuard::check(const MinePayload& payload, uint32_t now) {
    return check(payload, now, nullptr, 0);
}

bool ReplayGuard::check(const MinePayload& payload, uint32_t now,
                        const uint8_t* totpSeed, uint8_t seedLen) {
    if (payload.mine_id[0] == '\0') return false;
    if (payload.counter == 0) return false;
    if (!hashOk(payload)) return false;

    const uint8_t* seed = totpSeed;
    uint8_t slen = seedLen;

    int16_t found = findWindow(payload.mine_id);
    if (found >= 0) {
        MineWindow& window = windows_[static_cast<uint8_t>(found)];
        if (window.blocked) return false;

        for (uint8_t j = 0; j < REPLAY_WINDOW_PER_MINE; ++j) {
            if (window.counters[j] == payload.counter && window.last_counter != 0) {
                return false;
            }
        }

        if (window.last_counter != 0 && payload.counter <= window.last_counter) {
            return false;
        }

        if (window.last_timestamp != 0) {
            uint32_t diff = (now > window.last_timestamp) ? (now - window.last_timestamp) : 0;
            if (diff < TOTP_WINDOW_MIN_SEC || diff > TOTP_WINDOW_MAX_SEC) {
                return false;
            }
        }

        if (seed == nullptr || slen == 0) {
            if (window.has_seed) {
                seed = window.totp_seed;
                slen = TOTP_SEED_LEN;
            }
        }

        if (seed != nullptr && slen > 0) {
            GhostCrypto crypto;
            uint32_t expect = crypto.hmacSha1(seed, slen, payload.counter, now);
            uint32_t prevNow = (now > TOTP_STEP_SEC) ? (now - TOTP_STEP_SEC) : 0;
            uint32_t prev = crypto.hmacSha1(seed, slen, payload.counter, prevNow);
            if (payload.totp != expect && payload.totp != prev) {
                window.blocked = true;
                return false;
            }
        }

        window.counters[window.index] = payload.counter;
        window.index = (window.index + 1) % REPLAY_WINDOW_PER_MINE;
        window.last_timestamp = now;
        window.last_counter = payload.counter;
        return true;
    }

    if (windowCount_ >= MAX_TRACKED_MINES) return false;

    MineWindow& window = windows_[windowCount_];
    for (uint8_t j = 0; j < 32; ++j) {
        window.mine_id[j] = payload.mine_id[j];
        if (payload.mine_id[j] == '\0') break;
    }

    if (seed == nullptr || slen == 0) {
        if (window.has_seed) {
            seed = window.totp_seed;
            slen = TOTP_SEED_LEN;
        }
    }

    if (seed != nullptr && slen > 0) {
        GhostCrypto crypto;
        uint32_t expect = crypto.hmacSha1(seed, slen, payload.counter, now);
        if (payload.totp != expect) {
            window.blocked = true;
            window.index = 0;
            window.last_timestamp = 0;
            window.last_counter = 0;
            ++windowCount_;
            return false;
        }
    }

    window.index = 0;
    window.blocked = false;
    window.counters[0] = payload.counter;
    window.index = 1;
    window.last_timestamp = now;
    window.last_counter = payload.counter;
    ++windowCount_;
    return true;
}

bool ReplayGuard::acceptFromTransport(const MinePayload& payload, uint32_t now,
                                      Registry* registry) {
    if (!check(payload, now)) {
        if (registry != nullptr) {
            blockMine(payload.mine_id, *registry);
        } else {
            blockMine(payload.mine_id);
        }
        return false;
    }
    return true;
}

void ReplayGuard::blockMine(const char* mineId) {
    if (mineId == nullptr || mineId[0] == '\0') return;
    int16_t found = findWindow(mineId);
    if (found < 0) {
        if (windowCount_ >= MAX_TRACKED_MINES) return;
        MineWindow& window = windows_[windowCount_];
        uint8_t j = 0;
        while (j < 31 && mineId[j] != '\0') {
            window.mine_id[j] = mineId[j];
            ++j;
        }
        window.mine_id[j] = '\0';
        window.index = 0;
        window.last_timestamp = 0;
        window.last_counter = 0;
        window.has_seed = false;
        window.blocked = false;
        for (uint8_t j = 0; j < REPLAY_WINDOW_PER_MINE; ++j) {
            window.counters[j] = 0;
        }
        found = static_cast<int16_t>(windowCount_);
        ++windowCount_;
    }
    windows_[static_cast<uint8_t>(found)].blocked = true;
}

void ReplayGuard::blockMine(const char* mineId, Registry& registry) {
    blockMine(mineId);
    if (registry.getDevice(mineId) != nullptr) {
        registry.blockDevice(mineId);
    }
}

bool ReplayGuard::isBlocked(const char* mineId) const {
    int16_t found = findWindow(mineId);
    if (found < 0) return false;
    return windows_[static_cast<uint8_t>(found)].blocked;
}

bool ReplayGuard::setTotpSeed(const char* mineId, const uint8_t* seed, uint8_t len) {
    if (mineId == nullptr || seed == nullptr || len == 0 || len > TOTP_SEED_LEN) {
        return false;
    }

    int16_t found = findWindow(mineId);
    if (found < 0) {
        if (windowCount_ >= MAX_TRACKED_MINES) return false;
        MineWindow& window = windows_[windowCount_];
        for (uint8_t j = 0; j < 32; ++j) {
            window.mine_id[j] = mineId[j];
            if (mineId[j] == '\0') break;
        }
        found = static_cast<int16_t>(windowCount_);
        ++windowCount_;
    }

    MineWindow& window = windows_[static_cast<uint8_t>(found)];
    for (uint8_t i = 0; i < TOTP_SEED_LEN; ++i) window.totp_seed[i] = 0;
    for (uint8_t i = 0; i < len; ++i) window.totp_seed[i] = seed[i];
    window.has_seed = true;
    return true;
}

uint8_t ReplayGuard::trackedCount() const {
    return windowCount_;
}

const char* ReplayGuard::mineIdAt(uint8_t index) const {
    if (index >= windowCount_) return nullptr;
    return windows_[index].mine_id;
}

uint32_t ReplayGuard::lastCounterAt(uint8_t index) const {
    if (index >= windowCount_) return 0;
    return windows_[index].last_counter;
}

uint32_t ReplayGuard::lastTimestampAt(uint8_t index) const {
    if (index >= windowCount_) return 0;
    return windows_[index].last_timestamp;
}

bool ReplayGuard::blockedAt(uint8_t index) const {
    if (index >= windowCount_) return false;
    return windows_[index].blocked;
}
