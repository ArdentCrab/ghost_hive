#ifndef GHOST_LAB_COMMON_H
#define GHOST_LAB_COMMON_H

// Attack Lab helpers. Sim-only. Packets only to 127.0.0.1.

#include "ghost_core.h"
#include "ghost_keys.h"
#include "transport/transport_frame.h"
#include "invariants.h"

const char LAB_DIR[] = "/tmp/ghost_lab";
const char LAB_SOCK[] = "/tmp/ghost_lab/sim.sock";
const char LAB_BIND[] = "/tmp/ghost_lab/peer.bind";
const char LAB_READY[] = "/tmp/ghost_lab/ready";
const char LAB_HOST[] = "127.0.0.1";
const uint16_t LAB_PORT = 17471;

const char* lab_dir();
uint16_t lab_udp_port();
const char* lab_sock_path();
const char* lab_ready_path();
const char* lab_bind_path();

bool lab_host_ok(const char* host);
bool lab_ensure_dir();
bool lab_udp_open(int* fd, bool bind_loopback);
bool lab_udp_send(int fd, const uint8_t* wire, uint16_t len);
bool lab_unix_cmd(const char* cmd, char* reply, uint16_t cap);
bool lab_snap_now(LabSnap* s);
bool lab_set_now(uint32_t now);
void lab_copy_id(char* dst, const char* src);
void lab_kind_name(TransportKind k, char* dst, uint8_t cap);
void lab_etype_name(EventType t, char* dst, uint8_t cap);
void lab_policy_name(uint8_t action, char* dst, uint8_t cap);
bool lab_load_bind(GhostKeys* keys);
bool lab_sign_event(GhostKeys& keys, Event& ev);
bool lab_sign_mine(GhostKeys& keys, MinePayload& mine);
bool lab_encode_event(GhostKeys& keys, uint8_t role, const char* id,
                      Event ev, uint32_t stamp, uint8_t* wire, bool sign);
bool lab_encode_kill(GhostKeys& keys, uint8_t role, const char* id,
                     Event ev, uint32_t stamp, uint8_t* wire, bool sign);
bool lab_encode_mine(GhostKeys& keys, MinePayload mine, uint32_t stamp,
                     uint8_t* wire, bool sign);
void lab_fill_event(Event* ev, EventType t, const char* id, uint32_t now,
                    const char* payload);
bool lab_hex40(const uint8_t* w, uint16_t n, char* out, uint16_t cap);
bool lab_wait_ready(uint32_t ms);
bool lab_note_finding(const char* engine, const char* inv, const char* kind,
                      const char* payload_hex,
                      const LabSnap& before, const LabSnap& after);
int lab_eval_send(const char* engine, const uint8_t* wire, uint16_t len, int udp);

#endif
