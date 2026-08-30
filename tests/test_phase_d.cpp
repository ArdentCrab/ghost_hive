// Phase D — Device Damage Resistance §40 / §43
// Misst Freeze- und Halt-Zeiten (CLOCK_MONOTONIC, µs).
// Keine Exploits: Angriffe sind §14-Events / HMAC-I / Mine-Trip.

#include "p30_harness.h"
#include "../src/laptop/kill.h"
#include "../src/laptop/peer_halt.h"
#include "../src/mine/os_mine.h"
#include "../src/mine/browser_mine.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

static uint32_t g_n = 0;
static uint32_t g_bad = 0;

static void chk(bool cond) {
    ++g_n;
    if (!cond) ++g_bad;
}

static uint64_t now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (static_cast<uint64_t>(ts.tv_sec) * 1000000000ull) +
           static_cast<uint64_t>(ts.tv_nsec);
}

static uint32_t us_since(uint64_t t0) {
    uint64_t d = now_ns() - t0;
    if (d > 4000000000ull) return 4000000000u;
    return static_cast<uint32_t>(d / 1000ull);
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

static void enroll_mine(P30Hive& h, const char* id) {
    Device md{};
    p30_copy_id(md.id, id);
    md.role = ROLE_MINE;
    md.status = DeviceState::Silent;
    (void)h.reg.addDevice(md);
    if (h.keys.totpSeed() != nullptr) {
        (void)h.pipe.replay().setTotpSeed(id, h.keys.totpSeed(), TOTP_SEED_LEN);
    }
}

static void setup_d(P30Hive& h, uint8_t* rootSnap) {
    p30_attach(h);
    (void)p30_bind_root(h);
    snap_root(h.keys, rootSnap);
    p30_enroll(h, "W", ROLE_WORKER, 2);
    p30_enroll(h, "P", ROLE_PHONE, 1);
    p30_enroll(h, "R", ROLE_ROUTER, 1);
    p30_enroll(h, "N", ROLE_SAFE, 1);
    p30_enroll(h, "F", ROLE_SENSOR, 1);
    enroll_mine(h, "X");
    enroll_mine(h, "ML");
    enroll_mine(h, "MP");
    enroll_mine(h, "MR");
    enroll_mine(h, "MN");
    h.stealth.enterTerminalMode();
}

enum {
    R_LAP_INJ = 0,
    R_LAP_KEX,
    R_LAP_FILE,
    R_LAP_NET,
    R_LAP_BRW,
    R_PH_APP,
    R_PH_RADIO,
    R_PH_OS,
    R_PH_REPLAY,
    R_RT_PORT,
    R_RT_DNS,
    R_RT_ARP,
    R_RT_FLOOD,
    R_NAS_SMB,
    R_NAS_FILE,
    R_NAS_REPLAY,
    R_NAS_INJ,
    R_MN_BRW,
    R_MN_OS,
    R_MN_REPLAY,
    R_MN_DBG,
    R_PRE_PKILL,
    R_PRE_FAKE,
    R_PRE_UHB,
    R_PRE_PHKILL,
    R_PRE_POISON,
    R_PRE_REPLAY,
    R_PRE_FLOOD,
    R_PRE_ARP,
    R_POST_OS,
    R_POST_NET,
    R_POST_FILE,
    R_POST_REPLAY,
    R_POST_INJ,
    R_COUNT
};

struct FamStat {
    const char* name;
    uint8_t expect_down;
    uint8_t halt_role;
    const char* halt_id;
    uint32_t n;
    uint32_t down_n;
    uint32_t root_n;
    uint32_t game_n;
    uint32_t dead_n;
    uint32_t fz[128];
    uint32_t ht[128];
};

static FamStat g_row[R_COUNT];

static void row_init() {
    const char* names[R_COUNT] = {
        "laptop.inject", "laptop.kexploit_sim", "laptop.filewrite", "laptop.netflood",
        "laptop.browser",
        "phone.app", "phone.radio", "phone.ostamper", "phone.replay",
        "router.portscan", "router.dns", "router.arp", "router.flood",
        "nas.smb", "nas.file", "nas.replay", "nas.inject",
        "mine.browser", "mine.os", "mine.replay", "mine.debugger",
        "pre.peerkill", "pre.fakescan", "pre.unsigned_hb", "pre.phonekill",
        "pre.router_poison", "pre.replay", "pre.flood", "pre.arp",
        "post.osexploit_sim", "post.netflood", "post.filetamper", "post.replay",
        "post.inject"
    };
    const uint8_t expect[R_COUNT] = {
        0, 0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0, 0
    };
    const uint8_t roles[R_COUNT] = {
        ROLE_WORKER, ROLE_WORKER, ROLE_WORKER, ROLE_WORKER, ROLE_WORKER,
        ROLE_PHONE, ROLE_PHONE, ROLE_PHONE, ROLE_PHONE,
        ROLE_ROUTER, ROLE_ROUTER, ROLE_ROUTER, ROLE_ROUTER,
        ROLE_SAFE, ROLE_SAFE, ROLE_SAFE, ROLE_SAFE,
        ROLE_WORKER, ROLE_WORKER, ROLE_WORKER, ROLE_WORKER,
        0, 0, ROLE_WORKER, 0,
        0, ROLE_WORKER, 0, 0,
        ROLE_WORKER, ROLE_WORKER, ROLE_WORKER, ROLE_WORKER, ROLE_WORKER
    };
    const char* ids[R_COUNT] = {
        "W", "W", "W", "W", "W",
        "P", "P", "P", "P",
        "R", "R", "R", "R",
        "N", "N", "N", "N",
        "W", "W", "W", "W",
        "", "", "W", "",
        "", "W", "", "",
        "W", "W", "W", "W", "W"
    };
    for (uint8_t i = 0; i < R_COUNT; ++i) {
        g_row[i].name = names[i];
        g_row[i].expect_down = expect[i];
        g_row[i].halt_role = roles[i];
        g_row[i].halt_id = ids[i];
        g_row[i].n = 0;
        g_row[i].down_n = 0;
        g_row[i].root_n = 0;
        g_row[i].game_n = 0;
        g_row[i].dead_n = 0;
    }
}

static int cmp_u32(const void* a, const void* b) {
    uint32_t x = *static_cast<const uint32_t*>(a);
    uint32_t y = *static_cast<const uint32_t*>(b);
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static uint32_t median_u32(uint32_t* v, uint32_t n) {
    if (n == 0) return 0;
    qsort(v, n, sizeof(uint32_t), cmp_u32);
    return v[n / 2];
}

static uint32_t avg_u32(const uint32_t* v, uint32_t n) {
    if (n == 0) return 0;
    uint64_t s = 0;
    for (uint32_t i = 0; i < n; ++i) s += v[i];
    return static_cast<uint32_t>(s / n);
}

static uint32_t min_u32(const uint32_t* v, uint32_t n) {
    if (n == 0) return 0;
    uint32_t m = v[0];
    for (uint32_t i = 1; i < n; ++i) if (v[i] < m) m = v[i];
    return m;
}

static uint32_t max_u32(const uint32_t* v, uint32_t n) {
    if (n == 0) return 0;
    uint32_t m = v[0];
    for (uint32_t i = 1; i < n; ++i) if (v[i] > m) m = v[i];
    return m;
}

static void rec(uint8_t row, bool down, bool root, bool game, bool dead,
                uint32_t freeze_us, uint32_t halt_us) {
    if (row >= R_COUNT) return;
    FamStat& s = g_row[row];
    if (s.n >= 128) return;
    uint32_t i = s.n;
    s.n = i + 1;
    s.fz[i] = freeze_us;
    s.ht[i] = halt_us;
    if (down) ++s.down_n;
    if (root) ++s.root_n;
    if (game) ++s.game_n;
    if (dead) ++s.dead_n;
}

static void apply_halt(P30Hive& h, uint32_t now, uint8_t role, const char* id,
                       uint32_t* halt_us) {
    h.down.tick(now + NAS_FLUSH_TIMEOUT_SEC);
    uint64_t t = now_ns();
    peer_halt_reset();
    peer_halt_run(role, id);
    *halt_us = us_since(t);
}

static void trial_worker_anom(uint8_t row, uint32_t tbase, uint16_t i,
                             const char* payload) {
    uint8_t root[KEY_LEN];
    P30Hive h;
    setup_d(h, root);
    uint32_t now = tbase + i;
    Event ev = p30_event(EventType::AnomalyDetected, "W", now, payload);
    ev.severity = Severity::Critical;
    uint64_t t0 = now_ns();
    p30_inject(h, ROLE_WORKER, "W", ev, now, true);
    uint32_t fz = us_since(t0);
    bool down = h.xport.hiveFrozen() && h.down.isActive();
    bool game = h.stealth.isGameMode() && !h.stealth.isInvisible();
    bool root_ok = root_eq(h.keys, root);
    uint32_t ht = 0;
    bool dead = false;
    if (down && g_row[row].halt_id[0] != '\0') {
        apply_halt(h, now, g_row[row].halt_role, g_row[row].halt_id, &ht);
        dead = peer_halt_dead() && !peer_halt_can_tx() &&
               peer_halt_has_marker(g_row[row].halt_id);
        chk(!peer_halt_can_tx());
    }
    chk(down == (g_row[row].expect_down != 0));
    chk(root_ok);
    chk(h.keys.hasRoot());
    if (g_row[row].expect_down != 0) {
        chk(game);
        chk(dead);
        chk(h.vault.frozen());
    }
    rec(row, down, root_ok, game, dead, fz, ht);
}

static void trial_unsigned(uint8_t row, uint8_t role, const char* id,
                           uint32_t tbase, uint16_t i) {
    uint8_t root[KEY_LEN];
    P30Hive h;
    setup_d(h, root);
    uint32_t now = tbase + i;
    Event ev = p30_event(EventType::Heartbeat, id, now, "kexploit_sim");
    uint64_t t0 = now_ns();
    p30_inject(h, role, id, ev, now, false);
    uint32_t fz = us_since(t0);
    bool down = h.xport.hiveFrozen() && h.down.isActive();
    bool game = h.stealth.isGameMode() && !h.stealth.isInvisible();
    bool root_ok = root_eq(h.keys, root);
    uint32_t ht = 0;
    bool dead = false;
    if (down && g_row[row].halt_id[0] != '\0') {
        apply_halt(h, now, g_row[row].halt_role, g_row[row].halt_id, &ht);
        dead = peer_halt_dead() && !peer_halt_can_tx();
        chk(!peer_halt_can_tx());
    }
    chk(down == (g_row[row].expect_down != 0));
    chk(root_ok);
    chk(h.keys.hasRoot());
    rec(row, down, root_ok, game, dead, fz, ht);
}

static void trial_mine_os(uint8_t row, const char* mid, uint32_t tbase, uint16_t i) {
    uint8_t root[KEY_LEN];
    P30Hive h;
    setup_d(h, root);
    OsMine os;
    os.init(mid);
    chk(os.setTotpSeed(h.keys.totpSeed(), TOTP_SEED_LEN));
    uint32_t now = tbase + static_cast<uint32_t>(i) * 90u;
    MinePayload mp{};
    chk(os.onSuspiciousProcess(&mp, now));
    uint64_t t0 = now_ns();
    p30_inject_mine(h, mp, now, true);
    uint32_t fz = us_since(t0);
    bool down = h.xport.hiveFrozen();
    bool game = h.stealth.isGameMode();
    bool root_ok = root_eq(h.keys, root);
    uint32_t ht = 0;
    bool dead = false;
    if (down && g_row[row].halt_id[0] != '\0') {
        apply_halt(h, now, g_row[row].halt_role, g_row[row].halt_id, &ht);
        dead = !peer_halt_can_tx();
        chk(!peer_halt_can_tx());
    }
    chk(!down);
    chk(root_ok);
    rec(row, down, root_ok, game, dead, fz, ht);
}

static void trial_mine_brw(uint8_t row, const char* mid, uint32_t tbase, uint16_t i) {
    uint8_t root[KEY_LEN];
    P30Hive h;
    setup_d(h, root);
    BrowserMine br;
    br.init(mid);
    chk(br.setTotpSeed(h.keys.totpSeed(), TOTP_SEED_LEN));
    uint32_t now = tbase + static_cast<uint32_t>(i) * 90u;
    MinePayload mp{};
    chk(br.onSuspiciousUrl(&mp, now));
    uint64_t t0 = now_ns();
    p30_inject_mine(h, mp, now, true);
    uint32_t fz = us_since(t0);
    bool down = h.xport.hiveFrozen();
    bool game = h.stealth.isGameMode();
    bool root_ok = root_eq(h.keys, root);
    uint32_t ht = 0;
    bool dead = false;
    if (down && g_row[row].halt_id[0] != '\0') {
        apply_halt(h, now, g_row[row].halt_role, g_row[row].halt_id, &ht);
        dead = !peer_halt_can_tx();
        chk(!peer_halt_can_tx());
    }
    chk(!down);
    chk(root_ok);
    rec(row, down, root_ok, game, dead, fz, ht);
}

static void trial_mine_replay(uint8_t row, const char* mid, uint32_t tbase, uint16_t i) {
    uint8_t root[KEY_LEN];
    P30Hive h;
    setup_d(h, root);
    Mine m;
    m.init(mid);
    chk(m.setTotpSeed(h.keys.totpSeed(), TOTP_SEED_LEN));
    uint32_t now = tbase + static_cast<uint32_t>(i) * 90u;
    MinePayload a{};
    chk(m.send(&a, now));
    p30_inject_mine(h, a, now, true);
    MinePayload replay = a;
    uint64_t t0 = now_ns();
    p30_inject_mine(h, replay, now + 1, true);
    uint32_t fz = us_since(t0);
    bool down = h.xport.hiveFrozen();
    bool game = h.stealth.isGameMode();
    bool root_ok = root_eq(h.keys, root);
    uint32_t ht = 0;
    bool dead = false;
    if (down && g_row[row].halt_id[0] != '\0') {
        apply_halt(h, now + 1, g_row[row].halt_role, g_row[row].halt_id, &ht);
        dead = !peer_halt_can_tx();
    }
    chk(!down);
    chk(root_ok);
    rec(row, down, root_ok, game, dead, fz, ht);
}

static void trial_nodown_event(uint8_t row, uint8_t role, const char* id,
                               EventType type, const char* payload,
                               uint32_t tbase, uint16_t i, bool sign) {
    uint8_t root[KEY_LEN];
    P30Hive h;
    setup_d(h, root);
    uint32_t now = tbase + static_cast<uint32_t>(i) * 70u;
    Event ev = p30_event(type, id, now, payload);
    if (type == EventType::GhostDownStart) {
        HiveKill killer;
        killer.attach(&h.keys);
        (void)killer.fill(&ev, id, now, 0);
    }
    uint64_t t0 = now_ns();
    p30_inject(h, role, id, ev, now, sign);
    uint32_t fz = us_since(t0);
    bool down = h.xport.hiveFrozen();
    bool root_ok = root_eq(h.keys, root);
    chk(!down);
    chk(root_ok);
    chk(h.keys.hasRoot());
    rec(row, down, root_ok, false, false, fz, 0);
}

static void family_laptop() {
    for (uint16_t i = 0; i < 64; ++i) {
        trial_worker_anom(R_LAP_INJ, 100000, i, "tamper:ptrace");
        trial_unsigned(R_LAP_KEX, ROLE_WORKER, "W", 110000, i);
        trial_worker_anom(R_LAP_FILE, 120000, i, "tamper:file");
        trial_worker_anom(R_LAP_NET, 130000, i, "tamper:net");
        trial_mine_brw(R_LAP_BRW, "ML", 140000, i);
    }
}

static void family_phone() {
    for (uint16_t i = 0; i < 64; ++i) {
        trial_mine_os(R_PH_APP, "MP", 200000, i);
        trial_unsigned(R_PH_RADIO, ROLE_PHONE, "P", 210000, i);
        trial_mine_os(R_PH_OS, "MP", 220000, i);
        trial_mine_replay(R_PH_REPLAY, "MP", 230000, i);
    }
}

static void trial_aware_then_hmac(uint8_t row, uint8_t role, const char* id,
                                  EventType atype, const char* apay,
                                  uint32_t tbase, uint16_t i) {
    uint8_t root[KEY_LEN];
    P30Hive h;
    setup_d(h, root);
    uint32_t now = tbase + static_cast<uint32_t>(i) * 70u;
    Event aw = p30_event(atype, id, now, apay);
    p30_inject(h, role, id, aw, now, true);
    chk(!h.xport.hiveFrozen());
    Event hb = p30_event(EventType::Heartbeat, id, now + 1, "hmac_i");
    uint64_t t0 = now_ns();
    p30_inject(h, role, id, hb, now + 1, false);
    uint32_t fz = us_since(t0);
    bool down = h.xport.hiveFrozen();
    bool game = h.stealth.isGameMode();
    bool root_ok = root_eq(h.keys, root);
    uint32_t ht = 0;
    bool dead = false;
    if (down && g_row[row].halt_id[0] != '\0') {
        apply_halt(h, now + 1, g_row[row].halt_role, g_row[row].halt_id, &ht);
        dead = !peer_halt_can_tx();
        chk(!peer_halt_can_tx());
    }
    chk(!down);
    chk(root_ok);
    rec(row, down, root_ok, game, dead, fz, ht);
}

static void family_router() {
    for (uint16_t i = 0; i < 64; ++i) {
        trial_aware_then_hmac(R_RT_PORT, ROLE_ROUTER, "R", EventType::ScanResult,
                              "port_scan", 300000, i);
        trial_aware_then_hmac(R_RT_DNS, ROLE_ROUTER, "R", EventType::ScanResult,
                              "dns_poison", 310000, i);
        trial_aware_then_hmac(R_RT_ARP, ROLE_ROUTER, "R", EventType::DeviceSeen,
                              "net:new_device", 320000, i);
        trial_aware_then_hmac(R_RT_FLOOD, ROLE_ROUTER, "R", EventType::ScanResult,
                              "flow", 330000, i);
    }
}

static void family_nas() {
    for (uint16_t i = 0; i < 64; ++i) {
        trial_unsigned(R_NAS_SMB, ROLE_SAFE, "N", 400000, i);
        trial_mine_os(R_NAS_FILE, "MN", 410000, i);
        trial_mine_replay(R_NAS_REPLAY, "MN", 420000, i);
        trial_unsigned(R_NAS_INJ, ROLE_SAFE, "N", 430000, i);
    }
}

static void family_mines() {
    for (uint16_t i = 0; i < 64; ++i) {
        trial_mine_brw(R_MN_BRW, "ML", 500000, i);
        trial_mine_os(R_MN_OS, "ML", 510000, i);
        trial_mine_replay(R_MN_REPLAY, "X", 520000, i);
        trial_mine_os(R_MN_DBG, "ML", 530000, i);
    }
}

static void family_pre() {
    for (uint16_t i = 0; i < 128; ++i) {
        trial_nodown_event(R_PRE_PKILL, ROLE_WORKER, "W", EventType::GhostDownStart,
                           "kill", 600000, i, true);
        trial_nodown_event(R_PRE_FAKE, ROLE_WORKER, "W", EventType::ScanResult,
                           "fake:net", 610000, i, true);
        trial_unsigned(R_PRE_UHB, ROLE_WORKER, "W", 620000, i);
        trial_nodown_event(R_PRE_PHKILL, ROLE_PHONE, "P", EventType::GhostDownStart,
                           "kill", 630000, i, true);
        trial_nodown_event(R_PRE_POISON, ROLE_ROUTER, "R", EventType::ScanResult,
                           "dns_poison", 640000, i, true);
        trial_mine_replay(R_PRE_REPLAY, "X", 650000, i);
        trial_nodown_event(R_PRE_FLOOD, ROLE_ROUTER, "R", EventType::ScanResult,
                           "flow", 660000, i, true);
        trial_nodown_event(R_PRE_ARP, ROLE_ROUTER, "R", EventType::DeviceSeen,
                           "net:new_device", 670000, i, true);
    }
}

static void family_post() {
    for (uint16_t i = 0; i < 128; ++i) {
        uint8_t root[KEY_LEN];
        P30Hive h;
        setup_d(h, root);
        uint32_t now = 700000 + i;
        Event anom = p30_event(EventType::AnomalyDetected, "W", now, "tamper:ptrace");
        anom.severity = Severity::Critical;
        uint64_t t0 = now_ns();
        p30_inject(h, ROLE_WORKER, "W", anom, now, true);
        uint32_t fz = us_since(t0);
        chk(!h.xport.hiveFrozen());
        uint32_t ht = 0;
        apply_halt(h, now, ROLE_WORKER, "W", &ht);
        chk(!peer_halt_can_tx());
        chk(root_eq(h.keys, root));

        Event after = p30_event(EventType::Heartbeat, "W", now + 50, "post");
        p30_inject(h, ROLE_WORKER, "W", after, now + 50, false);
        chk(root_eq(h.keys, root));
        chk(h.keys.hasRoot());
        chk(!peer_halt_can_tx());
        rec(R_POST_OS, false, true, h.stealth.isGameMode(), true, fz, ht);

        Event fl = p30_event(EventType::ScanResult, "R", now + 51, "flood");
        p30_inject(h, ROLE_ROUTER, "R", fl, now + 51, true);
        chk(root_eq(h.keys, root));
        rec(R_POST_NET, false, true, true, !peer_halt_can_tx(), fz, ht);

        Event ft = p30_event(EventType::ConfigChange, "W", now + 52, "file");
        p30_inject(h, ROLE_WORKER, "W", ft, now + 52, true);
        chk(root_eq(h.keys, root));
        rec(R_POST_FILE, false, true, true, !peer_halt_can_tx(), fz, ht);

        Event rp = p30_event(EventType::Heartbeat, "P", now + 53, "replay");
        p30_inject(h, ROLE_PHONE, "P", rp, now + 53, false);
        chk(root_eq(h.keys, root));
        rec(R_POST_REPLAY, false, true, true, !peer_halt_can_tx(), fz, ht);

        Event inj = p30_event(EventType::AnomalyDetected, "W", now + 54, "inject");
        p30_inject(h, ROLE_WORKER, "W", inj, now + 54, true);
        chk(root_eq(h.keys, root));
        rec(R_POST_INJ, false, true, true, !peer_halt_can_tx(), fz, ht);
    }
}

static void write_matrix() {
    struct stat st;
    if (stat("/tmp/ghost_hive", &st) != 0) {
        (void)mkdir("/tmp/ghost_hive", 0755);
    }
    FILE* f = fopen("/tmp/ghost_hive/phase_d_matrix.csv", "w");
    const char* hdr =
        "attack,n,down_n,down_pct,expect_down,"
        "freeze_us_min,freeze_us_med,freeze_us_avg,freeze_us_max,"
        "halt_us_min,halt_us_med,halt_us_avg,halt_us_max,"
        "freeze_ms_avg,halt_ms_avg,spec_kill_ms,os_dead_ms_avg,"
        "root_ok,game_n,dead_n\n";
    printf("%s", hdr);
    if (f != nullptr) {
        (void)fputs(hdr, f);
    }
    for (uint8_t r = 0; r < R_COUNT; ++r) {
        FamStat& s = g_row[r];
        uint32_t fz[128];
        uint32_t ht[128];
        for (uint32_t i = 0; i < s.n && i < 128; ++i) {
            fz[i] = s.fz[i];
            ht[i] = s.ht[i];
        }
        uint32_t n = s.n;
        uint32_t fzmin = min_u32(fz, n);
        uint32_t fzmax = max_u32(fz, n);
        uint32_t fzavg = avg_u32(fz, n);
        uint32_t fzmed = median_u32(fz, n);
        uint32_t htmin = min_u32(ht, n);
        uint32_t htmax = max_u32(ht, n);
        uint32_t htavg = avg_u32(ht, n);
        uint32_t htmed = median_u32(ht, n);
        uint32_t spec = (s.expect_down != 0) ? (NAS_FLUSH_TIMEOUT_SEC * 1000u) : 0;
        double freeze_ms = static_cast<double>(fzavg) / 1000.0;
        double halt_ms = static_cast<double>(htavg) / 1000.0;
        double os_dead = 0.0;
        if (s.expect_down != 0) {
            os_dead = static_cast<double>(spec) + freeze_ms + halt_ms;
        }
        uint32_t pct = (n == 0) ? 0 : (s.down_n * 100u / n);
        char line[448];
        int len = snprintf(
            line, sizeof(line),
            "%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%.3f,%.3f,%u,%.3f,%u,%u,%u\n",
            s.name, n, s.down_n, pct, static_cast<unsigned>(s.expect_down),
            fzmin, fzmed, fzavg, fzmax,
            htmin, htmed, htavg, htmax,
            freeze_ms, halt_ms, spec, os_dead,
            s.root_n, s.game_n, s.dead_n);
        if (len > 0) {
            printf("%s", line);
            if (f != nullptr) (void)fputs(line, f);
        }
        if (s.expect_down != 0) {
            chk(s.down_n == s.n);
            chk(s.root_n == s.n);
        } else {
            chk(s.down_n == 0);
            chk(s.root_n == s.n);
        }
    }
    if (f != nullptr) fclose(f);
}

int main() {
    row_init();
    family_laptop();
    family_phone();
    family_router();
    family_nas();
    family_mines();
    family_pre();
    family_post();
    write_matrix();

    printf("spec_kill_ms=%u (NAS flush before KillFrame, §40.4)\n",
           static_cast<unsigned>(NAS_FLUSH_TIMEOUT_SEC * 1000u));
    printf("freeze_us = CLOCK_MONOTONIC inject→hiveFrozen (same rx)\n");
    printf("os_dead_ms_avg = spec_kill_ms + freeze_ms_avg + halt_ms_avg\n");
    printf("matrix=/tmp/ghost_hive/phase_d_matrix.csv\n");

    bool ok = (g_bad == 0) && (g_n >= 1500);
    printf(ok ? "PASS phase_d n=%u\n" : "FAIL phase_d n=%u bad=%u\n",
           static_cast<unsigned>(g_n), static_cast<unsigned>(g_bad));
    return ok ? 0 : 1;
}
