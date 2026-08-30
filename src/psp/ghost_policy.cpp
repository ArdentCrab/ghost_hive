#include "ghost_policy.h"

GhostPolicy::GhostPolicy() : ruleCount_(0) {
    init();
}

void GhostPolicy::init() {
    initDefaults();
}

void GhostPolicy::initDefaults() {
    // §16 DSL + §17 Verbundaktionen
    rules_[0]  = {"P01", "unknown_device",        0, "type=DeviceSeen AND payload=unknown_device", PolicyAction::LogOnly, 0};
    rules_[1]  = {"P02", "critical_device_lost",  0, "type=DeviceLost AND severity=Critical", PolicyAction::Alert, PX_VAULT};
    rules_[2]  = {"P03", "worker_degraded",       1, "type=HeartbeatMiss AND payload=worker_degraded", PolicyAction::Backup, 0};
    rules_[3]  = {"P04", "sensor_spam",           0, "type=PolicyViolation AND payload=sensor_spam", PolicyAction::Block, PX_SNAPSHOT};
    rules_[4]  = {"P05", "nas_full",              1, "type=BackupWritten AND payload=nas_full", PolicyAction::Backup, PX_VAULT};
    rules_[5]  = {"P06", "router_new_client",     1, "type=DeviceSeen AND payload=router_new_client", PolicyAction::LogOnly, PX_CLASSIFY};
    rules_[6]  = {"P07", "ir_signal_unknown",     0, "type=ScanResult AND payload=ir_signal_unknown", PolicyAction::LogOnly, PX_CLASSIFY};
    rules_[7]  = {"P08", "heartbeat_miss_worker", 1, "type=HeartbeatMiss", PolicyAction::Alert, PX_DEGRADED};
    rules_[8]  = {"P09", "ghost_down_enter",      2, "type=GhostDownStart", PolicyAction::Backup, PX_SNAPSHOT | PX_VAULT};
    rules_[9]  = {"P10", "danger_mode_enter",     2, "type=DangerModeEnter", PolicyAction::LogOnly, PX_PASSIVE};
    rules_[10] = {"P11", "mine_event_critical",   0, "type=MineEvent AND severity=Critical", PolicyAction::Alert, PX_SNAPSHOT};
    rules_[11] = {"P12", "mine_silent_too_long",  0, "type=DeviceLost AND payload=mine_silent", PolicyAction::LogOnly, PX_CHECK_MINE};
    rules_[12] = {"P13", "mine_replay_detected",  0, "type=PolicyViolation AND payload=mine_replay", PolicyAction::Block, PX_ALERT};
    rules_[13] = {"P14", "time_drift_detected",   0, "type=ConfigChange AND payload=time_drift", PolicyAction::LogOnly, PX_TIME_ANCHOR};
    rules_[14] = {"P15", "hmac_invalid",          0, "type=PolicyViolation AND payload=hmac_i", PolicyAction::Alert, 0};
    rules_[15] = {"P16", "telemetry_update",      0, "type=TelemetryUpdate", PolicyAction::LogOnly, 0};
    ruleCount_ = 16;
}

bool GhostPolicy::strEqRange(const char* a, uint8_t aLen, const char* b) {
    uint8_t i = 0;
    while (i < aLen && b[i] != '\0') {
        if (a[i] != b[i]) return false;
        ++i;
    }
    return i == aLen && b[i] == '\0';
}

bool GhostPolicy::matchExpr(const char* expr, uint8_t len, const Event& event) const {
    uint8_t s = 0;
    while (s < len && expr[s] == ' ') ++s;
    uint8_t e = len;
    while (e > s && expr[e - 1] == ' ') --e;
    if (e <= s) return false;

    uint8_t op = 0;
    bool ne = false;
    for (uint8_t i = s; i + 1 < e; ++i) {
        if (expr[i] == '!' && expr[i + 1] == '=') {
            op = i;
            ne = true;
            break;
        }
        if (expr[i] == '=') {
            op = i;
            break;
        }
    }
    if (op == 0) return false;

    uint8_t fieldEnd = op;
    while (fieldEnd > s && expr[fieldEnd - 1] == ' ') --fieldEnd;
    uint8_t valStart = static_cast<uint8_t>(op + (ne ? 2 : 1));
    while (valStart < e && expr[valStart] == ' ') ++valStart;

    const char* field = expr + s;
    uint8_t fieldLen = static_cast<uint8_t>(fieldEnd - s);
    const char* value = expr + valStart;
    uint8_t valueLen = static_cast<uint8_t>(e - valStart);

    bool eq = false;

    if (strEqRange(field, fieldLen, "type")) {
        const char* name = nullptr;
        switch (event.type) {
            case EventType::ScanResult: name = "ScanResult"; break;
            case EventType::DeviceSeen: name = "DeviceSeen"; break;
            case EventType::DeviceLost: name = "DeviceLost"; break;
            case EventType::Heartbeat: name = "Heartbeat"; break;
            case EventType::HeartbeatMiss: name = "HeartbeatMiss"; break;
            case EventType::PolicyViolation: name = "PolicyViolation"; break;
            case EventType::BackupWritten: name = "BackupWritten"; break;
            case EventType::GhostDownStart: name = "GhostDownStart"; break;
            case EventType::DangerModeEnter: name = "DangerModeEnter"; break;
            case EventType::MineEvent: name = "MineEvent"; break;
            case EventType::ConfigChange: name = "ConfigChange"; break;
            case EventType::TelemetryUpdate: name = "TelemetryUpdate"; break;
            default: name = ""; break;
        }
        eq = strEqRange(value, valueLen, name);
    } else if (strEqRange(field, fieldLen, "severity")) {
        const char* name = nullptr;
        switch (event.severity) {
            case Severity::Info: name = "Info"; break;
            case Severity::Warn: name = "Warn"; break;
            case Severity::High: name = "High"; break;
            case Severity::Critical: name = "Critical"; break;
            default: name = ""; break;
        }
        eq = strEqRange(value, valueLen, name);
    } else if (strEqRange(field, fieldLen, "payload")) {
        uint8_t i = 0;
        while (i < valueLen && event.payload[i] == value[i]) ++i;
        eq = (i == valueLen);
    } else {
        return false;
    }

    return ne ? !eq : eq;
}

bool GhostPolicy::matchDsl(const char* dsl, const Event& event) const {
    if (dsl == nullptr || dsl[0] == '\0') return false;

    uint8_t start = 0;
    uint8_t i = 0;
    while (dsl[i] != '\0' && i < 128) {
        if (dsl[i] == 'A' && dsl[i + 1] == 'N' && dsl[i + 2] == 'D' &&
            (i == 0 || dsl[i - 1] == ' ') && dsl[i + 3] == ' ') {
            if (!matchExpr(dsl + start, static_cast<uint8_t>(i - start), event)) {
                return false;
            }
            i = static_cast<uint8_t>(i + 4);
            start = i;
            continue;
        }
        ++i;
    }
    return matchExpr(dsl + start, static_cast<uint8_t>(i - start), event);
}

uint8_t GhostPolicy::evaluate(const Event& event, const char* deviceState) const {
    return evaluateFlags(event, deviceState);
}

PolicyAction GhostPolicy::evaluate(const Event& event) const {
    for (uint8_t i = 0; i < ruleCount_; ++i) {
        if (matchDsl(rules_[i].condition, event)) {
            return rules_[i].action;
        }
    }
    return PolicyAction::LogOnly;
}

uint8_t GhostPolicy::evaluateFlags(const Event& event, const char* deviceState) const {
    Event working = event;
    if (deviceState != nullptr) {
        bool unknown = true;
        const char* u = "unknown";
        uint8_t i = 0;
        while (u[i] != '\0') {
            if (deviceState[i] != u[i]) {
                unknown = false;
                break;
            }
            ++i;
        }
        if (unknown && deviceState[i] == '\0' &&
            event.type == EventType::DeviceSeen && working.payload[0] == '\0') {
            const char* tag = "unknown_device";
            i = 0;
            while (tag[i] != '\0' && i < 127) {
                working.payload[i] = tag[i];
                ++i;
            }
            working.payload[i] = '\0';
        }
    }

    uint8_t flags = extraFlags(working);
    PolicyAction action = evaluate(working);
    if (action == PolicyAction::Alert) flags = static_cast<uint8_t>(flags | PX_ALERT);
    if (action == PolicyAction::Backup) flags = static_cast<uint8_t>(flags | PX_VAULT);
    if (action == PolicyAction::Block) flags = static_cast<uint8_t>(flags | PX_ALERT);
    return flags;
}

uint8_t GhostPolicy::extraFlags(const Event& event) const {
    for (uint8_t i = 0; i < ruleCount_; ++i) {
        if (matchDsl(rules_[i].condition, event)) {
            return rules_[i].extra;
        }
    }
    return 0;
}

uint8_t GhostPolicy::ruleCount() const {
    return ruleCount_;
}

const PolicyRule* GhostPolicy::ruleAt(uint8_t index) const {
    if (index >= ruleCount_) return nullptr;
    return &rules_[index];
}
