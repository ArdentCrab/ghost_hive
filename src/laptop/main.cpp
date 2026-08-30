// =====================================================
// Ghost Hive v1.7.1 — Laptop Noah (Worker)
// Spec-Basis: §5, §7, §11, §25, §33, §38
// hive-worker + analyzer + log-client + alert + kill + Mine-Geschwister
// =====================================================

#include "worker.h"
#include "analyzer.h"
#include "log_client.h"
#include "alert.h"
#include "kill.h"
#include "peer_keys.h"
#include "peer_halt.h"
#include "tetact.h"
#include "../mine/mine.h"
#include "transport/worker_transport.h"
#include "transport/mine_transport.h"
#include "transport/medium_wlan.h"
#include "psp_time.h"
#include "ghost_heartbeat.h"
#include "host_telem.h"
#include "ghost_telemetry.h"

#include <stdio.h>

static const char PEER_BIND_PATH[] = "/tmp/ghost_hive/peer.bind";

static void showAlert(const HiveAlert& alert) {
    if (alert.danger()) fwrite("danger\n", 1, 7, stdout);
    if (alert.down()) fwrite("down\n", 1, 5, stdout);
    if (alert.kill()) fwrite("kill\n", 1, 5, stdout);
    fflush(stdout);
}

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
    copyArg(id, HIVE_ID_WORKER, 31);

    bool sendKill = false;
    bool decoy = false;
    char mineIds[HOST_MINE_SLOTS][32];
    uint8_t mineN = 0;
    bool gotIp = false;
    bool gotId = false;
    for (int i = 1; i < argc; ++i) {
        if (argIs(argv[i], "kill")) {
            sendKill = true;
            continue;
        }
        if (argIs(argv[i], "decoy")) {
            decoy = true;
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
        (void)mine_push_id(mineIds, &mineN, HOST_MINE_SLOTS, HIVE_ID_MINE_LAPTOP);
    }

    Worker worker;
    worker.init(id);
    HiveAnalyzer analyzer;
    analyzer.attach(&worker);
    HiveLogClient logs;
    logs.init();
    HiveAlert alert;
    alert.init();

    GhostKeys keys;
    keys.initEmpty();
    (void)peer_load_keys(keys, PEER_BIND_PATH);
    if (keys.hasDevice()) {
        (void)worker.provisionDeviceKey(keys.device(), KEY_LEN);
    }

    HiveKill killer;
    killer.attach(&keys);

    MediumWlan wlan;
    if (!wlan.connectKernel(id, ip, GHOST_UDP_PORT)) {
        return 1;
    }

    WorkerTransport link;
    link.init(id);
    link.attach(&wlan, nullptr);

    Mine mines[HOST_MINE_SLOTS];
    MineTransport mineLinks[HOST_MINE_SLOTS];
    uint32_t lastMineAt[HOST_MINE_SLOTS];
    for (uint8_t m = 0; m < mineN; ++m) {
        mines[m].init(mineIds[m]);
        mineLinks[m].init(mineIds[m]);
        mineLinks[m].attach(&wlan, nullptr);
        lastMineAt[m] = 0;
        if (keys.hasTotpSeed()) {
            (void)mines[m].setTotpSeed(keys.totpSeed(), TOTP_SEED_LEN);
        }
    }

    uint32_t lastBeat = 0;
    uint32_t lastTelem = 0;
    uint32_t lastTet = 0;
    bool killSent = false;
    bool decoySent = false;
    TetactState tet;
    tetact_init(tet);

    while (true) {
        uint32_t nowSec = psp_now_sec();

        Event incoming{};
        while (link.poll(incoming)) {
            Event analyzed{};
            if (!alert.down() && !alert.kill()) {
                (void)analyzer.analyze(incoming, analyzed);
            }
            if (incoming.type == EventType::GhostDownStart &&
                !peer_halt_authorized(keys, incoming)) {
                continue;
            }
            if (alert.ingest(incoming)) showAlert(alert);
            if (peer_halt_authorized(keys, incoming)) {
                peer_halt_run(ROLE_WORKER, id);
            }
            if (incoming.type == EventType::BackupWritten) {
                (void)logs.pullBackup(incoming);
            } else if (incoming.type == EventType::AlertSent) {
                (void)logs.ingest(incoming);
            }
        }
        if (peer_halt_dead()) break;
        link.tick(nowSec);

        if (alert.danger() || alert.down() || alert.kill()) {
            for (uint8_t m = 0; m < mineN; ++m) mines[m].freezeEvents();
        }

        bool isolated = alert.down() || alert.kill() || peer_halt_dead();
        if (!isolated) {
            Event obs{};
            TetactKind kind = TETACT_NONE;
            if (lastTet == 0 || (nowSec - lastTet) >= TETACT_INTERVAL_SEC) {
                kind = tetact_poll(tet, nowSec, &obs);
                lastTet = nowSec;
            } else {
                kind = tetact_watch(tet, nowSec, &obs);
            }
            if (kind != TETACT_NONE) {
                tetact_set_source(&obs, id);
                (void)peer_sign_event(keys, obs);
                (void)link.send(obs, nowSec);
                if (kind == TETACT_TAMPER && mineN > 0) {
                    MinePayload trip{};
                    if (mines[0].sendTrip(&trip, nowSec)) {
                        (void)peer_sign_mine(keys, trip);
                        (void)mineLinks[0].send(trip, nowSec);
                    }
                }
            }
        }
        if (!isolated) {
            if (lastBeat == 0 || (nowSec - lastBeat) >= HEARTBEAT_INTERVAL_SEC) {
                Event hb{};
                if (worker.fillHeartbeat(&hb, nowSec)) {
                    (void)peer_sign_event(keys, hb);
                    (void)link.send(hb, nowSec);
                    lastBeat = nowSec;
                }
            }
        }
        if (!isolated) {
            if (lastTelem == 0 || (nowSec - lastTelem) >= TELEM_INTERVAL_SEC) {
                uint16_t ram = 0, traf = 0, wifi = 0;
                uint8_t cpu = 0, gpu = 0, bat = 0;
                if (host_telem_sample(nowSec, &ram, &cpu, &gpu, &traf, &bat, &wifi)) {
                    host_telem_apply_role(ROLE_WORKER, &ram, &cpu, &gpu, &traf, &bat,
                                          &wifi);
                    Event te{};
                    if (worker.fillTelemetry(&te, nowSec, ram, cpu, gpu, traf, bat,
                                             wifi)) {
                        (void)peer_sign_event(keys, te);
                        (void)link.send(te, nowSec);
                        lastTelem = nowSec;
                    }
                }
            }
        }

        if (sendKill && !killSent) {
            // §43: Kill nur PSP. Worker sendet keinen GhostDownStart.
            killSent = true;
        }

        if (!alert.danger() && !isolated) {
            for (uint8_t m = 0; m < mineN; ++m) {
                bool due = (lastMineAt[m] == 0) ||
                           ((nowSec - lastMineAt[m]) >= MINE_INTERVAL_SEC);
                if (decoy && !decoySent) due = true;
                if (!due) continue;
                MinePayload mp{};
                bool ok = false;
                if (decoy && !decoySent) ok = mines[m].sendTrip(&mp, nowSec);
                else ok = mines[m].send(&mp, nowSec);
                if (ok) {
                    (void)peer_sign_mine(keys, mp);
                    (void)mineLinks[m].send(mp, nowSec);
                }
                lastMineAt[m] = nowSec;
            }
            decoySent = true;
        }

        MinePayload ignored{};
        for (uint8_t m = 0; m < mineN; ++m) {
            (void)mineLinks[m].recv(ignored);
        }

        psp_sleep_ms(10);
    }
}
