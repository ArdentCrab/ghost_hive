#include <cstdio>
#include "../src/psp/ghost_wrap.h"

int main() {
    bool ok = ghost_wrap_selftest();
    uint8_t master[32];
    if (!ghost_wrap_master(master)) ok = false;
    uint8_t pass[8];
    pass[0] = 'n';
    pass[1] = 'o';
    pass[2] = 'a';
    pass[3] = 'h';
    pass[4] = 0;
    uint8_t salt[16];
    for (uint8_t i = 0; i < 16; ++i) salt[i] = static_cast<uint8_t>(i * 3);
    uint8_t wk[32];
    if (!ghost_wrap_kdf(master, pass, 4, salt, wk)) ok = false;
    uint8_t root[32];
    for (uint8_t i = 0; i < 32; ++i) root[i] = static_cast<uint8_t>(0x11 * (i + 1));
    uint8_t blob[ROOT_WRAP_LEN];
    if (!ghost_wrap_seal(wk, root, blob, GHOST_WRAP_FLAG_PASS)) ok = false;
    uint8_t same = 1;
    for (uint8_t i = 0; i < 32; ++i) {
        if (blob[40 + i] != root[i]) same = 0;
    }
    if (same != 0) ok = false;
    uint8_t back[32];
    if (!ghost_wrap_open(wk, blob, back)) ok = false;
    for (uint8_t i = 0; i < 32; ++i) {
        if (back[i] != root[i]) ok = false;
    }
    printf(ok ? "PASS root_wrap\n" : "FAIL root_wrap\n");
    return ok ? 0 : 1;
}
