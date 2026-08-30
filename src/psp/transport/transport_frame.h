#ifndef TRANSPORT_FRAME_H
#define TRANSPORT_FRAME_H

// =====================================================
// Ghost Hive v1.7.1 — Transport Frame
// Spec-Basis: §18 ACK, §22 Fluss, §31 Retry, §44
// Kein neues Spec-Objekt. Nur Draht für Event / MinePayload.
// =====================================================

#include "ghost_core.h"

const uint32_t ACK_TIMEOUT_SEC = 2;
const uint32_t ACK_RETRY_SEC = 1;
const uint8_t ACK_RETRY_LIMIT = 1;
const uint8_t ACK_BUDGET_PER_SEC = 8;
const uint32_t PEEK_WINDOW_SEC = 300;
const uint8_t TRANSPORT_KERNEL_SLOTS = 16;
const uint8_t TRANSPORT_PEER_SLOTS = 8;
const uint8_t TRANSPORT_MAX_PEERS = 16;
const uint8_t TRANSPORT_PENDING = 16;
const uint8_t TRANSPORT_IR_SLOTS = 4;
const uint16_t GHOST_UDP_PORT = 17471;
const uint16_t TRANSPORT_WIRE_LEN = 346;

enum class TransportKind : uint8_t {
    EventFrame = 0,
    MineFrame = 1,
    AckFrame = 2,
    KillFrame = 3,
    FlushFrame = 4
};

struct TransportFrame {
    TransportKind kind;
    uint8_t src_role;
    uint8_t dst_role;
    char src_id[32];
    char dst_id[32];
    Event event;
    MinePayload mine;
    uint32_t stamp;
};

void transport_zero_id(char* id);
void transport_copy_id(char* dst, const char* src);
bool transport_same_id(const char* a, const char* b);
bool transport_is_bidi(uint8_t role);
void transport_clear_frame(TransportFrame& frame);
bool transport_encode(const TransportFrame& frame, uint8_t* out, uint16_t cap);
bool transport_decode(const uint8_t* in, uint16_t len, TransportFrame& frame);

#endif
