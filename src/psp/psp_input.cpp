#include "psp_input.h"

#include <stdlib.h>

#if defined(__PSP__) || defined(PSP_BUILD)
#include <pspctrl.h>
#define GHOST_PSP_HW 1
#endif

#ifndef GHOST_PSP_HW
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
static Key g_queued = Key::None;
static int g_keys_tty = 0;
static bool g_combo_held = false;

void psp_push_key(Key key) {
    g_queued = key;
}

void psp_set_combo_held(bool held) {
    g_combo_held = held;
}
#endif

void psp_input_init() {
#ifdef GHOST_PSP_HW
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
#else
    const char* k = getenv("GHOST_PSP_KEYS");
    if (k != nullptr && k[0] == '1' && isatty(STDIN_FILENO)) {
        int fl = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (fl >= 0) (void)fcntl(STDIN_FILENO, F_SETFL, fl | O_NONBLOCK);
        g_keys_tty = 1;
    }
#endif
}

Key psp_read_key() {
#ifdef GHOST_PSP_HW
    static unsigned int prev = 0;
    SceCtrlData pad;
    pad.Buttons = 0;
    sceCtrlPeekBufferPositive(&pad, 1);
    unsigned int now = pad.Buttons;
    unsigned int combo = PSP_CTRL_SELECT | PSP_CTRL_START;
    bool combo_now = (now & combo) == combo;
    bool combo_was = (prev & combo) == combo;
    unsigned int edge = now & ~prev;
    prev = now;

    if (combo_now && !combo_was) return Key::SelectStart;
    if (combo_now) return Key::None;

    if (edge & PSP_CTRL_UP) return Key::Up;
    if (edge & PSP_CTRL_DOWN) return Key::Down;
    if (edge & PSP_CTRL_LEFT) return Key::Left;
    if (edge & PSP_CTRL_RIGHT) return Key::Right;
    if (edge & PSP_CTRL_CROSS) return Key::X;
    if (edge & PSP_CTRL_CIRCLE) return Key::O;
    if (edge & PSP_CTRL_SQUARE) return Key::Square;
    if (edge & PSP_CTRL_TRIANGLE) return Key::Triangle;
    if (edge & PSP_CTRL_LTRIGGER) return Key::L;
    if (edge & PSP_CTRL_RTRIGGER) return Key::R;
    if (edge & PSP_CTRL_HOME) return Key::Home;
    return Key::None;
#else
    if (g_queued != Key::None) {
        Key k = g_queued;
        g_queued = Key::None;
        return k;
    }
    if (!g_keys_tty) return Key::None;
    unsigned char ch = 0;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n <= 0) return Key::None;
    if (ch == 'i' || ch == 'I') return Key::Up;
    if (ch == 'k' || ch == 'K') return Key::Down;
    if (ch == 'j' || ch == 'J' || ch == ',') return Key::Left;
    if (ch == 'l' || ch == 'L' || ch == '.') return Key::Right;
    if (ch == 0x1b) {
        unsigned char b2 = 0;
        unsigned char b3 = 0;
        if (read(STDIN_FILENO, &b2, 1) == 1 && b2 == '[') {
            if (read(STDIN_FILENO, &b3, 1) == 1) {
                if (b3 == 'A') return Key::Up;
                if (b3 == 'B') return Key::Down;
                if (b3 == 'C') return Key::Right;
                if (b3 == 'D') return Key::Left;
            }
        }
        return Key::None;
    }
    if (ch == 'x' || ch == 'X') return Key::X;
    if (ch == 'o' || ch == 'O') return Key::O;
    if (ch == 's' || ch == 'S') return Key::Square;
    if (ch == 't' || ch == 'T') return Key::Triangle;
    if (ch == 'g' || ch == 'G') return Key::SelectStart;
    if (ch == '[') return Key::L;
    if (ch == ']') return Key::R;
    if (ch == 'h' || ch == 'H') return Key::Home;
    return Key::None;
#endif
}

bool psp_combo_held() {
#ifdef GHOST_PSP_HW
    SceCtrlData pad;
    pad.Buttons = 0;
    sceCtrlPeekBufferPositive(&pad, 1);
    unsigned int combo = PSP_CTRL_SELECT | PSP_CTRL_START;
    return (pad.Buttons & combo) == combo;
#else
    return g_combo_held;
#endif
}

#ifndef GHOST_PSP_HW
void sceKernelDelayThread(unsigned int microseconds) {
    usleep(microseconds);
}
#endif
