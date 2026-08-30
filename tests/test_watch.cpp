#include "ghost_terminal.h"
#include "psp_input.h"
#include "registry.h"
#include "event_queue.h"
#include "ghost_vault.h"

#include <stdio.h>
#include <string.h>

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

int main() {
    Registry reg;
    EventQueue eq;
    GhostVault vault;
    vault.init();
    GhostTerminal term;
    term.init(&reg, &eq);
    term.attach(&vault, nullptr);
    psp_input_init();

    uint32_t now = 1000;
    term.tick(now);
    term.draw();
    chk(term.mode() == TermMode::Watch, "boot watch");
    chk(term.watchPage() == 0, "page hive");
    chk(lineHas(term.frameLine(0), "[GHv2]"), "title ghv2");
    chk(lineHas(term.frameLine(0), "PAGE:Hive"), "page hive name");
    chk(lineHas(term.frameLine(0), "STATE:Watch"), "state watch");
    {
        const char* tl = term.frameLine(0);
        uint8_t n = 0;
        while (tl[n] != '\0') ++n;
        chk(n == 48, "title pad 48");
    }
    chk(lineHas(term.frameLine(1), "----"), "separator");
    chk(lineHas(term.frameLine(2), "Hive: empty"), "hive empty");
    {
        uint8_t found_peers = 0;
        uint8_t found_roles = 0;
        uint8_t found_hmac = 0;
        for (uint8_t r = 2; r < TERM_ROWS; ++r) {
            if (lineHas(term.frameLine(r), "Peers: 0/32")) found_peers = 1;
            if (lineHas(term.frameLine(r), "Roles:")) found_roles = 1;
            if (lineHas(term.frameLine(r), "HMAC-I:")) found_hmac = 1;
        }
        chk(found_peers == 1, "hive peers census");
        chk(found_roles == 1, "hive roles census");
        chk(found_hmac == 1, "hive hmac");
    }
    chk(!lineHas(term.frameLine(0), "ghost_$"), "no prompt title");
    chk(!term.locked(), "no lock");

    tap(term, Key::X, &now);
    term.draw();
    chk(!term.locked(), "x dead");
    chk(term.mode() == TermMode::Watch, "x stays watch");

    tap(term, Key::O, &now);
    chk(term.mode() == TermMode::Black, "o black");
    tap(term, Key::X, &now);
    chk(term.mode() == TermMode::Black, "black x tot");
    tap(term, Key::O, &now);
    chk(term.mode() == TermMode::Watch, "o back watch");
    tap(term, Key::Up, &now);
    tap(term, Key::Down, &now);
    tap(term, Key::Left, &now);
    tap(term, Key::Right, &now);
    chk(term.watchPage() == 0, "dpad dead");

    tap(term, Key::R, &now);
    term.draw();
    chk(term.watchPage() == 1, "r kernel");
    chk(lineHas(term.frameLine(0), "PAGE:Kernel"), "kernel title");
    {
        uint8_t ib = 0;
        uint8_t psp = 0;
        for (uint8_t r = 2; r < TERM_ROWS; ++r) {
            if (lineHas(term.frameLine(r), "IBSS:")) ib = 1;
            if (lineHas(term.frameLine(r), "PSP: 1004")) psp = 1;
        }
        chk(ib == 1, "kernel ibss");
        chk(psp == 1, "kernel psp 1004");
    }
    chk(lineHas(term.frameLine(0), "STATE:Watch"), "ghost not down");

    tap(term, Key::R, &now);
    term.draw();
    chk(term.watchPage() == 2, "r net");
    chk(lineHas(term.frameLine(0), "PAGE:Net"), "net title");
    chk(lineHas(term.frameLine(2), "IBSS:"), "network ibss");
    {
        uint8_t twin = 0;
        uint8_t xap = 0;
        uint8_t ap1 = 0;
        for (uint8_t r = 2; r < TERM_ROWS; ++r) {
            if (lineHas(term.frameLine(r), "Twin:")) twin = 1;
            if (lineHas(term.frameLine(r), "XAP:")) xap = 1;
            if (lineHas(term.frameLine(r), "Ap1:")) ap1 = 1;
        }
        chk(twin == 1, "net twin");
        chk(xap == 1, "net xap");
        chk(ap1 == 1, "net ap1");
    }
    {
        WifiNetwork nets[2];
        uint8_t* p = reinterpret_cast<uint8_t*>(nets);
        for (uint16_t i = 0; i < sizeof(nets); ++i) p[i] = 0;
        const char* a = "GHSTHIVE";
        const char* b = "GHSTHIVE";
        uint8_t i = 0;
        while (a[i] != '\0') {
            nets[0].ssid[i] = a[i];
            nets[1].ssid[i] = b[i];
            ++i;
        }
        nets[0].rssi = -40;
        nets[0].channel = 6;
        nets[0].encryption = 0;
        nets[1].rssi = -70;
        nets[1].channel = 11;
        nets[1].encryption = 3;
        term.scanner().setTerminalMode(true);
        chk(term.scanner().loadWifiSnapshot(nets, 2), "scan hud inject");
        term.draw();
        uint8_t twin_yes = 0;
        uint8_t rssi = 0;
        for (uint8_t r = 2; r < TERM_ROWS; ++r) {
            if (lineHas(term.frameLine(r), "Twin: yes")) twin_yes = 1;
            if (lineHas(term.frameLine(r), "r=-40")) rssi = 1;
        }
        chk(twin_yes == 1, "evil twin");
        chk(rssi == 1, "ap rssi");
        term.scanner().releaseBuffer();
        term.draw();
        twin_yes = 0;
        for (uint8_t r = 2; r < TERM_ROWS; ++r) {
            if (lineHas(term.frameLine(r), "Twin: yes")) twin_yes = 1;
        }
        chk(twin_yes == 1, "hud after release");
    }

    tap(term, Key::R, &now);
    term.draw();
    chk(term.watchPage() == 3, "r peer");
    chk(lineHas(term.frameLine(0), "PAGE:Peer"), "peer title");
    chk(lineHas(term.frameLine(2), "idx: 0/0"), "devices empty idx");
    {
        uint8_t found_pt = 0;
        uint8_t found_ram = 0;
        for (uint8_t r = 2; r < TERM_ROWS; ++r) {
            if (lineHas(term.frameLine(r), "PatchTime:")) found_pt = 1;
            if (lineHas(term.frameLine(r), "PatchLoc:")) found_pt = 1;
            if (lineHas(term.frameLine(r), "RAM:")) found_ram = 1;
        }
        chk(found_pt == 0, "no patch hud");
        chk(found_ram == 0, "empty no metrics");
    }

    Device wdev{};
    wdev.id[0] = 'W';
    wdev.id[1] = '\0';
    wdev.role = ROLE_WORKER;
    wdev.trust_level = 2;
    wdev.status = DeviceState::Pending;
    wdev.capability_mask = 0x0003;
    wdev.tag_mask = 0x0a;
    chk(reg.addDevice(wdev), "enroll worker");
    Device pdev{};
    pdev.id[0] = 'P';
    pdev.id[1] = '\0';
    pdev.role = ROLE_PHONE;
    pdev.trust_level = 1;
    pdev.status = DeviceState::Pending;
    chk(reg.addDevice(pdev), "enroll phone");
    tap(term, Key::Square, &now);
    term.draw();
    chk(lineHas(term.frameLine(2), "Dev1:"), "focus dev1");
    chk(lineHas(term.frameLine(8), "Cap: 0003"), "caps mask");
    chk(lineHas(term.frameLine(9), "Tag: 0a"), "tag mask");
    chk(lineHas(term.frameLine(11), "RAM: --"), "absent ram");
    {
        uint8_t found_idx = 0;
        for (uint8_t r = 2; r < TERM_ROWS; ++r) {
            if (lineHas(term.frameLine(r), "idx: 1/2")) found_idx = 1;
        }
        chk(found_idx == 1, "idx 1/2");
    }
    tap(term, Key::Down, &now);
    term.draw();
    chk(lineHas(term.frameLine(2), "Dev2:"), "focus down");
    {
        uint8_t found_idx = 0;
        for (uint8_t r = 2; r < TERM_ROWS; ++r) {
            if (lineHas(term.frameLine(r), "idx: 2/2")) found_idx = 1;
        }
        chk(found_idx == 1, "idx 2/2");
    }
    tap(term, Key::Up, &now);
    term.draw();
    chk(lineHas(term.frameLine(2), "Dev1:"), "focus up");

    tap(term, Key::R, &now);
    chk(term.watchPage() == 0, "wrap to hive");

    tap(term, Key::L, &now);
    chk(term.watchPage() == 3, "l wrap peer");
    tap(term, Key::R, &now);
    chk(term.watchPage() == 0, "back hive");

    uint8_t pg = term.watchPage();
    tap(term, Key::Square, &now);
    chk(term.watchPage() == pg, "square same page");
    chk(term.mode() == TermMode::Watch, "square stays watch");

    tap(term, Key::SelectStart, &now);
    chk(term.mode() == TermMode::Watch, "select+start tot");
    chk(term.watchPage() == 0, "select+start page kept");
    {
        GhostTerminal hterm;
        hterm.init(&reg, &eq);
        hterm.attach(&vault, nullptr);
        uint32_t hn = now;
        hterm.tick(hn);
        psp_set_combo_held(true);
        hterm.tick(hn);
        hn += 3000;
        hterm.tick(hn);
        chk(!hterm.down().isActive(), "hold hive no down");
        psp_set_combo_held(false);
        tap(hterm, Key::R, &hn);
        chk(hterm.watchPage() == 1, "kernel for hold");
        psp_set_combo_held(true);
        hterm.tick(hn);
        hn += 3000;
        hterm.tick(hn);
        chk(hterm.down().isActive(), "hold kernel 3s down");
        chk(hterm.mode() == TermMode::Watch, "hold stays watch");
        psp_set_combo_held(false);
    }

    {
        GhostTerminal dterm;
        dterm.init(&reg, &eq);
        dterm.attach(&vault, nullptr);
        uint32_t dn = 2000;
        dterm.tick(dn);
        dterm.down().execute(dn / 1000u);
        dn += 10;
        dterm.tick(dn);
        dterm.draw();
        chk(dterm.mode() == TermMode::Watch, "down stays watch");
        chk(dterm.watchPage() == 1, "down focus kernel");
        chk(lineHas(dterm.frameLine(0), "PAGE:Kernel"), "down title kernel");
        chk(lineHas(dterm.frameLine(0), "STATE:Down"), "down state");
        chk(lineHas(dterm.frameLine(0), "[GHv2]"), "down ghv2");
        chk(lineHas(dterm.frameLine(2), "Down: active"), "down body");
        chk(lineHas(dterm.frameLine(0), "STATE:Down"), "state down");
        uint8_t found_gl = 0;
        for (uint8_t r = 2; r < TERM_ROWS; ++r) {
            if (lineHas(dterm.frameLine(r), "GameLook: off")) found_gl = 1;
        }
        chk(found_gl == 1, "game look off");
        tap(dterm, Key::L, &dn);
        chk(dterm.watchPage() == 0, "down l hive");
        dterm.draw();
        chk(lineHas(dterm.frameLine(0), "PAGE:Hive"), "down l title hive");
        chk(lineHas(dterm.frameLine(0), "STATE:Down"), "down l state");
        tap(dterm, Key::R, &dn);
        chk(dterm.watchPage() == 1, "down r kernel");
        tap(dterm, Key::SelectStart, &dn);
        chk(dterm.mode() == TermMode::Watch, "down no game look");
        tap(dterm, Key::Home, &dn);
        chk(dterm.isRunning(), "home keeps kernel");
        dterm.draw();
        chk(!lineHas(dterm.frameLine(0), "[GHv2]"), "home hides watch");
        tap(dterm, Key::Square, &dn);
        dterm.draw();
        chk(lineHas(dterm.frameLine(0), "STATE:Down"), "square restores");
    }

    {
        FILE* tf = tmpfile();
        chk(tf != nullptr, "mini tmpfile");
        if (tf != nullptr) {
            uint8_t pg = term.watchPage();
            term.dumpWatchStacked(tf);
            chk(term.watchPage() == pg, "mini page restore");
            rewind(tf);
            char blob[8192];
            size_t n = fread(blob, 1, sizeof(blob) - 1, tf);
            blob[n] = '\0';
            fclose(tf);
            chk(strstr(blob, "PAGE:Hive") != nullptr, "mini hive");
            chk(strstr(blob, "PAGE:Kernel") != nullptr, "mini kernel");
            chk(strstr(blob, "PAGE:Net") != nullptr, "mini net");
            chk(strstr(blob, "PAGE:Peer") != nullptr, "mini peer");
            chk(strstr(blob, "VIEWPORT") == nullptr, "mini no extra viewports");
        }
    }

    tap(term, Key::Home, &now);
    chk(!term.isRunning(), "home exit");

    if (fail) {
        printf("FAIL test_watch\n");
        return 1;
    }
    printf("PASS test_watch\n");
    return 0;
}
