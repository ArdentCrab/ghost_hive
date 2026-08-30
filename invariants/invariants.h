#ifndef GHOST_LAB_INVARIANTS_H
#define GHOST_LAB_INVARIANTS_H

// Ghost Attack Lab — INV-01..08
// Spec-Basis: §9, §10, §15, §40, §43
// Standalone predicates over introspection snapshots. Sim-only.

#include <stdint.h>

const uint16_t LAB_SNAP_LINE = 480;

struct LabSnap {
    uint8_t root_ok;
    char state[24];
    uint32_t game_n;
    char last_policy[24];
    uint32_t last_counter;
    uint32_t last_totp_win;
    uint8_t frozen;
    uint8_t snap_n;
    uint8_t snap_ran;
    uint8_t kill_sent;
    uint8_t hmac_ok;
    uint8_t accepted;
    uint8_t unsigned_kill;
    uint8_t uk_froze;
    uint8_t replay_attempt;
    uint8_t totp_out;
    uint8_t asan;
    uint8_t target_down;
    uint8_t recovered;
    uint32_t pid;
    char kind[16];
    char etype[24];
    uint32_t now;
};

struct LabInvResult {
    uint8_t ok;
    char id[12];
};

void lab_snap_clear(LabSnap* s);
bool lab_snap_encode(const LabSnap& s, char* out, uint16_t cap);
bool lab_snap_parse(const char* line, LabSnap* s);

bool lab_inv01(const LabSnap& s);
bool lab_inv02(const LabSnap& before, const LabSnap& after);
bool lab_inv03(const LabSnap& after);
bool lab_inv04(const LabSnap& after);
bool lab_inv05(const LabSnap& after);
bool lab_inv06(const LabSnap& after);
bool lab_inv07(const LabSnap& before, const LabSnap& after);
bool lab_inv08(const LabSnap& before, const LabSnap& after);

// Returns first failing INV id, or ok=1.
LabInvResult lab_check_pair(const LabSnap& before, const LabSnap& after);

#endif
