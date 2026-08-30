#ifndef GHOST_ARM_H
#define GHOST_ARM_H

// SPEC-v1 AMEND Arming: locked | observe | armed.
// Down/Kill only if armed. PSP-EBOOT v1 = locked until SPEC-v1 flips EBOOT.

#include <stdlib.h>
#include <stdint.h>

enum class GhostArming : uint8_t {
    Locked = 0,
    Observe = 1,
    Armed = 2
};

inline GhostArming ghost_arming() {
#if defined(GHOST_ARMING_LOCKED)
    return GhostArming::Locked;
#elif defined(GHOST_ARMING_ARMED)
    return GhostArming::Armed;
#elif defined(GHOST_ARMING_OBSERVE)
    return GhostArming::Observe;
#elif defined(__PSP__)
    return GhostArming::Locked;
#else
    const char* e = getenv("GHOST_DOWN_ARMED");
    if (e != nullptr && e[0] == '1') return GhostArming::Armed;
    const char* a = getenv("GHOST_ARMING");
    if (a != nullptr) {
        if (a[0] == 'a' && a[1] == 'r' && a[2] == 'm') return GhostArming::Armed;
        if (a[0] == 'o' && a[1] == 'b') return GhostArming::Observe;
        return GhostArming::Locked;
    }
    return GhostArming::Locked;
#endif
}

inline bool ghost_down_armed() {
    return ghost_arming() == GhostArming::Armed;
}

#endif
