// =====================================================
// Ghost Hive v1.7.1 — Family Sensor
// Spec-Basis: §5, §7, §11, §25
// hive-sensor only. Kein Alert, kein Honigtopf, kein Write.
// =====================================================

#include "sensor.h"
#include "../laptop/peer_keys.h"
#include "../laptop/peer_halt.h"
#include "../laptop/tetact.h"
#include "transport/transport_bidi.h"
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

int main(int argc, char** argv) {
    char ip[32];
    char id[32];
    copyArg(ip, "255.255.255.255", 31);
    copyArg(id, HIVE_ID_FAMILY, 31);
    if (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0') {
        copyArg(ip, argv[1], 31);
    }
    if (argc > 2 && argv[2] != nullptr && argv[2][0] != '\0') {
        copyArg(id, argv[2], 31);
    }

    Sensor sensor;
    sensor.init(id);

    GhostKeys keys;
    keys.initEmpty();
    (void)peer_load_keys(keys, PEER_BIND_PATH);

    MediumWlan wlan;
    if (!wlan.connectKernel(id, ip, GHOST_UDP_PORT)) {
        return 1;
    }

    TransportBidi link;
    link.init(id, ROLE_SENSOR);
    link.attach(&wlan, nullptr);

    uint32_t lastScan = 0;
    uint32_t lastTelem = 0;
    bool silent = false;
    TetactState tet;
    tetact_init(tet);

    while (true) {
        uint32_t nowSec = psp_now_sec();

        Event incoming{};
        while (link.poll(incoming)) {
            if (incoming.type == EventType::GhostDownStart) {
                if (peer_halt_authorized(keys, incoming)) {
                    peer_halt_run(ROLE_SENSOR, id);
                }
            }
            if (incoming.type == EventType::DangerModeEnter) silent = true;
            if (incoming.type == EventType::DangerModeExit) silent = false;
        }
        if (peer_halt_dead()) break;
        link.tick(nowSec);

        (void)sensor.recv(nullptr);

        if (!silent && !peer_halt_dead()) {
            Event obs{};
            TetactKind kind = TETACT_NONE;
            if (lastScan == 0 || (nowSec - lastScan) >= sensor.intervalSec()) {
                kind = tetact_poll(tet, nowSec, &obs);
                lastScan = nowSec;
                Event hb{};
                if (sensor.fillHeartbeat(&hb, nowSec)) {
                    (void)peer_sign_event(keys, hb);
                    (void)link.send(hb, nowSec);
                }
            } else {
                kind = tetact_watch(tet, nowSec, &obs);
            }
            if (kind != TETACT_NONE) {
                tetact_set_source(&obs, id);
                (void)peer_sign_event(keys, obs);
                (void)link.send(obs, nowSec);
            }
        }

        if (!silent && !peer_halt_dead()) {
            if (lastTelem == 0 || (nowSec - lastTelem) >= TELEM_INTERVAL_SEC) {
                uint16_t ram = 0, traf = 0, wifi = 0;
                uint8_t cpu = 0, gpu = 0, bat = 0;
                if (host_telem_sample(nowSec, &ram, &cpu, &gpu, &traf, &bat, &wifi)) {
                    host_telem_apply_role(ROLE_SENSOR, &ram, &cpu, &gpu, &traf, &bat,
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

        psp_sleep_ms(10);
    }
}
