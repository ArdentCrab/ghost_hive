#include "ghost_wrap.h"

#include <stdio.h>

#if defined(__PSP__)
#include <pspwlan.h>
#include <psprtc.h>
#else
#include <sys/stat.h>
#endif

static const uint8_t AES_SBOX[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b,
    0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
    0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26,
    0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
    0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
    0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed,
    0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f,
    0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec,
    0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14,
    0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
    0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
    0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f,
    0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
    0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
    0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f,
    0xb0, 0x54, 0xbb, 0x16};

static const uint8_t AES_RCON[10] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};

static void wrap_zero(uint8_t* p, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) p[i] = 0;
}

static uint8_t xtime(uint8_t x) {
    return static_cast<uint8_t>((x << 1) ^ (((x >> 7) & 1u) * 0x1bu));
}

static void mix_column(uint8_t* c) {
    uint8_t a0 = c[0];
    uint8_t a1 = c[1];
    uint8_t a2 = c[2];
    uint8_t a3 = c[3];
    uint8_t t = static_cast<uint8_t>(a0 ^ a1 ^ a2 ^ a3);
    c[0] = static_cast<uint8_t>(a0 ^ t ^ xtime(static_cast<uint8_t>(a0 ^ a1)));
    c[1] = static_cast<uint8_t>(a1 ^ t ^ xtime(static_cast<uint8_t>(a1 ^ a2)));
    c[2] = static_cast<uint8_t>(a2 ^ t ^ xtime(static_cast<uint8_t>(a2 ^ a3)));
    c[3] = static_cast<uint8_t>(a3 ^ t ^ xtime(static_cast<uint8_t>(a3 ^ a0)));
}

static void aes256_expand(const uint8_t key[32], uint8_t rk[240]) {
    for (uint8_t i = 0; i < 32; ++i) rk[i] = key[i];
    uint8_t rcon_i = 0;
    uint16_t bytes = 32;
    uint8_t t[4];
    while (bytes < 240) {
        t[0] = rk[bytes - 4];
        t[1] = rk[bytes - 3];
        t[2] = rk[bytes - 2];
        t[3] = rk[bytes - 1];
        if ((bytes % 32) == 0) {
            uint8_t u = t[0];
            t[0] = static_cast<uint8_t>(AES_SBOX[t[1]] ^ AES_RCON[rcon_i++]);
            t[1] = AES_SBOX[t[2]];
            t[2] = AES_SBOX[t[3]];
            t[3] = AES_SBOX[u];
        } else if ((bytes % 32) == 16) {
            t[0] = AES_SBOX[t[0]];
            t[1] = AES_SBOX[t[1]];
            t[2] = AES_SBOX[t[2]];
            t[3] = AES_SBOX[t[3]];
        }
        rk[bytes] = static_cast<uint8_t>(rk[bytes - 32] ^ t[0]);
        rk[bytes + 1] = static_cast<uint8_t>(rk[bytes - 31] ^ t[1]);
        rk[bytes + 2] = static_cast<uint8_t>(rk[bytes - 30] ^ t[2]);
        rk[bytes + 3] = static_cast<uint8_t>(rk[bytes - 29] ^ t[3]);
        bytes = static_cast<uint16_t>(bytes + 4);
    }
}

static void aes256_encrypt_block(const uint8_t rk[240], const uint8_t in[16],
                                 uint8_t out[16]) {
    uint8_t s[16];
    for (uint8_t i = 0; i < 16; ++i) {
        s[i] = static_cast<uint8_t>(in[i] ^ rk[i]);
    }
    for (uint8_t round = 1; round < 14; ++round) {
        for (uint8_t i = 0; i < 16; ++i) s[i] = AES_SBOX[s[i]];
        uint8_t tmp[16];
        tmp[0] = s[0];
        tmp[4] = s[4];
        tmp[8] = s[8];
        tmp[12] = s[12];
        tmp[1] = s[5];
        tmp[5] = s[9];
        tmp[9] = s[13];
        tmp[13] = s[1];
        tmp[2] = s[10];
        tmp[6] = s[14];
        tmp[10] = s[2];
        tmp[14] = s[6];
        tmp[3] = s[15];
        tmp[7] = s[3];
        tmp[11] = s[7];
        tmp[15] = s[11];
        for (uint8_t i = 0; i < 16; ++i) s[i] = tmp[i];
        for (uint8_t c = 0; c < 4; ++c) mix_column(s + c * 4);
        uint8_t off = static_cast<uint8_t>(round * 16);
        for (uint8_t i = 0; i < 16; ++i) {
            s[i] = static_cast<uint8_t>(s[i] ^ rk[off + i]);
        }
    }
    for (uint8_t i = 0; i < 16; ++i) s[i] = AES_SBOX[s[i]];
    uint8_t tmp[16];
    tmp[0] = s[0];
    tmp[4] = s[4];
    tmp[8] = s[8];
    tmp[12] = s[12];
    tmp[1] = s[5];
    tmp[5] = s[9];
    tmp[9] = s[13];
    tmp[13] = s[1];
    tmp[2] = s[10];
    tmp[6] = s[14];
    tmp[10] = s[2];
    tmp[14] = s[6];
    tmp[3] = s[15];
    tmp[7] = s[3];
    tmp[11] = s[7];
    tmp[15] = s[11];
    for (uint8_t i = 0; i < 16; ++i) {
        out[i] = static_cast<uint8_t>(tmp[i] ^ rk[224 + i]);
    }
}

static void aes256_ctr(const uint8_t key[32], const uint8_t iv[16],
                       const uint8_t* in, uint8_t* out, uint8_t len) {
    uint8_t rk[240];
    aes256_expand(key, rk);
    uint8_t ctr[16];
    for (uint8_t i = 0; i < 16; ++i) ctr[i] = iv[i];
    uint8_t ks[16];
    uint8_t n = 0;
    while (n < len) {
        aes256_encrypt_block(rk, ctr, ks);
        uint8_t take = static_cast<uint8_t>(len - n);
        if (take > 16) take = 16;
        for (uint8_t i = 0; i < take; ++i) {
            out[n + i] = static_cast<uint8_t>(in[n + i] ^ ks[i]);
        }
        n = static_cast<uint8_t>(n + take);
        for (int8_t i = 15; i >= 0; --i) {
            ctr[i] = static_cast<uint8_t>(ctr[i] + 1);
            if (ctr[i] != 0) break;
        }
    }
    wrap_zero(rk, 240);
    wrap_zero(ks, 16);
    wrap_zero(ctr, 16);
}

static bool fill_random(uint8_t* dst, uint8_t n) {
    if (dst == nullptr || n == 0) return false;
#if !defined(__PSP__)
    FILE* f = fopen("/dev/urandom", "rb");
    if (f == nullptr) return false;
    size_t got = fread(dst, 1, n, f);
    fclose(f);
    return got == n;
#else
    uint8_t hw[16];
    if (!ghost_wrap_hw_secret(hw)) return false;
    uint64_t tick = 0;
    (void)sceRtcGetCurrentTick(&tick);
    for (uint8_t i = 0; i < n; ++i) {
        tick = tick * 1103515245ull + 12345ull + hw[i % 16];
        dst[i] = static_cast<uint8_t>(tick >> 16);
        dst[i] = static_cast<uint8_t>(dst[i] ^ hw[i % 16]);
    }
    wrap_zero(hw, 16);
    return true;
#endif
}

bool ghost_wrap_hw_secret(uint8_t out[16]) {
    if (out == nullptr) return false;
    wrap_zero(out, 16);
#if defined(__PSP__)
    uint8_t mac[8];
    wrap_zero(mac, 8);
    if (sceWlanGetEtherAddr(mac) == 0) {
        for (uint8_t i = 0; i < 6; ++i) out[i] = mac[i];
        for (uint8_t i = 0; i < 6; ++i) out[6 + (i % 10)] ^= mac[i];
        out[12] = 0x50;
        out[13] = 0x53;
        out[14] = 0x50;
        out[15] = 0x31;
        wrap_zero(mac, 8);
        return true;
    }
    wrap_zero(mac, 8);
    return false;
#else
    (void)mkdir("/tmp/ghost_hive_psp", 0755);
    FILE* f = fopen("/tmp/ghost_hive_psp/hw.id", "rb");
    if (f != nullptr) {
        size_t n = fread(out, 1, 16, f);
        int extra = fgetc(f);
        fclose(f);
        if (n == 16 && extra == EOF) {
            bool z = true;
            for (uint8_t i = 0; i < 16; ++i) {
                if (out[i] != 0) z = false;
            }
            if (!z) return true;
        }
    }
    if (!fill_random(out, 16)) return false;
    FILE* w = fopen("/tmp/ghost_hive_psp/hw.id", "wb");
    if (w == nullptr) return false;
    bool ok = fwrite(out, 1, 16, w) == 16;
    if (fclose(w) != 0) ok = false;
    return ok;
#endif
}

bool ghost_wrap_master(uint8_t out[32]) {
    if (out == nullptr) return false;
    uint8_t hw[16];
    if (!ghost_wrap_hw_secret(hw)) return false;
    const uint8_t info0[16] = {'g', 'h', 'o', 's', 't', '-', 'm', 'a',
                               's', 't', 'e', 'r', '-', '0', 0, 0};
    const uint8_t info1[16] = {'g', 'h', 'o', 's', 't', '-', 'm', 'a',
                               's', 't', 'e', 'r', '-', '1', 0, 0};
    uint8_t h0[20];
    uint8_t h1[20];
    ghost_hmac_sha1(hw, 16, info0, 16, h0);
    ghost_hmac_sha1(hw, 16, info1, 16, h1);
    for (uint8_t i = 0; i < 20; ++i) out[i] = h0[i];
    for (uint8_t i = 0; i < 12; ++i) out[20 + i] = h1[i];
    wrap_zero(hw, 16);
    wrap_zero(h0, 20);
    wrap_zero(h1, 20);
    return true;
}

bool ghost_wrap_kdf(const uint8_t master[32], const uint8_t* pass,
                    uint8_t passLen, const uint8_t salt[16],
                    uint8_t wrapKey[32]) {
    if (master == nullptr || salt == nullptr || wrapKey == nullptr) return false;
    uint8_t ikm[32 + GHOST_WRAP_PASS_MAX];
    wrap_zero(ikm, 32 + GHOST_WRAP_PASS_MAX);
    for (uint8_t i = 0; i < 32; ++i) ikm[i] = master[i];
    uint8_t n = 32;
    if (pass != nullptr && passLen > 0) {
        uint8_t lim = passLen;
        if (lim > GHOST_WRAP_PASS_MAX) lim = GHOST_WRAP_PASS_MAX;
        for (uint8_t i = 0; i < lim; ++i) ikm[32 + i] = pass[i];
        n = static_cast<uint8_t>(32 + lim);
    }
    uint8_t prk[20];
    ghost_hmac_sha1(salt, 16, ikm, n, prk);
    const uint8_t x1[12] = {'w', 'r', 'a', 'p', '-', 'e', 'x', 'p', 'a', 'n', 'd', 1};
    const uint8_t x2[12] = {'w', 'r', 'a', 'p', '-', 'e', 'x', 'p', 'a', 'n', 'd', 2};
    uint8_t h1[20];
    uint8_t h2[20];
    ghost_hmac_sha1(prk, 20, x1, 12, h1);
    ghost_hmac_sha1(prk, 20, x2, 12, h2);
    for (uint8_t i = 0; i < 20; ++i) wrapKey[i] = h1[i];
    for (uint8_t i = 0; i < 12; ++i) wrapKey[20 + i] = h2[i];
    wrap_zero(ikm, 32 + GHOST_WRAP_PASS_MAX);
    wrap_zero(prk, 20);
    wrap_zero(h1, 20);
    wrap_zero(h2, 20);
    return true;
}

bool ghost_wrap_load_pass(uint8_t* pass, uint8_t* passLen) {
    if (pass == nullptr || passLen == nullptr) return false;
    wrap_zero(pass, GHOST_WRAP_PASS_MAX);
    *passLen = 0;
#if defined(__PSP__)
    return true;
#else
    FILE* f = fopen("/tmp/ghost_hive_psp/passphrase", "rb");
    if (f == nullptr) return true;
    size_t n = fread(pass, 1, GHOST_WRAP_PASS_MAX, f);
    fclose(f);
    while (n > 0 && (pass[n - 1] == '\n' || pass[n - 1] == '\r')) --n;
    *passLen = static_cast<uint8_t>(n);
    return true;
#endif
}

static void wrap_mac(const uint8_t wrapKey[32], const uint8_t* blob,
                     uint8_t mac[20]) {
    ghost_hmac_sha1(wrapKey, 32, blob, 72, mac);
}

bool ghost_wrap_seal(const uint8_t wrapKey[32], const uint8_t plain[32],
                     uint8_t blob[ROOT_WRAP_LEN], uint8_t flags) {
    if (wrapKey == nullptr || plain == nullptr || blob == nullptr) return false;
    wrap_zero(blob, ROOT_WRAP_LEN);
    blob[0] = 'G';
    blob[1] = 'H';
    blob[2] = 'W';
    blob[3] = '1';
    blob[4] = flags;
    blob[5] = 0;
    blob[6] = 0;
    blob[7] = 0;
    if (!fill_random(blob + 8, 16)) return false;
    if (!fill_random(blob + 24, 16)) return false;
    aes256_ctr(wrapKey, blob + 24, plain, blob + 40, 32);
    wrap_mac(wrapKey, blob, blob + 72);
    return true;
}

bool ghost_wrap_open(const uint8_t wrapKey[32], const uint8_t blob[ROOT_WRAP_LEN],
                     uint8_t plain[32]) {
    if (wrapKey == nullptr || blob == nullptr || plain == nullptr) return false;
    if (blob[0] != 'G' || blob[1] != 'H' || blob[2] != 'W' || blob[3] != '1') {
        return false;
    }
    uint8_t mac[20];
    wrap_mac(wrapKey, blob, mac);
    uint8_t diff = 0;
    for (uint8_t i = 0; i < 20; ++i) diff = static_cast<uint8_t>(diff | (mac[i] ^ blob[72 + i]));
    wrap_zero(mac, 20);
    if (diff != 0) return false;
    wrap_zero(plain, 32);
    aes256_ctr(wrapKey, blob + 24, blob + 40, plain, 32);
    return true;
}

bool ghost_wrap_protect(const uint8_t plain[32], uint8_t blob[ROOT_WRAP_LEN]) {
    if (plain == nullptr || blob == nullptr) return false;
    uint8_t master[32];
    uint8_t pass[GHOST_WRAP_PASS_MAX];
    uint8_t passLen = 0;
    uint8_t wk[32];
    if (!ghost_wrap_master(master)) return false;
    if (!ghost_wrap_load_pass(pass, &passLen)) {
        wrap_zero(master, 32);
        return false;
    }
    uint8_t flags = 0;
    if (passLen > 0) flags = GHOST_WRAP_FLAG_PASS;
    wrap_zero(blob, ROOT_WRAP_LEN);
    blob[0] = 'G';
    blob[1] = 'H';
    blob[2] = 'W';
    blob[3] = '1';
    blob[4] = flags;
    if (!fill_random(blob + 8, 16)) {
        wrap_zero(master, 32);
        wrap_zero(pass, GHOST_WRAP_PASS_MAX);
        return false;
    }
    if (!fill_random(blob + 24, 16)) {
        wrap_zero(master, 32);
        wrap_zero(pass, GHOST_WRAP_PASS_MAX);
        return false;
    }
    if (!ghost_wrap_kdf(master, pass, passLen, blob + 8, wk)) {
        wrap_zero(master, 32);
        wrap_zero(pass, GHOST_WRAP_PASS_MAX);
        return false;
    }
    aes256_ctr(wk, blob + 24, plain, blob + 40, 32);
    wrap_mac(wk, blob, blob + 72);
    wrap_zero(master, 32);
    wrap_zero(pass, GHOST_WRAP_PASS_MAX);
    wrap_zero(wk, 32);
    return true;
}

bool ghost_wrap_reveal(const uint8_t blob[ROOT_WRAP_LEN], uint8_t plain[32]) {
    if (blob == nullptr || plain == nullptr) return false;
    if (blob[0] != 'G' || blob[1] != 'H' || blob[2] != 'W' || blob[3] != '1') {
        return false;
    }
    uint8_t pass[GHOST_WRAP_PASS_MAX];
    uint8_t passLen = 0;
    if (!ghost_wrap_load_pass(pass, &passLen)) return false;
    if ((blob[4] & GHOST_WRAP_FLAG_PASS) != 0 && passLen == 0) {
        wrap_zero(pass, GHOST_WRAP_PASS_MAX);
        return false;
    }
    uint8_t master[32];
    uint8_t wk[32];
    if (!ghost_wrap_master(master)) {
        wrap_zero(pass, GHOST_WRAP_PASS_MAX);
        return false;
    }
    if (!ghost_wrap_kdf(master, pass, passLen, blob + 8, wk)) {
        wrap_zero(master, 32);
        wrap_zero(pass, GHOST_WRAP_PASS_MAX);
        return false;
    }
    bool ok = ghost_wrap_open(wk, blob, plain);
    wrap_zero(master, 32);
    wrap_zero(pass, GHOST_WRAP_PASS_MAX);
    wrap_zero(wk, 32);
    return ok;
}

bool ghost_wrap_selftest() {
    const uint8_t key[32] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
    const uint8_t pt[16] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    const uint8_t expect[16] = {
        0x8e, 0xa2, 0xb7, 0xca, 0x51, 0x67, 0x45, 0xbf,
        0xea, 0xfc, 0x49, 0x90, 0x4b, 0x49, 0x60, 0x89};
    uint8_t rk[240];
    uint8_t got[16];
    aes256_expand(key, rk);
    aes256_encrypt_block(rk, pt, got);
    wrap_zero(rk, 240);
    uint8_t bad = 0;
    for (uint8_t i = 0; i < 16; ++i) {
        if (got[i] != expect[i]) bad = 1;
    }
    if (bad != 0) return false;

    uint8_t master[32];
    if (!ghost_wrap_master(master)) return false;
    uint8_t salt[16];
    for (uint8_t i = 0; i < 16; ++i) salt[i] = static_cast<uint8_t>(i + 1);
    uint8_t wk[32];
    if (!ghost_wrap_kdf(master, nullptr, 0, salt, wk)) return false;
    uint8_t root[32];
    for (uint8_t i = 0; i < 32; ++i) root[i] = static_cast<uint8_t>(0xA0 + i);
    uint8_t blob[ROOT_WRAP_LEN];
    if (!ghost_wrap_seal(wk, root, blob, 0)) return false;
    uint8_t same = 1;
    for (uint8_t i = 0; i < 32; ++i) {
        if (blob[40 + i] != root[i]) same = 0;
    }
    if (same != 0) {
        wrap_zero(master, 32);
        wrap_zero(wk, 32);
        wrap_zero(root, 32);
        return false;
    }
    uint8_t back[32];
    if (!ghost_wrap_open(wk, blob, back)) {
        wrap_zero(master, 32);
        wrap_zero(wk, 32);
        wrap_zero(root, 32);
        return false;
    }
    for (uint8_t i = 0; i < 32; ++i) {
        if (back[i] != root[i]) bad = 1;
    }
    uint8_t wk2[32];
    wk2[0] = static_cast<uint8_t>(wk[0] ^ 1);
    for (uint8_t i = 1; i < 32; ++i) wk2[i] = wk[i];
    uint8_t fail[32];
    if (ghost_wrap_open(wk2, blob, fail)) bad = 1;
    wrap_zero(master, 32);
    wrap_zero(wk, 32);
    wrap_zero(wk2, 32);
    wrap_zero(root, 32);
    wrap_zero(back, 32);
    wrap_zero(fail, 32);
    return bad == 0;
}
