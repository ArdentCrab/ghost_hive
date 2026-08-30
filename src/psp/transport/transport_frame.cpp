#include "transport_frame.h"

void transport_zero_id(char* id) {
    if (id == nullptr) return;
    id[0] = '\0';
}

void transport_copy_id(char* dst, const char* src) {
    if (dst == nullptr) return;
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    uint8_t i = 0;
    while (src[i] != '\0' && i < 31) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

bool transport_same_id(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) return false;
    uint8_t i = 0;
    while (i < 32) {
        if (a[i] != b[i]) return false;
        if (a[i] == '\0') return true;
        ++i;
    }
    return true;
}

bool transport_is_bidi(uint8_t role) {
    return role == ROLE_WORKER ||
           role == ROLE_PHONE ||
           role == ROLE_SENSOR ||
           role == ROLE_SAFE ||
           role == ROLE_ROUTER;
}

static void wire_put_u8(uint8_t*& p, uint8_t v) {
    *p = v;
    ++p;
}

static void wire_put_u32(uint8_t*& p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
    p += 4;
}

static void wire_put_bytes(uint8_t*& p, const char* src, uint16_t n) {
    uint16_t i = 0;
    while (i < n) {
        p[i] = (src != nullptr) ? static_cast<uint8_t>(src[i]) : 0;
        ++i;
    }
    p += n;
}

static uint8_t wire_get_u8(const uint8_t*& p) {
    uint8_t v = *p;
    ++p;
    return v;
}

static uint32_t wire_get_u32(const uint8_t*& p) {
    uint32_t v = static_cast<uint32_t>(p[0]) |
                 (static_cast<uint32_t>(p[1]) << 8) |
                 (static_cast<uint32_t>(p[2]) << 16) |
                 (static_cast<uint32_t>(p[3]) << 24);
    p += 4;
    return v;
}

static void wire_get_bytes(const uint8_t*& p, char* dst, uint16_t n) {
    uint16_t i = 0;
    while (i < n) {
        dst[i] = static_cast<char>(p[i]);
        ++i;
    }
    p += n;
}

bool transport_encode(const TransportFrame& frame, uint8_t* out, uint16_t cap) {
    if (out == nullptr || cap < TRANSPORT_WIRE_LEN) return false;
    uint8_t* p = out;
    wire_put_u8(p, static_cast<uint8_t>(frame.kind));
    wire_put_u8(p, frame.src_role);
    wire_put_u8(p, frame.dst_role);
    wire_put_bytes(p, frame.src_id, 32);
    wire_put_bytes(p, frame.dst_id, 32);
    wire_put_u8(p, static_cast<uint8_t>(frame.event.type));
    wire_put_bytes(p, frame.event.source_device_id, 32);
    wire_put_u32(p, frame.event.timestamp);
    wire_put_bytes(p, frame.event.payload, 128);
    wire_put_u8(p, static_cast<uint8_t>(frame.event.severity));
    wire_put_bytes(p, frame.mine.mine_id, 32);
    wire_put_u32(p, frame.mine.counter);
    wire_put_u32(p, frame.mine.totp);
    wire_put_u8(p, static_cast<uint8_t>(frame.mine.event));
    wire_put_u32(p, frame.mine.timestamp);
    wire_put_bytes(p, frame.mine.hash, 64);
    wire_put_u32(p, frame.stamp);
    return (p - out) == static_cast<int>(TRANSPORT_WIRE_LEN);
}

bool transport_decode(const uint8_t* in, uint16_t len, TransportFrame& frame) {
    if (in == nullptr || len != TRANSPORT_WIRE_LEN) return false;
    transport_clear_frame(frame);
    const uint8_t* p = in;
    frame.kind = static_cast<TransportKind>(wire_get_u8(p));
    frame.src_role = wire_get_u8(p);
    frame.dst_role = wire_get_u8(p);
    wire_get_bytes(p, frame.src_id, 32);
    frame.src_id[31] = '\0';
    wire_get_bytes(p, frame.dst_id, 32);
    frame.dst_id[31] = '\0';
    frame.event.type = static_cast<EventType>(wire_get_u8(p));
    wire_get_bytes(p, frame.event.source_device_id, 32);
    frame.event.source_device_id[31] = '\0';
    frame.event.timestamp = wire_get_u32(p);
    wire_get_bytes(p, frame.event.payload, 128);
    frame.event.severity = static_cast<Severity>(wire_get_u8(p));
    wire_get_bytes(p, frame.mine.mine_id, 32);
    frame.mine.mine_id[31] = '\0';
    frame.mine.counter = wire_get_u32(p);
    frame.mine.totp = wire_get_u32(p);
    frame.mine.event = static_cast<EventType>(wire_get_u8(p));
    frame.mine.timestamp = wire_get_u32(p);
    wire_get_bytes(p, frame.mine.hash, 64);
    frame.mine.hash[63] = '\0';
    frame.stamp = wire_get_u32(p);
    return (p - in) == static_cast<int>(TRANSPORT_WIRE_LEN);
}

void transport_clear_frame(TransportFrame& frame) {
    frame.kind = TransportKind::EventFrame;
    frame.src_role = 0;
    frame.dst_role = 0;
    frame.src_id[0] = '\0';
    frame.dst_id[0] = '\0';
    frame.stamp = 0;
    frame.event.type = EventType::Heartbeat;
    frame.event.source_device_id[0] = '\0';
    frame.event.timestamp = 0;
    frame.event.payload[0] = '\0';
    frame.event.severity = Severity::Info;
    frame.mine.mine_id[0] = '\0';
    frame.mine.counter = 0;
    frame.mine.totp = 0;
    frame.mine.event = EventType::MineEvent;
    frame.mine.timestamp = 0;
    frame.mine.hash[0] = '\0';
}
