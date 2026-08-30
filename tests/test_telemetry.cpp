#include <cstdio>
#include "../src/psp/ghost_core.h"
#include "../src/psp/registry.h"
#include "../src/psp/ghost_vault.h"
#include "../src/psp/decision_pipeline.h"
#include "../src/psp/ghost_telemetry.h"
#include "../src/psp/ghost_keys.h"

static bool ok = true;

static void chk(bool v, const char* name) {
    if (v) printf("PASS %s\n", name);
    else {
        printf("FAIL %s\n", name);
        ok = false;
    }
}

int main() {
    Registry reg;
    GhostVault vault;
    vault.init();
    DecisionPipeline pipe;
    pipe.attach(&reg, &vault);

    Device w{};
    w.id[0] = 'W';
    w.id[1] = '\0';
    w.role = ROLE_WORKER;
    w.trust_level = 2;
    w.status = DeviceState::Pending;
    chk(reg.addDevice(w), "add worker");

    Device m{};
    m.id[0] = 'M';
    m.id[1] = '\0';
    m.role = ROLE_MINE;
    m.trust_level = 1;
    m.status = DeviceState::Pending;
    chk(reg.addDevice(m), "add mine");

    const Device* dw = reg.getDevice("W");
    chk(dw != nullptr && dw->ram_mb == TELEM_ABSENT16, "v1 absent");

    Event ev{};
    ev.type = EventType::TelemetryUpdate;
    ev.source_device_id[0] = 'W';
    ev.source_device_id[1] = '\0';
    ev.timestamp = 50;
    ev.severity = Severity::Critical;
    fillTelemetryPayload(&ev, 512, 12, 4, 32, 87, 54);
    chk(pipe.process(ev, 50) == PipelineResult::Accepted, "worker telem");
    dw = reg.getDevice("W");
    chk(dw != nullptr && dw->ram_mb == 512 && dw->cpu_percent == 12, "registry write");
    chk(dw->last_seen == 50, "trust2 last_seen event ts");
    chk(vault.getStoredCount() == 1, "store");

    Event dense = ev;
    dense.timestamp = 51;
    fillTelemetryPayload(&dense, 513, 13, 5, 33, 86, 54);
    chk(pipe.process(dense, 50) == PipelineResult::Blocked, "rate <1s");
    chk(pipe.telemDenseDrops("W") >= 1, "dense counter");
    dw = reg.getDevice("W");
    chk(dw != nullptr && dw->ram_mb == 512, "rate no write");

    Event later = ev;
    later.timestamp = 52;
    fillTelemetryPayload(&later, 600, 20, 8, 40, 80, 48);
    chk(pipe.process(later, 51) == PipelineResult::Accepted, "next second");
    dw = reg.getDevice("W");
    chk(dw != nullptr && dw->ram_mb == 600, "second write");

    chk(pipe.process(later, 60) == PipelineResult::Blocked, "replay");
    dw = reg.getDevice("W");
    chk(dw != nullptr && dw->ram_mb == 600, "replay no write");

    Event unk{};
    unk.type = EventType::TelemetryUpdate;
    unk.source_device_id[0] = 'Z';
    unk.source_device_id[1] = '\0';
    unk.timestamp = 70;
    fillTelemetryPayload(&unk, 1, 1, 1, 1, 1, 1);
    chk(pipe.process(unk, 70) == PipelineResult::Rejected, "unknown drop");

    Event mine{};
    mine.type = EventType::TelemetryUpdate;
    mine.source_device_id[0] = 'M';
    mine.source_device_id[1] = '\0';
    mine.timestamp = 80;
    fillTelemetryPayload(&mine, 1, 1, 1, 1, 1, 1);
    chk(pipe.process(mine, 80) == PipelineResult::Rejected, "mine drop");
    const Device* dm = reg.getDevice("M");
    chk(dm != nullptr && dm->ram_mb == TELEM_ABSENT16, "mine no telem");

    Event bad{};
    bad.type = EventType::TelemetryUpdate;
    bad.source_device_id[0] = 'W';
    bad.source_device_id[1] = '\0';
    bad.timestamp = 200;
    fillTelemetryPayload(&bad, 1, 1, 1, 1, 1, 1);
    bad.payload[0] = 0;
    uint32_t seen = dw != nullptr ? dw->last_seen : 0;
    chk(pipe.process(bad, 200) == PipelineResult::Rejected, "bad magic drop");
    dw = reg.getDevice("W");
    chk(dw != nullptr && dw->last_seen == seen && dw->ram_mb == 600, "reject no last_seen");

    chk(reg.removeDevice("W"), "remove worker");
    chk(reg.addDevice(w), "re-add worker");
    chk(pipe.process(later, 90) == PipelineResult::Accepted, "recycle replay window");

    {
        Registry r0;
        GhostVault v0;
        v0.init();
        DecisionPipeline p0;
        p0.attach(&r0, &v0);
        Device w0{};
        w0.id[0] = 'W';
        w0.id[1] = '\0';
        w0.role = ROLE_WORKER;
        w0.trust_level = 2;
        w0.status = DeviceState::Pending;
        chk(r0.addDevice(w0), "rate0 enroll");
        Event a{};
        a.type = EventType::TelemetryUpdate;
        a.source_device_id[0] = 'W';
        a.source_device_id[1] = '\0';
        a.timestamp = 1;
        fillTelemetryPayload(&a, 8, 1, 1, 1, 1, 1);
        chk(p0.process(a, 0) == PipelineResult::Accepted, "now 0 first");
        Event b = a;
        b.timestamp = 2;
        fillTelemetryPayload(&b, 9, 2, 2, 2, 2, 2);
        chk(p0.process(b, 0) == PipelineResult::Blocked, "now 0 rate");
        Event c = a;
        c.timestamp = 1;
        fillTelemetryPayload(&c, 10, 3, 3, 3, 3, 3);
        chk(p0.process(c, 5) == PipelineResult::Blocked, "same event ts");
        chk(p0.telemDenseDrops("W") >= 2, "dense ts+now");
    }

    {
        Registry rs;
        GhostVault vs;
        vs.init();
        GhostKeys keys;
        uint8_t kb[KEY_LEN];
        for (uint8_t i = 0; i < KEY_LEN; ++i) kb[i] = static_cast<uint8_t>(i + 3);
        chk(keys.provisionDerived(kb, KEY_LEN), "derived keys");
        vs.attachKeys(&keys);
        DecisionPipeline ps;
        ps.attach(&rs, &vs);
        Device ws{};
        ws.id[0] = 'W';
        ws.id[1] = '\0';
        ws.role = ROLE_WORKER;
        ws.trust_level = 2;
        ws.status = DeviceState::Pending;
        chk(rs.addDevice(ws), "hmac enroll");
        Event se{};
        se.type = EventType::TelemetryUpdate;
        se.source_device_id[0] = 'W';
        se.source_device_id[1] = '\0';
        se.timestamp = 40;
        se.severity = Severity::Info;
        fillTelemetryPayload(&se, 256, 11, 3, 16, 70, 24);
        chk(vs.signEvent(se), "sign telem");
        chk(vs.verifyEvent(se), "verify telem");
        chk(static_cast<uint8_t>(se.payload[88]) != 0, "mac region live");
        chk(ps.process(se, 40) == PipelineResult::Accepted, "signed telem accept");
        const Device* ds = rs.getDevice("W");
        chk(ds != nullptr && ds->ram_mb == 256, "signed write");
    }

    printf(ok ? "PASS telemetry\n" : "FAIL telemetry\n");
    return ok ? 0 : 1;
}
