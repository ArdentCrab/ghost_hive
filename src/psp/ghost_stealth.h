#ifndef GHOST_STEALTH_H
#define GHOST_STEALTH_H

// =====================================================
// Ghost Hive v1.7.1
// Stealth — P2 FINAL
// Spec-Basis: §2.2, §10
// Game/Invisible: WLAN aus. Terminal: Scan erlaubt.
// =====================================================

#include "ghost_core.h"
#include "ghost_data.h"

class GhostStealth {
public:
    GhostStealth();

    void init();
    void enterGameMode();
    void enterTerminalMode();
    void enterInvisibleMode();

    void radioOff();

    bool isGameMode() const;
    bool isInvisible() const;
    StealthInfo getInfo() const;

private:
    bool gameMode_;
    bool invisible_;
};

#endif // GHOST_STEALTH_H
