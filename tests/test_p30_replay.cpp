#include "p30_harness.h"

int main() {
    bool ok = true;
    P30Hive h;
    p30_attach(h);
    if (!p30_bind_root(h)) ok = false;

    Mine mine;
    mine.init("M1");
    if (!mine.setTotpSeed(h.keys.totpSeed(), TOTP_SEED_LEN)) ok = false;
    if (!h.pipe.replay().setTotpSeed("M1", h.keys.totpSeed(), TOTP_SEED_LEN)) ok = false;

    MinePayload first{};
    if (!mine.send(&first, 7000)) ok = false;
    p30_inject_mine(h, first, 7000, true);
    if (h.pipe.replay().isBlocked("M1")) ok = false;

    uint8_t before = h.vault.getStoredCount();
    p30_inject_mine(h, first, 7090, true);
    if (!h.pipe.replay().isBlocked("M1")) ok = false;
    if (h.vault.getStoredCount() <= before) ok = false;
    if (!p30_has_type(h.vault, EventType::PolicyViolation)) ok = false;
    if (h.xport.hiveFrozen()) ok = false;
    if (h.vault.frozen()) ok = false;

    Mine mine2;
    mine2.init("M2");
    if (!mine2.setTotpSeed(h.keys.totpSeed(), TOTP_SEED_LEN)) ok = false;
    MinePayload oldt{};
    if (!mine2.send(&oldt, 8000)) ok = false;
    oldt.totp = 1;
    p30_inject_mine(h, oldt, 8000, true);

    char buf[OUTPUT_BUFFER_LEN];
    GhostOutput out;
    out.buildVault(h.vault, buf);
    if (buf[0] == '\0') ok = false;

    // §38 / P11: Lockvogel-Treffer (sendTrip) → Critical MineEvent → Ghost Down
    P30Hive trip;
    p30_attach(trip);
    if (!p30_bind_root(trip)) ok = false;
    Mine decoy;
    decoy.init("MN");
    if (!decoy.setTotpSeed(trip.keys.totpSeed(), TOTP_SEED_LEN)) ok = false;
    if (!trip.pipe.replay().setTotpSeed("MN", trip.keys.totpSeed(), TOTP_SEED_LEN)) ok = false;
    MinePayload hit{};
    if (!decoy.sendTrip(&hit, 9000)) ok = false;
    if (hit.event != EventType::AnomalyDetected) ok = false;
    p30_inject_mine(trip, hit, 9000, true);
    if (trip.xport.hiveFrozen()) ok = false;
    if (!p30_has_type(trip.vault, EventType::MineEvent)) ok = false;

    printf(ok ? "PASS p30_replay\n" : "FAIL p30_replay\n");
    return ok ? 0 : 1;
}
