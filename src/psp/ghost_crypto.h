#ifndef GHOST_CRYPTO_H
#define GHOST_CRYPTO_H

// =====================================================
// Ghost Hive v1.7.1
// HMAC-SHA1 / TOTP
// Spec-Basis: §15, §33
// TOTP-Schritt 90s (Fenster 60–120s)
// =====================================================

#include <stdint.h>

const uint32_t TOTP_STEP_SEC = 90;
const uint32_t TOTP_WINDOW_MIN_SEC = 60;
const uint32_t TOTP_WINDOW_MAX_SEC = 120;
const uint8_t TOTP_SEED_LEN = 16;

void ghost_sha1(const uint8_t* data, uint32_t len, uint8_t out[20]);
void ghost_hmac_sha1(const uint8_t* key, uint8_t keyLen,
                     const uint8_t* msg, uint32_t msgLen,
                     uint8_t out[20]);
uint32_t ghost_totp(const uint8_t* seed, uint8_t seedLen, uint32_t unixSec);
uint32_t ghost_checksum32(const uint8_t* data, uint32_t len);
void ghost_xor(uint8_t* data, uint32_t len, const uint8_t* key, uint8_t keyLen);

class GhostCrypto {
public:
    GhostCrypto();

    uint32_t hmacSha1(const uint8_t* key, uint8_t keyLen,
                      uint32_t counter, uint32_t now) const;
    uint32_t crc32(const uint8_t* data, uint32_t len) const;
};

#endif // GHOST_CRYPTO_H
