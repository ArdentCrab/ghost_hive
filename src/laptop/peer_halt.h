#ifndef PEER_HALT_H
#define PEER_HALT_H

// =====================================================
// Peer-OS-Halt nach Kernel-Kill
// Spec-Basis: §40, §43
// GhostDownStart (signiert, Kernel) = device shutdown.
// Kein neues Event. Nie gegen PSP. Host ohne GHOST_OS_HALT
// schreibt nur Marker und beendet den Peer-Prozess.
// =====================================================

#include "ghost_core.h"
#include "ghost_keys.h"

bool peer_halt_is_kill(const Event& event);
// §40 / §43: Halt nur bei GhostDownStart, Quelle Kernel, gültige MAC.
// Fail-closed: kein Bind / bad MAC / Peer-Quelle → kein Halt.
bool peer_halt_authorized(const GhostKeys& keys, const Event& event);
void peer_halt_run(uint8_t role, const char* id);
bool peer_halt_dead();
bool peer_halt_can_tx();
void peer_halt_reset();
bool peer_halt_has_marker(const char* id);

#endif
