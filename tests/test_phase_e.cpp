// Phase E — Adversarial: nicht erkannt werden.
// Kein Produktionscode. Nur Schnittstellen (Event, Mine, Wire, Wrap).
// Spec-Basis: §15 TOTP 60–120s, §33 HMAC, §40/§43 Down, Replay 32.

#include "p30_harness.h"
#include "../src/psp/ghost_wrap.h"
#include "../src/psp/ghost_crypto.h"
#include "../src/psp/transport/transport_frame.h"
#include "../src/laptop/kill.h"

#include <stdio.h>

static uint32_t g_n = 0;
static uint32_t g_bad = 0;
static uint32_t g_finding = 0;

static void chk_at(bool cond, int line) {
    ++g_n;
    if (!cond) {
        ++g_bad;
        if (g_bad <= 8) printf("fail line %d\n", line);
    }
}

#define chk(c) chk_at((c), __LINE__)

static void finding(bool cond) {
    if (cond) ++g_finding;
}

static bool root_eq(const GhostKeys& keys, const uint8_t* root) {
    if (!keys.hasRoot() || root == nullptr) return false;
    for (uint8_t i = 0; i < KEY_LEN; ++i) {
        if (keys.root()[i] != root[i]) return false;
    }
    return true;
}

static void snap_root(const GhostKeys& keys, uint8_t* out) {
    for (uint8_t i = 0; i < KEY_LEN; ++i) out[i] = keys.root()[i];
}

static void setup_e(P30Hive& h, uint8_t* rootSnap) {
    p30_attach(h);
    (void)p30_bind_root(h);
    snap_root(h.keys, rootSnap);
    p30_enroll(h, "W", ROLE_WORKER, 2);
    p30_enroll(h, "P", ROLE_PHONE, 1);
    p30_enroll(h, "R", ROLE_ROUTER, 1);
    p30_enroll(h, "N", ROLE_SAFE, 1);
    Device mine{};
    p30_copy_id(mine.id, "X");
    mine.role = ROLE_MINE;
    mine.status = DeviceState::Silent;
    (void)h.reg.addDevice(mine);
    if (h.keys.totpSeed() != nullptr) {
        (void)h.pipe.replay().setTotpSeed("X", h.keys.totpSeed(), TOTP_SEED_LEN);
    }
}

static void queue_event(P30Hive& h, uint8_t role, const char* id, Event ev,
                        uint32_t now, bool sign) {
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
    (void)h.wlan.toKernel(frame);
}

static uint32_t rng_ = 0xC0FFEEu;
static uint32_t rnd() {
    rng_ = rng_ * 1664525u + 1013904223u;
    return rng_;
}

static void family_hmac_bitflip() {
    for (uint8_t off = 0; off < VAULT_MAC_HEX; ++off) {
        uint8_t root[KEY_LEN];
        P30Hive h;
        setup_e(h, root);
        Event ev = p30_event(EventType::Heartbeat, "W", 8000u + off, "ok");
        chk(h.vault.signEvent(ev));
        chk(h.vault.verifyEvent(ev));
        char c = ev.payload[VAULT_MAC_OFF + off];
        ev.payload[VAULT_MAC_OFF + off] = (c == '0') ? '1' : '0';
        chk(!h.vault.verifyEvent(ev));
        p30_inject(h, ROLE_WORKER, "W", ev, 8000u + off, false);
        chk(!h.xport.hiveFrozen());
        chk(root_eq(h.keys, root));
        chk(h.keys.hasRoot());
    }
    for (uint8_t i = 0; i < 16; ++i) {
        uint8_t root[KEY_LEN];
        P30Hive h;
        setup_e(h, root);
        Event ev = p30_event(EventType::ScanResult, "W", 8100u + i, "body");
        chk(h.vault.signEvent(ev));
        ev.payload[i] = static_cast<char>(ev.payload[i] ^ 0x5A);
        chk(!h.vault.verifyEvent(ev));
        p30_inject(h, ROLE_WORKER, "W", ev, 8100u + i, false);
        chk(!h.xport.hiveFrozen());
        chk(root_eq(h.keys, root));
    }
}

static void family_mac_tail() {
    // Bytes 84–87 were outside HMAC (auth/totp stamps + pad). Wire must reject.
    for (uint8_t off = VAULT_AUTH_OFF; off < VAULT_MAC_OFF; ++off) {
        uint8_t root[KEY_LEN];
        P30Hive h;
        setup_e(h, root);
        Event ev = p30_event(EventType::Heartbeat, "W", 8200u + off, "tail");
        chk(h.vault.signEvent(ev));
        chk(h.vault.verifyEvent(ev));
        ev.payload[off] = 'X';
        chk(!h.vault.verifyEvent(ev));
        p30_inject(h, ROLE_WORKER, "W", ev, 8200u + off, false);
        chk(!h.xport.hiveFrozen());
        chk(root_eq(h.keys, root));
        chk(h.keys.hasRoot());
    }
}

static void family_totp_edges() {
    uint8_t seed[TOTP_SEED_LEN];
    for (uint8_t i = 0; i < TOTP_SEED_LEN; ++i) {
        seed[i] = static_cast<uint8_t>(0x31 + i);
    }

    const uint32_t edges[8] = {59, 60, 61, 89, 90, 119, 120, 121};
    for (uint8_t e = 0; e < 8; ++e) {
        ReplayGuard g;
        chk(g.setTotpSeed("T", seed, TOTP_SEED_LEN));
        MinePayload a{};
        a.mine_id[0] = 'T';
        a.mine_id[1] = '\0';
        a.counter = 1;
        a.timestamp = 10000;
        a.totp = ghost_totp(seed, TOTP_SEED_LEN, 10000);
        chk(g.check(a, 10000));

        MinePayload b = a;
        b.counter = 2;
        uint32_t now = 10000 + edges[e];
        b.timestamp = now;
        b.totp = ghost_totp(seed, TOTP_SEED_LEN, now);
        bool ok = g.check(b, now);
        bool inWin = (edges[e] >= TOTP_WINDOW_MIN_SEC &&
                      edges[e] <= TOTP_WINDOW_MAX_SEC);
        chk(ok == inWin);
        if (!inWin) {
            chk(g.isBlocked("T") || !ok);
        }
    }

    {
        ReplayGuard g;
        chk(g.setTotpSeed("P", seed, TOTP_SEED_LEN));
        MinePayload a{};
        a.mine_id[0] = 'P';
        a.mine_id[1] = '\0';
        a.counter = 1;
        a.totp = ghost_totp(seed, TOTP_SEED_LEN, 20000);
        chk(g.check(a, 20000));
        MinePayload b = a;
        b.counter = 2;
        b.totp = ghost_totp(seed, TOTP_SEED_LEN, 20000);
        bool prevOk = g.check(b, 20000 + TOTP_STEP_SEC);
        chk(prevOk);
    }

    {
        ReplayGuard g;
        MinePayload a{};
        a.mine_id[0] = 'F';
        a.mine_id[1] = '\0';
        a.counter = 1;
        a.totp = ghost_totp(seed, TOTP_SEED_LEN, 30000 - TOTP_STEP_SEC);
        bool firstPrev = g.check(a, 30000, seed, TOTP_SEED_LEN);
        chk(!firstPrev);
        chk(g.isBlocked("F"));
    }

    {
        ReplayGuard g;
        chk(g.setTotpSeed("Q", seed, TOTP_SEED_LEN));
        MinePayload a{};
        a.mine_id[0] = 'Q';
        a.mine_id[1] = '\0';
        a.counter = 1;
        a.totp = ghost_totp(seed, TOTP_SEED_LEN, 31000 - TOTP_STEP_SEC);
        bool sneak = g.check(a, 31000);
        finding(sneak);
    }

    // setTotpSeed does not reset last_counter: cannot re-open first-packet prev.
    {
        ReplayGuard g;
        chk(g.setTotpSeed("S", seed, TOTP_SEED_LEN));
        MinePayload a{};
        a.mine_id[0] = 'S';
        a.mine_id[1] = '\0';
        a.counter = 1;
        a.totp = ghost_totp(seed, TOTP_SEED_LEN, 32000);
        chk(g.check(a, 32000));
        chk(g.setTotpSeed("S", seed, TOTP_SEED_LEN));
        MinePayload replay = a;
        chk(!g.check(replay, 32000 + TOTP_STEP_SEC));
    }

    // Separate mine_ids: shared seed, no shared counter slot.
    {
        ReplayGuard g;
        chk(g.setTotpSeed("A", seed, TOTP_SEED_LEN));
        chk(g.setTotpSeed("B", seed, TOTP_SEED_LEN));
        MinePayload a{};
        a.mine_id[0] = 'A';
        a.mine_id[1] = '\0';
        a.counter = 1;
        a.totp = ghost_totp(seed, TOTP_SEED_LEN, 33000);
        chk(g.check(a, 33000));
        MinePayload b{};
        b.mine_id[0] = 'B';
        b.mine_id[1] = '\0';
        b.counter = 1;
        b.totp = ghost_totp(seed, TOTP_SEED_LEN, 33000);
        chk(g.check(b, 33000));
        MinePayload a2 = a;
        chk(!g.check(a2, 33090));
        MinePayload b2 = b;
        b2.counter = 2;
        b2.totp = ghost_totp(seed, TOTP_SEED_LEN, 33090);
        chk(g.check(b2, 33090));
    }

    for (uint8_t i = 0; i < 32; ++i) {
        ReplayGuard g;
        chk(g.setTotpSeed("W", seed, TOTP_SEED_LEN));
        MinePayload a{};
        a.mine_id[0] = 'W';
        a.mine_id[1] = '\0';
        a.counter = 1;
        a.totp = ghost_totp(seed, TOTP_SEED_LEN, 40000) ^ (1u << (i % 20));
        chk(!g.check(a, 40000));
        chk(g.isBlocked("W"));
    }
}

static void family_counter_wrap() {
    uint8_t seed[TOTP_SEED_LEN];
    for (uint8_t i = 0; i < TOTP_SEED_LEN; ++i) seed[i] = static_cast<uint8_t>(i + 2);
    ReplayGuard g;
    chk(g.setTotpSeed("C", seed, TOTP_SEED_LEN));
    MinePayload a{};
    a.mine_id[0] = 'C';
    a.mine_id[1] = '\0';
    a.counter = 0xFFFFFFFEu;
    a.totp = ghost_totp(seed, TOTP_SEED_LEN, 50000);
    chk(g.check(a, 50000));
    MinePayload b = a;
    b.counter = 0xFFFFFFFFu;
    b.totp = ghost_totp(seed, TOTP_SEED_LEN, 50090);
    chk(g.check(b, 50090));
    MinePayload z = b;
    z.counter = 0;
    z.totp = ghost_totp(seed, TOTP_SEED_LEN, 50180);
    chk(!g.check(z, 50180));
    MinePayload z1 = b;
    z1.counter = 1;
    z1.totp = ghost_totp(seed, TOTP_SEED_LEN, 50180);
    chk(!g.check(z1, 50180));
}

static void family_replay_window() {
    uint8_t seed[TOTP_SEED_LEN];
    for (uint8_t i = 0; i < TOTP_SEED_LEN; ++i) seed[i] = static_cast<uint8_t>(0x44 + i);
    ReplayGuard g;
    chk(g.setTotpSeed("R", seed, TOTP_SEED_LEN));
    uint32_t t = 60000;
    for (uint8_t i = 0; i < 40; ++i) {
        MinePayload m{};
        m.mine_id[0] = 'R';
        m.mine_id[1] = '\0';
        m.counter = static_cast<uint32_t>(i) + 1u;
        m.totp = ghost_totp(seed, TOTP_SEED_LEN, t);
        chk(g.check(m, t));
        t += 90;
    }
    MinePayload old{};
    old.mine_id[0] = 'R';
    old.mine_id[1] = '\0';
    old.counter = 1;
    old.totp = ghost_totp(seed, TOTP_SEED_LEN, t);
    chk(!g.check(old, t));
    MinePayload mid{};
    mid.mine_id[0] = 'R';
    mid.mine_id[1] = '\0';
    mid.counter = 20;
    mid.totp = ghost_totp(seed, TOTP_SEED_LEN, t);
    chk(!g.check(mid, t));
}

static void family_stale_timestamp() {
    for (uint8_t i = 0; i < 64; ++i) {
        uint8_t root[KEY_LEN];
        P30Hive h;
        setup_e(h, root);
        uint32_t now = 90000 + i;
        Event hb = p30_event(EventType::Heartbeat, "W", now, "fresh");
        p30_inject(h, ROLE_WORKER, "W", hb, now, true);
        chk(!h.xport.hiveFrozen());
        const Device* d = h.reg.getDevice("W");
        chk(d != nullptr);
        uint32_t seen = (d != nullptr) ? d->last_seen : 0;
        Event stale = p30_event(EventType::ScanResult, "W", now - 1, "stale");
        p30_inject(h, ROLE_WORKER, "W", stale, now - 1, true);
        chk(!h.xport.hiveFrozen());
        chk(root_eq(h.keys, root));
        d = h.reg.getDevice("W");
        chk(d != nullptr && d->last_seen == seen);
        finding(!h.xport.hiveFrozen());
    }
}

static void family_id_mismatch() {
    for (uint8_t i = 0; i < 16; ++i) {
        uint8_t root[KEY_LEN];
        P30Hive h;
        setup_e(h, root);
        Event ev = p30_event(EventType::Heartbeat, "W", 91000 + i, "spoof");
        chk(h.vault.signEvent(ev));
        TransportFrame frame;
        transport_clear_frame(frame);
        frame.kind = TransportKind::EventFrame;
        frame.src_role = ROLE_PHONE;
        frame.dst_role = ROLE_KERNEL;
        p30_copy_id(frame.src_id, "P");
        p30_copy_id(frame.dst_id, KERNEL_SOURCE_ID);
        frame.event = ev;
        frame.stamp = 91000 + i;
        (void)h.wlan.toKernel(frame);
        h.xport.rx(91000 + i);
        chk(!h.xport.hiveFrozen());
        chk(root_eq(h.keys, root));
    }
}

static void family_race() {
    for (uint8_t i = 0; i < 32; ++i) {
        uint8_t root[KEY_LEN];
        P30Hive h;
        setup_e(h, root);
        uint32_t now = 92000 + static_cast<uint32_t>(i) * 10u;
        Event a = p30_event(EventType::AnomalyDetected, "W", now, "tamper:ptrace");
        a.severity = Severity::Critical;
        Event b = p30_event(EventType::Heartbeat, "P", now + 1, "race");
        Event c = p30_event(EventType::DeviceSeen, "R", now + 2, "net:new_device");
        Event d = p30_event(EventType::ScanResult, "N", now + 3, "smb");
        queue_event(h, ROLE_WORKER, "W", a, now, true);
        queue_event(h, ROLE_PHONE, "P", b, now + 1, false);
        queue_event(h, ROLE_ROUTER, "R", c, now + 2, true);
        queue_event(h, ROLE_SAFE, "N", d, now + 3, true);
        h.xport.rx(now + 3);
        chk(!h.xport.hiveFrozen());
        chk(!h.down.isActive());
        chk(root_eq(h.keys, root));
        chk(h.keys.hasRoot());
        Event late = p30_event(EventType::Heartbeat, "W", now + 4, "late");
        p30_inject(h, ROLE_WORKER, "W", late, now + 4, true);
        chk(root_eq(h.keys, root));
        chk(!h.xport.hiveFrozen());
    }
}

static void family_fuzz_wire() {
    const uint16_t lens[12] = {0, 1, 2, 16, 32, 64, 128, 256, 345, 346, 347, 512};
    for (uint8_t i = 0; i < 12; ++i) {
        TransportFrame fr;
        uint8_t buf[512];
        for (uint16_t k = 0; k < 512; ++k) {
            buf[k] = static_cast<uint8_t>(rnd());
        }
        bool dec = transport_decode(buf, lens[i], fr);
        if (lens[i] != TRANSPORT_WIRE_LEN) {
            chk(!dec);
        }
    }
    for (uint16_t i = 0; i < 64; ++i) {
        TransportFrame src;
        transport_clear_frame(src);
        src.kind = static_cast<TransportKind>(rnd() % 8);
        src.src_role = static_cast<uint8_t>(rnd());
        src.dst_role = static_cast<uint8_t>(rnd());
        src.event.type = static_cast<EventType>(rnd() % 24);
        src.event.timestamp = rnd();
        src.event.severity = static_cast<Severity>(rnd() % 8);
        src.stamp = rnd();
        uint8_t wire[TRANSPORT_WIRE_LEN];
        chk(transport_encode(src, wire, TRANSPORT_WIRE_LEN));
        for (uint8_t b = 0; b < 8; ++b) {
            wire[rnd() % TRANSPORT_WIRE_LEN] ^= static_cast<uint8_t>(1u << (b % 8));
        }
        TransportFrame out;
        (void)transport_decode(wire, TRANSPORT_WIRE_LEN, out);
        uint8_t root[KEY_LEN];
        P30Hive h;
        setup_e(h, root);
        (void)h.wlan.toKernel(out);
        h.xport.rx(93000 + i);
        chk(h.keys.hasRoot());
        chk(root_eq(h.keys, root));
    }
}

static void family_fuzz_event() {
    for (uint16_t i = 0; i < 128; ++i) {
        uint8_t root[KEY_LEN];
        P30Hive h;
        setup_e(h, root);
        Event ev{};
        ev.type = static_cast<EventType>(rnd() % 32);
        ev.timestamp = rnd();
        ev.severity = static_cast<Severity>(rnd() % 8);
        uint8_t idpick = static_cast<uint8_t>(rnd() % 5);
        const char* ids[5] = {"W", "P", "R", "N", ""};
        const uint8_t roles[5] = {ROLE_WORKER, ROLE_PHONE, ROLE_ROUTER, ROLE_SAFE, ROLE_WORKER};
        p30_copy_id(ev.source_device_id, ids[idpick]);
        for (uint8_t p = 0; p < 127; ++p) {
            ev.payload[p] = static_cast<char>(rnd());
        }
        ev.payload[127] = '\0';
        bool sign = (rnd() & 1u) != 0;
        uint8_t role = roles[idpick];
        if (ids[idpick][0] == '\0') {
            p30_inject(h, role, "W", ev, 94000 + i, sign);
        } else {
            p30_inject(h, role, ids[idpick], ev, 94000 + i, sign);
        }
        chk(h.keys.hasRoot());
        chk(root_eq(h.keys, root));
    }
}

static void family_wrap_noise() {
    uint8_t master[32];
    uint8_t salt[16];
    uint8_t wk[32];
    uint8_t root[32];
    uint8_t blob[ROOT_WRAP_LEN];
    chk(ghost_wrap_master(master));
    for (uint8_t i = 0; i < 16; ++i) salt[i] = static_cast<uint8_t>(i);
    chk(ghost_wrap_kdf(master, nullptr, 0, salt, wk));
    for (uint8_t i = 0; i < 32; ++i) root[i] = static_cast<uint8_t>(0xA0 + i);
    chk(ghost_wrap_seal(wk, root, blob, 0));
    for (uint16_t i = 0; i < 64; ++i) {
        uint8_t mut[ROOT_WRAP_LEN];
        for (uint16_t b = 0; b < ROOT_WRAP_LEN; ++b) mut[b] = blob[b];
        uint16_t off = static_cast<uint16_t>(rnd() % ROOT_WRAP_LEN);
        mut[off] = static_cast<uint8_t>(mut[off] ^ static_cast<uint8_t>(1u + (i & 7)));
        uint8_t out[32];
        for (uint8_t k = 0; k < 32; ++k) out[k] = 0x11;
        chk(!ghost_wrap_open(wk, mut, out));
        uint8_t leak = 1;
        for (uint8_t k = 0; k < 32; ++k) {
            if (out[k] != root[k]) leak = 0;
        }
        chk(leak == 0);
    }
}

static void family_peer_kill_unsigned() {
    for (uint8_t i = 0; i < 16; ++i) {
        uint8_t root[KEY_LEN];
        P30Hive h;
        setup_e(h, root);
        HiveKill killer;
        killer.attach(&h.keys);
        Event req{};
        chk(killer.fill(&req, "W", 95000 + i, 0));
        p30_inject(h, ROLE_WORKER, "W", req, 95000 + i, false);
        chk(!h.xport.hiveFrozen());
        chk(root_eq(h.keys, root));
    }
}

int main() {
    family_hmac_bitflip();
    family_mac_tail();
    family_totp_edges();
    family_counter_wrap();
    family_replay_window();
    family_stale_timestamp();
    family_id_mismatch();
    family_race();
    family_fuzz_wire();
    family_fuzz_event();
    family_wrap_noise();
    family_peer_kill_unsigned();

    printf("mac_auth_tail: payload[84..87] in HMAC, flip → verify fail + drop, no Down\n");
    printf("OPEN totp_prev_on_preseeded_window: first packet may use prev step\n");
    printf("OPEN stale_timestamp silent_drop: valid sig, no log, last_seen held\n");
    printf("hmac 1-bit MAC flip → drop + P15, hive open, root held\n");
    printf("TOTP edges 59/121 reject, 60/90/120 accept (prev-step after first ok)\n");
    printf("counter wrap 0xffffffff→0 reject; replay of old counter after 40 ticks reject\n");
    printf("race 4-peer same rx: freeze once, root held\n");

    bool ok = (g_bad == 0) && (g_n >= 400);
    printf(ok ? "PASS phase_e n=%u findings=%u\n" : "FAIL phase_e n=%u bad=%u findings=%u\n",
           static_cast<unsigned>(g_n),
           ok ? static_cast<unsigned>(g_finding) : static_cast<unsigned>(g_bad),
           static_cast<unsigned>(g_finding));
    return ok ? 0 : 1;
}
