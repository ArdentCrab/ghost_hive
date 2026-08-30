#include "decision_pipeline.h"
#include "transport/ghost_transport.h"
#include "ghost_telemetry.h"

DecisionPipeline::DecisionPipeline()
    : registry_(nullptr),
      vault_(nullptr),
      transport_(nullptr),
      lastPriority_(Priority::Psp),
      lastFallback_(TaskTarget::Psp),
      classRoute_(ROLE_KERNEL),
      priorityHasPeer_(false) {
    policy_.initDefaults();
    for (uint8_t i = 0; i < MAX_DEVICES; ++i) {
        telem_rep_[i].id[0] = '\0';
        telem_rep_[i].next = 0;
        telem_rep_[i].live = 0;
        telem_rep_[i].rate_used = 0;
        telem_rep_[i].last_ok = 0;
        telem_rep_[i].last_event_ts = 0;
        telem_rep_[i].dense_drops = 0;
        for (uint8_t s = 0; s < TELEM_REPLAY_SLOTS; ++s) {
            telem_rep_[i].slot[s].used = 0;
            telem_rep_[i].slot[s].ts = 0;
        }
    }
}

DecisionPipeline::~DecisionPipeline() {
    if (registry_ != nullptr) {
        registry_->setRemoveHook(nullptr, nullptr);
        registry_ = nullptr;
    }
}

void DecisionPipeline::attach(Registry* registry, GhostVault* vault) {
    if (registry_ != nullptr && registry_ != registry) {
        registry_->setRemoveHook(nullptr, nullptr);
    }
    registry_ = registry;
    vault_ = vault;
    if (registry_ != nullptr) {
        registry_->setRemoveHook(&DecisionPipeline::onDeviceRemoved, this);
    }
}

void DecisionPipeline::onDeviceRemoved(void* ctx, const char* id) {
    if (ctx == nullptr) return;
    static_cast<DecisionPipeline*>(ctx)->recycleTelem(id);
}

void DecisionPipeline::clearTelemDev(uint8_t i) {
    telem_rep_[i].id[0] = '\0';
    telem_rep_[i].next = 0;
    telem_rep_[i].live = 0;
    telem_rep_[i].rate_used = 0;
    telem_rep_[i].last_ok = 0;
    telem_rep_[i].last_event_ts = 0;
    telem_rep_[i].dense_drops = 0;
    for (uint8_t s = 0; s < TELEM_REPLAY_SLOTS; ++s) {
        telem_rep_[i].slot[s].used = 0;
        telem_rep_[i].slot[s].ts = 0;
    }
}

void DecisionPipeline::recycleTelem(const char* id) {
    int16_t idx = telemDevIndex(id);
    if (idx < 0) return;
    clearTelemDev(static_cast<uint8_t>(idx));
}

void DecisionPipeline::attachTransport(GhostTransport* transport) {
    transport_ = transport;
}

void DecisionPipeline::drain(EventQueue& queue, uint32_t now) {
    Event ev;
    while (queue.pop(ev)) {
        process(ev, now);
    }
}

ReplayGuard& DecisionPipeline::replay() {
    return replay_;
}

GhostHeartbeat& DecisionPipeline::heartbeat() {
    return heartbeat_;
}

GhostPolicy& DecisionPipeline::policy() {
    return policy_;
}

uint32_t DecisionPipeline::telemDenseDrops(const char* id) const {
    int16_t idx = telemDevIndex(id);
    if (idx < 0) return 0;
    return telem_rep_[static_cast<uint8_t>(idx)].dense_drops;
}

PipelineResult DecisionPipeline::process(const Event& event, uint32_t now) {
    if (transport_ != nullptr && transport_->hiveFrozen()) {
        return PipelineResult::Rejected;
    }
    if (vault_ != nullptr && vault_->frozen()) {
        return PipelineResult::Rejected;
    }
    if (!validate(event)) return PipelineResult::Rejected;

    if (!replayGuard(event, now)) return PipelineResult::Blocked;

    Event working = event;

    if (!enrich(working, now)) return PipelineResult::Rejected;
    if (!classify(working)) return PipelineResult::Rejected;
    if (!checkPolicies(working)) return PipelineResult::Rejected;
    if (!computePriority(working)) return PipelineResult::Rejected;
    if (!computeFallback(working)) return PipelineResult::Rejected;
    if (!computeEscalation(working)) return PipelineResult::Escalated;
    if (!route(working, now)) return PipelineResult::Rejected;
    if (!store(working, now)) return PipelineResult::Rejected;
    if (!ack(working, now)) return PipelineResult::Rejected;

    return PipelineResult::Accepted;
}

bool DecisionPipeline::validate(const Event& event) const {
    if (event.source_device_id[0] == '\0') return false;
    if (event.type != EventType::TelemetryUpdate) return true;
    if (registry_ == nullptr) return false;
    const Device* d = registry_->getDevice(event.source_device_id);
    if (d == nullptr) return false;
    return validateTelemetryPayload(event, d->role);
}

bool DecisionPipeline::replayGuard(const Event& event, uint32_t now) {
    if (event.type == EventType::TelemetryUpdate) {
        return telemReplayOk(event, now);
    }

    if (event.type != EventType::MineEvent) {
        return true;
    }

    MinePayload payload{};

    for (uint8_t i = 0; i < 32; ++i) {
        payload.mine_id[i] = event.source_device_id[i];
        if (event.source_device_id[i] == '\0') break;
    }

    payload.event = event.type;
    payload.timestamp = event.timestamp;

    uint32_t counter = 0;
    uint32_t totp = 0;
    parseMinePayload(event.payload, counter, totp);
    payload.counter = counter;
    payload.totp = totp;

    if (!replay_.check(payload, now)) {
        if (registry_ != nullptr) {
            replay_.blockMine(payload.mine_id, *registry_);
        } else {
            replay_.blockMine(payload.mine_id);
        }

        if (vault_ != nullptr) {
            Event evidence = event;
            evidence.severity = Severity::High;
            (void)vault_->store(evidence, now);
            Event viol{};
            viol.type = EventType::PolicyViolation;
            viol.severity = Severity::High;
            viol.timestamp = now;
            for (uint8_t i = 0; i < 32; ++i) {
                viol.source_device_id[i] = payload.mine_id[i];
                if (payload.mine_id[i] == '\0') break;
            }
            const char* tag = "mine_replay";
            uint8_t i = 0;
            while (tag[i] != '\0' && i < 127) {
                viol.payload[i] = tag[i];
                ++i;
            }
            viol.payload[i] = '\0';
            (void)vault_->signEvent(viol);
            vault_->store(viol, now);
        }
        return false;
    }

    return true;
}

int16_t DecisionPipeline::telemDevIndex(const char* id) const {
    if (id == nullptr || id[0] == '\0') return -1;
    for (uint8_t i = 0; i < MAX_DEVICES; ++i) {
        if (telem_rep_[i].live == 0) continue;
        uint8_t k = 0;
        while (k < 32 && id[k] != '\0' && telem_rep_[i].id[k] == id[k]) ++k;
        bool a_end = (k == 32) || (id[k] == '\0');
        bool b_end = (k == 32) || (telem_rep_[i].id[k] == '\0');
        if (a_end && b_end) return static_cast<int16_t>(i);
    }
    return -1;
}

bool DecisionPipeline::telemReplayOk(const Event& event, uint32_t now) {
    int16_t idx = telemDevIndex(event.source_device_id);
    if (idx < 0) {
        for (uint8_t i = 0; i < MAX_DEVICES; ++i) {
            if (telem_rep_[i].live == 0) {
                idx = static_cast<int16_t>(i);
                uint8_t k = 0;
                while (event.source_device_id[k] != '\0' && k < 31) {
                    telem_rep_[i].id[k] = event.source_device_id[k];
                    ++k;
                }
                telem_rep_[i].id[k] = '\0';
                telem_rep_[i].live = 1;
                telem_rep_[i].next = 0;
                telem_rep_[i].rate_used = 0;
                telem_rep_[i].last_ok = 0;
                telem_rep_[i].last_event_ts = 0;
                telem_rep_[i].dense_drops = 0;
                break;
            }
        }
    }
    if (idx < 0) return false;
    TelemRepDev& dev = telem_rep_[static_cast<uint8_t>(idx)];
    bool dense = false;
    if (dev.rate_used != 0) {
        if (now <= dev.last_ok || (now - dev.last_ok) < 1u) dense = true;
        if (event.timestamp <= dev.last_event_ts ||
            (event.timestamp - dev.last_event_ts) < 1u) {
            dense = true;
        }
    }
    if (dense) {
        if (dev.dense_drops < 0xFFFFFFFFu) ++dev.dense_drops;
        return false;
    }

    const uint8_t* blob = reinterpret_cast<const uint8_t*>(event.payload);
    for (uint8_t s = 0; s < TELEM_REPLAY_SLOTS; ++s) {
        if (dev.slot[s].used == 0) continue;
        if (dev.slot[s].ts != event.timestamp) continue;
        uint8_t same = 1;
        for (uint8_t b = 0; b < 10; ++b) {
            if (dev.slot[s].blob[b] != blob[b]) {
                same = 0;
                break;
            }
        }
        if (same) return false;
    }
    uint8_t n = dev.next;
    dev.slot[n].ts = event.timestamp;
    for (uint8_t b = 0; b < 10; ++b) dev.slot[n].blob[b] = blob[b];
    dev.slot[n].used = 1;
    dev.next = static_cast<uint8_t>((n + 1u) % TELEM_REPLAY_SLOTS);
    dev.rate_used = 1;
    dev.last_ok = now;
    dev.last_event_ts = event.timestamp;
    return true;
}

bool DecisionPipeline::handleTelemetryUpdate(Event& event, uint32_t now) {
    event.severity = Severity::Info;
    uint16_t ram = 0, traf = 0, wifi = 0;
    uint8_t cpu = 0, gpu = 0, bat = 0;
    if (!parseTelemetryFields(event, &ram, &cpu, &gpu, &traf, &bat, &wifi)) {
        return false;
    }
    if (registry_ == nullptr) return false;
    const Device* d = registry_->getDevice(event.source_device_id);
    if (d == nullptr) return false;
    uint32_t ls = now;
    if (d->role == ROLE_WORKER && d->trust_level >= 2) ls = event.timestamp;
    return registry_->updateTelemetry(event.source_device_id, ram, cpu, gpu,
                                      traf, bat, wifi, ls);
}

bool DecisionPipeline::enrich(Event& event, uint32_t now) {
    if (event.type == EventType::TelemetryUpdate) {
        return handleTelemetryUpdate(event, now);
    }
    if (event.type == EventType::Heartbeat) {
        heartbeat_.update(event.source_device_id, event.timestamp);
        if (registry_ != nullptr) {
            const Device* d = registry_->getDevice(event.source_device_id);
            if (d != nullptr) {
                Device copy = *d;
                copy.last_seen = event.timestamp;
                registry_->updateDevice(event.source_device_id, copy);
            }
        }
    }
    return true;
}

bool DecisionPipeline::payloadIs(const Event& event, const char* tag) const {
    if (tag == nullptr || tag[0] == '\0') return false;
    uint8_t i = 0;
    while (tag[i] != '\0') {
        if (event.payload[i] != tag[i]) return false;
        ++i;
    }
    return true;
}

void DecisionPipeline::degradeOnline(const char* id) {
    if (registry_ == nullptr || id == nullptr || id[0] == '\0') return;
    const Device* d = registry_->getDevice(id);
    if (d == nullptr) return;
    if (d->status == DeviceState::Online) {
        (void)registry_->setState(id, DeviceState::Degraded);
    }
}

void DecisionPipeline::applyTimeAnchor(const Event& event) {
    // §17 P14 / §31 / §44: Worker-Zeit nur bei trust_level ≥ 2
    if (registry_ == nullptr) return;
    const Device* d = registry_->getDevice(event.source_device_id);
    if (d == nullptr) return;
    if (d->role != ROLE_WORKER) return;
    if (d->trust_level < 2) return;
    Device copy = *d;
    copy.last_seen = event.timestamp;
    (void)registry_->updateDevice(event.source_device_id, copy);
    heartbeat_.update(event.source_device_id, event.timestamp);
}

void DecisionPipeline::applyFallback(const Event& event) {
    // §20: priority lieferte keine Peer-Route → Fallback
    if (registry_ == nullptr) return;
    const Device* d = registry_->getDevice(event.source_device_id);

    if (event.type == EventType::HeartbeatMiss) {
        if (d != nullptr && d->role == ROLE_WORKER) degradeOnline(d->id);
        if (d != nullptr && d->role == ROLE_SENSOR) degradeOnline(d->id);
    }

    if (event.type == EventType::DeviceLost && payloadIs(event, "mine_silent")) {
        if (d != nullptr && d->role == ROLE_MINE && d->status == DeviceState::Silent) {
            (void)registry_->setState(event.source_device_id, DeviceState::Suspected);
        }
    }

    (void)lastFallback_;
}

bool DecisionPipeline::classify(Event& event) {
    // §18 classify: jede Klassifikation setzt eine Route (und ggf. State).
    classRoute_ = ROLE_KERNEL;
    const Device* d = nullptr;
    if (registry_ != nullptr) {
        d = registry_->getDevice(event.source_device_id);
    }

    switch (event.type) {
        case EventType::Heartbeat:
            if (d != nullptr) classRoute_ = d->role;
            break;
        case EventType::HeartbeatMiss:
            classRoute_ = ROLE_KERNEL;
            break;
        case EventType::DeviceSeen:
            classRoute_ = ROLE_KERNEL;
            break;
        case EventType::DeviceLost:
            if (d != nullptr && d->role == ROLE_MINE) classRoute_ = ROLE_MINE;
            else classRoute_ = ROLE_PHONE;
            break;
        case EventType::BackupWritten:
            classRoute_ = ROLE_SAFE;
            break;
        case EventType::MineEvent:
            classRoute_ = ROLE_MINE;
            break;
        case EventType::AlertSent:
            classRoute_ = ROLE_PHONE;
            break;
        case EventType::AnomalyDetected:
            classRoute_ = ROLE_WORKER;
            if (d != nullptr && d->role == ROLE_WORKER) {
                degradeOnline(d->id);
            }
            break;
        case EventType::ConfigChange:
            classRoute_ = ROLE_KERNEL;
            if (payloadIs(event, "time_drift")) applyTimeAnchor(event);
            break;
        case EventType::PolicyViolation:
            classRoute_ = ROLE_KERNEL;
            if (payloadIs(event, "mine_replay")) {
                if (registry_ != nullptr) {
                    replay_.blockMine(event.source_device_id, *registry_);
                } else {
                    replay_.blockMine(event.source_device_id);
                }
            }
            break;
        case EventType::GhostDownStart:
            classRoute_ = ROLE_SAFE;
            break;
        case EventType::DangerModeEnter:
            classRoute_ = ROLE_KERNEL;
            break;
        case EventType::TelemetryUpdate:
            classRoute_ = ROLE_KERNEL;
            event.severity = Severity::Info;
            break;
        default:
            if (d != nullptr) classRoute_ = d->role;
            else classRoute_ = ROLE_KERNEL;
            break;
    }
    return true;
}

bool DecisionPipeline::checkPolicies(const Event& event) {
    PolicyAction action = policy_.evaluate(event);
    uint8_t extra = policy_.extraFlags(event);

    if ((extra & PX_DEGRADED) != 0) {
        degradeOnline(event.source_device_id);
    }

    if ((extra & PX_TIME_ANCHOR) != 0) {
        applyTimeAnchor(event);
    }

    if ((extra & PX_CHECK_MINE) != 0 && registry_ != nullptr) {
        const Device* d = registry_->getDevice(event.source_device_id);
        if (d != nullptr && d->role == ROLE_MINE && d->status == DeviceState::Silent) {
            (void)registry_->setState(event.source_device_id, DeviceState::Suspected);
        }
    }

    if (action == PolicyAction::Block) {
        if (payloadIs(event, "mine_replay") || event.type == EventType::MineEvent) {
            if (registry_ != nullptr) {
                replay_.blockMine(event.source_device_id, *registry_);
            } else {
                replay_.blockMine(event.source_device_id);
            }
        }
        if (payloadIs(event, "sensor_spam") && registry_ != nullptr) {
            const Device* d = registry_->getDevice(event.source_device_id);
            if (d != nullptr &&
                (d->role == ROLE_SENSOR || d->role == ROLE_PHONE ||
                 d->role == ROLE_ROUTER)) {
                if (d->status == DeviceState::Online ||
                    d->status == DeviceState::Degraded) {
                    (void)registry_->setState(event.source_device_id, DeviceState::Silent);
                }
                (void)registry_->blockDevice(event.source_device_id);
            }
        }
        // §17 P04: Block + snapshot → Event muss ins Vault, dann stop.
        if ((extra & PX_SNAPSHOT) != 0) return true;
        if ((extra & PX_ALERT) == 0) return false;
    }

    if (action == PolicyAction::GhostDown) {
        return false;
    }

    return true;
}

bool DecisionPipeline::computePriority(const Event& event) {
    uint8_t role = ROLE_KERNEL;
    uint8_t trust = 0;
    bool online = false;
    bool hbOk = false;
    const Device* d = nullptr;

    if (registry_ != nullptr) {
        d = registry_->getDevice(event.source_device_id);
        if (d != nullptr) {
            role = d->role;
            trust = d->trust_level;
            online = (d->status == DeviceState::Online);
            hbOk = heartbeat_.isAlive(event.source_device_id, event.timestamp);
        }
    }

    if (event.type == EventType::HeartbeatMiss) hbOk = false;

    lastPriority_ = priority_.compute(role, trust, online, hbOk);
    priorityHasPeer_ = (lastPriority_ == Priority::Worker ||
                        lastPriority_ == Priority::Phone ||
                        lastPriority_ == Priority::Nas);

    // §17 P08 / §20: Heartbeat-Miss → Worker degraded, wenn compute keine Worker-Route gibt
    if (event.type == EventType::HeartbeatMiss &&
        d != nullptr &&
        d->role == ROLE_WORKER &&
        lastPriority_ != Priority::Worker) {
        degradeOnline(d->id);
    }

    return true;
}

bool DecisionPipeline::computeFallback(const Event& event) {
    bool workerOnline = false;
    bool phoneOnline = false;
    bool nasOnline = false;

    if (registry_ != nullptr) {
        const Device* w = registry_->findByRole(ROLE_WORKER);
        const Device* p = registry_->findByRole(ROLE_PHONE);
        const Device* n = registry_->findByRole(ROLE_SAFE);
        workerOnline = (w != nullptr && w->status == DeviceState::Online);
        phoneOnline = (p != nullptr && p->status == DeviceState::Online);
        nasOnline = (n != nullptr && n->status == DeviceState::Online);
    }

    lastFallback_ = fallback_.resolve(workerOnline, phoneOnline, nasOnline);

    if (!priorityHasPeer_) {
        applyFallback(event);
    }

    (void)nasOnline;
    return true;
}

bool DecisionPipeline::computeEscalation(const Event& event) {
    (void)event;
    return true;
}

bool DecisionPipeline::route(const Event& event, uint32_t now) {
    // §17 Alert → Worker/Phone-Link, Backup → Safe-Link. Keine neuen Peers.
    (void)classRoute_;
    if (transport_ == nullptr) return true;
    return transport_->route(event, now);
}

bool DecisionPipeline::store(const Event& event, uint32_t now) {
    if (vault_ == nullptr) return false;
    Event work = event;
    (void)vault_->signEvent(work);
    return vault_->store(work, now);
}

bool DecisionPipeline::ack(const Event& event, uint32_t now) {
    if (event.type == EventType::MineEvent) return true;
    if (transport_ == nullptr) return true;
    return transport_->sendAck(event, now);
}

void DecisionPipeline::parseMinePayload(const char* payload,
                                        uint32_t& counter,
                                        uint32_t& totp) const {
    counter = 0;
    totp = 0;

    if (payload == nullptr || payload[0] == '\0') return;

    uint8_t i = 0;
    while (i < 127 && payload[i] >= '0' && payload[i] <= '9') {
        if (counter > (0xFFFFFFFFu / 10u)) return;
        counter = counter * 10u + static_cast<uint32_t>(payload[i] - '0');
        ++i;
    }

    if (i >= 127 || payload[i] != ':') return;
    ++i;

    while (i < 127 && payload[i] >= '0' && payload[i] <= '9') {
        if (totp > (0xFFFFFFFFu / 10u)) return;
        totp = totp * 10u + static_cast<uint32_t>(payload[i] - '0');
        ++i;
    }
}
