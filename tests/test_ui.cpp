#include "ghost_terminal.h"
#include "psp_input.h"
#include "registry.h"
#include "event_queue.h"
#include "ghost_vault.h"
#include "decision_pipeline.h"
#include "ghost_telemetry.h"

#include <cstdio>
#include <cstring>
#include <stdint.h>

static bool ok = true;

static void chk(bool v, const char* name) {
    if (v) printf("PASS %s\n", name);
    else {
        printf("FAIL %s\n", name);
        ok = false;
    }
}

static void tap(GhostTerminal& t, Key k, uint32_t* now) {
    psp_push_key(k);
    *now += 10;
    t.tick(*now);
}

static bool has(const char* line, const char* needle) {
    return line != nullptr && needle != nullptr && std::strstr(line, needle) != nullptr;
}

static uint8_t linelen(const char* s) {
    uint8_t n = 0;
    if (s == nullptr) return 0;
    while (s[n] != '\0') ++n;
    return n;
}

static bool raster_ok(const GhostTerminal& t) {
    for (uint8_t r = 0; r < TERM_ROWS; ++r) {
        const char* ln = t.frameLine(r);
        if (linelen(ln) != TERM_COLS) return false;
        if (ln[TERM_COLS] != '\0') return false;
        for (uint8_t c = 0; c < TERM_COLS; ++c) {
            unsigned char ch = static_cast<unsigned char>(ln[c]);
            if (ch < 32u || ch > 126u) return false;
        }
    }
    return true;
}

static bool title_pad(const char* line, const char* page, const char* state) {
    char exp[49];
    uint8_t n = 0;
    const char* a = "[GHv2] PAGE:";
    uint8_t i = 0;
    while (a[i] != '\0' && n < 48) exp[n++] = a[i++];
    i = 0;
    while (page[i] != '\0' && n < 48) exp[n++] = page[i++];
    if (n < 48) exp[n++] = ' ';
    const char* b = "STATE:";
    i = 0;
    while (b[i] != '\0' && n < 48) exp[n++] = b[i++];
    i = 0;
    while (state[i] != '\0' && n < 48) exp[n++] = state[i++];
    while (n < 48) exp[n++] = ' ';
    exp[n] = '\0';
    return std::strcmp(line, exp) == 0;
}

static bool no_v1(const GhostTerminal& t) {
    for (uint8_t r = 0; r < TERM_ROWS; ++r) {
        const char* ln = t.frameLine(r);
        if (has(ln, "[GHv1]")) return false;
        if (has(ln, "PatchTime")) return false;
        if (has(ln, "PatchLoc")) return false;
        if (has(ln, "device_patch")) return false;
        if (has(ln, "ghost_$")) return false;
        if (has(ln, "{")) return false;
        if (has(ln, "JSON")) return false;
        if (has(ln, "debug")) return false;
        if (has(ln, "DEBUG")) return false;
    }
    return true;
}

static void snap(const GhostTerminal& t, char dst[TERM_ROWS][TERM_COLS + 1]) {
    for (uint8_t r = 0; r < TERM_ROWS; ++r) {
        const char* ln = t.frameLine(r);
        uint8_t c = 0;
        while (c < TERM_COLS) {
            dst[r][c] = ln[c];
            ++c;
        }
        dst[r][TERM_COLS] = '\0';
    }
}

static bool same_frame(const char a[TERM_ROWS][TERM_COLS + 1],
                       const GhostTerminal& t) {
    for (uint8_t r = 0; r < TERM_ROWS; ++r) {
        if (std::strcmp(a[r], t.frameLine(r)) != 0) return false;
    }
    return true;
}

static void copy_id(char* dst, const char* src) {
    uint8_t i = 0;
    while (src[i] != '\0' && i < 31) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static void enroll(Registry& reg, const char* id, uint8_t role) {
    Device d{};
    copy_id(d.id, id);
    d.role = role;
    d.trust_level = (role == ROLE_WORKER) ? 2 : 1;
    d.status = DeviceState::Pending;
    (void)reg.addDevice(d);
}

static Event wtelem(uint32_t ts, uint16_t ram, uint8_t cpu, uint8_t gpu,
                    uint16_t traf, uint8_t bat, uint16_t wifi) {
    Event ev{};
    ev.type = EventType::TelemetryUpdate;
    copy_id(ev.source_device_id, "W");
    ev.timestamp = ts;
    ev.severity = Severity::Info;
    fillTelemetryPayload(&ev, ram, cpu, gpu, traf, bat, wifi);
    return ev;
}

int main() {
    chk(TERM_COLS == 48 && TERM_ROWS == 24, "ui_raster dims");
    chk(WATCH_GLYPH_W == 9 && WATCH_GLYPH_H == 11, "ui_glyphs 9x11");
    chk(static_cast<uint16_t>(TERM_COLS) * WATCH_GLYPH_W <= WATCH_FB_W,
        "ui_glyphs fit width");
    chk(static_cast<uint16_t>(TERM_ROWS) * WATCH_GLYPH_H <= WATCH_FB_H,
        "ui_glyphs fit height");
    chk((TERM_COLS - 1u) * WATCH_GLYPH_W + (WATCH_GLYPH_W - 1u) < WATCH_FB_W,
        "ui_glyphs last col");
    chk((TERM_ROWS - 1u) * WATCH_GLYPH_H + (WATCH_GLYPH_H - 1u) < WATCH_FB_H,
        "ui_glyphs last row");

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

    enroll(reg, "W", ROLE_WORKER);
    enroll(reg, "P", ROLE_PHONE);
    enroll(reg, "F", ROLE_SENSOR);

    term.tick(now);
    term.draw();
    chk(raster_ok(term), "ui_raster boot");
    chk(title_pad(term.frameLine(0), "Hive", "Watch"), "ui_titlepad hive");
    chk(has(term.frameLine(1), "----"), "ui_raster sep");
    chk(linelen(term.frameLine(1)) == 48, "ui_raster sep 48");
    chk(no_v1(term), "ui_no_v1_rest boot");
    chk(term.mode() != TermMode::GhostDown, "ui_no_debug_rest boot");

    tap(term, Key::R, &now);
    term.draw();
    chk(title_pad(term.frameLine(0), "Kernel", "Watch"), "ui_titlepad kernel");
    tap(term, Key::R, &now);
    term.draw();
    chk(title_pad(term.frameLine(0), "Net", "Watch"), "ui_titlepad net");
    tap(term, Key::R, &now);
    term.draw();
    chk(title_pad(term.frameLine(0), "Peer", "Watch"), "ui_titlepad peer");

    chk(has(term.frameLine(2), "Dev1:"), "ui_devices_block dev1");
    chk(has(term.frameLine(2), "id=W"), "ui_devices_block id");
    chk(has(term.frameLine(2), "role=Worker"), "ui_devices_block role");
    chk(has(term.frameLine(3), "Status:"), "ui_state");
    chk(has(term.frameLine(8), "Cap:"), "ui_caps");
    chk(has(term.frameLine(11), "RAM: --"), "ui_telem");
    chk(has(term.frameLine(11), "RAM: --"), "ui_absent_sentinel ram");
    chk(has(term.frameLine(17), "idx: 1/3"), "ui_idx_line 1/3");
    for (uint8_t r = 18; r < TERM_ROWS; ++r) {
        const char* ln = term.frameLine(r);
        bool blank = true;
        for (uint8_t c = 0; c < TERM_COLS; ++c) {
            if (ln[c] != ' ') blank = false;
        }
        if (!blank) {
            chk(false, "ui_devices_block rest spaces");
            break;
        }
    }
    chk(no_v1(term), "ui_no_v1_rest devices");

    Event ev = wtelem(now / 1000u + 50, 512, 12, 4, 32, 87, 54);
    chk(pipe.process(ev, now / 1000u + 50) == PipelineResult::Accepted,
        "ui_telem first");
    term.draw();
    chk(has(term.frameLine(11), "RAM: 512MB"), "ui_units ram");
    chk(has(term.frameLine(11), "RAM: 512MB"), "ui_units ram n");
    chk(has(term.frameLine(12), "CPU: 12%"), "ui_units cpu");
    chk(has(term.frameLine(13), "GPU: 4%"), "ui_units gpu");
    chk(has(term.frameLine(14), "Traffic: 32KB/s"), "ui_units traffic");
    chk(has(term.frameLine(15), "Battery: 87%"), "ui_units battery");
    chk(has(term.frameLine(16), "WiFi: 54Mbit/s"), "ui_units wifi");
    chk(raster_ok(term), "ui_draw_after_telem raster");
    chk(title_pad(term.frameLine(0), "Peer", "Watch"), "ui_no_frame_shift telem");

    bool wrap_ok = true;
    for (uint16_t i = 0; i < 1000; ++i) {
        tap(term, Key::Down, &now);
        term.draw();
        uint8_t expect = static_cast<uint8_t>((i + 1u) % 3u);
        char lab[8];
        lab[0] = 'D'; lab[1] = 'e'; lab[2] = 'v';
        lab[3] = static_cast<char>('1' + expect);
        lab[4] = ':'; lab[5] = '\0';
        if (!has(term.frameLine(2), lab)) wrap_ok = false;
        char idx[12];
        idx[0] = 'i'; idx[1] = 'd'; idx[2] = 'x'; idx[3] = ':'; idx[4] = ' ';
        idx[5] = static_cast<char>('1' + expect);
        idx[6] = '/'; idx[7] = '3'; idx[8] = '\0';
        if (!has(term.frameLine(17), idx)) wrap_ok = false;
        if (!raster_ok(term)) wrap_ok = false;
    }
    chk(wrap_ok, "ui_focus_wrap 1000");
    chk(has(term.frameLine(2), "Dev2:"), "ui_focus_wrap land");
    tap(term, Key::Down, &now);
    tap(term, Key::Down, &now);
    term.draw();
    chk(has(term.frameLine(2), "Dev1:"), "ui_focus_wrap to dev1");

    char f0[TERM_ROWS][TERM_COLS + 1];
    snap(term, f0);
    bool sq_ok = true;
    uint8_t pg = term.watchPage();
    for (uint16_t i = 0; i < 1000; ++i) {
        tap(term, Key::Square, &now);
        term.draw();
        if (term.watchPage() != pg) sq_ok = false;
        if (!has(term.frameLine(2), "Dev1:")) sq_ok = false;
        if (!raster_ok(term)) sq_ok = false;
        if (!title_pad(term.frameLine(0), "Peer", "Watch")) sq_ok = false;
    }
    chk(sq_ok, "ui_square_refresh 1000");
    term.draw();
    chk(same_frame(f0, term), "ui_no_flackern square");

    bool draw_ok = true;
    for (uint16_t i = 0; i < 1000; ++i) {
        term.draw();
        if (!raster_ok(term)) draw_ok = false;
        if (term.mode() != TermMode::Watch) draw_ok = false;
        if (!term.isRunning()) draw_ok = false;
    }
    chk(draw_ok, "ui_raster 1000 draws");
    chk(term.isRunning(), "ui_no_freeze");

    bool race_ok = true;
    uint32_t ts = now / 1000u + 200;
    for (uint16_t i = 0; i < 1000; ++i) {
        Event u = wtelem(ts + i, static_cast<uint16_t>(400u + (i % 50u)),
                         10, 3, 16, 70, 24);
        (void)pipe.process(u, ts + i);
        term.draw();
        if (!has(term.frameLine(11), "RAM:")) race_ok = false;
        if (!has(term.frameLine(11), "MB")) race_ok = false;
        if (!raster_ok(term)) race_ok = false;
        if (!title_pad(term.frameLine(0), "Peer", "Watch")) race_ok = false;
    }
    chk(race_ok, "ui_no_race 1000 telem+draw");
    chk(has(term.frameLine(11), "MB"), "ui_draw_after_telem units");

    {
        GhostTerminal dterm;
        dterm.init(&reg, &eq);
        dterm.attach(&vault, &pipe);
        uint32_t dn = 8000;
        dterm.tick(dn);
        dterm.down().execute(dn / 1000u);
        dn += 10;
        dterm.tick(dn);
        dterm.draw();
        chk(dterm.mode() == TermMode::Watch, "ui_down_red stays watch");
        chk(dterm.mode() != TermMode::GhostDown, "ui_no_debug_rest down");
        chk(title_pad(dterm.frameLine(0), "Kernel", "Down"), "ui_down_red title");
        chk(has(dterm.frameLine(2), "Down: active"), "ui_down_red body");
        chk(has(dterm.frameLine(0), "STATE:Down"), "ui_down_red state");
        chk(raster_ok(dterm), "ui_down_red raster");
        bool dtick = true;
        for (uint16_t i = 0; i < 1000; ++i) {
            dn += 1;
            dterm.tick(dn);
            dterm.draw();
            if (dterm.mode() != TermMode::Watch) dtick = false;
            if (!title_pad(dterm.frameLine(0), "Kernel", "Down")) dtick = false;
            if (!raster_ok(dterm)) dtick = false;
        }
        chk(dtick, "ui_no_freeze 1000 down ticks");
        tap(dterm, Key::Home, &dn);
        chk(dterm.isRunning(), "ui_home_exit down kernel");
        dterm.draw();
        chk(!has(dterm.frameLine(0), "[GHv2]"), "ui_home_exit hide");
        chk(linelen(dterm.frameLine(0)) == 48, "ui_home_exit blank raster");
        tap(dterm, Key::Square, &dn);
        dterm.draw();
        chk(title_pad(dterm.frameLine(0), "Kernel", "Down"), "ui_home_exit square back");
        chk(dterm.watchPage() == 1, "ui_no_frame_shift down page");
    }

    bool home_idle = true;
    for (uint16_t i = 0; i < 1000; ++i) {
        GhostTerminal ht;
        ht.init(&reg, &eq);
        ht.attach(&vault, &pipe);
        uint32_t hn = 9000u + i;
        ht.tick(hn);
        ht.draw();
        if (ht.watchPage() != 0) home_idle = false;
        if (!title_pad(ht.frameLine(0), "Hive", "Watch")) home_idle = false;
        tap(ht, Key::Home, &hn);
        if (ht.isRunning()) home_idle = false;
    }
    chk(home_idle, "ui_home_exit 1000 idle");

    term.draw();
    chk(no_v1(term), "ui_no_v1_rest final");
    chk(term.mode() == TermMode::Watch, "ui_no_debug_rest final");
    chk(raster_ok(term), "ui_raster final");

    printf(ok ? "PASS ui\n" : "FAIL ui\n");
    return ok ? 0 : 1;
}
