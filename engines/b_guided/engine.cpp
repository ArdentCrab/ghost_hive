#include "lab_common.h"
#include "ghost_crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const uint16_t W_PAYLOAD = 104;
static const uint16_t W_AUTH = 188;
static const uint16_t W_MAC = 192;

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
        printf("FAIL engine_b no sim\n");
        return 1;
    }
    GhostKeys keys;
    if (!lab_load_bind(&keys)) {
        printf("FAIL engine_b bind\n");
        return 1;
    }
    int udp = -1;
    if (!lab_udp_open(&udp, false)) return 1;
    uint32_t limit = seconds_arg(argc, argv);
    time_t end = time(nullptr) + static_cast<time_t>(limit);
    uint32_t t = 20000;
    uint32_t n = 0;
    while (time(nullptr) < end) {
        Event ev;
        lab_fill_event(&ev, EventType::Heartbeat, "W", t, "guided");
        uint8_t wire[TRANSPORT_WIRE_LEN];
        if (!lab_encode_event(keys, ROLE_WORKER, "W", ev, t, wire, true)) break;
        (void)lab_eval_send("b_guided", wire, TRANSPORT_WIRE_LEN, udp);
        ++n;
        uint8_t mut[TRANSPORT_WIRE_LEN];
        memcpy(mut, wire, TRANSPORT_WIRE_LEN);
        mut[W_PAYLOAD] = static_cast<uint8_t>(mut[W_PAYLOAD] ^ 0x01);
        (void)lab_eval_send("b_guided", mut, TRANSPORT_WIRE_LEN, udp);
        memcpy(mut, wire, TRANSPORT_WIRE_LEN);
        mut[W_AUTH] = 'X';
        (void)lab_eval_send("b_guided", mut, TRANSPORT_WIRE_LEN, udp);
        memcpy(mut, wire, TRANSPORT_WIRE_LEN);
        mut[W_MAC] = static_cast<uint8_t>(mut[W_MAC] ^ 0x01);
        (void)lab_eval_send("b_guided", mut, TRANSPORT_WIRE_LEN, udp);
        t += 3;
        usleep(15000);
    }
    close(udp);
    printf("PASS engine_b n=%u\n", n);
    return 0;
}
