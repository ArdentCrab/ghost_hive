#include "p30_harness.h"
#include "../src/laptop/alert.h"

static void fail(bool* ok, bool cond, const char* msg) {
    if (cond) return;
    fprintf(stderr, "p30_worker: %s\n", msg);
    *ok = false;
}

int main() {
    bool ok = true;
    P30Hive h;
    p30_attach(h);
    fail(&ok, p30_bind_root(h), "bind_root");

    p30_enroll(h, "W", ROLE_WORKER, 2);
    Device mineDev{};
    p30_copy_id(mineDev.id, "X");
    mineDev.role = ROLE_MINE;
    mineDev.status = DeviceState::Silent;
    fail(&ok, h.reg.addDevice(mineDev), "add mine");
    fail(&ok, h.reg.setState("X", DeviceState::Suspected), "mine suspected");

    Event anom = p30_event(EventType::AnomalyDetected, "W", 3000, "flow");
    anom.severity = Severity::Warn;
    p30_inject(h, ROLE_WORKER, "W", anom, 3000, true);
    fail(&ok, h.reg.getState("W") != DeviceState::GhostDown, "worker not auto-down");

    h.xport.tick(3000);
    fail(&ok, h.xport.dangerMode(), "danger");
    fail(&ok, !h.xport.hiveFrozen(), "frozen");

    Event miss = p30_event(EventType::HeartbeatMiss, "W", 3100, "worker_degraded");
    miss.severity = Severity::Warn;
    p30_inject(h, ROLE_WORKER, "W", miss, 3100, true);

    fail(&ok, p30_has_type(h.vault, EventType::AnomalyDetected), "anom stored");
    fail(&ok, p30_hmac_any(h.vault, 'V'), "hmac V");

    h.down.execute(3200);
    h.down.tick(3205);
    fail(&ok, h.vault.getStoredCount() != 0, "vault nonempty");
    fail(&ok, h.down.snapshotCount() != 0, "snapshot");
    fail(&ok, h.down.killSent(), "killSent");

    HiveAlert alert;
    alert.init();
    Event danger = p30_event(EventType::DangerModeEnter, "kernel", 3300, nullptr);
    (void)alert.ingest(danger);
    Event downEv = p30_event(EventType::GhostDownStart, "kernel", 3300, "kill");
    (void)alert.ingest(downEv);
    fail(&ok, alert.danger() && alert.down() && alert.kill(), "alert flags");

    char buf[OUTPUT_BUFFER_LEN];
    GhostOutput out;
    out.buildDanger(buf);
    fail(&ok, p30_contains(buf, "passive"), "danger cli");

    printf(ok ? "PASS p30_worker\n" : "FAIL p30_worker\n");
    return ok ? 0 : 1;
}
