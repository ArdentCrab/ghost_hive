// =====================================================
// Ghost Hive v1.7.1 — Router net-sensor
// Spec-Basis: §5, §7, §11, §25, §38
// Send-only Net-Events. Keine Config. Honigtopf-Mine optional.
// =====================================================

#include "net_sensor.h"
#include "../laptop/peer_keys.h"
#include "../laptop/peer_halt.h"
#include "../laptop/tetact.h"
#include "../mine/router_mine.h"
#include "transport/transport_bidi.h"
#include "transport/mine_transport.h"
#include "transport/medium_wlan.h"
#include "psp_time.h"
#include "host_telem.h"
#include "ghost_telemetry.h"

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
    copyArg(id, HIVE_ID_ROUTER, 31);

    bool decoy = false;
    char mineIds[HOST_MINE_SLOTS][32];
    uint8_t mineN = 0;
    bool gotIp = false;
    bool gotId = false;
    for (int i = 1; i < argc; ++i) {
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
        (void)mine_push_id(mineIds, &mineN, HOST_MINE_SLOTS, HIVE_ID_MINE_ROUTER);
    }

    Sensor sensor;
    sensor.init(id);
    NetSensor net;
    net.attach(&sensor);

    GhostKeys keys;
    keys.initEmpty();
    (void)peer_load_keys(keys, PEER_BIND_PATH);

    MediumWlan wlan;
    if (!wlan.connectKernel(id, ip, GHOST_UDP_PORT)) {
        return 1;
    }

    TransportBidi link;
    link.init(id, ROLE_ROUTER);
    link.attach(&wlan, nullptr);

    RouterMine honeypots[HOST_MINE_SLOTS];
    MineTransport mineLinks[HOST_MINE_SLOTS];
    uint32_t lastMineAt[HOST_MINE_SLOTS];
    for (uint8_t m = 0; m < mineN; ++m) {
        honeypots[m].init(mineIds[m]);
        mineLinks[m].init(mineIds[m]);
        mineLinks[m].attach(&wlan, nullptr);
        lastMineAt[m] = 0;
        if (keys.hasTotpSeed()) {
            (void)honeypots[m].setTotpSeed(keys.totpSeed(), TOTP_SEED_LEN);
        }
    }

    uint32_t last = 0;
    uint32_t lastTelem = 0;
    bool decoySent = false;
    bool danger = false;
    TetactState tet;
    tetact_init(tet);

    while (true) {
        uint32_t nowSec = psp_now_sec();

        Event incoming{};
        while (link.poll(incoming)) {
            if (incoming.type == EventType::GhostDownStart) {
                if (peer_halt_authorized(keys, incoming)) {
                    peer_halt_run(ROLE_ROUTER, id);
                }
            }
            if (incoming.type == EventType::DangerModeEnter) danger = true;
            if (incoming.type == EventType::DangerModeExit) danger = false;
        }
        if (peer_halt_dead()) break;
        link.tick(nowSec);

        if (danger) {
            for (uint8_t m = 0; m < mineN; ++m) honeypots[m].freezeEvents();
        }

        if (!danger) {
            Event obs{};
            TetactKind kind = TETACT_NONE;
            if (last == 0 || (nowSec - last) >= sensor.intervalSec()) {
                kind = tetact_poll(tet, nowSec, &obs);
                last = nowSec;
                if (kind != TETACT_NONE) {
                    tetact_set_source(&obs, id);
                    (void)peer_sign_event(keys, obs);
                    (void)link.send(obs, nowSec);
                } else if (!danger) {
                    Event ev{};
                    if (net.fillPortScan(&ev, nowSec)) {
                        (void)peer_sign_event(keys, ev);
                        (void)link.send(ev, nowSec);
                    }
                }
            } else {
                kind = tetact_watch(tet, nowSec, &obs);
                if (kind != TETACT_NONE) {
                    tetact_set_source(&obs, id);
                    (void)peer_sign_event(keys, obs);
                    (void)link.send(obs, nowSec);
                }
            }
        }

        if (!danger) {
            if (lastTelem == 0 || (nowSec - lastTelem) >= TELEM_INTERVAL_SEC) {
                uint16_t ram = 0, traf = 0, wifi = 0;
                uint8_t cpu = 0, gpu = 0, bat = 0;
                if (host_telem_sample(nowSec, &ram, &cpu, &gpu, &traf, &bat, &wifi)) {
                    host_telem_apply_role(ROLE_ROUTER, &ram, &cpu, &gpu, &traf, &bat,
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

        if (!danger) {
            for (uint8_t m = 0; m < mineN; ++m) {
                bool due = (lastMineAt[m] == 0) ||
                           ((nowSec - lastMineAt[m]) >= MINE_INTERVAL_SEC);
                if (decoy && !decoySent) due = true;
                if (!due) continue;
                MinePayload mp{};
                bool ok = false;
                if (decoy && !decoySent) ok = honeypots[m].onDecoyHit(&mp, nowSec);
                else ok = honeypots[m].send(&mp, nowSec);
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
            (void)honeypots[m].recv(&ignored);
        }

        psp_sleep_ms(10);
    }
}
