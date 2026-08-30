#ifndef P30_HARNESS_H
#define P30_HARNESS_H

// =====================================================
// SPEC v1.7.1 §30 Gegner-Suite — Test-Helfer
// Keine neuen Kernel-Module. Nur bestehende Frames/Keys.
// =====================================================

#include "../src/psp/ghost_core.h"
#include "../src/psp/registry.h"
#include "../src/psp/event_queue.h"
#include "../src/psp/ghost_policy.h"
#include "../src/psp/replay_guard.h"
#include "../src/psp/decision_pipeline.h"
#include "../src/psp/ghost_vault.h"
#include "../src/psp/ghost_keys.h"
#include "../src/psp/ghost_down.h"
#include "../src/psp/ghost_stealth.h"
#include "../src/psp/ghost_peek.h"
#include "../src/psp/ghost_ir.h"
#include "../src/psp/ghost_output.h"
#include "../src/psp/transport/ghost_transport.h"
#include "../src/psp/transport/medium_wlan.h"
#include "../src/psp/transport/medium_ir.h"
#include "../src/mine/mine.h"

#include <cstdio>

struct P30Hive {
    Registry reg;
    GhostVault vault;
    GhostKeys keys;
    DecisionPipeline pipe;
    GhostTransport xport;
    GhostDown down;
    GhostStealth stealth;
    GhostPeek peek;
    GhostIR ir;
    MediumWlan wlan;
    MediumIr mediumIr;
};

static void p30_copy_id(char* dst, const char* src) {
    uint8_t i = 0;
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i < 31) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

#if defined(__GNUC__)
#define P30_UNUSED __attribute__((unused))
#else
#define P30_UNUSED
#endif

P30_UNUSED static bool p30_contains(const char* hay, const char* needle) {
    if (hay == nullptr || needle == nullptr || needle[0] == '\0') return false;
    uint16_t i = 0;
    while (hay[i] != '\0') {
        uint16_t j = 0;
        while (needle[j] != '\0' && hay[i + j] == needle[j]) ++j;
        if (needle[j] == '\0') return true;
        ++i;
    }
    return false;
}

static bool p30_bind_root(P30Hive& h) {
    h.keys.initEmpty();
    uint8_t root[KEY_LEN];
    for (uint8_t i = 0; i < KEY_LEN; ++i) {
        root[i] = static_cast<uint8_t>(i + 1);
    }
    if (!h.keys.provisionRoot(root, KEY_LEN)) return false;
    if (!h.keys.provisionDerived(root, KEY_LEN)) return false;
    h.vault.attachKeys(&h.keys);
    return h.vault.authBound();
}

static void p30_attach(P30Hive& h) {
    h.pipe.attach(&h.reg, &h.vault);
    h.pipe.attachTransport(&h.xport);
    h.down.attach(&h.vault, &h.stealth);
    h.down.attachTransport(&h.xport);
    h.vault.attachTransport(&h.xport);
    h.xport.attach(&h.reg, &h.pipe, &h.vault, &h.down,
                   &h.peek, &h.ir, &h.wlan, &h.mediumIr);
}

P30_UNUSED static void p30_enroll(P30Hive& h, const char* id, uint8_t role, uint8_t trust) {
    Device d{};
    p30_copy_id(d.id, id);
    d.role = role;
    d.trust_level = trust;
    d.status = DeviceState::Pending;
    d.last_seen = 0;
    (void)h.reg.addDevice(d);
    (void)h.reg.pairDevice(id);
    h.wlan.registerPeer(id, role);
}

P30_UNUSED static Event p30_event(EventType type, const char* id, uint32_t now,
                       const char* payload) {
    Event ev{};
    ev.type = type;
    p30_copy_id(ev.source_device_id, id);
    ev.timestamp = now;
    ev.severity = Severity::Info;
    ev.payload[0] = '\0';
    if (payload != nullptr) {
        uint8_t i = 0;
        while (payload[i] != '\0' && i < 80) {
            ev.payload[i] = payload[i];
            ++i;
        }
        ev.payload[i] = '\0';
    }
    return ev;
}

P30_UNUSED static void p30_inject(P30Hive& h, uint8_t role, const char* id,
                       Event ev, uint32_t now, bool sign) {
    if (sign) (void)h.vault.signEvent(ev);
    TransportFrame frame;
    transport_clear_frame(frame);
    frame.kind = TransportKind::EventFrame;
    frame.src_role = role;
    frame.dst_role = ROLE_KERNEL;
    p30_copy_id(frame.src_id, id);
    p30_copy_id(frame.dst_id, KERNEL_SOURCE_ID);
    frame.event = ev;
    p30_copy_id(frame.event.source_device_id, id);
    frame.stamp = now;
    (void)h.wlan.toKernel(frame);
    h.xport.rx(now);
}

P30_UNUSED static void p30_inject_mine(P30Hive& h, MinePayload mp, uint32_t now, bool sign) {
    if (sign) (void)h.vault.signMine(mp);
    TransportFrame frame;
    transport_clear_frame(frame);
    frame.kind = TransportKind::MineFrame;
    frame.src_role = ROLE_MINE;
    frame.dst_role = ROLE_KERNEL;
    p30_copy_id(frame.src_id, mp.mine_id);
    p30_copy_id(frame.dst_id, KERNEL_SOURCE_ID);
    frame.mine = mp;
    frame.stamp = now;
    (void)h.wlan.toKernel(frame);
    h.xport.rx(now);
}

P30_UNUSED static bool p30_hmac_any(const GhostVault& vault, char mark) {
    uint8_t n = vault.getStoredCount();
    for (uint8_t i = 0; i < n; ++i) {
        if (vault.hmacMark(i) == mark) return true;
    }
    return false;
}

P30_UNUSED static bool p30_totp_any(const GhostVault& vault, char mark) {
    uint8_t n = vault.getStoredCount();
    for (uint8_t i = 0; i < n; ++i) {
        if (vault.totpMark(i) == mark) return true;
    }
    return false;
}

P30_UNUSED static bool p30_has_type(const GhostVault& vault, EventType type) {
    uint8_t n = vault.getStoredCount();
    for (uint8_t i = 0; i < n; ++i) {
        Event plain{};
        if (!vault.copyPlain(i, &plain)) continue;
        if (plain.type == type) return true;
    }
    return false;
}

#endif
