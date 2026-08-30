// =====================================================
// Ghost Hive v1.7.1 — Phone Noah
// Spec-Basis: §5, §7, §11, §25
// hive-sensor + hive-alert. Kein Vault, kein Persist, kein Kill-Flag.
// =====================================================

#include "sensor.h"
#include "../laptop/alert.h"
#include "../laptop/peer_keys.h"
#include "../laptop/peer_halt.h"
#include "../laptop/tetact.h"
#include "../mine/mine.h"
#include "transport/transport_bidi.h"
#include "transport/mine_transport.h"
#include "transport/medium_wlan.h"
#include "psp_time.h"
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

int main(int argc, char** argv) {
    char ip[32];
    char id[32];
    copyArg(ip, "255.255.255.255", 31);
    copyArg(id, HIVE_ID_PHONE, 31);

    char mineIds[HOST_MINE_SLOTS][32];
    uint8_t mineN = 0;
    bool gotIp = false;
    bool gotId = false;
    for (int i = 1; i < argc; ++i) {
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
        (void)mine_push_id(mineIds, &mineN, HOST_MINE_SLOTS, HIVE_ID_MINE_PHONE);
    }

    Sensor sensor;
    sensor.init(id);
    HiveAlert alert;
    alert.init();

    GhostKeys keys;
    keys.initEmpty();
    (void)peer_load_keys(keys, PEER_BIND_PATH);

    MediumWlan wlan;
    if (!wlan.connectKernel(id, ip, GHOST_UDP_PORT)) {
        return 1;
    }

    TransportBidi link;
    link.init(id, ROLE_PHONE);
    link.attach(&wlan, nullptr);

    Mine pots[HOST_MINE_SLOTS];
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

    uint32_t lastScan = 0;
    uint32_t lastTelem = 0;
    TetactState tet;
    tetact_init(tet);

    while (true) {
        uint32_t nowSec = psp_now_sec();

        Event incoming{};
        while (link.poll(incoming)) {
            if (incoming.type == EventType::GhostDownStart &&
                !peer_halt_authorized(keys, incoming)) {
                continue;
            }
            if (alert.ingest(incoming)) showAlert(alert);
            if (peer_halt_authorized(keys, incoming)) {
                peer_halt_run(ROLE_PHONE, id);
            }
        }
        if (peer_halt_dead()) break;
        link.tick(nowSec);

        (void)sensor.recv(nullptr);

        if (alert.danger() || alert.down() || alert.kill()) {
            for (uint8_t m = 0; m < mineN; ++m) pots[m].freezeEvents();
        }

        bool silent = alert.down() || alert.kill() || peer_halt_dead();
        bool minimal = alert.danger();
        if (!silent && !minimal) {
            Event obs{};
            TetactKind kind = TETACT_NONE;
            if (lastScan == 0 || (nowSec - lastScan) >= sensor.intervalSec()) {
                kind = tetact_poll(tet, nowSec, &obs);
                lastScan = nowSec;
            } else {
                kind = tetact_watch(tet, nowSec, &obs);
            }
            if (kind != TETACT_NONE) {
                tetact_set_source(&obs, id);
                (void)peer_sign_event(keys, obs);
                (void)link.send(obs, nowSec);
                if (kind == TETACT_TAMPER && mineN > 0) {
                    MinePayload trip{};
                    if (pots[0].sendTrip(&trip, nowSec)) {
                        (void)peer_sign_mine(keys, trip);
                        (void)mineLinks[0].send(trip, nowSec);
                    }
                }
            }
        }

        if (!silent && !minimal) {
            if (lastTelem == 0 || (nowSec - lastTelem) >= TELEM_INTERVAL_SEC) {
                uint16_t ram = 0, traf = 0, wifi = 0;
                uint8_t cpu = 0, gpu = 0, bat = 0;
                if (host_telem_sample(nowSec, &ram, &cpu, &gpu, &traf, &bat, &wifi)) {
                    host_telem_apply_role(ROLE_PHONE, &ram, &cpu, &gpu, &traf, &bat,
                                          &wifi);
                    Event te{};
                    if (sensor.fillTelemetry(&te, nowSec, ram, cpu, gpu, traf, bat,
                                             wifi)) {
                        (void)peer_sign_event(keys, te);
                        (void)link.send(te, nowSec);
                        lastTelem = nowSec;
                    }
                }
            }
        }

        if (!silent && !minimal) {
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
        }

        psp_sleep_ms(10);
    }
}
