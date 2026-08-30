#ifndef PEER_KEYS_H
#define PEER_KEYS_H

// =====================================================
// Peer-Key-Bind — Device/Session/Mine aus Root-Datei
// Spec-Basis: §5, §33, §44
// PSP hält Root. Peers speichern Root nicht.
// HMAC-SHA1 wie Vault (§15 / §33).
// Host: nur lokales /tmp/ghost_hive, kein Cloud-Pfad.
// =====================================================

#include "ghost_keys.h"

const uint32_t PEER_BIND_TTL_SEC = 900;
const char PEER_KEYD_SOCK[] = "/tmp/ghost_hive/keyd.sock";

void peer_os_harden();
bool peer_bind_path_ok(const char* bindPath);
bool peer_bind_keys(GhostKeys& keys, const char* bindPath);
bool peer_bind_from_keyd(GhostKeys& keys);
bool peer_load_keys(GhostKeys& keys, const char* bindPath);
bool peer_sign_event(const GhostKeys& keys, Event& event);
bool peer_verify_event(const GhostKeys& keys, const Event& event);
bool peer_sign_mine(const GhostKeys& keys, MinePayload& mine);

#endif
