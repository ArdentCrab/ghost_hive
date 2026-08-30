#include "ghost_core.h"
#include "registry.h"
#include "ghost_vault.h"
#include "decision_pipeline.h"
#include "ghost_telemetry.h"
#include "ghost_scanner.h"
#include "ghost_terminal.h"
#include "event_queue.h"
#include "ghost_keys.h"
#include "ghost_down.h"
#include "ghost_stealth.h"
#include "transport/transport_frame.h"
#include "hive_net.h"
#include "psp_input.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <stdint.h>

static bool ok = true;

static void chk(bool v, const char* name) {
    if (v) printf("PASS %s\n", name);
    else {
        printf("FAIL %s\n", name);
        ok = false;
    }
}

static uint64_t ns_now() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull +
           static_cast<uint64_t>(ts.tv_nsec);
}

static void copy_id(char* dst, const char* src) {
    uint8_t i = 0;
    while (src[i] != '\0' && i < 31) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static void add_peer(Registry& reg, const char* id, uint8_t role) {
    Device d{};
    copy_id(d.id, id);
    d.role = role;
    d.trust_level = (role == ROLE_WORKER) ? 2 : 1;
    d.status = DeviceState::Pending;
    (void)reg.addDevice(d);
}

static Event worker_telem(const char* id, uint32_t ts, uint16_t ram) {
    Event ev{};
    ev.type = EventType::TelemetryUpdate;
    copy_id(ev.source_device_id, id);
    ev.timestamp = ts;
    ev.severity = Severity::Info;
    fillTelemetryPayload(&ev, ram, 10, 4, 8, 50, 12);
    return ev;
}

static uint8_t line_len(const char* s) {
    uint8_t n = 0;
    if (s == nullptr) return 0;
    while (s[n] != '\0' && n < 64) ++n;
    return n;
}

int main() {
    const uint32_t mb24 = 24u * 1024u * 1024u;
    const uint64_t frame_ns = 16ull * 1000000ull;
    const uint16_t scan_raw = 0xA80;

    chk(sizeof(DecisionPipeline) < 96u * 1024u, "perf_replay sizeof pipeline");
    chk(sizeof(GhostScanner) + scan_raw < 8u * 1024u, "perf_scanner 2.7k raw+wifi");
    chk(SCAN_BUFFER_SIZE == 32, "perf_scanner cap 32");
    chk(MAX_DEVICES == 32, "perf_heap 32 devices");
    chk(VAULT_MAC_HEX == 40 && VAULT_MAC_OFF == 88, "perf_vault mac region");
    chk(sizeof(GhostTerminal) + sizeof(DecisionPipeline) + sizeof(GhostVault) +
            sizeof(Registry) + scan_raw < mb24,
        "perf_heapstack under 24mb");

    {
        Registry reg;
        GhostVault vault;
        vault.init();
        DecisionPipeline pipe;
        pipe.attach(&reg, &vault);
        add_peer(reg, "W", ROLE_WORKER);

        chk(TELEM_REPLAY_SLOTS == 64, "perf_replay window 64");
        bool slots_ok = true;
        uint64_t t0 = ns_now();
        uint32_t now = 1000;
        for (uint8_t s = 0; s < 8; ++s) {
            Event ev = worker_telem("W", now + s, static_cast<uint16_t>(100u + s));
            if (pipe.process(ev, now + s) != PipelineResult::Accepted) slots_ok = false;
        }
        uint64_t t1 = ns_now();
        chk(slots_ok, "perf_replay vault 8/peer fill");
        Event ninth = worker_telem("W", now + 8, 200);
        chk(pipe.process(ninth, now + 8) == PipelineResult::Rejected, "perf_replay vault cap");
        Event old = worker_telem("W", now, 100);
        chk(pipe.process(old, now + 40) == PipelineResult::Blocked, "perf_replay still guards");
        chk((t1 - t0) < 2ull * 1000000000ull, "perf_replay 8 slots bounded");
    }

    {
        GhostScanner sc;
        sc.init();
        sc.setTerminalMode(false);
        chk(!sc.scanWifi(), "perf_scanner game blocked");
        chk(sc.lastScanBlocked(), "perf_scanner no game scan");
        chk(sc.getWifi(0) == nullptr, "perf_scanner empty index");
        sc.setTerminalMode(true);
        chk(sc.scanWifi(), "perf_scanner terminal ok");
        chk(sc.getWifi(sc.getWifiCount()) == nullptr, "perf_scanner no overflow");
        sc.releaseBuffer();
        chk(sc.getWifiCount() == 0, "perf_scanner released");
        chk(sc.scanBluetooth(), "perf_scanner bt slot");
        chk(sc.getBtCount() == 0, "perf_scanner bt empty");
    }

    {
        GhostKeys keys;
        uint8_t kb[KEY_LEN];
        for (uint8_t i = 0; i < KEY_LEN; ++i) kb[i] = static_cast<uint8_t>(i + 1);
        chk(keys.provisionDerived(kb, KEY_LEN), "perf_vault keys");
        GhostVault vault;
        vault.init();
        vault.attachKeys(&keys);
        Event ev = worker_telem("W", 1, 64);
        uint64_t t0 = ns_now();
        chk(vault.signEvent(ev), "perf_vault sign");
        chk(vault.verifyEvent(ev), "perf_vault verify once");
        chk(vault.signEvent(ev), "perf_vault resign");
        chk(vault.verifyEvent(ev), "perf_vault verify resign");
        uint64_t t1 = ns_now();
        chk(ev.payload[VAULT_MAC_OFF] != 0, "perf_vault mac live");
        chk((t1 - t0) < frame_ns * 8ull, "perf_vault hmac budget");
    }

    {
        Registry reg;
        EventQueue eq;
        GhostVault vault;
        vault.init();
        GhostTerminal term;
        term.init(&reg, &eq);
        term.attach(&vault, nullptr);
        psp_input_init();
        uint32_t now = 2000;
        term.tick(now);
        term.draw();
        char snap[TERM_ROWS][TERM_COLS + 1];
        bool cols_ok = true;
        for (uint8_t r = 0; r < TERM_ROWS; ++r) {
            const char* ln = term.frameLine(r);
            uint8_t c = 0;
            while (c < TERM_COLS && ln[c] != '\0') {
                snap[r][c] = ln[c];
                ++c;
            }
            snap[r][c] = '\0';
            if (line_len(ln) > TERM_COLS) cols_ok = false;
        }
        chk(cols_ok, "perf_watch col cap");
        chk(line_len(term.frameLine(0)) == TERM_COLS, "perf_watch title 48");
        term.draw();
        bool same = true;
        for (uint8_t r = 0; r < TERM_ROWS; ++r) {
            if (std::strcmp(snap[r], term.frameLine(r)) != 0) same = false;
        }
        chk(same, "perf_watch no flicker");

        DecisionPipeline pipe;
        pipe.attach(&reg, &vault);
        term.attach(&vault, &pipe);
        add_peer(reg, "W", ROLE_WORKER);
        Event ev = worker_telem("W", 2100, 512);
        (void)pipe.process(ev, 2100);
        term.tick(2100);
        term.draw();
        chk(line_len(term.frameLine(0)) == TERM_COLS, "perf_watch after telem");
        chk(term.watchPage() < 4, "perf_watch page bounded");
    }

    {
        GhostVault vault;
        vault.init();
        GhostStealth stealth;
        GhostDown down;
        down.attach(&vault, &stealth);
        uint64_t t0 = ns_now();
        down.execute(5000);
        down.tick(5000);
        down.tick(5000 + NAS_FLUSH_TIMEOUT_SEC);
        down.tick(5000 + NAS_FLUSH_TIMEOUT_SEC + STORAGE_FLUSH_DELAY_SEC);
        down.tick(5000 + NAS_FLUSH_TIMEOUT_SEC + 2u * STORAGE_FLUSH_DELAY_SEC);
        uint64_t t1 = ns_now();
        chk(down.step() == DownStep::Done ||
                down.step() == DownStep::StorageFlushWait ||
                down.step() == DownStep::StorageFlushRetry,
            "perf_down finite");
        chk((t1 - t0) < 2ull * 1000000000ull, "perf_down tick bounded");
    }

    {
        chk(hive_net_up(), "perf_ibss host netinit");
        chk(!hive_net_ready(), "perf_ibss host not psp");
        hive_net_down();
        uint8_t wire[TRANSPORT_WIRE_LEN];
        TransportFrame fr;
        transport_clear_frame(fr);
        fr.src_role = ROLE_WORKER;
        fr.dst_role = ROLE_KERNEL;
        copy_id(fr.src_id, "W");
        fr.event = worker_telem("W", 9, 8);
        bool enc_ok = true;
        uint64_t t0 = ns_now();
        for (uint8_t i = 0; i < 64; ++i) {
            if (!transport_encode(fr, wire, TRANSPORT_WIRE_LEN)) enc_ok = false;
            TransportFrame out;
            if (!transport_decode(wire, TRANSPORT_WIRE_LEN, out)) enc_ok = false;
        }
        uint64_t t1 = ns_now();
        chk(enc_ok, "perf_ibss encode/decode");
        chk((t1 - t0) < frame_ns * 8ull, "perf_ibss 64 frames");
        chk(sizeof(wire) == TRANSPORT_WIRE_LEN, "perf_ibss stack frame");
    }

    {
        Registry reg;
        EventQueue eq;
        GhostVault vault;
        vault.init();
        GhostTerminal term;
        term.init(&reg, &eq);
        term.attach(&vault, nullptr);
        psp_input_init();
        term.tick(3000);
        uint64_t worst = 0;
        for (uint8_t i = 0; i < 60; ++i) {
            uint64_t a = ns_now();
            term.draw();
            uint64_t b = ns_now();
            if (b - a > worst) worst = b - a;
        }
        chk(worst <= frame_ns, "perf_frametime 16ms");
        printf("INFO frametime_ns_max %llu\n",
               static_cast<unsigned long long>(worst));
    }

    printf(ok ? "PASS perf\n" : "FAIL perf\n");
    return ok ? 0 : 1;
}
