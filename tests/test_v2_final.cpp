// Ghost Hive v2 final soak — 10× the 1000-iter UI loops plus pipeline/HUD/opsec.
#include "ghost_terminal.h"
#include "psp_input.h"
#include "registry.h"
#include "event_queue.h"
#include "ghost_vault.h"
#include "decision_pipeline.h"
#include "ghost_telemetry.h"
#include "ghost_policy.h"
#include "replay_guard.h"
#include "transport/transport_frame.h"
#include "peer_keys.h"
#include "hive_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const uint32_t SOAK = 10000;
static int fail = 0;

static void chk(bool ok, const char* name) {
    if (ok) printf("PASS %s\n", name);
    else {
        printf("FAIL %s\n", name);
        fail = 1;
    }
}

static void tap(GhostTerminal& t, Key k, uint32_t* now) {
    psp_push_key(k);
    *now += 10;
    t.tick(*now);
}

static bool lineHas(const char* line, const char* needle) {
    return line != nullptr && needle != nullptr && strstr(line, needle) != nullptr;
}

static bool raster_ok(const GhostTerminal& t) {
    for (uint8_t r = 0; r < TERM_ROWS; ++r) {
        const char* ln = t.frameLine(r);
        uint8_t n = 0;
        while (ln[n] != '\0') ++n;
        if (n != TERM_COLS) return false;
        for (uint8_t c = 0; c < n; ++c) {
            unsigned char ch = static_cast<unsigned char>(ln[c]);
            if (ch < 0x20 || ch > 0x7e) return false;
        }
    }
    return true;
}

static void enroll(Registry& reg, const char* id, uint8_t role) {
    Device d{};
    uint8_t i = 0;
    while (id[i] != '\0' && i < 31) {
        d.id[i] = id[i];
        ++i;
    }
    d.id[i] = '\0';
    d.role = role;
    d.trust_level = (role == ROLE_WORKER) ? 2 : 1;
    d.status = DeviceState::Pending;
    d.capability_mask = 0x0001;
    d.tag_mask = 0x02;
    (void)reg.addDevice(d);
}

static Event telem(const char* id, uint32_t ts, uint16_t ram) {
    Event ev{};
    ev.type = EventType::TelemetryUpdate;
    uint8_t i = 0;
    while (id[i] != '\0' && i < 31) {
        ev.source_device_id[i] = id[i];
        ++i;
    }
    ev.source_device_id[i] = '\0';
    ev.timestamp = ts;
    ev.severity = Severity::Critical;
    fillTelemetryPayload(&ev, ram, 12, 4, 32, 87, 54);
    return ev;
}

static uint64_t ns_now() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull +
           static_cast<uint64_t>(ts.tv_nsec);
}

int main() {
    uint64_t t0 = ns_now();
    GhostPolicy pol;
    chk(pol.ruleCount() == 16, "policy 16");
    chk(REPLAY_WINDOW_PER_MINE == 64, "replay window 64");
    chk(TELEM_REPLAY_SLOTS == 64, "telem replay 64");
    chk(ACK_BUDGET_PER_SEC == 8, "ack budget 8");
    chk(VAULT_SLOTS_PER_PEER == 8, "vault per peer 8");
    chk(VAULT_RAM_SLOTS == 64, "vault ram 64");
    {
        const char* ss = HIVE_IBSS_SSID;
        chk(ss[0] == 'G' && ss[7] == 'E' && ss[8] == '\0', "ssid GHSTHIVE");
    }

    chk(!peer_bind_path_ok("https://cloud/peer.bind"), "opsec no https");
    chk(peer_bind_path_ok("/tmp/ghost_hive/peer.bind"), "opsec hive path");

    Registry reg;
    EventQueue eq;
    GhostVault vault;
    vault.init();
    DecisionPipeline pipe;
    pipe.attach(&reg, &vault);
    GhostTerminal term;
    term.init(&reg, &eq);
    term.attach(&vault, &pipe);
    psp_input_init();

    uint32_t now = 1000;
    term.tick(now);
    term.draw();
    chk(term.mode() == TermMode::Watch, "boot watch");
    chk(lineHas(term.frameLine(0), "[GHv2]"), "title ghv2");
    chk(!lineHas(term.frameLine(0), "ghost_$"), "no live cli");
    chk(raster_ok(term), "boot raster");

    enroll(reg, "W", ROLE_WORKER);
    enroll(reg, "P", ROLE_PHONE);
    enroll(reg, "F", ROLE_SENSOR);

    uint32_t acc = 0;
    uint32_t blk = 0;
    for (uint32_t i = 0; i < SOAK; ++i) {
        uint32_t ts = 1000u + i;
        Event ev = telem("W", ts, static_cast<uint16_t>(400u + (i % 200u)));
        PipelineResult r = pipe.process(ev, ts);
        if (r == PipelineResult::Accepted) ++acc;
        else ++blk;
    }
    chk(acc == 8, "vault peer cap 8 stores");
    chk(blk == SOAK - 8, "rest telem blocked");
    const Device* dw = reg.getDevice("W");
    chk(dw != nullptr && dw->ram_mb != TELEM_ABSENT16, "telem wrote ram");

    Event mine{};
    mine.type = EventType::TelemetryUpdate;
    mine.source_device_id[0] = 'M';
    mine.source_device_id[1] = '\0';
    mine.timestamp = 5000;
    fillTelemetryPayload(&mine, 1, 1, 1, 1, 1, 1);
    Device md{};
    md.id[0] = 'M';
    md.id[1] = '\0';
    md.role = ROLE_MINE;
    md.status = DeviceState::Pending;
    (void)reg.addDevice(md);
    chk(pipe.process(mine, 5000) != PipelineResult::Accepted, "mine telem drop");

    bool wrap_ok = true;
    for (uint32_t i = 0; i < SOAK; ++i) {
        tap(term, Key::R, &now);
        term.draw();
        uint8_t expect = static_cast<uint8_t>((i + 1u) % 4u);
        if (term.watchPage() != expect) wrap_ok = false;
        if (!raster_ok(term)) wrap_ok = false;
        if (!lineHas(term.frameLine(0), "[GHv2]")) wrap_ok = false;
        if (lineHas(term.frameLine(0), "ghost_$")) wrap_ok = false;
    }
    chk(wrap_ok, "soak 10000 L/R wrap");
    chk(term.watchPage() == 0, "wrap land hive");

    tap(term, Key::L, &now);
    chk(term.watchPage() == 3, "L to peer");
    term.draw();
    chk(lineHas(term.frameLine(0), "PAGE:Peer"), "peer page");
    chk(lineHas(term.frameLine(8), "Cap:"), "peer cap");
    chk(lineHas(term.frameLine(3), "Status:"), "peer state");
    chk(lineHas(term.frameLine(11), "RAM:"), "peer telem");

    bool focus_ok = true;
    for (uint32_t i = 0; i < SOAK; ++i) {
        tap(term, Key::Down, &now);
        term.draw();
        uint8_t expect = static_cast<uint8_t>((i + 1u) % 4u);
        char lab[8];
        lab[0] = 'D';
        lab[1] = 'e';
        lab[2] = 'v';
        lab[3] = static_cast<char>('1' + expect);
        lab[4] = ':';
        lab[5] = '\0';
        if (!lineHas(term.frameLine(2), lab)) focus_ok = false;
        if (!raster_ok(term)) focus_ok = false;
    }
    chk(focus_ok, "soak 10000 peer focus");

    tap(term, Key::X, &now);
    chk(term.mode() == TermMode::Watch, "X tot");
    tap(term, Key::SelectStart, &now);
    chk(term.mode() == TermMode::Watch, "select+start tot");
    chk(term.watchPage() == 3, "select+start page kept");

    bool sq_ok = true;
    uint8_t pg = term.watchPage();
    for (uint32_t i = 0; i < SOAK; ++i) {
        tap(term, Key::Square, &now);
        term.draw();
        if (term.watchPage() != pg) sq_ok = false;
        if (!raster_ok(term)) sq_ok = false;
        if (term.mode() != TermMode::Watch) sq_ok = false;
    }
    chk(sq_ok, "soak 10000 square");

    bool telem_draw = true;
    for (uint32_t i = 0; i < SOAK; ++i) {
        term.draw();
        if (!raster_ok(term)) telem_draw = false;
        if (!lineHas(term.frameLine(11), "RAM:")) telem_draw = false;
        if (!lineHas(term.frameLine(0), "PAGE:Peer")) telem_draw = false;
    }
    chk(telem_draw, "soak 10000 peer draw");

    tap(term, Key::O, &now);
    chk(term.mode() == TermMode::Black, "O stealth");
    tap(term, Key::R, &now);
    chk(term.mode() == TermMode::Black, "black R tot");
    tap(term, Key::O, &now);
    chk(term.mode() == TermMode::Watch, "O back watch");

    tap(term, Key::L, &now);
    chk(term.watchPage() == 2, "net page");
    {
        WifiNetwork nets[2];
        uint8_t* p = reinterpret_cast<uint8_t*>(nets);
        for (uint16_t i = 0; i < sizeof(nets); ++i) p[i] = 0;
        const char* hive = "GHSTHIVE";
        uint8_t k = 0;
        while (hive[k] != '\0') {
            nets[0].ssid[k] = hive[k];
            nets[1].ssid[k] = hive[k];
            ++k;
        }
        nets[0].rssi = -30;
        nets[0].channel = 1;
        nets[1].rssi = -80;
        nets[1].channel = 11;
        term.scanner().setTerminalMode(true);
        chk(term.scanner().loadWifiSnapshot(nets, 2), "hud load");
        bool twin_ok = true;
        for (uint32_t i = 0; i < 1000; ++i) {
            term.scanner().releaseBuffer();
            term.draw();
            if (!lineHas(term.frameLine(0), "PAGE:Net")) twin_ok = false;
            bool twin = false;
            for (uint8_t r = 2; r < TERM_ROWS; ++r) {
                if (lineHas(term.frameLine(r), "Twin: yes")) twin = true;
            }
            if (!twin) twin_ok = false;
            if (!raster_ok(term)) twin_ok = false;
        }
        chk(twin_ok, "soak 1000 twin after release");
    }

    GhostTerminal dterm;
    dterm.init(&reg, &eq);
    dterm.attach(&vault, &pipe);
    uint32_t dn = 8000;
    dterm.tick(dn);
    dterm.down().execute(dn / 1000u);
    dn += 10;
    dterm.tick(dn);
    dterm.draw();
    chk(dterm.mode() == TermMode::Watch, "down watch");
    chk(dterm.watchPage() == 1, "down kernel");
    chk(lineHas(dterm.frameLine(0), "STATE:Down"), "down state");
    bool down_wrap = true;
    for (uint32_t i = 0; i < 4000; ++i) {
        tap(dterm, Key::R, &dn);
        dterm.draw();
        if (dterm.mode() != TermMode::Watch) down_wrap = false;
        if (!lineHas(dterm.frameLine(0), "STATE:Down")) down_wrap = false;
        if (!raster_ok(dterm)) down_wrap = false;
        tap(dterm, Key::O, &dn);
        if (dterm.mode() != TermMode::Watch) down_wrap = false;
        tap(dterm, Key::SelectStart, &dn);
        if (dterm.mode() != TermMode::Watch) down_wrap = false;
        tap(dterm, Key::Up, &dn);
        tap(dterm, Key::Down, &dn);
    }
    chk(down_wrap, "soak 4000 down wrap+dead keys");

    tap(dterm, Key::Home, &dn);
    chk(dterm.isRunning(), "down home keeps kernel");
    while (term.watchPage() != 0) tap(term, Key::R, &now);
    chk(term.watchPage() == 0, "idle hive before home");
    tap(term, Key::Home, &now);
    chk(!term.isRunning(), "idle home exit");

    uint64_t t1 = ns_now();
    uint64_t ms = (t1 - t0) / 1000000ull;
    printf("TIME soak_ms=%llu acc=%u blk=%u\n",
           static_cast<unsigned long long>(ms), acc, blk);

    if (fail) {
        printf("FAIL test_v2_final\n");
        return 1;
    }
    printf("PASS test_v2_final\n");
    return 0;
}
