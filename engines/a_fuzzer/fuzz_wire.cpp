#include "ghost_vault.h"
#include "ghost_keys.h"
#include "transport/transport_frame.h"
#include "lab_common.h"
#include "peer_keys.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static GhostKeys g_keys;
static GhostVault g_vault;
static uint8_t g_ready = 0;

static void fuzz_boot() {
    if (g_ready) return;
    g_keys.initEmpty();
    uint8_t root[KEY_LEN];
    for (uint8_t i = 0; i < KEY_LEN; ++i) root[i] = static_cast<uint8_t>(i + 1);
    (void)g_keys.provisionRoot(root, KEY_LEN);
    (void)g_keys.provisionDerived(root, KEY_LEN);
    g_vault.attachKeys(&g_keys);
    g_ready = 1;
    for (uint8_t i = 0; i < KEY_LEN; ++i) root[i] = 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fuzz_boot();
    if (data == nullptr || size == 0) return 0;
    uint8_t buf[TRANSPORT_WIRE_LEN];
    memset(buf, 0, sizeof(buf));
    size_t n = size;
    if (n > TRANSPORT_WIRE_LEN) n = TRANSPORT_WIRE_LEN;
    memcpy(buf, data, n);
    TransportFrame frame;
    if (transport_decode(buf, TRANSPORT_WIRE_LEN, frame)) {
        (void)g_vault.verifyEvent(frame.event);
        (void)g_vault.verifyMine(frame.mine);
    }
    (void)transport_decode(data, static_cast<uint16_t>(
        size > 0xFFFFu ? 0xFFFFu : size), frame);
    return 0;
}

#if !defined(GHOST_LIBFUZZER)
#include <dirent.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    const char* dir = "engines/a_fuzzer/corpus";
    if (argc > 1) dir = argv[1];
    fuzz_boot();
    DIR* d = opendir(dir);
    if (d == nullptr) {
        printf("PASS fuzz_smoke (no corpus dir)\n");
        return 0;
    }
    uint32_t n = 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        if (ent->d_name[0] == '.') continue;
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        FILE* f = fopen(path, "rb");
        if (f == nullptr) continue;
        uint8_t buf[512];
        size_t r = fread(buf, 1, sizeof(buf), f);
        fclose(f);
        (void)LLVMFuzzerTestOneInput(buf, r);
        ++n;
    }
    closedir(d);
    printf("PASS fuzz_smoke n=%u\n", n);
    return 0;
}
#endif
