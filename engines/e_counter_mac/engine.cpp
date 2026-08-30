#include "lab_common.h"
#include "ghost_crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
        printf("FAIL engine_e no sim\n");
        return 1;
    }
    GhostKeys keys;
    if (!lab_load_bind(&keys)) return 1;
    int udp = -1;
    if (!lab_udp_open(&udp, false)) return 1;
    uint32_t limit = seconds_arg(argc, argv);
    time_t end = time(nullptr) + static_cast<time_t>(limit);
    uint32_t now = 60000;
    uint32_t n = 0;
    const uint32_t jumps[6] = {1, 1000, 0, 0xFFFFFFFFu, 2, 5};
    while (time(nullptr) < end) {
        for (uint8_t i = 0; i < 6 && time(nullptr) < end; ++i) {
            MinePayload mp{};
            lab_copy_id(mp.mine_id, "X");
            mp.counter = jumps[i];
            mp.timestamp = now;
            mp.event = EventType::MineEvent;
            if (keys.totpSeed() != nullptr) {
                mp.totp = ghost_totp(keys.totpSeed(), TOTP_SEED_LEN, now);
            }
            uint8_t wire[TRANSPORT_WIRE_LEN];
            if (lab_encode_mine(keys, mp, now, wire, true)) {
                (void)lab_set_now(now);
                (void)lab_eval_send("e_counter_mac", wire, TRANSPORT_WIRE_LEN, udp);
            }
            now += 90;
            ++n;
        }
        Event ev;
        lab_fill_event(&ev, EventType::Heartbeat, "W", now, "macgrid");
        uint8_t wire[TRANSPORT_WIRE_LEN];
        if (lab_encode_event(keys, ROLE_WORKER, "W", ev, now, wire, true)) {
            uint8_t mut[TRANSPORT_WIRE_LEN];
            memcpy(mut, wire, TRANSPORT_WIRE_LEN);
            for (uint16_t off = W_AUTH; off < W_MAC + 8 && time(nullptr) < end; ++off) {
                memcpy(mut, wire, TRANSPORT_WIRE_LEN);
                mut[off] = static_cast<uint8_t>(mut[off] ^ 0x5A);
                (void)lab_eval_send("e_counter_mac", mut, TRANSPORT_WIRE_LEN, udp);
                ++n;
            }
        }
        now += 3;
        usleep(15000);
    }
    close(udp);
    printf("PASS engine_e n=%u\n", n);
    return 0;
}
