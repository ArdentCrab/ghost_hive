#include "ghost_keys.h"

#if !defined(__PSP__)
#include <stdio.h>
#endif

GhostKeys::GhostKeys() {
    initEmpty();
}

void GhostKeys::zero(uint8_t* buf, uint8_t len) {
    for (uint8_t i = 0; i < len; ++i) buf[i] = 0;
}

bool GhostKeys::copyKey(uint8_t* dst, uint8_t dstLen,
                        const uint8_t* src, uint8_t srcLen) {
    if (src == nullptr || srcLen == 0 || srcLen > dstLen) return false;
    zero(dst, dstLen);
    for (uint8_t i = 0; i < srcLen; ++i) dst[i] = src[i];
    return true;
}

void GhostKeys::initEmpty() {
    zero(root_, KEY_LEN);
    zero(device_, KEY_LEN);
    zero(log_, KEY_LEN);
    zero(session_, KEY_LEN);
    zero(mine_, KEY_LEN);
    zero(totpSeed_, TOTP_SEED_LEN);
    hasRoot_ = false;
    hasDevice_ = false;
    hasLog_ = false;
    hasSession_ = false;
    hasMine_ = false;
    hasTotpSeed_ = false;
}

void GhostKeys::physicalReset() {
    initEmpty();
}

bool GhostKeys::hasRoot() const { return hasRoot_; }
bool GhostKeys::hasDevice() const { return hasDevice_; }
bool GhostKeys::hasLog() const { return hasLog_; }
bool GhostKeys::hasSession() const { return hasSession_; }
bool GhostKeys::hasMine() const { return hasMine_; }
bool GhostKeys::hasTotpSeed() const { return hasTotpSeed_; }

bool GhostKeys::provisionRoot(const uint8_t* key, uint8_t len) {
    if (!copyKey(root_, KEY_LEN, key, len)) return false;
    hasRoot_ = true;
    return true;
}

bool GhostKeys::provisionDevice(const uint8_t* key, uint8_t len) {
    if (!copyKey(device_, KEY_LEN, key, len)) return false;
    hasDevice_ = true;
    return true;
}

bool GhostKeys::provisionLog(const uint8_t* key, uint8_t len) {
    if (!copyKey(log_, KEY_LEN, key, len)) return false;
    hasLog_ = true;
    return true;
}

bool GhostKeys::provisionSession(const uint8_t* key, uint8_t len) {
    if (!copyKey(session_, KEY_LEN, key, len)) return false;
    hasSession_ = true;
    return true;
}

bool GhostKeys::provisionMine(const uint8_t* key, uint8_t len) {
    if (!copyKey(mine_, KEY_LEN, key, len)) return false;
    hasMine_ = true;
    return true;
}

bool GhostKeys::provisionTotpSeed(const uint8_t* seed, uint8_t len) {
    if (!copyKey(totpSeed_, TOTP_SEED_LEN, seed, len)) return false;
    hasTotpSeed_ = true;
    return true;
}

static void keys_derive(const uint8_t* root, const char* label,
                        uint8_t* out, uint8_t outLen) {
    uint8_t msg[32];
    uint8_t labLen = 0;
    while (label[labLen] != '\0' && labLen < 30) {
        msg[labLen] = static_cast<uint8_t>(label[labLen]);
        ++labLen;
    }
    msg[labLen] = 1;
    uint8_t h1[20];
    uint8_t h2[20];
    ghost_hmac_sha1(root, KEY_LEN, msg, static_cast<uint32_t>(labLen) + 1u, h1);
    msg[labLen] = 2;
    ghost_hmac_sha1(root, KEY_LEN, msg, static_cast<uint32_t>(labLen) + 1u, h2);
    for (uint8_t i = 0; i < outLen; ++i) {
        out[i] = (i < 20) ? h1[i] : h2[i - 20];
    }
}

bool GhostKeys::provisionDerived(const uint8_t* root, uint8_t len) {
    if (root == nullptr || len != KEY_LEN) return false;
    uint8_t device[KEY_LEN];
    uint8_t log[KEY_LEN];
    uint8_t session[KEY_LEN];
    uint8_t mine[KEY_LEN];
    uint8_t totp[TOTP_SEED_LEN];
    keys_derive(root, "device", device, KEY_LEN);
    keys_derive(root, "log", log, KEY_LEN);
    keys_derive(root, "session", session, KEY_LEN);
    keys_derive(root, "mine", mine, KEY_LEN);
    if (!provisionDevice(device, KEY_LEN)) return false;
    if (!provisionLog(log, KEY_LEN)) return false;
    if (!provisionSession(session, KEY_LEN)) return false;
    if (!provisionMine(mine, KEY_LEN)) return false;
    keys_derive(mine, "totp", totp, TOTP_SEED_LEN);
    if (!provisionTotpSeed(totp, TOTP_SEED_LEN)) return false;
    return hasDevice_ && hasLog_ && hasSession_ && hasMine_ && hasTotpSeed_;
}

bool GhostKeys::exportPeerBind(uint8_t* dst, uint8_t dstLen) const {
    if (dst == nullptr || dstLen < PEER_BIND_LEN) return false;
    if (!hasDevice_ || !hasSession_ || !hasMine_ || !hasTotpSeed_) return false;
    uint8_t i = 0;
    for (uint8_t n = 0; n < KEY_LEN; ++n) dst[i++] = device_[n];
    for (uint8_t n = 0; n < KEY_LEN; ++n) dst[i++] = session_[n];
    for (uint8_t n = 0; n < KEY_LEN; ++n) dst[i++] = mine_[n];
    for (uint8_t n = 0; n < TOTP_SEED_LEN; ++n) dst[i++] = totpSeed_[n];
    return true;
}

bool GhostKeys::importPeerBind(const uint8_t* src, uint8_t srcLen) {
    if (src == nullptr || srcLen != PEER_BIND_LEN) return false;
    if (!provisionDevice(src, KEY_LEN)) return false;
    if (!provisionSession(src + KEY_LEN, KEY_LEN)) return false;
    if (!provisionMine(src + KEY_LEN * 2, KEY_LEN)) return false;
    if (!provisionTotpSeed(src + KEY_LEN * 3, TOTP_SEED_LEN)) return false;
    hasRoot_ = false;
    zero(root_, KEY_LEN);
    return !hasRoot_ && hasDevice_ && hasSession_ && hasMine_ && hasTotpSeed_;
}

bool GhostKeys::generateRoot() {
    uint8_t raw[KEY_LEN];
#if !defined(__PSP__)
    FILE* f = fopen("/dev/urandom", "rb");
    if (f == nullptr) return false;
    size_t n = fread(raw, 1, KEY_LEN, f);
    fclose(f);
    if (n != KEY_LEN) return false;
#else
    uint32_t t = 0;
    for (uint8_t i = 0; i < KEY_LEN; ++i) {
        t = t * 1103515245u + 12345u + static_cast<uint32_t>(i);
        raw[i] = static_cast<uint8_t>(t >> 16);
    }
    uintptr_t mix = reinterpret_cast<uintptr_t>(this);
    for (uint8_t i = 0; i < KEY_LEN; ++i) {
        raw[i] = static_cast<uint8_t>(raw[i] ^ static_cast<uint8_t>(mix >> (i % 8)));
    }
#endif
    bool z = true;
    for (uint8_t i = 0; i < KEY_LEN; ++i) {
        if (raw[i] != 0) z = false;
    }
    if (z) return false;
    bool ok = provisionRoot(raw, KEY_LEN);
    zero(raw, KEY_LEN);
    return ok;
}

const uint8_t* GhostKeys::root() const { return root_; }
const uint8_t* GhostKeys::device() const { return device_; }
const uint8_t* GhostKeys::log() const { return log_; }
const uint8_t* GhostKeys::session() const { return session_; }
const uint8_t* GhostKeys::mine() const { return mine_; }
const uint8_t* GhostKeys::totpSeed() const { return totpSeed_; }

bool GhostKeys::rotateManual(const uint8_t* newRoot, uint8_t len) {
    // §33: Rotation manuell, nur wenn bereits Root existiert.
    if (!hasRoot_) return false;
    return provisionRoot(newRoot, len);
}
