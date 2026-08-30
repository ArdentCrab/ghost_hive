#ifndef PSP_INPUT_H
#define PSP_INPUT_H

// =====================================================
// Ghost Hive v1.7.1
// PSP Input — P2 FINAL
// Spec-Basis: §10
// Peek, nicht Read: Loop darf nicht blockieren.
// =====================================================

#include "ghost_core.h"

enum class Key : uint8_t {
    None,
    Up,
    Down,
    Left,
    Right,
    X,
    O,
    Square,
    Triangle,
    L,
    R,
    Home,
    SelectStart,
    VolumeUpDown
};

Key psp_read_key();
void psp_input_init();
bool psp_combo_held();
#if !defined(__PSP__) && !defined(PSP_BUILD)
void psp_push_key(Key key);
void psp_set_combo_held(bool held);
#endif

#if !defined(__PSP__) && !defined(PSP_BUILD)
void sceKernelDelayThread(unsigned int microseconds);
#endif

#endif // PSP_INPUT_H
