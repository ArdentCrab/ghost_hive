// =====================================================
// Ghost Hive v1.7.1 — NAS Safe + Honigtopf
// Spec-Basis: §5, §7, §11, §25, §28, §38
// hive-safe + hive-index + hive-access + ghost-mine-nas
// Alarmiert nie. Honigtopf nie auf der PSP.
// =====================================================

#include "safe.h"
#include "index.h"
#include "access.h"
#include "honeypot.h"
#include "../laptop/peer_keys.h"
#include "../laptop/peer_halt.h"
#include "transport/safe_transport.h"
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
    char id[32];
    copyArg(ip, "255.255.255.255", 31);
    copyArg(id, HIVE_ID_SAFE, 31);

    const char* share = nullptr;
    char mineIds[HOST_MINE_SLOTS][32];
    uint8_t mineN = 0;
    bool gotIp = false;
    bool gotId = false;
    for (int i = 1; i < argc; ++i) {
        if (argIs(argv[i], "lockvogel") || argIs(argv[i], "honeypot")) {
            share = argv[i];
            continue;
        }
        char mid[32];
        if (mine_arg_id(argv[i], mid)) {
            (void)mine_push_id(mineIds, &mineN, HOST_MINE_SLOTS, mid);
            continue;
        }
        if (argv[i] == nullptr || argv[i][0] == '\0' || argv[i][0] == '-') continue;
        if (!gotIp) {
            copyArg(ip, argv[i], 31);
            gotIp = true;
        } else if (!gotId) {
            copyArg(id, argv[i], 31);
            gotId = true;
        }
    }
    if (mineN == 0) {
        (void)mine_push_id(mineIds, &mineN, HOST_MINE_SLOTS, HIVE_ID_MINE_NAS);
    }

    Safe safe;
    safe.init(id);
    HiveIndex index;
    index.attach(&safe);
    HiveAccess access;

    GhostKeys keys;
    keys.initEmpty();
    (void)peer_load_keys(keys, PEER_BIND_PATH);
    if (keys.hasDevice()) {
        (void)safe.provisionDeviceKey(keys.device(), KEY_LEN);
    }

    MediumWlan wlan;
    if (!wlan.connectKernel(id, ip, GHOST_UDP_PORT)) {
        return 1;
    }

    SafeTransport link;
    link.init(id);
    link.attach(&wlan, nullptr);

    NasHoneypot pots[HOST_MINE_SLOTS];
    MineTransport mineLinks[HOST_MINE_SLOTS];
    uint32_t lastMineAt[HOST_MINE_SLOTS];
    for (uint8_t m = 0; m < mineN; ++m) {
        pots[m].init(mineIds[m]);
        mineLinks[m].init(mineIds[m]);
        mineLinks[m].attach(&wlan, nullptr);
        lastMineAt[m] = 0;
        if (keys.hasTotpSeed()) {
            (void)pots[m].setTotpSeed(keys.totpSeed(), TOTP_SEED_LEN);
        }
    }

    bool decoySent = false;
    char idxOut[32];
    (void)index.write("safe", idxOut, 32);

    while (true) {
        uint32_t nowSec = psp_now_sec();

        Event incoming{};
        while (link.poll(incoming)) {
            if (incoming.type == EventType::GhostDownStart) {
                if (!peer_halt_authorized(keys, incoming)) continue;
                safe.setWriteLock(true);
                for (uint8_t m = 0; m < mineN; ++m) pots[m].freezeEvents();
                peer_halt_run(ROLE_SAFE, id);
                continue;
            }
            if (incoming.type == EventType::DangerModeEnter) {
                safe.setWriteLock(true);
                for (uint8_t m = 0; m < mineN; ++m) pots[m].freezeEvents();
                continue;
            }
            if (incoming.type == EventType::DangerModeExit) {
                continue;
            }
            if (!safe.writeLocked()) {
                (void)safe.ingestBackup(incoming);
            }
        }
        if (peer_halt_dead()) break;
        link.tick(nowSec);

        if (share != nullptr && access.onShareAccess(share) && !decoySent &&
            !safe.writeLocked()) {
            for (uint8_t m = 0; m < mineN; ++m) {
                MinePayload mp{};
                if (pots[m].onLockvogel(&mp, nowSec)) {
                    (void)peer_sign_mine(keys, mp);
                    (void)mineLinks[m].send(mp, nowSec);
                }
                lastMineAt[m] = nowSec;
            }
            decoySent = true;
        }

        if (!safe.writeLocked()) {
            for (uint8_t m = 0; m < mineN; ++m) {
                if (lastMineAt[m] != 0 &&
                    (nowSec - lastMineAt[m]) < MINE_INTERVAL_SEC) {
                    continue;
                }
                MinePayload mp{};
                if (pots[m].send(&mp, nowSec)) {
                    (void)peer_sign_mine(keys, mp);
                    (void)mineLinks[m].send(mp, nowSec);
                }
                lastMineAt[m] = nowSec;
            }
        }

        MinePayload ignored{};
        for (uint8_t m = 0; m < mineN; ++m) {
            (void)mineLinks[m].recv(ignored);
            (void)pots[m].recv(&ignored);
        }

        psp_sleep_ms(10);
    }
}
