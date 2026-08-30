#include "p30_harness.h"

static void fail(bool* ok, bool cond, const char* msg) {
    if (cond) return;
    fprintf(stderr, "p30_mitm: %s\n", msg);
    *ok = false;
}

int main() {
    bool ok = true;
    P30Hive h;
    p30_attach(h);
    fail(&ok, p30_bind_root(h), "bind_root");

    p30_enroll(h, "W", ROLE_WORKER, 2);
    p30_enroll(h, "R", ROLE_ROUTER, 1);

    Event hb = p30_event(EventType::Heartbeat, "W", 1000, nullptr);
    p30_inject(h, ROLE_WORKER, "W", hb, 1000, true);
    const Device* w = h.reg.getDevice("W");
    fail(&ok, w != nullptr, "worker enrolled");
    uint32_t seen = (w != nullptr) ? w->last_seen : 0;
    uint8_t before = h.vault.getStoredCount();

    Event mitm = p30_event(EventType::Heartbeat, "W", 2000, "tamper");
    (void)h.vault.signEvent(mitm);
    mitm.payload[0] = 'X';
    p30_inject(h, ROLE_WORKER, "W", mitm, 2000, false);

    Event naked = p30_event(EventType::Heartbeat, "R", 2100, "scan");
    p30_inject(h, ROLE_ROUTER, "R", naked, 2100, false);

    w = h.reg.getDevice("W");
    fail(&ok, w != nullptr && w->last_seen == seen, "worker last_seen unchanged");
    fail(&ok, h.reg.getState("W") != DeviceState::GhostDown, "worker not GhostDown");
    fail(&ok, !h.xport.dangerMode(), "no danger after hmac-i");
    fail(&ok, !h.xport.hiveFrozen(), "no freeze after hmac-i");
    fail(&ok, !h.vault.frozen(), "vault not frozen after hmac-i");
    fail(&ok, h.vault.getStoredCount() > before, "hmac-i evidence stored");
    fail(&ok, p30_hmac_any(h.vault, 'I'), "hmac mark I");
    fail(&ok, p30_has_type(h.vault, EventType::PolicyViolation), "policy violation");

    char buf[OUTPUT_BUFFER_LEN];
    GhostOutput out;
    out.buildVault(h.vault, buf);
    fail(&ok, p30_contains(buf, "I N"), "vault H-column I");

    P30Hive spoof;
    p30_attach(spoof);
    fail(&ok, p30_bind_root(spoof), "spoof bind_root");
    p30_enroll(spoof, "W", ROLE_WORKER, 2);
    p30_enroll(spoof, "F", ROLE_SENSOR, 1);
    Event fake = p30_event(EventType::Heartbeat, "W", 5000, nullptr);
    (void)spoof.vault.signEvent(fake);
    TransportFrame sf;
    transport_clear_frame(sf);
    sf.kind = TransportKind::EventFrame;
    sf.src_role = ROLE_SENSOR;
    sf.dst_role = ROLE_KERNEL;
    p30_copy_id(sf.src_id, "F");
    p30_copy_id(sf.dst_id, KERNEL_SOURCE_ID);
    sf.event = fake;
    p30_copy_id(sf.event.source_device_id, "W");
    sf.stamp = 5000;
    (void)spoof.wlan.toKernel(sf);
    spoof.xport.rx(5000);
    fail(&ok, !spoof.xport.hiveFrozen(), "false peer no down");

    P30Hive park;
    p30_attach(park);
    fail(&ok, p30_bind_root(park), "park bind_root");
    Event stranger = p30_event(EventType::Heartbeat, "Z", 6000, nullptr);
    p30_inject(park, ROLE_SENSOR, "Z", stranger, 6000, true);
    const Device* z = park.reg.getDevice("Z");
    fail(&ok, z != nullptr && z->status == DeviceState::Pending, "unknown stays pending");
    fail(&ok, !park.xport.hiveFrozen(), "unknown no freeze");
    fail(&ok, p30_has_type(park.vault, EventType::DeviceSeen), "DeviceSeen");

    P30Hive replay;
    p30_attach(replay);
    fail(&ok, p30_bind_root(replay), "replay bind_root");
    p30_enroll(replay, "W", ROLE_WORKER, 2);
    Event beat = p30_event(EventType::Heartbeat, "W", 7000, nullptr);
    p30_inject(replay, ROLE_WORKER, "W", beat, 7000, true);
    uint32_t seen2 = replay.reg.getDevice("W")->last_seen;
    p30_inject(replay, ROLE_WORKER, "W", beat, 7000, true);
    fail(&ok, replay.reg.getDevice("W")->last_seen == seen2, "ts replay no last_seen bump");
    fail(&ok, !replay.xport.hiveFrozen(), "ts replay no freeze");

    P30Hive spam;
    p30_attach(spam);
    fail(&ok, p30_bind_root(spam), "spam bind_root");
    p30_enroll(spam, "F", ROLE_SENSOR, 1);
    Event s1 = p30_event(EventType::ScanResult, "F", 8000, "os");
    Event s2 = p30_event(EventType::ScanResult, "F", 8010, "app");
    Event s3 = p30_event(EventType::ScanResult, "F", 8020, "net");
    p30_inject(spam, ROLE_SENSOR, "F", s1, 8000, true);
    p30_inject(spam, ROLE_SENSOR, "F", s2, 8010, true);
    p30_inject(spam, ROLE_SENSOR, "F", s3, 8020, true);
    fail(&ok, p30_has_type(spam.vault, EventType::PolicyViolation), "spam violation");
    fail(&ok, spam.reg.getState("F") == DeviceState::Blocked, "spam blocked");
    fail(&ok, !spam.xport.hiveFrozen(), "spam no freeze");

    printf(ok ? "PASS p30_mitm\n" : "FAIL p30_mitm\n");
    return ok ? 0 : 1;
}
