// =====================================================
// Ghost Hive v1.7.1 — Browser / OS / IoT Minen
// Spec-Basis: §4, §11, §22, §38
// Send-only MinePayload. Kein recv().
// =====================================================

#include "mine.h"
#include "browser_mine.h"
#include "os_mine.h"
#include "iot_mine.h"
#include "../laptop/peer_keys.h"
#include "transport/mine_transport.h"
#include "transport/medium_wlan.h"
#include "psp_time.h"

static const char PEER_BIND_PATH[] = "/tmp/ghost_hive/peer.bind";

static void copyArg(char* dst, const char* src, uint8_t max) {
    uint8_t i = 0;
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i < max) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static bool argIs(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) return false;
    uint8_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return false;
        ++i;
    }
    return a[i] == b[i];
}

int main(int argc, char** argv) {
    char ip[32];
    copyArg(ip, "255.255.255.255", 31);

    const char* kind = "browser";
    const char* trigger = "url";
    char id[32];
    id[0] = '\0';
    bool gotIp = false;
    for (int i = 1; i < argc; ++i) {
        if (argIs(argv[i], "os") || argIs(argv[i], "browser") ||
            argIs(argv[i], "iot")) {
            kind = argv[i];
            continue;
        }
        if (argIs(argv[i], "url") || argIs(argv[i], "process") ||
            argIs(argv[i], "file")) {
            trigger = argv[i];
            continue;
        }
        char mid[32];
        if (mine_arg_id(argv[i], mid)) {
            copyArg(id, mid, 31);
            continue;
        }
        if (argv[i] == nullptr || argv[i][0] == '\0' || argv[i][0] == '-') continue;
        bool looksIp = false;
        uint8_t d = 0;
        while (argv[i][d] != '\0') {
            if (argv[i][d] == '.') looksIp = true;
            ++d;
        }
        if (looksIp && !gotIp) {
            copyArg(ip, argv[i], 31);
            gotIp = true;
        } else if (id[0] == '\0') {
            copyArg(id, argv[i], 31);
        }
    }
    if (id[0] == '\0') {
        if (argIs(kind, "os")) copyArg(id, HIVE_ID_MINE_OS, 31);
        else if (argIs(kind, "iot")) copyArg(id, HIVE_ID_MINE_IOT, 31);
        else copyArg(id, HIVE_ID_MINE_BROWSER, 31);
    }

    GhostKeys keys;
    keys.initEmpty();
    (void)peer_load_keys(keys, PEER_BIND_PATH);

    MediumWlan wlan;
    if (!wlan.connectKernel(id, ip, GHOST_UDP_PORT)) {
        return 1;
    }

    MineTransport mineLink;
    mineLink.init(id);
    mineLink.attach(&wlan, nullptr);

    BrowserMine browser;
    OsMine os;
    IotMine iot;
    browser.init(id);
    os.init(id);
    iot.init(id);
    if (keys.hasTotpSeed()) {
        (void)browser.setTotpSeed(keys.totpSeed(), TOTP_SEED_LEN);
        (void)os.setTotpSeed(keys.totpSeed(), TOTP_SEED_LEN);
        (void)iot.setTotpSeed(keys.totpSeed(), TOTP_SEED_LEN);
    }

    bool sent = false;
    uint32_t lastMine = 0;
    while (true) {
        uint32_t nowSec = psp_now_sec();
        if (!sent || (nowSec - lastMine) >= MINE_INTERVAL_SEC) {
            MinePayload mp{};
            bool ok = false;
            if (argIs(kind, "os")) {
                if (argIs(trigger, "file")) ok = os.onSuspiciousFile(&mp, nowSec);
                else ok = os.onSuspiciousProcess(&mp, nowSec);
            } else if (argIs(kind, "iot")) {
                ok = iot.send(&mp, nowSec);
            } else {
                ok = browser.onSuspiciousUrl(&mp, nowSec);
            }
            if (ok) {
                (void)peer_sign_mine(keys, mp);
                (void)mineLink.send(mp, nowSec);
            }
            lastMine = nowSec;
            sent = true;
        }
        MinePayload ignored{};
        (void)mineLink.recv(ignored);
        psp_sleep_ms(10);
    }
}
