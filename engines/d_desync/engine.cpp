#include "lab_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint32_t seconds_arg(int argc, char** argv) {
    for (int i = 1; i < argc - 1; ++i) {
        if (strcmp(argv[i], "--seconds") == 0) {
            int v = atoi(argv[i + 1]);
            if (v > 0) return static_cast<uint32_t>(v);
        }
    }
    return 8;
}

int main(int argc, char** argv) {
    if (!lab_wait_ready(8000)) {
        printf("FAIL engine_d no sim\n");
        return 1;
    }
    GhostKeys keys;
    if (!lab_load_bind(&keys)) return 1;
    int udp = -1;
    if (!lab_udp_open(&udp, false)) return 1;
    uint32_t limit = seconds_arg(argc, argv);
    time_t end = time(nullptr) + static_cast<time_t>(limit);
    uint32_t t = 50000;
    uint32_t n = 0;
    uint8_t noise[TRANSPORT_WIRE_LEN];
    uint8_t wire[TRANSPORT_WIRE_LEN];
    while (time(nullptr) < end) {
        Event ev;
        lab_fill_event(&ev, EventType::GhostDownStart, "W", t, "kill");
        ev.severity = Severity::Critical;
        if (lab_encode_event(keys, ROLE_WORKER, "W", ev, t, wire, false)) {
            (void)lab_eval_send("d_desync", wire, TRANSPORT_WIRE_LEN, udp);
        }
        lab_fill_event(&ev, EventType::GhostDownStart, "W", t, "kill");
        ev.severity = Severity::Critical;
        if (lab_encode_kill(keys, ROLE_WORKER, "W", ev, t, wire, false)) {
            (void)lab_eval_send("d_desync", wire, TRANSPORT_WIRE_LEN, udp);
        }
        lab_fill_event(&ev, EventType::GhostDownStart, "W", t, "kill");
        ev.severity = Severity::Critical;
        if (lab_encode_kill(keys, ROLE_WORKER, "W", ev, t, wire, true)) {
            (void)lab_eval_send("d_desync", wire, TRANSPORT_WIRE_LEN, udp);
        }
        lab_fill_event(&ev, EventType::PeekScan, "W", t + 4, "peek");
        if (lab_encode_event(keys, ROLE_WORKER, "W", ev, t + 4, wire, true)) {
            (void)lab_eval_send("d_desync", wire, TRANSPORT_WIRE_LEN, udp);
        }
        lab_fill_event(&ev, EventType::Heartbeat, "W", t + 1, "d_ok");
        if (lab_encode_event(keys, ROLE_WORKER, "W", ev, t + 1, wire, true)) {
            (void)lab_eval_send("d_desync", wire, TRANSPORT_WIRE_LEN, udp);
        }
        lab_fill_event(&ev, EventType::Heartbeat, "W", t + 2, "d_bad");
        if (lab_encode_event(keys, ROLE_WORKER, "W", ev, t + 2, wire, false)) {
            (void)lab_eval_send("d_desync", wire, TRANSPORT_WIRE_LEN, udp);
        }
        lab_fill_event(&ev, EventType::ScanResult, "W", t + 3, "d_ok2");
        if (lab_encode_event(keys, ROLE_WORKER, "W", ev, t + 3, wire, true)) {
            (void)lab_eval_send("d_desync", wire, TRANSPORT_WIRE_LEN, udp);
        }
        memset(noise, 0xA5, TRANSPORT_WIRE_LEN);
        noise[0] = 9;
        (void)lab_eval_send("d_desync", noise, TRANSPORT_WIRE_LEN, udp);
        lab_fill_event(&ev, EventType::Heartbeat, "W", t - 50, "old");
        if (lab_encode_event(keys, ROLE_WORKER, "W", ev, t - 50, wire, true)) {
            (void)lab_eval_send("d_desync", wire, TRANSPORT_WIRE_LEN, udp);
        }
        t += 10;
        n += 9;
        usleep(20000);
    }
    close(udp);
    printf("PASS engine_d n=%u\n", n);
    return 0;
}
