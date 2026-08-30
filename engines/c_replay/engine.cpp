#include "lab_common.h"
#include "ghost_crypto.h"

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

static void send_mine(GhostKeys& keys, int udp, uint32_t now, uint32_t counter) {
    MinePayload mp{};
    lab_copy_id(mp.mine_id, "X");
    mp.counter = counter;
    mp.timestamp = now;
    mp.event = EventType::MineEvent;
    if (keys.totpSeed() != nullptr) {
        mp.totp = ghost_totp(keys.totpSeed(), TOTP_SEED_LEN, now);
    }
    uint8_t wire[TRANSPORT_WIRE_LEN];
    if (!lab_encode_mine(keys, mp, now, wire, true)) return;
    (void)lab_set_now(now);
    (void)lab_eval_send("c_replay", wire, TRANSPORT_WIRE_LEN, udp);
}

int main(int argc, char** argv) {
    if (!lab_wait_ready(8000)) {
        printf("FAIL engine_c no sim\n");
        return 1;
    }
    GhostKeys keys;
    if (!lab_load_bind(&keys)) {
        printf("FAIL engine_c bind\n");
        return 1;
    }
    int udp = -1;
    if (!lab_udp_open(&udp, false)) return 1;
    uint32_t limit = seconds_arg(argc, argv);
    time_t end = time(nullptr) + static_cast<time_t>(limit);
    uint32_t base = 40000;
    uint32_t c = 1;
    send_mine(keys, udp, base, c);
    ++c;
    const uint32_t edges[8] = {59, 60, 61, 89, 90, 119, 120, 121};
    uint32_t n = 1;
    while (time(nullptr) < end) {
        for (uint8_t i = 0; i < 8 && time(nullptr) < end; ++i) {
            send_mine(keys, udp, base + edges[i], c);
            ++n;
            usleep(12000);
        }
        Event ev;
        lab_fill_event(&ev, EventType::Heartbeat, "W", base, "early");
        uint8_t wire[TRANSPORT_WIRE_LEN];
        if (lab_encode_event(keys, ROLE_WORKER, "W", ev, base, wire, true)) {
            (void)lab_eval_send("c_replay", wire, TRANSPORT_WIRE_LEN, udp);
        }
        ++n;
        usleep(20000);
    }
    close(udp);
    printf("PASS engine_c n=%u\n", n);
    return 0;
}
