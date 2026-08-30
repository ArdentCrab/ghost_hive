#include "ghost_crypto.h"

static uint32_t rotl32(uint32_t x, uint32_t n) {
    return (x << n) | (x >> (32 - n));
}

static uint32_t load_be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}

static void store_be32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v >> 24);
    p[1] = static_cast<uint8_t>(v >> 16);
    p[2] = static_cast<uint8_t>(v >> 8);
    p[3] = static_cast<uint8_t>(v);
}

static void sha1_block(uint32_t h[5], const uint8_t blk[64]) {
    uint32_t w[80];
    for (uint8_t i = 0; i < 16; ++i) {
        w[i] = load_be32(blk + i * 4);
    }
    for (uint8_t i = 16; i < 80; ++i) {
        w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = h[0];
    uint32_t b = h[1];
    uint32_t c = h[2];
    uint32_t d = h[3];
    uint32_t e = h[4];

    for (uint8_t i = 0; i < 80; ++i) {
        uint32_t f;
        uint32_t k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        uint32_t temp = rotl32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotl32(b, 30);
        b = a;
        a = temp;
    }

    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
}

void ghost_sha1(const uint8_t* data, uint32_t len, uint8_t out[20]) {
    uint32_t h[5];
    h[0] = 0x67452301u;
    h[1] = 0xEFCDAB89u;
    h[2] = 0x98BADCFEu;
    h[3] = 0x10325476u;
    h[4] = 0xC3D2E1F0u;

    uint8_t block[64];
    uint32_t offset = 0;
    uint32_t bitlen_hi = (len >> 29);
    uint32_t bitlen_lo = (len << 3);

    while (offset + 64 <= len) {
        sha1_block(h, data + offset);
        offset += 64;
    }

    uint32_t rem = len - offset;
    for (uint8_t i = 0; i < 64; ++i) block[i] = 0;
    for (uint32_t i = 0; i < rem; ++i) block[i] = data[offset + i];
    block[rem] = 0x80;

    if (rem >= 56) {
        sha1_block(h, block);
        for (uint8_t i = 0; i < 64; ++i) block[i] = 0;
    }

    store_be32(block + 56, bitlen_hi);
    store_be32(block + 60, bitlen_lo);
    sha1_block(h, block);

    store_be32(out, h[0]);
    store_be32(out + 4, h[1]);
    store_be32(out + 8, h[2]);
    store_be32(out + 12, h[3]);
    store_be32(out + 16, h[4]);
}

void ghost_hmac_sha1(const uint8_t* key, uint8_t keyLen,
                     const uint8_t* msg, uint32_t msgLen,
                     uint8_t out[20]) {
    uint8_t kpad[64];
    for (uint8_t i = 0; i < 64; ++i) kpad[i] = 0;

    if (keyLen > 64) {
        ghost_sha1(key, keyLen, kpad);
    } else {
        for (uint8_t i = 0; i < keyLen; ++i) kpad[i] = key[i];
    }

    uint8_t ipad[64];
    uint8_t opad[64];
    for (uint8_t i = 0; i < 64; ++i) {
        ipad[i] = static_cast<uint8_t>(kpad[i] ^ 0x36);
        opad[i] = static_cast<uint8_t>(kpad[i] ^ 0x5c);
    }

    uint8_t inner_in[320];
    uint32_t useLen = msgLen;
    if (useLen > 256) useLen = 256;
    uint32_t inner_len = 64 + useLen;
    for (uint8_t i = 0; i < 64; ++i) inner_in[i] = ipad[i];
    for (uint32_t i = 0; i < useLen; ++i) inner_in[64 + i] = msg[i];

    uint8_t inner[20];
    ghost_sha1(inner_in, inner_len, inner);

    uint8_t outer_in[84];
    for (uint8_t i = 0; i < 64; ++i) outer_in[i] = opad[i];
    for (uint8_t i = 0; i < 20; ++i) outer_in[64 + i] = inner[i];
    ghost_sha1(outer_in, 84, out);
}

uint32_t ghost_totp(const uint8_t* seed, uint8_t seedLen, uint32_t unixSec) {
    uint32_t steps = unixSec / TOTP_STEP_SEC;
    uint8_t msg[8];
    store_be32(msg, 0);
    store_be32(msg + 4, steps);

    uint8_t hmac[20];
    ghost_hmac_sha1(seed, seedLen, msg, 8, hmac);

    uint8_t off = hmac[19] & 0x0f;
    uint32_t bin = (static_cast<uint32_t>(hmac[off] & 0x7f) << 24) |
                   (static_cast<uint32_t>(hmac[off + 1]) << 16) |
                   (static_cast<uint32_t>(hmac[off + 2]) << 8) |
                   static_cast<uint32_t>(hmac[off + 3]);
    return bin % 1000000u;
}

uint32_t ghost_checksum32(const uint8_t* data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; ++b) {
            uint32_t mask = static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1u)));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

void ghost_xor(uint8_t* data, uint32_t len, const uint8_t* key, uint8_t keyLen) {
    if (key == nullptr || keyLen == 0) return;
    for (uint32_t i = 0; i < len; ++i) {
        data[i] = static_cast<uint8_t>(data[i] ^ key[i % keyLen]);
    }
}

GhostCrypto::GhostCrypto() {
}

uint32_t GhostCrypto::hmacSha1(const uint8_t* key, uint8_t keyLen,
                               uint32_t counter, uint32_t now) const {
    (void)counter;
    return ghost_totp(key, keyLen, now);
}

uint32_t GhostCrypto::crc32(const uint8_t* data, uint32_t len) const {
    return ghost_checksum32(data, len);
}
