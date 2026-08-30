#include "ghost_transport.h"
#include "registry.h"
#include "decision_pipeline.h"
#include "ghost_vault.h"
#include "ghost_down.h"
#include "ghost_peek.h"
#include "ghost_ir.h"
#include "ghost_policy.h"
#include "ghost_arm.h"

GhostTransport::GhostTransport()
    : registry_(nullptr),
      pipeline_(nullptr),
      vault_(nullptr),
      down_(nullptr),
      peek_(nullptr),
      wlan_(nullptr),
      ir_(nullptr),
      danger_(false),
      kernelDown_(false),
      hiveFrozen_(false),
      terminalMode_(true),
      peek_until_(0),
      nas_flush_acked_(false),
      hmacI_window_(0),
      hmacI_count_(0),
      hmacI_alerted_(false),
      ack_sec_(0),
      ack_n_(0) {
    for (uint8_t i = 0; i < TRANSPORT_PENDING; ++i) {
        pending_[i].used = false;
        pending_[i].dst_id[0] = '\0';
        pending_[i].retries = 0;
        pending_[i].sent_at = 0;
    }
    for (uint8_t i = 0; i < 16; ++i) {
        scanRate_[i].id[0] = '\0';
        scanRate_[i].window_start = 0;
        scanRate_[i].count = 0;
    }
}

void GhostTransport::attach(Registry* registry,
                            DecisionPipeline* pipeline,
                            GhostVault* vault,
                            GhostDown* down,
                            GhostPeek* peek,
                            GhostIR* ir,
                            MediumWlan* wlan,
                            MediumIr* mediumIr) {
    registry_ = registry;
    pipeline_ = pipeline;
    vault_ = vault;
    down_ = down;
    peek_ = peek;
    wlan_ = wlan;
    ir_ = mediumIr;
    if (ir_ != nullptr) {
        ir_->attach(ir);
    }
    (void)ir;
}

void GhostTransport::setDangerMode(bool on) {
    danger_ = on;
}

bool GhostTransport::dangerMode() const {
    return danger_;
}

void GhostTransport::setKernelDown(bool on) {
    kernelDown_ = on;
}

bool GhostTransport::kernelDown() const {
    return kernelDown_;
}

bool GhostTransport::hiveFrozen() const {
    return hiveFrozen_;
}

uint8_t GhostTransport::hmacICount() const {
    return hmacI_count_;
}

uint8_t GhostTransport::hmacIAlerted() const {
    return hmacI_alerted_ ? 1u : 0u;
}

uint8_t GhostTransport::pendingAckCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < TRANSPORT_PENDING; ++i) {
        if (pending_[i].used) ++n;
    }
    return n;
}

uint8_t GhostTransport::ackBudgetUsed() const {
    return ack_n_;
}

bool GhostTransport::enterHiveDown(uint32_t now) {
    if (down_ != nullptr && !down_->isActive()) {
        down_->execute(now);
    }
    if (!ghost_down_armed()) {
        kernelDown_ = true;
        return true;
    }
    if (hiveFrozen_) return true;
    hiveFrozen_ = true;
    danger_ = true;
    if (registry_ != nullptr) {
        registry_->applyGlobalDown();
    }
    if (vault_ != nullptr) {
        vault_->freeze();
    }
    return true;
}

void GhostTransport::setTerminalMode(bool on) {
    terminalMode_ = on;
}

bool GhostTransport::terminalMode() const {
    return terminalMode_;
}

void GhostTransport::openPeekWindow() {
    peek_until_ = 0xFFFFFFFFu;
}

void GhostTransport::openPeekWindow(uint32_t now) {
    peek_until_ = now + PEEK_WINDOW_SEC;
}

bool GhostTransport::peekWindowOpen(uint32_t now) const {
    return peek_until_ != 0 && now <= peek_until_;
}

const Device* GhostTransport::enrolled(const char* id) const {
    if (registry_ == nullptr || id == nullptr) return nullptr;
    return registry_->getDevice(id);
}

void GhostTransport::rememberPending(const char* dst, const Event& event, uint32_t now) {
    for (uint8_t i = 0; i < TRANSPORT_PENDING; ++i) {
        if (!pending_[i].used) {
            transport_copy_id(pending_[i].dst_id, dst);
            pending_[i].event = event;
            pending_[i].sent_at = now;
            pending_[i].retries = 0;
            pending_[i].used = true;
            return;
        }
    }
}

bool GhostTransport::txToDevice(const char* id, uint8_t role, TransportKind kind,
                                const Event& event, uint32_t now) {
    if (kernelDown_) return false;
    if (id == nullptr || id[0] == '\0') return false;
    if (transport_same_id(id, KERNEL_SOURCE_ID)) return false;
    if (role == ROLE_KERNEL) return false;
    if (role == ROLE_MINE) return false;
    if (!transport_is_bidi(role)) return false;

    const Device* d = enrolled(id);
    if (d == nullptr) return false;
    if (danger_ && d->status == DeviceState::Pending) return false;

    if (wlan_ != nullptr) {
        wlan_->registerPeer(id, role);
    }

    TransportFrame frame;
    transport_clear_frame(frame);
    frame.kind = kind;
    frame.src_role = ROLE_KERNEL;
    frame.dst_role = role;
    transport_copy_id(frame.src_id, KERNEL_SOURCE_ID);
    transport_copy_id(frame.dst_id, id);
    frame.event = event;
    frame.stamp = now;
    if (vault_ != nullptr && !vault_->signEvent(frame.event)) return false;

    bool sent = false;
    if (wlan_ != nullptr) sent = wlan_->toPeer(id, frame);
    if (!sent && ir_ != nullptr) sent = ir_->toKernel(frame);
    if (sent) rememberPending(id, event, now);
    return sent;
}

bool GhostTransport::txToRole(uint8_t role, TransportKind kind,
                              const Event& event, uint32_t now) {
    if (registry_ == nullptr) return false;
    bool any = false;
    uint8_t n = registry_->getDeviceCount();
    for (uint8_t i = 0; i < n; ++i) {
        DeviceInfo info = registry_->getDeviceInfo(i);
        if (info.id[0] == '\0') continue;
        if (info.role != role) continue;
        if (txToDevice(info.id, role, kind, event, now)) any = true;
    }
    return any;
}

void GhostTransport::handleAck(const TransportFrame& frame) {
    for (uint8_t i = 0; i < TRANSPORT_PENDING; ++i) {
        if (!pending_[i].used) continue;
        if (!transport_same_id(pending_[i].dst_id, frame.src_id)) continue;
        if (pending_[i].event.timestamp != frame.stamp) continue;
        pending_[i].used = false;
        const Device* d = enrolled(frame.src_id);
        if (d != nullptr && d->role == ROLE_SAFE) {
            nas_flush_acked_ = true;
        }
        return;
    }
}

static void mine_evidence(Event& ev, const MinePayload& payload) {
    ev.type = EventType::MineEvent;
    transport_copy_id(ev.source_device_id, payload.mine_id);
    ev.timestamp = payload.timestamp;
    ev.severity = Severity::High;
    uint32_t c = payload.counter;
    uint8_t len = 0;
    if (c == 0) {
        ev.payload[len++] = '0';
    } else {
        char tmp[11];
        uint8_t t = 0;
        while (c > 0 && t < 10) {
            tmp[t++] = static_cast<char>('0' + (c % 10));
            c /= 10;
        }
        while (t > 0) ev.payload[len++] = tmp[--t];
    }
    ev.payload[len++] = ':';
    uint32_t totp = payload.totp;
    if (totp == 0) {
        ev.payload[len++] = '0';
    } else {
        char tmp[11];
        uint8_t t = 0;
        while (totp > 0 && t < 10) {
            tmp[t++] = static_cast<char>('0' + (totp % 10));
            totp /= 10;
        }
        while (t > 0) ev.payload[len++] = tmp[--t];
    }
    ev.payload[len] = '\0';
}

void GhostTransport::handleMine(const MinePayload& payload, uint32_t now) {
    if (kernelDown_ || hiveFrozen_) return;
    // PeekFrame nur Terminal-Mode; Game-Mode blockiert wie scanWifi.
    if (payload.event == EventType::PeekScan && !terminalMode_) return;

    // §15: ohne TOTP-Seed Replay → Block. Ohne HMAC (Keys gebunden) → Drop.
    if (vault_ != nullptr && vault_->authBound()) {
        if (!vault_->totpBound()) {
            if (pipeline_ != nullptr && registry_ != nullptr) {
                pipeline_->replay().blockMine(payload.mine_id, *registry_);
            }
            Event ev{};
            mine_evidence(ev, payload);
            (void)vault_->store(ev, now);
            return;
        }
        if (!vault_->verifyMine(payload)) {
            Event ev{};
            mine_evidence(ev, payload);
            rejectHmacI(ev, now);
            return;
        }
    }

    MinePayload working = payload;
    if (vault_ != nullptr && vault_->totpBound()) {
        working.hash[0] = '\0';
        if (pipeline_ != nullptr && vault_->totpSeed() != nullptr) {
            (void)pipeline_->replay().setTotpSeed(
                working.mine_id, vault_->totpSeed(), TOTP_SEED_LEN);
        }
    }

    if (!admitMine(working, now)) {
        const Device* clash = enrolled(working.mine_id);
        if (clash != nullptr && clash->role != ROLE_MINE) {
            Event ev{};
            mine_evidence(ev, working);
            if (vault_ != nullptr) (void)vault_->store(ev, now);
        }
        return;
    }
    if (pipeline_ != nullptr) {
        pipeline_->heartbeat().update(working.mine_id, now);
    }

    Event ev{};
    ev.type = EventType::MineEvent;
    transport_copy_id(ev.source_device_id, working.mine_id);
    ev.timestamp = working.timestamp;
    ev.severity = Severity::Info;
    uint32_t c = working.counter;
    uint8_t i = 0;
    char buf[32];
    uint8_t len = 0;
    if (c == 0) {
        buf[len++] = '0';
    } else {
        char tmp[11];
        uint8_t t = 0;
        while (c > 0 && t < 10) {
            tmp[t++] = static_cast<char>('0' + (c % 10));
            c /= 10;
        }
        while (t > 0) buf[len++] = tmp[--t];
    }
    buf[len++] = ':';
    uint32_t totp = working.totp;
    if (totp == 0) {
        buf[len++] = '0';
    } else {
        char tmp[11];
        uint8_t t = 0;
        while (totp > 0 && t < 10) {
            tmp[t++] = static_cast<char>('0' + (totp % 10));
            totp /= 10;
        }
        while (t > 0) buf[len++] = tmp[--t];
    }
    buf[len] = '\0';
    while (i < len && i < 127) {
        ev.payload[i] = buf[i];
        ++i;
    }
    ev.payload[i] = '\0';
    if (working.event == EventType::AnomalyDetected) {
        ev.severity = Severity::Critical;
        if (i < 120) {
            ev.payload[i++] = ':';
            const char* t = "tamper";
            uint8_t k = 0;
            while (t[k] != '\0' && i < 127) {
                ev.payload[i++] = t[k++];
            }
            ev.payload[i] = '\0';
        }
    }
    if (vault_ != nullptr) {
        (void)vault_->signEvent(ev);
    }
    if (pipeline_ == nullptr) {
        return;
    }
    PipelineResult pr = pipeline_->process(ev, now);
    if (pr != PipelineResult::Accepted) {
        return;
    }
    if (peek_ != nullptr && peekWindowOpen(now) && terminalMode_) {
        peek_->ingestMine(working);
    }
}

bool GhostTransport::admitPeer(const TransportFrame& frame, uint8_t role, uint8_t trust,
                              bool* parked) {
    if (parked != nullptr) *parked = false;
    if (kernelDown_ || hiveFrozen_) return false;
    if (frame.src_role != role) return false;
    if (frame.src_id[0] == '\0') return false;
    if (transport_same_id(frame.src_id, KERNEL_SOURCE_ID)) return false;
    if (registry_ == nullptr) return false;

    const Device* existing = enrolled(frame.src_id);
    if (existing != nullptr) {
        if (existing->role != role) return false;
        if (existing->status == DeviceState::Blocked) return false;
        if (existing->status == DeviceState::Pending) {
            return false;
        }
        return true;
    }

    // §2.2 / §32 / §44: kein Auto-Enroll. Unbekannte ID bleibt pending (P01).
    if (danger_) return false;

    Device d{};
    transport_copy_id(d.id, frame.src_id);
    d.role = role;
    d.trust_level = trust;
    d.status = DeviceState::Pending;
    d.last_seen = 0;
    d.capability_mask = 0;
    d.tag_mask = 0;
    if (!registry_->addDevice(d)) return false;
    if (parked != nullptr) *parked = true;
    return false;
}

bool GhostTransport::admitWorker(const TransportFrame& frame) {
    bool parked = false;
    return admitPeer(frame, ROLE_WORKER, 2, &parked);
}

void GhostTransport::noteUnknown(const char* id, uint32_t now) {
    Event ev{};
    ev.type = EventType::DeviceSeen;
    ev.severity = Severity::Warn;
    ev.timestamp = now;
    transport_copy_id(ev.source_device_id, id);
    const char* tag = "unknown_device";
    uint8_t i = 0;
    while (tag[i] != '\0' && i < 80) {
        ev.payload[i] = tag[i];
        ++i;
    }
    ev.payload[i] = '\0';
    if (vault_ != nullptr) {
        (void)vault_->signEvent(ev);
        (void)vault_->store(ev, now);
    }
    if (pipeline_ != nullptr) {
        (void)pipeline_->process(ev, now);
    }
}

void GhostTransport::rejectHmacI(Event ev, uint32_t now) {
    // §18 / §43 / P15: drop always. Evidence once per 60s window.
    // N=3 HMAC-I in T → one Alert, not N. Never Ghost Down.
    const uint32_t win = 60u;
    const uint8_t alert_n = 3;
    if (now < hmacI_window_ || (now - hmacI_window_) >= win) {
        hmacI_window_ = now;
        hmacI_count_ = 0;
        hmacI_alerted_ = 0;
    }
    if (hmacI_count_ < 0xFFu) ++hmacI_count_;

    if (ev.timestamp == 0) ev.timestamp = now;
    Event viol{};
    viol.type = EventType::PolicyViolation;
    viol.severity = Severity::High;
    viol.timestamp = now;
    transport_copy_id(viol.source_device_id, ev.source_device_id);
    const char* tag = "hmac_i";
    uint8_t i = 0;
    while (tag[i] != '\0' && i < 80) {
        viol.payload[i] = tag[i];
        ++i;
    }
    viol.payload[i] = '\0';

    if (hmacI_count_ == 1 && vault_ != nullptr) {
        (void)vault_->store(ev, now);
        (void)vault_->signEvent(viol);
        (void)vault_->store(viol, now);
    }
    if (hmacI_alerted_ == 0 && hmacI_count_ >= alert_n && pipeline_ != nullptr) {
        hmacI_alerted_ = 1;
        if (viol.source_device_id[0] == '\0') {
            transport_copy_id(viol.source_device_id, ev.source_device_id);
        }
        (void)pipeline_->process(viol, now);
    }
}

bool GhostTransport::sensorSpam(const char* id, uint32_t now) {
    // §17 P04 / §23: Sensor-Scan 60–300s. Mehr als 2 Scan-Events im 60s-Fenster = Spam.
    if (id == nullptr || id[0] == '\0') return false;
    int16_t found = -1;
    int16_t empty = -1;
    for (uint8_t i = 0; i < 16; ++i) {
        if (scanRate_[i].id[0] == '\0') {
            if (empty < 0) empty = static_cast<int16_t>(i);
            continue;
        }
        if (transport_same_id(scanRate_[i].id, id)) {
            found = static_cast<int16_t>(i);
            break;
        }
    }
    if (found < 0) {
        if (empty < 0) return true;
        found = empty;
        transport_copy_id(scanRate_[found].id, id);
        scanRate_[found].window_start = now;
        scanRate_[found].count = 1;
        return false;
    }
    ScanRate& r = scanRate_[static_cast<uint8_t>(found)];
    if (now < r.window_start || (now - r.window_start) >= 60u) {
        r.window_start = now;
        r.count = 1;
        return false;
    }
    if (r.count < 0xFFu) ++r.count;
    return r.count > 2;
}

bool GhostTransport::admitMine(const MinePayload& payload, uint32_t now) {
    if (kernelDown_ || hiveFrozen_) return false;
    if (registry_ == nullptr) return false;
    if (payload.mine_id[0] == '\0') return false;

    const Device* existing = enrolled(payload.mine_id);
    if (existing != nullptr) return existing->role == ROLE_MINE;

    if (danger_) return false;

    Device d{};
    transport_copy_id(d.id, payload.mine_id);
    d.role = ROLE_MINE;
    d.trust_level = 0;
    d.status = DeviceState::Silent;
    d.last_seen = now;
    d.capability_mask = 0;
    d.tag_mask = 0;
    return registry_->addDevice(d);
}

void GhostTransport::handleFrame(const TransportFrame& frame, uint32_t now) {
    if (vault_ != nullptr && vault_->keysAttached() && !vault_->authBound()) {
        return;
    }
    if (frame.kind == TransportKind::AckFrame) {
        if (vault_ != nullptr && vault_->eventHasMac(frame.event) &&
            !vault_->verifyEvent(frame.event)) {
            Event ev = frame.event;
            if (ev.source_device_id[0] == '\0') {
                transport_copy_id(ev.source_device_id, frame.src_id);
            }
            rejectHmacI(ev, now);
            return;
        }
        handleAck(frame);
        return;
    }
    if (hiveFrozen_ && frame.kind != TransportKind::AckFrame) {
        return;
    }
    if (frame.kind == TransportKind::MineFrame) {
        handleMine(frame.mine, now);
        return;
    }
    if (vault_ != nullptr && vault_->keysAttached() && !vault_->verifyEvent(frame.event)) {
        // §18 validate: HMAC-I is drop, not freeze.
        Event ev = frame.event;
        if (ev.source_device_id[0] == '\0') {
            transport_copy_id(ev.source_device_id, frame.src_id);
        }
        rejectHmacI(ev, now);
        return;
    }

    Event bound = frame.event;
    if (bound.source_device_id[0] == '\0') {
        transport_copy_id(bound.source_device_id, frame.src_id);
    }
    // §30 / §33: Device-Key ist gemeinsam. Frame-ID muss Event-Quelle sein.
    if (!transport_same_id(bound.source_device_id, frame.src_id)) {
        if (vault_ != nullptr) (void)vault_->store(bound, now);
        return;
    }

    bool parked = false;
    bool admitted = false;
    if (frame.src_role == ROLE_WORKER) {
        admitted = admitPeer(frame, ROLE_WORKER, 2, &parked);
    } else if (frame.src_role == ROLE_SENSOR) {
        admitted = admitPeer(frame, ROLE_SENSOR, 1, &parked);
    } else if (frame.src_role == ROLE_SAFE) {
        admitted = admitPeer(frame, ROLE_SAFE, 1, &parked);
    } else if (frame.src_role == ROLE_PHONE) {
        admitted = admitPeer(frame, ROLE_PHONE, 1, &parked);
    } else if (frame.src_role == ROLE_ROUTER) {
        admitted = admitPeer(frame, ROLE_ROUTER, 1, &parked);
    } else {
        return;
    }
    if (!admitted) {
        if (parked) {
            noteUnknown(frame.src_id, now);
            return;
        }
        const Device* hold = enrolled(frame.src_id);
        if (hold != nullptr && hold->status == DeviceState::Pending) {
            return;
        }
        if (vault_ != nullptr) {
            (void)vault_->store(bound, now);
        }
        return;
    }

    handleEvent(bound, now);
}

void GhostTransport::handleEvent(const Event& event, uint32_t now) {
    if (kernelDown_ || hiveFrozen_) return;
    if (event.source_device_id[0] == '\0') return;
    if (transport_same_id(event.source_device_id, KERNEL_SOURCE_ID)) return;

    const Device* d = enrolled(event.source_device_id);
    if (d == nullptr) return;
    if (d->status == DeviceState::Pending) return;
    if (d->status == DeviceState::Blocked) return;

    // HMAC bindet timestamp. Gleiches Packet erneut → Replay, drop (kein GD).
    if (d->last_seen != 0 && event.timestamp <= d->last_seen) {
        return;
    }

    bool scanLike = (event.type == EventType::ScanResult ||
                     event.type == EventType::DeviceSeen ||
                     event.type == EventType::AnomalyDetected);
    if (scanLike &&
        (d->role == ROLE_SENSOR || d->role == ROLE_PHONE || d->role == ROLE_ROUTER)) {
        if (sensorSpam(event.source_device_id, now)) {
            Event viol{};
            viol.type = EventType::PolicyViolation;
            viol.severity = Severity::High;
            viol.timestamp = now;
            transport_copy_id(viol.source_device_id, event.source_device_id);
            const char* tag = "sensor_spam";
            uint8_t i = 0;
            while (tag[i] != '\0' && i < 80) {
                viol.payload[i] = tag[i];
                ++i;
            }
            viol.payload[i] = '\0';
            if (vault_ != nullptr) (void)vault_->signEvent(viol);
            if (pipeline_ != nullptr) (void)pipeline_->process(viol, now);
            return;
        }
    }

    if (event.type == EventType::DangerModeEnter) danger_ = true;
    if (event.type == EventType::DangerModeExit) danger_ = false;

    // §43 / §44: Kill nur Kernel. Peer-GhostDownStart nicht routen, nicht frieren.
    if (event.type == EventType::GhostDownStart) {
        Event viol{};
        viol.type = EventType::PolicyViolation;
        viol.severity = Severity::High;
        viol.timestamp = now;
        transport_copy_id(viol.source_device_id, event.source_device_id);
        const char* tag = "peer_kill";
        uint8_t i = 0;
        while (tag[i] != '\0' && i < 80) {
            viol.payload[i] = tag[i];
            ++i;
        }
        viol.payload[i] = '\0';
        if (vault_ != nullptr) {
            (void)vault_->signEvent(viol);
            (void)vault_->store(viol, now);
        }
        return;
    }

    if (pipeline_ != nullptr) {
        PipelineResult pr = pipeline_->process(event, now);
        if (pr == PipelineResult::Accepted &&
            event.type != EventType::TelemetryUpdate &&
            registry_ != nullptr) {
            const Device* cur = enrolled(event.source_device_id);
            if (cur != nullptr) {
                Device copy = *cur;
                copy.last_seen = event.timestamp;
                (void)registry_->updateDevice(event.source_device_id, copy);
            }
        }
    }
}

void GhostTransport::rx(uint32_t now) {
    if (wlan_ != nullptr) wlan_->pump();
    TransportFrame frame;
    uint8_t n = 0;
    while (wlan_ != nullptr && wlan_->fromKernel(frame) && n < TRANSPORT_KERNEL_SLOTS) {
        handleFrame(frame, now);
        ++n;
    }
    n = 0;
    while (ir_ != nullptr && ir_->fromKernel(frame) && n < TRANSPORT_IR_SLOTS) {
        handleFrame(frame, now);
        ++n;
    }
}

void GhostTransport::ingest(const TransportFrame& frame, uint32_t now) {
    handleFrame(frame, now);
}

void GhostTransport::tick(uint32_t now) {
    if (kernelDown_) {
        for (uint8_t i = 0; i < TRANSPORT_PENDING; ++i) {
            pending_[i].used = false;
        }
        return;
    }

    if (registry_ != nullptr && !danger_) {
        const Device* w = registry_->findByRole(ROLE_WORKER);
        const Device* m = registry_->findByRole(ROLE_MINE);
        if (w != nullptr && m != nullptr &&
            w->status == DeviceState::Degraded &&
            m->status == DeviceState::Suspected) {
            danger_ = true;
        }
    }

    for (uint8_t i = 0; i < TRANSPORT_PENDING; ++i) {
        if (!pending_[i].used) continue;
        uint32_t gap = (now > pending_[i].sent_at) ? (now - pending_[i].sent_at) : 0;
        if (gap < ACK_RETRY_SEC) continue;
        const Device* d = enrolled(pending_[i].dst_id);
        if (d == nullptr || !transport_is_bidi(d->role)) {
            pending_[i].used = false;
            continue;
        }
        if (pending_[i].retries < ACK_RETRY_LIMIT) {
            TransportFrame frame;
            transport_clear_frame(frame);
            frame.kind = TransportKind::EventFrame;
            frame.src_role = ROLE_KERNEL;
            frame.dst_role = d->role;
            transport_copy_id(frame.src_id, KERNEL_SOURCE_ID);
            transport_copy_id(frame.dst_id, pending_[i].dst_id);
            frame.event = pending_[i].event;
            frame.stamp = now;
            if (vault_ != nullptr && !vault_->signEvent(frame.event)) continue;
            if (wlan_ != nullptr && wlan_->toPeer(pending_[i].dst_id, frame)) {
                ++pending_[i].retries;
                pending_[i].sent_at = now;
            }
            continue;
        }
        if (gap >= ACK_TIMEOUT_SEC) {
            pending_[i].used = false;
        }
    }
}

bool GhostTransport::route(const Event& event, uint32_t now) {
    if (kernelDown_) return false;
    if (pipeline_ == nullptr) return true;

    if (event.type == EventType::DangerModeEnter) danger_ = true;
    if (event.type == EventType::DangerModeExit) danger_ = false;

    PolicyAction action = pipeline_->policy().evaluate(event);
    uint8_t extra = pipeline_->policy().extraFlags(event);

    if (action == PolicyAction::Alert || (extra & PX_ALERT) != 0) {
        (void)txToRole(ROLE_PHONE, TransportKind::EventFrame, event, now);
        (void)txToRole(ROLE_WORKER, TransportKind::EventFrame, event, now);
    }
    if (action == PolicyAction::Backup ||
        (extra & PX_VAULT) != 0 ||
        event.type == EventType::BackupWritten) {
        (void)txToRole(ROLE_SAFE, TransportKind::FlushFrame, event, now);
        (void)txToRole(ROLE_WORKER, TransportKind::EventFrame, event, now);
    }
    if (event.type == EventType::DangerModeEnter ||
        event.type == EventType::DangerModeExit ||
        event.type == EventType::GhostDownStart ||
        event.type == EventType::GhostDownEnd) {
        // §42 Router informiert, NAS read-only, Sensoren Silent — alle Bidi-Rollen.
        (void)txToRole(ROLE_PHONE, TransportKind::EventFrame, event, now);
        (void)txToRole(ROLE_WORKER, TransportKind::EventFrame, event, now);
        (void)txToRole(ROLE_SENSOR, TransportKind::EventFrame, event, now);
        (void)txToRole(ROLE_SAFE, TransportKind::EventFrame, event, now);
        (void)txToRole(ROLE_ROUTER, TransportKind::EventFrame, event, now);
    }
    // §43: KillFrame nur über sendKill nach Snapshot, nicht bei Inbound-GhostDownStart.
    if (event.type == EventType::PeekScan) {
        openPeekWindow(now);
    }
    return true;
}

bool GhostTransport::sendAck(const Event& event, uint32_t now) {
    if (kernelDown_) return true;
    if (down_ != nullptr && down_->isActive()) return true;
    if (event.type == EventType::MineEvent) return true;
    const Device* d = enrolled(event.source_device_id);
    if (d == nullptr) return true;
    if (!transport_is_bidi(d->role)) return true;

    if (now != ack_sec_) {
        ack_sec_ = now;
        ack_n_ = 0;
    }
    if (ack_n_ >= ACK_BUDGET_PER_SEC) return true;
    ++ack_n_;

    TransportFrame frame;
    transport_clear_frame(frame);
    frame.kind = TransportKind::AckFrame;
    frame.src_role = ROLE_KERNEL;
    frame.dst_role = d->role;
    transport_copy_id(frame.src_id, KERNEL_SOURCE_ID);
    transport_copy_id(frame.dst_id, event.source_device_id);
    frame.event = event;
    frame.stamp = event.timestamp;
    if (vault_ != nullptr && !vault_->signEvent(frame.event)) return true;
    if (wlan_ != nullptr) {
        wlan_->registerPeer(event.source_device_id, d->role);
        (void)wlan_->toPeer(event.source_device_id, frame);
    }
    return true;
}

bool GhostTransport::sendKill(uint32_t now) {
    // §33 / §43: Kill nur mit Root (authBound) und Snapshot-Referenz.
    if (kernelDown_) return false;
    if (vault_ != nullptr && !vault_->authBound()) return false;
    uint8_t snap = 0;
    if (vault_ != nullptr) snap = vault_->getStoredCount();
    if (snap == 0) return false;

    Event ev{};
    ev.type = EventType::GhostDownStart;
    transport_copy_id(ev.source_device_id, KERNEL_SOURCE_ID);
    ev.timestamp = now;
    ev.severity = Severity::Critical;
    ev.payload[0] = 'k';
    ev.payload[1] = 'i';
    ev.payload[2] = 'l';
    ev.payload[3] = 'l';
    ev.payload[4] = ':';
    uint8_t i = 5;
    uint32_t n = snap;
    char tmp[4];
    uint8_t t = 0;
    while (n > 0 && t < 4) {
        tmp[t++] = static_cast<char>('0' + (n % 10));
        n /= 10;
    }
    while (t > 0 && i < 80) ev.payload[i++] = tmp[--t];
    ev.payload[i] = '\0';

    bool any = false;
    if (txToRole(ROLE_WORKER, TransportKind::KillFrame, ev, now)) any = true;
    if (txToRole(ROLE_PHONE, TransportKind::KillFrame, ev, now)) any = true;
    if (txToRole(ROLE_SENSOR, TransportKind::KillFrame, ev, now)) any = true;
    if (txToRole(ROLE_SAFE, TransportKind::KillFrame, ev, now)) any = true;
    if (txToRole(ROLE_ROUTER, TransportKind::KillFrame, ev, now)) any = true;
    return any;
}

bool GhostTransport::flushVault(uint32_t now) {
    if (kernelDown_) return false;
    if (vault_ == nullptr) return false;
    bool any = false;
    uint8_t n = vault_->getStoredCount();
    for (uint8_t i = 0; i < n; ++i) {
        const Event* ev = vault_->peekRam(i);
        if (ev == nullptr) continue;
        Event out = *ev;
        out.type = EventType::BackupWritten;
        if (txToRole(ROLE_SAFE, TransportKind::FlushFrame, out, now)) any = true;
    }
    return any;
}

bool GhostTransport::flushNas(uint32_t now) {
    nas_flush_acked_ = false;
    return flushVault(now);
}

bool GhostTransport::nasFlushAcked() const {
    return nas_flush_acked_;
}
