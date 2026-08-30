#include <cstdio>
#include "../src/psp/replay_guard.h"

int main() {
    ReplayGuard guard;

    MinePayload p1{};
    p1.mine_id[0] = 'M';
    p1.mine_id[1] = '\0';
    p1.counter = 1;
    p1.totp = 100;
    p1.timestamp = 1000;

    bool ok = true;

    // Erster gültiger Eintrag
    if (!guard.check(p1, 1000)) ok = false;

    // Gleicher Counter → Replay
    if (guard.check(p1, 1120)) ok = false;

    // Neuer Counter, gültiges Fenster
    MinePayload p2 = p1;
    p2.counter = 2;
    p2.totp = 200;
    if (!guard.check(p2, 1120)) ok = false;

    // Alten Counter erneut senden → Replay
    if (guard.check(p1, 1180)) ok = false;

    // Mine blockieren
    guard.blockMine("M");

    MinePayload p3 = p1;
    p3.counter = 3;
    p3.totp = 300;

    // Blockierte Mine → false
    if (guard.check(p3, 1240)) ok = false;

    ReplayGuard cli;
    cli.blockMine("Z");
    if (!cli.isBlocked("Z")) ok = false;

    uint8_t seed[TOTP_SEED_LEN];
    for (uint8_t i = 0; i < TOTP_SEED_LEN; ++i) seed[i] = static_cast<uint8_t>(i + 1);
    ReplayGuard totp;
    if (!totp.setTotpSeed("T", seed, TOTP_SEED_LEN)) ok = false;
    MinePayload oldt{};
    oldt.mine_id[0] = 'T';
    oldt.mine_id[1] = '\0';
    oldt.counter = 1;
    oldt.totp = 1;
    oldt.timestamp = 2000;
    if (totp.check(oldt, 2000)) ok = false;
    if (!totp.isBlocked("T")) ok = false;

    ReplayGuard many;
    for (uint8_t i = 0; i < MAX_TRACKED_MINES; ++i) {
        MinePayload m{};
        m.mine_id[0] = 'A';
        m.mine_id[1] = static_cast<char>('0' + (i / 10));
        m.mine_id[2] = static_cast<char>('0' + (i % 10));
        m.mine_id[3] = '\0';
        m.counter = 1;
        m.totp = 100;
        m.timestamp = 3000;
        if (!many.check(m, 3000)) ok = false;
    }

    printf(ok ? "PASS replay_guard\n" : "FAIL replay_guard\n");
    return ok ? 0 : 1;
}
