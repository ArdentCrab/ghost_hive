#include "ghost_keys.h"
#include "ghost_vault.h"
#include "lab_common.h"
#include "peer_keys.h"
#include "ghost_crypto.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static void write_bin(const char* path, const uint8_t* w, uint16_t n) {
    FILE* f = fopen(path, "wb");
    if (f == nullptr) return;
    (void)fwrite(w, 1, n, f);
    fclose(f);
}

int main() {
    (void)mkdir("engines/a_fuzzer/corpus", 0755);
    GhostKeys keys;
    keys.initEmpty();
    uint8_t root[KEY_LEN];
    for (uint8_t i = 0; i < KEY_LEN; ++i) root[i] = static_cast<uint8_t>(i + 1);
    (void)keys.provisionRoot(root, KEY_LEN);
    (void)keys.provisionDerived(root, KEY_LEN);
    GhostVault vault;
    vault.attachKeys(&keys);
    (void)vault.keysAttached();

    uint8_t wire[TRANSPORT_WIRE_LEN];
    Event ev;
    lab_fill_event(&ev, EventType::Heartbeat, "W", 10000, "ok");
    if (lab_encode_event(keys, ROLE_WORKER, "W", ev, 10000, wire, true)) {
        write_bin("engines/a_fuzzer/corpus/hb.bin", wire, TRANSPORT_WIRE_LEN);
    }
    lab_fill_event(&ev, EventType::ScanResult, "W", 10010, "scan");
    if (lab_encode_event(keys, ROLE_WORKER, "W", ev, 10010, wire, true)) {
        write_bin("engines/a_fuzzer/corpus/scan.bin", wire, TRANSPORT_WIRE_LEN);
    }
    lab_fill_event(&ev, EventType::AnomalyDetected, "W", 10020, "tamper:ptrace");
    ev.severity = Severity::Critical;
    if (lab_encode_event(keys, ROLE_WORKER, "W", ev, 10020, wire, true)) {
        write_bin("engines/a_fuzzer/corpus/anom.bin", wire, TRANSPORT_WIRE_LEN);
    }
    lab_fill_event(&ev, EventType::GhostDownStart, "W", 10030, "kill");
    ev.severity = Severity::Critical;
    if (lab_encode_event(keys, ROLE_WORKER, "W", ev, 10030, wire, true)) {
        write_bin("engines/a_fuzzer/corpus/kill_signed.bin", wire, TRANSPORT_WIRE_LEN);
    }
    lab_fill_event(&ev, EventType::GhostDownStart, "W", 10035, "kill");
    ev.severity = Severity::Critical;
    if (lab_encode_event(keys, ROLE_WORKER, "W", ev, 10035, wire, false)) {
        write_bin("engines/a_fuzzer/corpus/kill_unsigned.bin", wire, TRANSPORT_WIRE_LEN);
    }
    lab_fill_event(&ev, EventType::Heartbeat, "W", 10040, "nosig");
    if (lab_encode_event(keys, ROLE_WORKER, "W", ev, 10040, wire, false)) {
        write_bin("engines/a_fuzzer/corpus/hb_unsigned.bin", wire, TRANSPORT_WIRE_LEN);
    }

    MinePayload mp{};
    lab_copy_id(mp.mine_id, "X");
    mp.counter = 1;
    mp.timestamp = 10000;
    mp.event = EventType::MineEvent;
    if (keys.totpSeed() != nullptr) {
        mp.totp = ghost_totp(keys.totpSeed(), TOTP_SEED_LEN, 10000);
    }
    if (lab_encode_mine(keys, mp, 10000, wire, true)) {
        write_bin("engines/a_fuzzer/corpus/mine.bin", wire, TRANSPORT_WIRE_LEN);
    }
    for (uint8_t i = 0; i < KEY_LEN; ++i) root[i] = 0;
    printf("PASS gen_corpus\n");
    return 0;
}
