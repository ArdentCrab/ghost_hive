// hive_redteam — SPEC-v2 peer attack matrix with T1/T2/T3 latency.
// Attacks Hive peers only (no PSP direct, no radio HW).

#include "p30_harness.h"
#include "watch_hud.h"
#include "ghost_telemetry.h"
#include "ghost_scanner.h"
#include "kill.h"
#include "mine.h"
#include "os_mine.h"
#include "browser_mine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct AttackRow {
    uint32_t id;
    char family[4];
    char role[4];
    char device_id[8];
    char vector[24];
    char param[64];
    uint8_t exp_headline;
    uint8_t exp_alert;
    uint8_t exp_drop;
    uint8_t exp_block;
};

struct Outcome {
    uint32_t t1_us;
    uint32_t t2_us;
    uint32_t t3_us;
    uint8_t headline;
    uint8_t alert;
    uint8_t drop;
    uint8_t block;
    uint8_t down;
    uint8_t registry;
    int8_t pipe_code;
    char notes[32];
};

static uint32_t g_total = 0;
static uint32_t g_match = 0;
static uint32_t g_mismatch = 0;

static uint64_t mono_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull +
           static_cast<uint64_t>(ts.tv_nsec);
}

static uint32_t ns_to_us(uint64_t d) {
    if (d > 4000000000ull) return 4000000000u;
    return static_cast<uint32_t>(d / 1000ull);
}

static bool str_eq(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) return false;
    return strcmp(a, b) == 0;
}

static bool alert_active(const char* al) {
    return al[0] != '\0' && !(al[0] == '-' && al[1] == '-' && al[2] == '\0');
}

static uint8_t role_from_char(char c) {
    switch (c) {
        case 'W': return ROLE_WORKER;
        case 'P': return ROLE_PHONE;
        case 'R': return ROLE_ROUTER;
        case 'N': return ROLE_SAFE;
        case 'F': return ROLE_SENSOR;
        case 'M': return ROLE_MINE;
        default: return ROLE_SENSOR;
    }
}

static void enroll_mine(Registry& reg, ReplayGuard& replay, GhostKeys& keys,
                        const char* id) {
    Device md{};
    p30_copy_id(md.id, id);
    md.role = ROLE_MINE;
    md.status = DeviceState::Silent;
    (void)reg.addDevice(md);
    if (keys.totpSeed() != nullptr) {
        (void)replay.setTotpSeed(id, keys.totpSeed(), TOTP_SEED_LEN);
    }
}

static void seed_telemetry(P30Hive& h, const char* id, uint8_t role, uint32_t now) {
    Event ev{};
    ev.type = EventType::TelemetryUpdate;
    p30_copy_id(ev.source_device_id, id);
    ev.timestamp = now;
    if (role == ROLE_PHONE) {
        fillTelemetryPayload(&ev, 512, 12, TELEM_ABSENT8, TELEM_ABSENT16, 87, 54);
    } else if (role == ROLE_SENSOR) {
        fillTelemetryPayload(&ev, TELEM_ABSENT16, 12, TELEM_ABSENT8, TELEM_ABSENT16,
                             87, TELEM_ABSENT16);
    } else if (role == ROLE_ROUTER) {
        fillTelemetryPayload(&ev, TELEM_ABSENT16, TELEM_ABSENT8, TELEM_ABSENT8, 32,
                             TELEM_ABSENT8, 54);
    } else {
        fillTelemetryPayload(&ev, 512, 12, 4, 32, 87, 54);
    }
    (void)h.pipe.process(ev, now);
}

static void redteam_setup(P30Hive& h) {
    p30_attach(h);
    (void)p30_bind_root(h);
    p30_enroll(h, "W", ROLE_WORKER, 2);
    p30_enroll(h, "P", ROLE_PHONE, 1);
    p30_enroll(h, "R", ROLE_ROUTER, 1);
    p30_enroll(h, "N", ROLE_SAFE, 1);
    p30_enroll(h, "F", ROLE_SENSOR, 1);
    for (uint8_t i = 0; i < 8; ++i) {
        static const char* ids[] = {"X", "ML", "MP", "MR", "MN", "MB", "MO", "MI"};
        enroll_mine(h.reg, h.pipe.replay(), h.keys, ids[i]);
    }
    h.stealth.enterTerminalMode();
    Event boot = p30_event(EventType::Heartbeat, "W", 10, nullptr);
    p30_inject(h, ROLE_WORKER, "W", boot, 10, true);
    Event ph = p30_event(EventType::Heartbeat, "P", 10, nullptr);
    p30_inject(h, ROLE_PHONE, "P", ph, 10, true);
    seed_telemetry(h, "W", ROLE_WORKER, 15);
    seed_telemetry(h, "P", ROLE_PHONE, 15);
    seed_telemetry(h, "R", ROLE_ROUTER, 15);
    seed_telemetry(h, "F", ROLE_SENSOR, 15);
}

static void fill_telem_valid(Event* ev, uint8_t role) {
    if (role == ROLE_PHONE) {
        fillTelemetryPayload(ev, 512, 12, TELEM_ABSENT8, TELEM_ABSENT16, 87, 54);
    } else if (role == ROLE_SENSOR) {
        fillTelemetryPayload(ev, TELEM_ABSENT16, 12, TELEM_ABSENT8, TELEM_ABSENT16,
                             87, TELEM_ABSENT16);
    } else if (role == ROLE_ROUTER) {
        fillTelemetryPayload(ev, TELEM_ABSENT16, TELEM_ABSENT8, TELEM_ABSENT8, 32,
                             TELEM_ABSENT8, 54);
    } else {
        fillTelemetryPayload(ev, 512, 12, 4, 32, 87, 54);
    }
}

static uint32_t device_seen(const Registry& reg, const char* id) {
    const Device* d = reg.getDevice(id);
    return (d != nullptr) ? d->last_seen : 0;
}

static uint8_t wire_drop(const P30Hive& h, const char* id, uint32_t seen,
                         uint8_t hmac_before) {
    if (h.xport.hmacICount() > hmac_before) return 1;
    return (device_seen(h.reg, id) == seen) ? 1 : 0;
}

static void build_src(P30Hive& h, GhostScanner* scan,
                      GhostOutput::WatchSrc* s) {
    memset(s, 0, sizeof(*s));
    s->registry = &h.reg;
    s->vault = &h.vault;
    s->pipeline = &h.pipe;
    s->replay = &h.pipe.replay();
    s->heartbeat = &h.pipe.heartbeat();
    s->down = &h.down;
    s->scanner = scan;
    s->hmac_alert = h.xport.hmacIAlerted();
    s->hmac_i = h.xport.hmacICount();
}

static void measure_ui(P30Hive& h, GhostScanner* scan, uint64_t t0,
                       Outcome* o) {
    GhostOutput::WatchSrc src;
    build_src(h, scan, &src);
    uint64_t t2 = mono_ns();
    o->headline = watch_danger_headline(src) ? 1 : 0;
    o->t2_us = ns_to_us(t2 - t0);
    char al[48];
    uint64_t t3 = mono_ns();
    watch_fill_alert(src, al);
    o->alert = alert_active(al) ? 1 : 0;
    o->t3_us = ns_to_us(t3 - t0);
    (void)al;
}

static void inject_frame(P30Hive& h, TransportFrame& frame, uint32_t now,
                         uint64_t t0, Outcome* o) {
    (void)h.wlan.toKernel(frame);
    h.xport.rx(now);
    uint64_t t1 = mono_ns();
    o->t1_us = ns_to_us(t1 - t0);
}

static void inject_event(P30Hive& h, uint8_t role, const char* id, Event ev,
                         uint32_t now, bool sign, uint64_t t0, Outcome* o) {
    if (sign) (void)h.vault.signEvent(ev);
    TransportFrame frame;
    transport_clear_frame(frame);
    frame.kind = TransportKind::EventFrame;
    frame.src_role = role;
    frame.dst_role = ROLE_KERNEL;
    p30_copy_id(frame.src_id, id);
    p30_copy_id(frame.dst_id, KERNEL_SOURCE_ID);
    frame.event = ev;
    p30_copy_id(frame.event.source_device_id, id);
    frame.stamp = now;
    inject_frame(h, frame, now, t0, o);
}

static void run_unsigned(P30Hive& h, uint8_t role, const char* id, uint32_t now,
                         EventType type, const char* pay, Outcome* o) {
    uint32_t seen = device_seen(h.reg, id);
    uint8_t hi = h.xport.hmacICount();
    uint64_t t0 = mono_ns();
    Event ev = p30_event(type, id, now, pay);
    inject_event(h, role, id, ev, now, false, t0, o);
    o->drop = wire_drop(h, id, seen, hi);
    o->registry = (h.vault.getStoredCount() > 0) ? 1 : 0;
    o->pipe_code = -1;
    measure_ui(h, nullptr, t0, o);
    o->down = (h.xport.hiveFrozen() && h.down.isActive()) ? 1 : 0;
}

static void run_hmac_flip(P30Hive& h, uint8_t role, const char* id, uint32_t bit,
                          uint32_t now, Outcome* o) {
    uint32_t seen = device_seen(h.reg, id);
    uint8_t vb = h.vault.getStoredCount();
    uint8_t hi = h.xport.hmacICount();
    uint64_t t0 = mono_ns();
    Event ev = p30_event(EventType::Heartbeat, id, now, "flip");
    (void)h.vault.signEvent(ev);
    uint8_t off = static_cast<uint8_t>(bit % VAULT_MAC_HEX);
    char c = ev.payload[VAULT_MAC_OFF + off];
    ev.payload[VAULT_MAC_OFF + off] = (c == '0') ? '1' : '0';
    inject_event(h, role, id, ev, now, false, t0, o);
    o->drop = wire_drop(h, id, seen, hi);
    o->registry = (h.vault.getStoredCount() > vb || p30_hmac_any(h.vault, 'I')) ? 1 : 0;
    o->pipe_code = -1;
    measure_ui(h, nullptr, t0, o);
    o->down = h.down.isActive() ? 1 : 0;
}

static void run_wrong_src(P30Hive& h, const char* id, uint8_t role, uint32_t now,
                          Outcome* o) {
    uint32_t seen = device_seen(h.reg, id);
    uint8_t vb = h.vault.getStoredCount();
    uint8_t hi = h.xport.hmacICount();
    uint64_t t0 = mono_ns();
    Event ev = p30_event(EventType::Heartbeat, id, now, "spoof");
    (void)h.vault.signEvent(ev);
    TransportFrame frame;
    transport_clear_frame(frame);
    frame.kind = TransportKind::EventFrame;
    if (role == ROLE_SENSOR) {
        frame.src_role = ROLE_WORKER;
        p30_copy_id(frame.src_id, "W");
    } else {
        frame.src_role = ROLE_SENSOR;
        p30_copy_id(frame.src_id, "F");
    }
    frame.dst_role = ROLE_KERNEL;
    p30_copy_id(frame.dst_id, KERNEL_SOURCE_ID);
    frame.event = ev;
    p30_copy_id(frame.event.source_device_id, id);
    frame.stamp = now;
    inject_frame(h, frame, now, t0, o);
    o->drop = wire_drop(h, id, seen, hi);
    o->registry = (h.vault.getStoredCount() > vb) ? 1 : 0;
    o->pipe_code = -1;
    measure_ui(h, nullptr, t0, o);
    o->down = h.down.isActive() ? 1 : 0;
}

static void run_hb_replay(P30Hive& h, const char* id, uint8_t role, uint32_t now,
                          Outcome* o) {
    Event beat = p30_event(EventType::Heartbeat, id, now, nullptr);
    p30_inject(h, role, id, beat, now, true);
    uint64_t t0 = mono_ns();
    p30_inject(h, role, id, beat, now, true);
    uint64_t t1 = mono_ns();
    o->t1_us = ns_to_us(t1 - t0);
    o->drop = 0;
    o->registry = 1;
    o->pipe_code = -1;
    measure_ui(h, nullptr, t0, o);
    o->down = h.down.isActive() ? 1 : 0;
}

static void run_telem_bad_magic(P30Hive& h, const char* id, uint8_t role,
                                uint32_t now, Outcome* o) {
    uint64_t t0 = mono_ns();
    Event ev{};
    ev.type = EventType::TelemetryUpdate;
    p30_copy_id(ev.source_device_id, id);
    ev.timestamp = now;
    fill_telem_valid(&ev, role);
    ev.payload[0] = static_cast<char>(0x41);
    PipelineResult pr = h.pipe.process(ev, now);
    uint64_t t1 = mono_ns();
    o->t1_us = ns_to_us(t1 - t0);
    o->pipe_code = static_cast<int8_t>(pr);
    o->drop = (pr != PipelineResult::Accepted) ? 1 : 0;
    o->registry = o->drop;
    measure_ui(h, nullptr, t0, o);
    o->down = h.down.isActive() ? 1 : 0;
}

static void run_telem_bad_pct(P30Hive& h, const char* id, uint8_t role, uint32_t pct,
                              uint32_t now, Outcome* o) {
    uint64_t t0 = mono_ns();
    Event ev{};
    ev.type = EventType::TelemetryUpdate;
    p30_copy_id(ev.source_device_id, id);
    ev.timestamp = now;
    fill_telem_valid(&ev, role);
    if (role == ROLE_ROUTER) ev.payload[4] = static_cast<char>(pct);
    else ev.payload[3] = static_cast<char>(pct);
    PipelineResult pr = h.pipe.process(ev, now);
    uint64_t t1 = mono_ns();
    o->t1_us = ns_to_us(t1 - t0);
    o->pipe_code = static_cast<int8_t>(pr);
    o->drop = (pr != PipelineResult::Accepted) ? 1 : 0;
    o->registry = o->drop;
    measure_ui(h, nullptr, t0, o);
    o->down = h.down.isActive() ? 1 : 0;
}

static void run_telem_dense(P30Hive& h, const char* id, uint8_t role, uint32_t now,
                            Outcome* o) {
    Event ev{};
    ev.type = EventType::TelemetryUpdate;
    p30_copy_id(ev.source_device_id, id);
    ev.timestamp = now;
    fill_telem_valid(&ev, role);
    (void)h.pipe.process(ev, now);
    uint64_t t0 = mono_ns();
    PipelineResult pr = h.pipe.process(ev, now);
    uint64_t t1 = mono_ns();
    o->t1_us = ns_to_us(t1 - t0);
    o->pipe_code = static_cast<int8_t>(pr);
    o->drop = (h.pipe.telemDenseDrops(id) > 0) ? 1 : 0;
    o->registry = o->drop;
    measure_ui(h, nullptr, t0, o);
    o->down = h.down.isActive() ? 1 : 0;
}

static void run_telem_absent(P30Hive& h, uint32_t now, Outcome* o) {
    Device* pd = const_cast<Device*>(h.reg.getDevice("P"));
    if (pd != nullptr) {
        pd->last_seen = now;
        device_telem_clear(pd);
    }
    uint64_t t0 = mono_ns();
    measure_ui(h, nullptr, t0, o);
    o->t1_us = o->t2_us;
    o->registry = o->headline;
    o->pipe_code = -1;
    o->down = h.down.isActive() ? 1 : 0;
}

static void run_mine_trip(P30Hive& h, const char* mid, uint32_t counter,
                          uint32_t now, Outcome* o) {
    Mine m;
    m.init(mid);
    (void)m.setTotpSeed(h.keys.totpSeed(), TOTP_SEED_LEN);
    for (uint32_t c = 0; c < counter; ++c) {
        MinePayload step{};
        (void)m.send(&step, now + c * 90u);
    }
    MinePayload mp{};
    (void)m.sendTrip(&mp, now + counter * 90u);
    uint64_t t0 = mono_ns();
    p30_inject_mine(h, mp, now + counter * 90u, true);
    uint64_t t1 = mono_ns();
    o->t1_us = ns_to_us(t1 - t0);
    o->registry = (h.vault.getStoredCount() > 0) ? 1 : 0;
    o->pipe_code = -1;
    measure_ui(h, nullptr, t0, o);
    o->down = h.down.isActive() ? 1 : 0;
}

static void run_mine_replay(P30Hive& h, const char* mid, uint32_t counter,
                            uint32_t now, Outcome* o) {
    Mine m;
    m.init(mid);
    (void)m.setTotpSeed(h.keys.totpSeed(), TOTP_SEED_LEN);
    MinePayload a{};
    for (uint32_t c = 0; c < counter; ++c) {
        (void)m.send(&a, now + c * 90u);
    }
    p30_inject_mine(h, a, now + counter * 90u, true);
    uint64_t t0 = mono_ns();
    p30_inject_mine(h, a, now + counter * 90u + 1u, true);
    uint64_t t1 = mono_ns();
    o->t1_us = ns_to_us(t1 - t0);
    o->block = h.pipe.replay().isBlocked(mid) ? 1 : 0;
    o->registry = o->block;
    o->pipe_code = -1;
    measure_ui(h, nullptr, t0, o);
    o->down = h.down.isActive() ? 1 : 0;
}

static void run_twin_scan(GhostScanner& scan, Outcome* o) {
    uint64_t t0 = mono_ns();
    WifiNetwork nets[2];
    memset(nets, 0, sizeof(nets));
    const char* ss = "GHSTHIVE";
    for (uint8_t i = 0; i < 8; ++i) {
        nets[0].ssid[i] = ss[i];
        nets[1].ssid[i] = ss[i];
    }
    nets[0].rssi = -40;
    nets[0].channel = 6;
    nets[1].rssi = -70;
    nets[1].channel = 11;
    nets[1].encryption = 3;
    scan.setTerminalMode(true);
    (void)scan.loadWifiSnapshot(nets, 2);
    P30Hive h;
    redteam_setup(h);
    uint64_t t1 = mono_ns();
    o->t1_us = ns_to_us(t1 - t0);
    o->registry = 1;
    o->pipe_code = -1;
    measure_ui(h, &scan, t0, o);
    o->down = h.down.isActive() ? 1 : 0;
}

static void run_hb_miss(P30Hive& h, const char* id, uint32_t gap, uint32_t now,
                        Outcome* o) {
    h.pipe.heartbeat().send(id, now);
    uint64_t t0 = mono_ns();
    h.pipe.heartbeat().tick(now + gap, &h.reg);
    uint64_t t1 = mono_ns();
    o->t1_us = ns_to_us(t1 - t0);
    o->registry = (h.pipe.heartbeat().getMissCount(id) > 0) ? 1 : 0;
    o->pipe_code = -1;
    measure_ui(h, nullptr, t0, o);
    o->down = h.down.isActive() ? 1 : 0;
}

static void run_role_host(P30Hive& h, const char* fam, uint32_t seed,
                          Outcome* o) {
    uint32_t now = 100000u + seed * 17u;
    uint64_t t0 = mono_ns();
    if (str_eq(fam, "laptop.inject") || str_eq(fam, "post.inject") ||
        str_eq(fam, "post.osexploit_sim")) {
        Event ev = p30_event(EventType::AnomalyDetected, "W", now, "tamper");
        ev.severity = Severity::Critical;
        inject_event(h, ROLE_WORKER, "W", ev, now, true, t0, o);
    } else if (str_eq(fam, "laptop.kexploit_sim") || str_eq(fam, "pre.unsigned_hb") ||
               str_eq(fam, "phone.radio") || str_eq(fam, "nas.smb") ||
               str_eq(fam, "nas.inject")) {
        Event ev = p30_event(EventType::Heartbeat, "W", now, fam);
        uint8_t role = ROLE_WORKER;
        const char* id = "W";
        if (str_eq(fam, "phone.radio")) {
            role = ROLE_PHONE;
            id = "P";
            ev = p30_event(EventType::Heartbeat, "P", now, fam);
        } else if (str_eq(fam, "nas.smb") || str_eq(fam, "nas.inject")) {
            role = ROLE_SAFE;
            id = "N";
            ev = p30_event(EventType::Heartbeat, "N", now, fam);
        }
        uint32_t seen = device_seen(h.reg, id);
        uint8_t hi = h.xport.hmacICount();
        inject_event(h, role, id, ev, now, false, t0, o);
        o->drop = wire_drop(h, id, seen, hi);
    } else if (str_eq(fam, "laptop.filewrite") || str_eq(fam, "post.filetamper")) {
        Event ev = p30_event(EventType::ConfigChange, "W", now, "file");
        inject_event(h, ROLE_WORKER, "W", ev, now, true, t0, o);
    } else if (str_eq(fam, "laptop.netflood") || str_eq(fam, "post.netflood") ||
               str_eq(fam, "router.flood") || str_eq(fam, "pre.flood")) {
        Event ev = p30_event(EventType::ScanResult, "R", now, "flood");
        inject_event(h, ROLE_ROUTER, "R", ev, now, true, t0, o);
    } else if (str_eq(fam, "laptop.browser") || str_eq(fam, "mine.browser")) {
        BrowserMine br;
        br.init("ML");
        (void)br.setTotpSeed(h.keys.totpSeed(), TOTP_SEED_LEN);
        MinePayload mp{};
        (void)br.onSuspiciousUrl(&mp, now);
        p30_inject_mine(h, mp, now, true);
        uint64_t t1 = mono_ns();
        o->t1_us = ns_to_us(t1 - t0);
    } else if (str_eq(fam, "phone.app") || str_eq(fam, "phone.ostamper") ||
               str_eq(fam, "nas.file") || str_eq(fam, "mine.os") ||
               str_eq(fam, "mine.debugger")) {
        OsMine os;
        const char* mid = "MP";
        if (str_eq(fam, "nas.file")) mid = "MN";
        if (str_eq(fam, "mine.os") || str_eq(fam, "mine.debugger")) mid = "ML";
        os.init(mid);
        (void)os.setTotpSeed(h.keys.totpSeed(), TOTP_SEED_LEN);
        MinePayload mp{};
        (void)os.onSuspiciousProcess(&mp, now);
        p30_inject_mine(h, mp, now, true);
        uint64_t t1 = mono_ns();
        o->t1_us = ns_to_us(t1 - t0);
    } else if (str_eq(fam, "phone.replay") || str_eq(fam, "nas.replay") ||
               str_eq(fam, "mine.replay") || str_eq(fam, "pre.replay")) {
        const char* mid = "X";
        if (str_eq(fam, "phone.replay")) mid = "MP";
        if (str_eq(fam, "nas.replay")) mid = "MN";
        run_mine_replay(h, mid, 1, now, o);
        return;
    } else if (str_eq(fam, "router.portscan") || str_eq(fam, "pre.fakescan")) {
        Event ev = p30_event(EventType::ScanResult, "R", now, "port_scan");
        inject_event(h, ROLE_ROUTER, "R", ev, now, true, t0, o);
    } else if (str_eq(fam, "router.dns") || str_eq(fam, "pre.router_poison")) {
        Event ev = p30_event(EventType::ScanResult, "R", now, "dns_poison");
        inject_event(h, ROLE_ROUTER, "R", ev, now, true, t0, o);
    } else if (str_eq(fam, "router.arp") || str_eq(fam, "pre.arp")) {
        Event ev = p30_event(EventType::DeviceSeen, "R", now, "net:new_device");
        inject_event(h, ROLE_ROUTER, "R", ev, now, true, t0, o);
    } else if (str_eq(fam, "pre.peerkill") || str_eq(fam, "pre.phonekill")) {
        Event ev{};
        HiveKill killer;
        killer.attach(&h.keys);
        const char* id = str_eq(fam, "pre.phonekill") ? "P" : "W";
        (void)killer.fill(&ev, id, now, 0);
        inject_event(h, role_from_char(id[0]), id, ev, now, true, t0, o);
    } else if (str_eq(fam, "post.replay")) {
        uint32_t seen = device_seen(h.reg, "P");
        uint8_t hi = h.xport.hmacICount();
        Event ev = p30_event(EventType::Heartbeat, "P", now, "replay");
        inject_event(h, ROLE_PHONE, "P", ev, now, false, t0, o);
        o->drop = wire_drop(h, "P", seen, hi);
    } else {
        Event ev = p30_event(EventType::Heartbeat, "W", now, fam);
        inject_event(h, ROLE_WORKER, "W", ev, now, true, t0, o);
    }
    if (o->t1_us == 0) {
        uint64_t t1 = mono_ns();
        o->t1_us = ns_to_us(t1 - t0);
    }
    o->registry = (h.vault.getStoredCount() > 0) ? 1 : 0;
    if (o->block == 0 && h.pipe.replay().isBlocked("X")) {
        o->block = 1;
    }
    measure_ui(h, nullptr, t0, o);
    o->down = h.down.isActive() ? 1 : 0;
}

static void run_attack(const AttackRow& a, Outcome* o) {
    memset(o, 0, sizeof(*o));
    o->pipe_code = -1;
    uint32_t now = 50000u + (a.id % 10000u);
    const char* dev = a.device_id[0] != '\0' ? a.device_id : "W";
    uint8_t role = role_from_char(a.role[0]);

    if (str_eq(a.vector, "unsigned_hb")) {
        P30Hive h;
        redteam_setup(h);
        run_unsigned(h, role, dev, now, EventType::Heartbeat, "hb", o);
        return;
    }
    if (str_eq(a.vector, "unsigned_anom")) {
        P30Hive h;
        redteam_setup(h);
        run_unsigned(h, role, dev, now, EventType::AnomalyDetected, "anom", o);
        return;
    }
    if (str_eq(a.vector, "hmac_flip")) {
        P30Hive h;
        redteam_setup(h);
        run_hmac_flip(h, role, dev, static_cast<uint32_t>(atoi(a.param)), now, o);
        return;
    }
    if (str_eq(a.vector, "wrong_src")) {
        P30Hive h;
        redteam_setup(h);
        run_wrong_src(h, dev, role, now, o);
        return;
    }
    if (str_eq(a.vector, "hb_replay")) {
        P30Hive h;
        redteam_setup(h);
        run_hb_replay(h, dev, role, now, o);
        return;
    }
    if (str_eq(a.vector, "telem_bad_magic")) {
        P30Hive h;
        redteam_setup(h);
        run_telem_bad_magic(h, dev, role, now, o);
        return;
    }
    if (str_eq(a.vector, "telem_bad_pct")) {
        P30Hive h;
        redteam_setup(h);
        run_telem_bad_pct(h, dev, role, static_cast<uint32_t>(atoi(a.param)), now, o);
        return;
    }
    if (str_eq(a.vector, "telem_dense")) {
        P30Hive h;
        redteam_setup(h);
        run_telem_dense(h, dev, role, now, o);
        return;
    }
    if (str_eq(a.vector, "telem_absent")) {
        P30Hive h;
        redteam_setup(h);
        run_telem_absent(h, now, o);
        return;
    }
    if (str_eq(a.vector, "mine_trip")) {
        P30Hive h;
        redteam_setup(h);
        run_mine_trip(h, dev, static_cast<uint32_t>(atoi(a.param)), now, o);
        return;
    }
    if (str_eq(a.vector, "mine_replay")) {
        P30Hive h;
        redteam_setup(h);
        run_mine_replay(h, dev, static_cast<uint32_t>(atoi(a.param)), now, o);
        return;
    }
    if (str_eq(a.vector, "twin_scan")) {
        GhostScanner scan;
        run_twin_scan(scan, o);
        return;
    }
    if (str_eq(a.vector, "hb_miss")) {
        P30Hive h;
        redteam_setup(h);
        run_hb_miss(h, dev, static_cast<uint32_t>(atoi(a.param)), now, o);
        return;
    }
    if (str_eq(a.vector, "role_host")) {
        P30Hive h;
        redteam_setup(h);
        char fam[64];
        uint32_t seed = 0;
        const char* colon = strchr(a.param, ':');
        if (colon != nullptr) {
            size_t len = static_cast<size_t>(colon - a.param);
            if (len >= sizeof(fam)) len = sizeof(fam) - 1;
            memcpy(fam, a.param, len);
            fam[len] = '\0';
            seed = static_cast<uint32_t>(atoi(colon + 1));
        } else {
            p30_copy_id(fam, a.param);
        }
        run_role_host(h, fam, seed, o);
        return;
    }
    snprintf(o->notes, sizeof(o->notes), "unknown:%s", a.vector);
}

static bool outcome_match(const AttackRow& a, const Outcome& o) {
    if (o.headline != a.exp_headline) return false;
    if (o.alert != a.exp_alert) return false;
    if (o.drop != a.exp_drop) return false;
    if (o.block != a.exp_block) return false;
    if (o.down != 0) return false;
    return true;
}

static bool parse_row(char* line, AttackRow* a) {
    if (line == nullptr || a == nullptr) return false;
    char* fields[10];
    uint8_t n = 0;
    char* p = line;
    while (n < 10) {
        fields[n++] = p;
        char* comma = strchr(p, ',');
        if (comma == nullptr) break;
        *comma = '\0';
        p = comma + 1;
    }
    if (n < 10) return false;
    a->id = static_cast<uint32_t>(atoi(fields[0]));
    p30_copy_id(a->family, fields[1]);
    p30_copy_id(a->role, fields[2]);
    p30_copy_id(a->device_id, fields[3]);
    p30_copy_id(a->vector, fields[4]);
    p30_copy_id(a->param, fields[5]);
    a->exp_headline = static_cast<uint8_t>(atoi(fields[6]));
    a->exp_alert = static_cast<uint8_t>(atoi(fields[7]));
    a->exp_drop = static_cast<uint8_t>(atoi(fields[8]));
    a->exp_block = static_cast<uint8_t>(atoi(fields[9]));
    return true;
}

int main(int argc, char** argv) {
    const char* in_path = "tests/hive_redteam/attacks.csv";
    const char* out_path = "tests/hive_redteam/results.csv";
    uint32_t limit = 0;
    if (argc > 1) in_path = argv[1];
    if (argc > 2) out_path = argv[2];
    const char* lim = getenv("REDTEAM_LIMIT");
    if (lim != nullptr) limit = static_cast<uint32_t>(atoi(lim));

    FILE* fin = fopen(in_path, "r");
    if (fin == nullptr) {
        fprintf(stderr, "redteam: cannot open %s\n", in_path);
        return 1;
    }
    FILE* fout = fopen(out_path, "w");
    if (fout == nullptr) {
        fclose(fin);
        fprintf(stderr, "redteam: cannot write %s\n", out_path);
        return 1;
    }
    (void)fputs(
        "id,t1_us,t2_us,t3_us,headline,alert,drop,block,down,registry,"
        "pipe_code,match,notes\n",
        fout);

    char line[512];
    if (fgets(line, sizeof(line), fin) == nullptr) {
        fclose(fin);
        fclose(fout);
        return 1;
    }

    uint64_t sum_t1 = 0;
    uint64_t sum_t2 = 0;
    uint64_t sum_t3 = 0;
    uint32_t hl_n = 0;

    while (fgets(line, sizeof(line), fin) != nullptr) {
        if (line[0] == '\n' || line[0] == '#') continue;
        AttackRow row{};
        if (!parse_row(line, &row)) continue;
        if (limit != 0 && g_total >= limit) break;

        Outcome out{};
        run_attack(row, &out);
        bool match = outcome_match(row, out);
        ++g_total;
        if (match) ++g_match;
        else ++g_mismatch;

        if (out.headline != 0) {
            sum_t1 += out.t1_us;
            sum_t2 += out.t2_us;
            sum_t3 += out.t3_us;
            ++hl_n;
        }

        fprintf(fout,
                "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u,%s\n",
                row.id, out.t1_us, out.t2_us, out.t3_us,
                out.headline, out.alert, out.drop, out.block, out.down,
                out.registry, static_cast<int>(out.pipe_code),
                match ? 1u : 0u, out.notes);
    }

    fclose(fin);
    fclose(fout);

    printf("redteam attacks=%u match=%u mismatch=%u\n",
           g_total, g_match, g_mismatch);
    if (hl_n > 0) {
        printf("redteam headline_avg_us t1=%llu t2=%llu t3=%llu (n=%u)\n",
               static_cast<unsigned long long>(sum_t1 / hl_n),
               static_cast<unsigned long long>(sum_t2 / hl_n),
               static_cast<unsigned long long>(sum_t3 / hl_n),
               hl_n);
    }
    printf("redteam results=%s\n", out_path);

    bool ok = (g_total >= 2000u) && (g_mismatch == 0);
    printf(ok ? "PASS hive_redteam\n" : "FAIL hive_redteam\n");
    return ok ? 0 : 1;
}
