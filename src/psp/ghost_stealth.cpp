#include "ghost_stealth.h"
#include "hive_net.h"

#if defined(__PSP__) || defined(PSP_BUILD)
#define GHOST_PSP_HW 1
#endif

GhostStealth::GhostStealth() {
    init();
}

void GhostStealth::radioOff() {
#ifdef GHOST_PSP_HW
    hive_net_down();
#endif
}

void GhostStealth::init() {
    gameMode_ = true;
    invisible_ = false;
    radioOff();
}

void GhostStealth::enterGameMode() {
    gameMode_ = true;
    invisible_ = false;
    radioOff();
}

void GhostStealth::enterTerminalMode() {
    gameMode_ = false;
}

void GhostStealth::enterInvisibleMode() {
    invisible_ = true;
    gameMode_ = false;
    radioOff();
}

bool GhostStealth::isGameMode() const {
    return gameMode_;
}

bool GhostStealth::isInvisible() const {
    return invisible_;
}

StealthInfo GhostStealth::getInfo() const {
    StealthInfo info{};
    info.gameMode = gameMode_ ? 1 : 0;
    info.invisible = invisible_ ? 1 : 0;
    return info;
}
