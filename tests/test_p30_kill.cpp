#include "p30_harness.h"
#include "../src/laptop/kill.h"
#include "../src/laptop/peer_halt.h"

int main() {
    bool ok = true;
    P30Hive h;
    p30_attach(h);
    if (!p30_bind_root(h)) ok = false;
    p30_enroll(h, "W", ROLE_WORKER, 2);
    p30_enroll(h, "P", ROLE_PHONE, 1);

    h.down.execute(1000);
    h.down.tick(1005);
    if (h.down.snapshotCount() != 0) ok = false;
    if (h.down.killSent()) ok = false;

    TransportFrame killRx;
    if (h.wlan.fromPeer("W", killRx)) {
        if (killRx.kind == TransportKind::KillFrame) ok = false;
    }
    if (h.wlan.fromPeer("P", killRx)) {
        if (killRx.kind == TransportKind::KillFrame) ok = false;
    }

    HiveKill killer;
    killer.attach(&h.keys);
    Event req{};
    if (!killer.fill(&req, "W", 1100, 0)) ok = false;
    if (killer.canWriteFlag()) ok = false;
    p30_inject(h, ROLE_WORKER, "W", req, 1100, true);
    if (h.xport.hiveFrozen()) ok = false;
    if (!p30_has_type(h.vault, EventType::PolicyViolation)) ok = false;
    (void)h.xport.enterHiveDown(1100);
    if (!h.xport.hiveFrozen()) ok = false;

    bool sawKill = false;
    TransportFrame fr;
    while (h.wlan.fromPeer("P", fr)) {
        if (fr.kind == TransportKind::KillFrame) sawKill = true;
    }
    while (h.wlan.fromPeer("W", fr)) {
        if (fr.kind == TransportKind::KillFrame) sawKill = true;
    }
    if (sawKill) ok = false;

    Event log = p30_event(EventType::Heartbeat, "W", 1200, nullptr);
    (void)h.vault.signEvent(log);
    if (h.vault.store(log, 1200)) ok = false;
    h.down.execute(1300);
    h.down.tick(1305);
    if (h.down.snapshotCount() == 0) ok = false;
    if (!h.down.killSent()) ok = false;

    bool sawKill2 = false;
    while (h.wlan.fromPeer("P", fr)) {
        if (fr.kind == TransportKind::KillFrame) sawKill2 = true;
    }
    if (!sawKill2) ok = false;

    peer_halt_reset();
    peer_halt_run(ROLE_PHONE, "P");
    if (peer_halt_can_tx()) ok = false;
    if (!peer_halt_has_marker("P")) ok = false;
    peer_halt_reset();
    peer_halt_run(ROLE_WORKER, "W");
    if (peer_halt_can_tx()) ok = false;
    if (!peer_halt_has_marker("W")) ok = false;
    if (!h.keys.hasRoot()) ok = false;

    printf(ok ? "PASS p30_kill\n" : "FAIL p30_kill\n");
    return ok ? 0 : 1;
}
