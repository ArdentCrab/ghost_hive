#ifndef GHOST_WRAP_H
#define GHOST_WRAP_H

// =====================================================
// Root-Wrap — §33.1
// AES-256-CTR + HMAC-SHA1. Master nie auf USB-Drop.
// Plaintext-Root nur PSP-RAM.
// =====================================================

#include "ghost_crypto.h"

const uint16_t ROOT_WRAP_LEN = 92;
const uint8_t GHOST_WRAP_PASS_MAX = 64;
const uint8_t GHOST_WRAP_FLAG_PASS = 1;

bool ghost_wrap_hw_secret(uint8_t out[16]);
bool ghost_wrap_master(uint8_t out[32]);
bool ghost_wrap_kdf(const uint8_t master[32], const uint8_t* pass,
                    uint8_t passLen, const uint8_t salt[16],
                    uint8_t wrapKey[32]);
bool ghost_wrap_load_pass(uint8_t* pass, uint8_t* passLen);
bool ghost_wrap_seal(const uint8_t wrapKey[32], const uint8_t plain[32],
                     uint8_t blob[ROOT_WRAP_LEN], uint8_t flags);
bool ghost_wrap_open(const uint8_t wrapKey[32], const uint8_t blob[ROOT_WRAP_LEN],
                     uint8_t plain[32]);
bool ghost_wrap_protect(const uint8_t plain[32], uint8_t blob[ROOT_WRAP_LEN]);
bool ghost_wrap_reveal(const uint8_t blob[ROOT_WRAP_LEN], uint8_t plain[32]);
bool ghost_wrap_selftest();

#endif
