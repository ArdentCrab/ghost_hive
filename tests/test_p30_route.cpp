#include "p30_harness.h"
#include "../src/laptop/alert.h"

int main() {
    bool ok = true;
    P30Hive h;
    p30_attach(h);
    if (!p30_bind_root(h)) ok = false;
    p30_enroll(h, "W", ROLE_WORKER, 2);
    p30_enroll(h, "P", ROLE_PHONE, 1);

    Event danger = p30_event(EventType::DangerModeEnter, "W", 4000, nullptr);
    p30_inject(h, ROLE_WORKER, "W", danger, 4000, true);
    if (!h.xport.dangerMode()) ok = false;
    if (!p30_has_type(h.vault, EventType::DangerModeEnter)) ok = false;
    if (!p30_hmac_any(h.vault, 'V')) ok = false;

    HiveAlert phone;
    phone.init();
    HiveAlert laptop;
    laptop.init();

    TransportFrame fr;
    bool phoneDanger = false;
    bool laptopDanger = false;
    while (h.wlan.fromPeer("P", fr)) {
        if (fr.event.type == EventType::DangerModeEnter) {
            phoneDanger = true;
            (void)phone.ingest(fr.event);
        }
        if (fr.kind == TransportKind::KillFrame) ok = false;
    }
    while (h.wlan.fromPeer("W", fr)) {
        if (fr.event.type == EventType::DangerModeEnter) {
            laptopDanger = true;
            (void)laptop.ingest(fr.event);
        }
        if (fr.kind == TransportKind::KillFrame) ok = false;
    }
    if (!phoneDanger || !laptopDanger) ok = false;
    if (!phone.danger() || !laptop.danger()) ok = false;

    Event downEv = p30_event(EventType::GhostDownStart, "kernel", 4100, "kill");
    downEv.severity = Severity::Critical;
    (void)h.vault.signEvent(downEv);
    (void)h.xport.route(downEv, 4100);
    if (!p30_has_type(h.vault, EventType::GhostDownStart)) {
        (void)h.vault.store(downEv, 4100);
    }

    bool phoneDown = false;
    bool laptopDown = false;
    bool inboundKill = false;
    while (h.wlan.fromPeer("P", fr)) {
        if (fr.kind == TransportKind::KillFrame) inboundKill = true;
        if (fr.event.type == EventType::GhostDownStart) {
            phoneDown = true;
            (void)phone.ingest(fr.event);
        }
    }
    while (h.wlan.fromPeer("W", fr)) {
        if (fr.kind == TransportKind::KillFrame) inboundKill = true;
        if (fr.event.type == EventType::GhostDownStart) {
            laptopDown = true;
            (void)laptop.ingest(fr.event);
        }
    }
    if (inboundKill) ok = false;
    if (!phoneDown || !laptopDown) ok = false;
    if (!phone.down() || !laptop.down()) ok = false;

    h.down.execute(4200);
    h.down.tick(4205);
    if (!h.down.killSent()) ok = false;

    bool phoneKill = false;
    bool laptopKill = false;
    while (h.wlan.fromPeer("P", fr)) {
        if (fr.kind == TransportKind::KillFrame) {
            phoneKill = true;
            (void)phone.ingest(fr.event);
        }
    }
    while (h.wlan.fromPeer("W", fr)) {
        if (fr.kind == TransportKind::KillFrame) {
            laptopKill = true;
            (void)laptop.ingest(fr.event);
        }
    }
    if (!phoneKill || !laptopKill) ok = false;
    if (!phone.kill() || !laptop.kill()) ok = false;
    if (h.xport.dangerMode() != true && h.xport.kernelDown()) ok = false;

    char buf[OUTPUT_BUFFER_LEN];
    GhostOutput out;
    out.buildAlert(buf);
    if (!p30_contains(buf, "fired")) ok = false;
    out.buildDanger(buf);
    if (!p30_contains(buf, "passive")) ok = false;

    printf(ok ? "PASS p30_route\n" : "FAIL p30_route\n");
    return ok ? 0 : 1;
}
