// Phase C — §33.1 Wrap, §43 Kill, §46 TETACT, P01–P15
// Familien mit Variationen. Hive darf fallen. PSP/Root/Kill nicht.

#include "p30_harness.h"
#include "../src/psp/ghost_wrap.h"
#include "../src/laptop/tetact.h"
#include "../src/laptop/kill.h"
#include "../src/laptop/peer_halt.h"
#include "../src/mine/os_mine.h"
#include "../src/mine/browser_mine.h"

static uint16_t g_n = 0;
static uint16_t g_bad = 0;

static void chk(bool cond) {
    ++g_n;
    if (!cond) ++g_bad;
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

static void fill_key(uint8_t* k, uint8_t seed) {
    for (uint8_t i = 0; i < 32; ++i) {
        k[i] = static_cast<uint8_t>(seed + i * 3);
    }
}

static void family_usb() {
    chk(ghost_wrap_selftest());

    uint8_t master[32];
    uint8_t salt[16];
    uint8_t wk[32];
    uint8_t root[32];
    uint8_t blob[ROOT_WRAP_LEN];
    chk(ghost_wrap_master(master));
    for (uint8_t i = 0; i < 16; ++i) salt[i] = static_cast<uint8_t>(0xA0 + i);
    const uint8_t pass[5] = {'n', 'o', 'a', 'h', 0};
    chk(ghost_wrap_kdf(master, pass, 4, salt, wk));
    fill_key(root, 0x21);
    chk(ghost_wrap_seal(wk, root, blob, GHOST_WRAP_FLAG_PASS));

    uint8_t same = 1;
    for (uint8_t i = 0; i < 32; ++i) {
        if (blob[40 + i] != root[i]) same = 0;
    }
    chk(same == 0);

    for (uint16_t off = 0; off < ROOT_WRAP_LEN; ++off) {
        uint8_t mut[ROOT_WRAP_LEN];
        for (uint16_t i = 0; i < ROOT_WRAP_LEN; ++i) mut[i] = blob[i];
        mut[off] = static_cast<uint8_t>(mut[off] ^ 0x5A);
        uint8_t out[32];
        for (uint8_t i = 0; i < 32; ++i) out[i] = 0xFF;
        chk(!ghost_wrap_open(wk, mut, out));
        uint8_t leak = 1;
        for (uint8_t i = 0; i < 32; ++i) {
            if (out[i] != root[i]) leak = 0;
        }
        chk(leak == 0);
    }

    for (uint8_t t = 0; t < 64; ++t) {
        uint8_t bad[32];
        fill_key(bad, static_cast<uint8_t>(t + 1));
        uint8_t match = 1;
        for (uint8_t i = 0; i < 32; ++i) {
            if (bad[i] != wk[i]) match = 0;
        }
        if (match != 0) continue;
        uint8_t out[32];
        for (uint8_t i = 0; i < 32; ++i) out[i] = 0;
        chk(!ghost_wrap_open(bad, blob, out));
        uint8_t leak = 1;
        for (uint8_t i = 0; i < 32; ++i) {
            if (out[i] != root[i]) leak = 0;
        }
        chk(leak == 0);
    }

    for (uint8_t t = 0; t < 16; ++t) {
        uint8_t wrong[5] = {'n', 'o', 'a', 'h', 0};
        wrong[3] = static_cast<uint8_t>('A' + t);
        if (wrong[3] == 'h') wrong[3] = 'x';
        uint8_t wk2[32];
        chk(ghost_wrap_kdf(master, wrong, 4, salt, wk2));
        uint8_t out[32];
        chk(!ghost_wrap_open(wk2, blob, out));
    }

    for (uint8_t t = 0; t < 8; ++t) {
        uint8_t wk_old[32];
        uint8_t wk_new[32];
        uint8_t salt2[16];
        uint8_t blob_old[ROOT_WRAP_LEN];
        fill_key(wk_old, static_cast<uint8_t>(0x30 + t));
        fill_key(wk_new, static_cast<uint8_t>(0x90 + t));
        for (uint8_t i = 0; i < 16; ++i) salt2[i] = static_cast<uint8_t>(t + i);
        (void)salt2;
        chk(ghost_wrap_seal(wk_old, root, blob_old, 0));
        uint8_t out[32];
        chk(!ghost_wrap_open(wk_new, blob_old, out));
    }

    for (uint8_t t = 0; t < 8; ++t) {
        uint8_t r[32];
        fill_key(r, static_cast<uint8_t>(0x40 + t));
        uint8_t b[ROOT_WRAP_LEN];
        uint8_t back[32];
        chk(ghost_wrap_seal(wk, r, b, 0));
        chk(ghost_wrap_open(wk, b, back));
        uint8_t ok = 1;
        for (uint8_t i = 0; i < 32; ++i) {
            if (back[i] != r[i]) ok = 0;
        }
        chk(ok == 1);
    }
}

static void setup_hive(P30Hive& h, uint8_t* rootSnap) {
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
}

static void family_worker() {
    uint8_t root[KEY_LEN];
    {
        P30Hive h;
        setup_hive(h, root);
        HiveKill killer;
        killer.attach(&h.keys);
        chk(!killer.canWriteFlag());
        for (uint8_t i = 0; i < 16; ++i) {
            Event req{};
            chk(killer.fill(&req, "W", static_cast<uint32_t>(4000 + i * 7), 0));
            p30_inject(h, ROLE_WORKER, "W", req, static_cast<uint32_t>(4000 + i * 7), true);
            chk(!h.xport.hiveFrozen());
            chk(root_eq(h.keys, root));
        }
        chk(p30_has_type(h.vault, EventType::PolicyViolation));
    }
    {
        P30Hive h;
        setup_hive(h, root);
        for (uint8_t i = 0; i < 16; ++i) {
            char tag[16];
            tag[0] = 'f';
            tag[1] = 'a';
            tag[2] = 'k';
            tag[3] = 'e';
            tag[4] = ':';
            tag[5] = static_cast<char>('A' + (i % 26));
            tag[6] = '\0';
            Event sc = p30_event(EventType::ScanResult, "W",
                                 static_cast<uint32_t>(5000 + i), tag);
            p30_inject(h, ROLE_WORKER, "W", sc, static_cast<uint32_t>(5000 + i), true);
            chk(!h.xport.hiveFrozen());
            chk(root_eq(h.keys, root));
        }
    }
    {
        P30Hive h;
        setup_hive(h, root);
        for (uint8_t i = 0; i < 2; ++i) {
            Event miss = p30_event(EventType::HeartbeatMiss, "W",
                                   static_cast<uint32_t>(6000 + i),
                                   "worker_degraded");
            p30_inject(h, ROLE_WORKER, "W", miss,
                       static_cast<uint32_t>(6000 + i), true);
            chk(!h.xport.hiveFrozen());
            chk(root_eq(h.keys, root));
        }
    }
    {
        P30Hive h;
        setup_hive(h, root);
        Event anom = p30_event(EventType::AnomalyDetected, "W", 7000,
                               "tamper:ptrace");
        anom.severity = Severity::Critical;
        p30_inject(h, ROLE_WORKER, "W", anom, 7000, true);
        chk(!h.xport.hiveFrozen());
        chk(root_eq(h.keys, root));
    }
    {
        const uint32_t deltas[12] = {
            301, 400, 600, 900, 1200, 2400, 5000, 9000, 50, 1, 299, 100000};
        for (uint8_t i = 0; i < 12; ++i) {
            TetactState st;
            tetact_init(st);
            Event ev{};
            (void)tetact_watch(st, 8000, &ev);
            TetactKind k = tetact_watch(st, 8000 - deltas[i], &ev);
            if (deltas[i] >= TETACT_TIME_WARP_SEC) {
                chk(k == TETACT_TAMPER);
                chk(ev.type == EventType::AnomalyDetected);
            } else {
                chk(k != TETACT_TAMPER || ev.type == EventType::AnomalyDetected);
            }
        }
    }
}

static void family_phone_router() {
    uint8_t root[KEY_LEN];
    {
        P30Hive h;
        setup_hive(h, root);
        HiveKill killer;
        killer.attach(&h.keys);
        for (uint8_t i = 0; i < 8; ++i) {
            uint32_t t = static_cast<uint32_t>(20000 + i * 70);
            Event req{};
            chk(killer.fill(&req, "P", t, 0));
            p30_inject(h, ROLE_PHONE, "P", req, t, true);
            chk(!h.xport.hiveFrozen());
            chk(root_eq(h.keys, root));
        }
    }
    {
        P30Hive h;
        setup_hive(h, root);
        for (uint8_t i = 0; i < 16; ++i) {
            uint32_t t = static_cast<uint32_t>(21000 + i * 70);
            Event sc = p30_event(EventType::ScanResult, "P", t, "os:wlan");
            p30_inject(h, ROLE_PHONE, "P", sc, t, true);
            chk(root_eq(h.keys, root));
        }
        chk(!h.xport.hiveFrozen());
    }
    {
        P30Hive h;
        setup_hive(h, root);
        for (uint8_t i = 0; i < 8; ++i) {
            uint32_t t = static_cast<uint32_t>(22000 + i * 70);
            Event an = p30_event(EventType::AnomalyDetected, "P", t, "tamper:time");
            an.severity = Severity::Critical;
            p30_inject(h, ROLE_PHONE, "P", an, t, true);
            chk(!h.xport.hiveFrozen());
            chk(root_eq(h.keys, root));
        }
    }
    {
        P30Hive h;
        setup_hive(h, root);
        for (uint8_t i = 0; i < 8; ++i) {
            uint32_t t = static_cast<uint32_t>(23000 + i * 70);
            Event ds = p30_event(EventType::DeviceSeen, "P", t, "net:new_device");
            p30_inject(h, ROLE_PHONE, "P", ds, t, true);
            chk(root_eq(h.keys, root));
        }
        chk(!h.xport.hiveFrozen());
    }
    {
        P30Hive h;
        setup_hive(h, root);
        for (uint8_t i = 0; i < 16; ++i) {
            uint32_t t = static_cast<uint32_t>(24000 + i * 70);
            Event ds = p30_event(EventType::DeviceSeen, "R", t, "router_new_client");
            p30_inject(h, ROLE_ROUTER, "R", ds, t, true);
            chk(root_eq(h.keys, root));
        }
        chk(!h.xport.hiveFrozen());
        for (uint8_t i = 0; i < 8; ++i) {
            uint32_t t = static_cast<uint32_t>(25000 + i * 70);
            Event sc = p30_event(EventType::ScanResult, "R", t, "port_scan");
            p30_inject(h, ROLE_ROUTER, "R", sc, t, true);
            chk(root_eq(h.keys, root));
        }
        for (uint8_t i = 0; i < 8; ++i) {
            uint32_t t = static_cast<uint32_t>(26000 + i * 70);
            Event fl = p30_event(EventType::ScanResult, "R", t, "dns_poison");
            p30_inject(h, ROLE_ROUTER, "R", fl, t, true);
            chk(root_eq(h.keys, root));
        }
    }
    {
        P30Hive h;
        setup_hive(h, root);
        for (uint8_t i = 0; i < 12; ++i) {
            Event fl = p30_event(EventType::ScanResult, "R",
                                 static_cast<uint32_t>(27000 + i), "flow");
            p30_inject(h, ROLE_ROUTER, "R", fl,
                       static_cast<uint32_t>(27000 + i), true);
            chk(root_eq(h.keys, root));
        }
        chk(h.keys.hasRoot());
    }
}

static void family_wlan_mines() {
    uint8_t root[KEY_LEN];
    {
        P30Hive h;
        setup_hive(h, root);
        for (uint8_t i = 0; i < 16; ++i) {
            Event ap = p30_event(EventType::ScanResult, "W",
                                 static_cast<uint32_t>(11000 + i), "wifi:evil");
            p30_inject(h, ROLE_WORKER, "W", ap,
                       static_cast<uint32_t>(11000 + i), true);
            chk(!h.xport.hiveFrozen());
            chk(root_eq(h.keys, root));
        }
        for (uint8_t i = 0; i < 8; ++i) {
            uint32_t t = static_cast<uint32_t>(11100 + i * 70);
            Event de = p30_event(EventType::AnomalyDetected, "P", t, "deauth");
            p30_inject(h, ROLE_PHONE, "P", de, t, true);
            chk(!h.xport.hiveFrozen());
            chk(root_eq(h.keys, root));
        }
        chk(!h.xport.hiveFrozen());
    }
    {
        P30Hive h;
        setup_hive(h, root);
        Event hb = p30_event(EventType::Heartbeat, "W", 13000, "tamper");
        p30_inject(h, ROLE_WORKER, "W", hb, 13000, false);
        chk(root_eq(h.keys, root));
        chk(!h.xport.hiveFrozen());
        chk(p30_has_type(h.vault, EventType::PolicyViolation));
    }
    {
        P30Hive h;
        setup_hive(h, root);
        OsMine os;
        os.init("ML");
        chk(os.setTotpSeed(h.keys.totpSeed(), TOTP_SEED_LEN));
        Device md{};
        p30_copy_id(md.id, "ML");
        md.role = ROLE_MINE;
        md.status = DeviceState::Silent;
        (void)h.reg.addDevice(md);
        for (uint8_t i = 0; i < 4; ++i) {
            MinePayload mp{};
            chk(os.onSuspiciousProcess(&mp, static_cast<uint32_t>(12000 + i * 90)));
            p30_inject_mine(h, mp, static_cast<uint32_t>(12000 + i * 90), true);
            chk(root_eq(h.keys, root));
        }
        chk(!h.xport.hiveFrozen());
        chk(os.recv(nullptr) == false);
        chk(os.canReceive() == false);
    }
    {
        P30Hive h;
        setup_hive(h, root);
        BrowserMine br;
        br.init("MB");
        chk(br.setTotpSeed(h.keys.totpSeed(), TOTP_SEED_LEN));
        Device md{};
        p30_copy_id(md.id, "MB");
        md.role = ROLE_MINE;
        md.status = DeviceState::Silent;
        (void)h.reg.addDevice(md);
        for (uint8_t i = 0; i < 4; ++i) {
            MinePayload mp{};
            chk(br.onSuspiciousUrl(&mp, static_cast<uint32_t>(13000 + i * 90)));
            p30_inject_mine(h, mp, static_cast<uint32_t>(13000 + i * 90), true);
            chk(root_eq(h.keys, root));
        }
        chk(!h.xport.hiveFrozen());
        chk(br.canReceive() == false);
    }
    {
        P30Hive h;
        setup_hive(h, root);
        Mine m;
        m.init("X");
        chk(m.setTotpSeed(h.keys.totpSeed(), TOTP_SEED_LEN));
        MinePayload a{};
        chk(m.send(&a, 14000));
        p30_inject_mine(h, a, 14000, true);
        MinePayload replay = a;
        p30_inject_mine(h, replay, 14001, true);
        chk(root_eq(h.keys, root));
        m.freezeEvents();
        MinePayload z{};
        chk(!m.send(&z, 14100));
        chk(!m.canReceive());
    }
}

static void family_combo() {
    uint8_t root[KEY_LEN];
    const uint8_t orders[8][3] = {
        {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0},
        {2, 0, 1}, {2, 1, 0}, {0, 1, 2}, {2, 0, 1}};
    for (uint8_t o = 0; o < 8; ++o) {
        P30Hive h;
        setup_hive(h, root);
        HiveKill killer;
        killer.attach(&h.keys);
        for (uint8_t s = 0; s < 3; ++s) {
            uint8_t step = orders[o][s];
            if (step == 0) {
                Event req{};
                (void)killer.fill(&req, "W", static_cast<uint32_t>(15000 + o), 0);
                p30_inject(h, ROLE_WORKER, "W", req,
                           static_cast<uint32_t>(15000 + o), true);
                chk(!h.xport.hiveFrozen() || h.keys.hasRoot());
            } else if (step == 1) {
                Event ap = p30_event(EventType::ScanResult, "P",
                                     static_cast<uint32_t>(15100 + o), "wifi:evil");
                p30_inject(h, ROLE_PHONE, "P", ap,
                           static_cast<uint32_t>(15100 + o), true);
            } else {
                Event ds = p30_event(EventType::DeviceSeen, "R",
                                     static_cast<uint32_t>(15200 + o),
                                     "router_new_client");
                p30_inject(h, ROLE_ROUTER, "R", ds,
                           static_cast<uint32_t>(15200 + o), true);
            }
            chk(root_eq(h.keys, root));
        }
        chk(!h.xport.hiveFrozen());
        chk(h.keys.hasRoot());
    }
}

static void family_pre_down() {
    // Angriffe BEVOR Ghost Down aktiv ist. Hive darf fallen. Root/Kill nicht.
    uint8_t root[KEY_LEN];
    {
        P30Hive h;
        setup_hive(h, root);
        h.stealth.enterTerminalMode();
        chk(!h.stealth.isGameMode());
        chk(!h.down.isActive());

        HiveKill killer;
        killer.attach(&h.keys);
        for (uint8_t i = 0; i < 8; ++i) {
            Event req{};
            chk(killer.fill(&req, "W", static_cast<uint32_t>(30000 + i), 0));
            p30_inject(h, ROLE_WORKER, "W", req, static_cast<uint32_t>(30000 + i), true);
            chk(!h.down.isActive());
            chk(!h.xport.hiveFrozen());
            chk(root_eq(h.keys, root));
        }
        for (uint8_t i = 0; i < 8; ++i) {
            Event sc = p30_event(EventType::ScanResult, "W",
                                 static_cast<uint32_t>(30100 + i), "pre:fake");
            p30_inject(h, ROLE_WORKER, "W", sc, static_cast<uint32_t>(30100 + i), true);
            chk(!h.down.isActive());
            chk(root_eq(h.keys, root));
        }

        Event anom = p30_event(EventType::AnomalyDetected, "W", 30200, "tamper:ptrace");
        anom.severity = Severity::Critical;
        p30_inject(h, ROLE_WORKER, "W", anom, 30200, true);
        chk(!h.xport.hiveFrozen());
        chk(!h.down.isActive());
        chk(root_eq(h.keys, root));

        // NAS-Fenster: Tarnung muss schon Game sein.
        h.down.tick(30201);
        chk(!h.stealth.isGameMode());
        chk(root_eq(h.keys, root));

        for (uint8_t i = 0; i < 8; ++i) {
            Event hb = p30_event(EventType::Heartbeat, "W",
                                 static_cast<uint32_t>(30300 + i), "post");
            p30_inject(h, ROLE_WORKER, "W", hb,
                       static_cast<uint32_t>(30300 + i), false);
            chk(!h.stealth.isGameMode());
            chk(root_eq(h.keys, root));
            chk(h.keys.hasRoot());
        }
    }
    {
        P30Hive h;
        setup_hive(h, root);
        h.stealth.enterTerminalMode();
        (void)h.xport.enterHiveDown(31000);
        chk(h.down.isActive());
        chk(h.stealth.isGameMode());
        chk(!h.stealth.isInvisible());
        HiveKill killer;
        killer.attach(&h.keys);
        Event req{};
        chk(killer.fill(&req, "P", 31001, 0));
        p30_inject(h, ROLE_PHONE, "P", req, 31001, true);
        chk(root_eq(h.keys, root));
        h.down.tick(31006);
        chk(h.stealth.isGameMode());
        chk(h.down.peekAllowed());
        chk(root_eq(h.keys, root));
        peer_halt_reset();
        peer_halt_run(ROLE_WORKER, "W");
        chk(!peer_halt_can_tx());
        chk(peer_halt_has_marker("W"));
        peer_halt_reset();
        peer_halt_run(ROLE_PHONE, "P");
        chk(peer_halt_has_marker("P"));
        peer_halt_reset();
        peer_halt_run(ROLE_ROUTER, "R");
        chk(peer_halt_has_marker("R"));
        peer_halt_reset();
        peer_halt_run(ROLE_SAFE, "N");
        chk(peer_halt_has_marker("N"));
        peer_halt_reset();
        peer_halt_run(ROLE_SENSOR, "F");
        chk(peer_halt_has_marker("F"));
        chk(root_eq(h.keys, root));
        chk(h.keys.hasRoot());
        peer_halt_run(ROLE_KERNEL, "K");
        chk(h.keys.hasRoot());
        chk(!peer_halt_has_marker("K"));
    }
}

int main() {
    family_usb();
    family_worker();
    family_phone_router();
    family_wlan_mines();
    family_combo();
    family_pre_down();

    bool ok = (g_bad == 0) && (g_n >= 300);
    printf(ok ? "PASS phase_c n=%u\n" : "FAIL phase_c n=%u bad=%u\n",
           static_cast<unsigned>(g_n), static_cast<unsigned>(g_bad));
    return ok ? 0 : 1;
}
