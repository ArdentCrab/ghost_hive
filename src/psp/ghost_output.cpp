#include "ghost_output.h"
#include "ghost_arm.h"

GhostOutput::GhostOutput() : level_(OutputLevel::Normal) {
}

void GhostOutput::setLevel(OutputLevel level) {
    level_ = level;
}

OutputLevel GhostOutput::level() const {
    return level_;
}

void GhostOutput::append(char* buffer, const char* text) {
    if (buffer == nullptr || text == nullptr) return;

    uint16_t pos = 0;
    while (buffer[pos] != '\0') ++pos;

    uint16_t i = 0;
    while (text[i] != '\0' && pos < (OUTPUT_BUFFER_LEN - 1)) {
        buffer[pos++] = text[i++];
    }
    buffer[pos] = '\0';
}

void GhostOutput::appendNumber(char* buffer, int32_t value) {
    if (value < 0) {
        append(buffer, "-");
        if (value == static_cast<int32_t>(0x80000000)) {
            appendU32(buffer, 2147483648u);
            return;
        }
        value = -value;
    }
    appendU32(buffer, static_cast<uint32_t>(value));
}

void GhostOutput::appendU32(char* buffer, uint32_t value) {
    char tmp[11];
    uint8_t len = 0;

    if (value == 0) {
        append(buffer, "0");
        return;
    }

    while (value > 0 && len < 10) {
        tmp[len++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }

    for (uint8_t i = len; i > 0; --i) {
        char c[2] = {tmp[i - 1], '\0'};
        append(buffer, c);
    }
}

void GhostOutput::appendHex(char* buffer, uint32_t value) {
    char tmp[9];
    const char* hex = "0123456789ABCDEF";

    for (uint8_t i = 0; i < 8; ++i) {
        tmp[7 - i] = hex[value & 0xF];
        value >>= 4;
    }
    tmp[8] = '\0';
    append(buffer, tmp);
}

void GhostOutput::appendEventType(char* buffer, EventType type) {
    switch (type) {
        case EventType::ScanResult: append(buffer, "ScanResult"); break;
        case EventType::DeviceSeen: append(buffer, "DeviceSeen"); break;
        case EventType::DeviceLost: append(buffer, "DeviceLost"); break;
        case EventType::ProfileUpdate: append(buffer, "ProfileUpdate"); break;
        case EventType::AnomalyDetected: append(buffer, "AnomalyDetected"); break;
        case EventType::PolicyViolation: append(buffer, "PolicyViolation"); break;
        case EventType::BackupWritten: append(buffer, "BackupWritten"); break;
        case EventType::AlertSent: append(buffer, "AlertSent"); break;
        case EventType::Heartbeat: append(buffer, "Heartbeat"); break;
        case EventType::HeartbeatMiss: append(buffer, "HeartbeatMiss"); break;
        case EventType::RoleChange: append(buffer, "RoleChange"); break;
        case EventType::ConfigChange: append(buffer, "ConfigChange"); break;
        case EventType::GhostDownStart: append(buffer, "GhostDownStart"); break;
        case EventType::GhostDownEnd: append(buffer, "GhostDownEnd"); break;
        case EventType::PeekScan: append(buffer, "PeekScan"); break;
        case EventType::DangerModeEnter: append(buffer, "DangerModeEnter"); break;
        case EventType::DangerModeExit: append(buffer, "DangerModeExit"); break;
        case EventType::MineEvent: append(buffer, "MineEvent"); break;
        case EventType::TelemetryUpdate: append(buffer, "TelemetryUpdate"); break;
        default: append(buffer, "?"); break;
    }
}

void GhostOutput::kv(char* buffer, const char* key, const char* val) {
    append(buffer, key);
    uint8_t n = 0;
    if (key != nullptr) {
        while (key[n] != '\0') ++n;
    }
    while (n < 9) {
        append(buffer, " ");
        ++n;
    }
    if (val != nullptr) append(buffer, val);
    append(buffer, "\n");
}

void GhostOutput::wlab(char* buffer, const char* lab, const char* val) {
    char line[49];
    uint8_t n = 0;
    uint8_t i = 0;
    if (lab != nullptr) {
        while (lab[i] != '\0' && n < 48) line[n++] = lab[i++];
    }
    if (n < 48) line[n++] = ':';
    if (n < 48) line[n++] = ' ';
    i = 0;
    if (val != nullptr) {
        while (val[i] != '\0' && n < 48) line[n++] = val[i++];
    }
    line[n] = '\0';
    append(buffer, line);
    append(buffer, "\n");
}

void GhostOutput::wlabn(char* buffer, const char* lab, int32_t val) {
    char num[12];
    uint8_t n = 0;
    uint32_t v;
    if (val < 0) {
        num[n++] = '-';
        v = static_cast<uint32_t>(-val);
    } else {
        v = static_cast<uint32_t>(val);
    }
    if (v == 0) num[n++] = '0';
    else {
        char dig[11];
        uint8_t d = 0;
        while (v > 0 && d < 10) {
            dig[d++] = static_cast<char>('0' + (v % 10));
            v /= 10;
        }
        while (d > 0) num[n++] = dig[--d];
    }
    num[n] = '\0';
    wlab(buffer, lab, num);
}

void GhostOutput::wraw(char* buffer, const char* line) {
    char tmp[49];
    uint8_t n = 0;
    if (line != nullptr) {
        while (line[n] != '\0' && n < 48) {
            tmp[n] = line[n];
            ++n;
        }
    }
    tmp[n] = '\0';
    append(buffer, tmp);
    append(buffer, "\n");
}

void GhostOutput::kvn(char* buffer, const char* key, int32_t val) {
    append(buffer, key);
    uint8_t n = 0;
    if (key != nullptr) {
        while (key[n] != '\0') ++n;
    }
    while (n < 9) {
        append(buffer, " ");
        ++n;
    }
    appendNumber(buffer, val);
    append(buffer, "\n");
}

void GhostOutput::cell(char* buffer, const char* text, uint8_t width) {
    uint8_t n = 0;
    if (text != nullptr) {
        while (text[n] != '\0' && n < width) {
            char c[2] = {text[n], '\0'};
            append(buffer, c);
            ++n;
        }
    }
    while (n < width) {
        append(buffer, " ");
        ++n;
    }
}

void GhostOutput::cellN(char* buffer, int32_t value, uint8_t width) {
    char tmp[12];
    uint8_t n = 0;
    uint32_t v;
    if (value < 0) {
        tmp[n++] = '-';
        if (value == static_cast<int32_t>(0x80000000)) v = 2147483648u;
        else v = static_cast<uint32_t>(-value);
    } else {
        v = static_cast<uint32_t>(value);
    }
    if (v == 0) tmp[n++] = '0';
    else {
        char dig[11];
        uint8_t d = 0;
        while (v > 0 && d < 10) {
            dig[d++] = static_cast<char>('0' + (v % 10));
            v /= 10;
        }
        while (d > 0) tmp[n++] = dig[--d];
    }
    tmp[n] = '\0';
    cell(buffer, tmp, width);
}

const char* GhostOutput::roleName(uint8_t role) const {
    switch (role) {
        case ROLE_WORKER: return "worker";
        case ROLE_PHONE: return "phone";
        case ROLE_KERNEL: return "kernel";
        case ROLE_SAFE: return "safe";
        case ROLE_SENSOR: return "sensor";
        case ROLE_ROUTER: return "router";
        case ROLE_MINE: return "mine";
        default: return "?";
    }
}

const char* GhostOutput::stateName(DeviceState state) const {
    switch (state) {
        case DeviceState::Online: return "online";
        case DeviceState::Degraded: return "degraded";
        case DeviceState::Offline: return "offline";
        case DeviceState::Unknown: return "unknown";
        case DeviceState::Suspected: return "suspect";
        case DeviceState::Blocked: return "blocked";
        case DeviceState::Pending: return "pending";
        case DeviceState::GhostDown: return "down";
        case DeviceState::DangerMode: return "danger";
        case DeviceState::Silent: return "silent";
        default: return "?";
    }
}

const char* GhostOutput::actionName(PolicyAction action) const {
    switch (action) {
        case PolicyAction::LogOnly: return "log";
        case PolicyAction::Alert: return "alert";
        case PolicyAction::Backup: return "backup";
        case PolicyAction::Block: return "block";
        case PolicyAction::Kill: return "kill";
        case PolicyAction::GhostDown: return "gdown";
        default: return "?";
    }
}

const char* GhostOutput::encName(uint8_t enc) const {
    switch (enc) {
        case 0: return "open";
        case 1: return "wep";
        case 2: return "wpa";
        case 3: return "wpa2";
        case 4: return "wpa3";
        default: return "?";
    }
}

void GhostOutput::appendRole(char* buffer, uint8_t role) {
    append(buffer, roleName(role));
}

void GhostOutput::appendState(char* buffer, DeviceState state) {
    append(buffer, stateName(state));
}

void GhostOutput::appendAction(char* buffer, PolicyAction action) {
    append(buffer, actionName(action));
}

void GhostOutput::buildStatus(char* buffer) {
    buffer[0] = '\0';
    kv(buffer, "spec", "1.7.3");
    kv(buffer, "kernel", "psp");
    kv(buffer, "mode", "terminal");
}

void GhostOutput::buildStatus(char* buffer,
                              Registry& registry,
                              EventQueue& queue,
                              GhostVault& vault,
                              GhostStealth& stealth) {
    buffer[0] = '\0';
    kv(buffer, "spec", "1.7.3");
    kv(buffer, "radio", stealth.isGameMode() ? "off" : "on");
    kv(buffer, "game", stealth.isGameMode() ? "1" : "0");
    append(buffer, "devices  ");
    appendNumber(buffer, registry.getDeviceCount());
    append(buffer, "/");
    appendNumber(buffer, MAX_DEVICES);
    append(buffer, "\n");
    append(buffer, "queue    ");
    appendNumber(buffer, queue.getSize());
    append(buffer, "/");
    appendNumber(buffer, EVENT_QUEUE_SIZE);
    append(buffer, "\n");
    append(buffer, "vault    ");
    appendNumber(buffer, vault.getStoredCount());
    append(buffer, "/");
    appendNumber(buffer, VAULT_RAM_SLOTS);
    append(buffer, "\n");
    append(buffer, "crc      ");
    appendHex(buffer, vault.checksum());
    append(buffer, "\n");
    kv(buffer, "flush", vault.lastFlushOk() ? "ok" : "wait");
    kv(buffer, "safe", vault.safeMode() ? "1" : "0");
}

void GhostOutput::buildDevices(Registry& registry, char* buffer) {
    buffer[0] = '\0';
    appendNumber(buffer, registry.getDeviceCount());
    append(buffer, "/");
    appendNumber(buffer, MAX_DEVICES);
    append(buffer, "  wk ");
    appendNumber(buffer, registry.getRoleCount(ROLE_WORKER));
    append(buffer, " se ");
    appendNumber(buffer, registry.getRoleCount(ROLE_SENSOR));
    append(buffer, " sf ");
    appendNumber(buffer, registry.getRoleCount(ROLE_SAFE));
    append(buffer, " rt ");
    appendNumber(buffer, registry.getRoleCount(ROLE_ROUTER));
    append(buffer, " mn ");
    appendNumber(buffer, registry.getRoleCount(ROLE_MINE));
    append(buffer, "\n");
    cell(buffer, "id", 16);
    cell(buffer, "role", 8);
    cell(buffer, "state", 10);
    append(buffer, "\n");
    uint8_t count = registry.getDeviceCount();
    for (uint8_t i = 0; i < count; ++i) {
        DeviceInfo info = registry.getDeviceInfo(i);
        if (info.id[0] == '\0') continue;
        cell(buffer, info.id, 16);
        cell(buffer, roleName(info.role), 8);
        cell(buffer, stateName(static_cast<DeviceState>(info.status)), 10);
        if (level_ != OutputLevel::Normal) {
            appendU32(buffer, info.lastSeen);
        }
        append(buffer, "\n");
    }
}

void GhostOutput::buildEvents(EventQueue& queue, char* buffer) {
    buffer[0] = '\0';
    kvn(buffer, "queued", queue.getSize());
    cell(buffer, "type", 16);
    cell(buffer, "src", 12);
    cell(buffer, "sev", 4);
    append(buffer, "\n");
    uint8_t n = queue.getSize();
    for (uint8_t i = 0; i < n; ++i) {
        const Event* ev = queue.peek(i);
        if (ev == nullptr) continue;
        uint16_t mark = 0;
        while (buffer[mark] != '\0') ++mark;
        appendEventType(buffer, ev->type);
        uint16_t after = mark;
        while (buffer[after] != '\0') ++after;
        uint8_t used = static_cast<uint8_t>(after - mark);
        while (used < 16) {
            append(buffer, " ");
            ++used;
        }
        cell(buffer, ev->source_device_id, 12);
        cellN(buffer, static_cast<int32_t>(ev->severity), 4);
        if (level_ == OutputLevel::Trace && ev->payload[0] != '\0') {
            append(buffer, ev->payload);
        }
        append(buffer, "\n");
    }
}

void GhostOutput::buildHeartbeat(GhostHeartbeat& heartbeat,
                                const char* deviceId,
                                char* buffer) {
    buffer[0] = '\0';
    HeartbeatInfo info = heartbeat.getInfo(deviceId);
    if (deviceId != nullptr && deviceId[0] != '\0') kv(buffer, "id", deviceId);
    append(buffer, "last     ");
    appendU32(buffer, info.lastBeat);
    append(buffer, "\n");
    kvn(buffer, "miss", info.missCount);
}

void GhostOutput::buildHeartbeat(GhostHeartbeat& heartbeat,
                                Registry& registry,
                                char* buffer) {
    buffer[0] = '\0';
    kvn(buffer, "tracked", heartbeat.trackedCount());
    cell(buffer, "id", 12);
    cell(buffer, "last", 10);
    cell(buffer, "miss", 5);
    cell(buffer, "state", 10);
    append(buffer, "\n");
    uint8_t n = heartbeat.trackedCount();
    for (uint8_t i = 0; i < n; ++i) {
        const char* id = heartbeat.idAt(i);
        if (id == nullptr) continue;
        HeartbeatInfo info = heartbeat.getInfo(id);
        cell(buffer, id, 12);
        cellN(buffer, static_cast<int32_t>(info.lastBeat), 10);
        cellN(buffer, info.missCount, 5);
        const Device* d = registry.getDevice(id);
        cell(buffer, d != nullptr ? stateName(d->status) : "-", 10);
        append(buffer, "\n");
    }
}

void GhostOutput::buildVault(GhostVault& vault, char* buffer) {
    buffer[0] = '\0';
    append(buffer, "entries  ");
    appendNumber(buffer, vault.getStoredCount());
    append(buffer, "/");
    appendNumber(buffer, VAULT_RAM_SLOTS);
    append(buffer, "\n");
    kv(buffer, "full", vault.isFull() ? "1" : "0");
    append(buffer, "crc      ");
    appendHex(buffer, vault.checksum());
    append(buffer, "\n");
    append(buffer, "flushat  ");
    appendU32(buffer, vault.lastFlushAt());
    append(buffer, "\n");
    kv(buffer, "flushok", vault.lastFlushOk() ? "1" : "0");
    kv(buffer, "safemode", vault.safeMode() ? "1" : "0");
    cell(buffer, "type", 16);
    cell(buffer, "src", 10);
    cell(buffer, "H", 2);
    cell(buffer, "T", 2);
    append(buffer, "\n");
    uint8_t n = vault.getStoredCount();
    if (n > 12) n = 12;
    for (uint8_t i = 0; i < n; ++i) {
        Event plain{};
        if (!vault.copyPlain(i, &plain)) continue;
        uint16_t mark = 0;
        while (buffer[mark] != '\0') ++mark;
        appendEventType(buffer, plain.type);
        uint16_t after = mark;
        while (buffer[after] != '\0') ++after;
        uint8_t used = static_cast<uint8_t>(after - mark);
        while (used < 16) {
            append(buffer, " ");
            ++used;
        }
        cell(buffer, plain.source_device_id, 10);
        char h[2] = {vault.hmacMark(i), '\0'};
        cell(buffer, h, 2);
        char t[2] = {vault.totpMark(i), '\0'};
        cell(buffer, t, 2);
        append(buffer, "\n");
    }
}

void GhostOutput::buildPolicy(GhostPolicy& policy, char* buffer) {
    buffer[0] = '\0';
    cell(buffer, "id", 5);
    cell(buffer, "act", 7);
    cell(buffer, "name", 20);
    append(buffer, "\n");
    uint8_t n = policy.ruleCount();
    for (uint8_t i = 0; i < n; ++i) {
        const PolicyRule* rule = policy.ruleAt(i);
        if (rule == nullptr) continue;
        cell(buffer, rule->id, 5);
        cell(buffer, actionName(rule->action), 7);
        cell(buffer, rule->name, 20);
        append(buffer, "\n");
    }
}

void GhostOutput::buildPolicyView(GhostPolicy& policy, char* buffer) {
    buildPolicy(policy, buffer);
}

void GhostOutput::buildPolicies(char* buffer) {
    GhostPolicy policy;
    policy.initDefaults();
    buildPolicy(policy, buffer);
}

void GhostOutput::buildMines(ReplayGuard& guard, char* buffer) {
    buffer[0] = '\0';
    cell(buffer, "id", 12);
    cell(buffer, "ctr", 10);
    cell(buffer, "blk", 4);
    append(buffer, "\n");
    uint8_t n = guard.trackedCount();
    for (uint8_t i = 0; i < n; ++i) {
        const char* id = guard.mineIdAt(i);
        if (id == nullptr) continue;
        cell(buffer, id, 12);
        cellN(buffer, static_cast<int32_t>(guard.lastCounterAt(i)), 10);
        cellN(buffer, guard.blockedAt(i) ? 1 : 0, 4);
        append(buffer, "\n");
    }
}

void GhostOutput::buildReplay(ReplayGuard& guard, char* buffer) {
    buffer[0] = '\0';
    kvn(buffer, "window", REPLAY_WINDOW_PER_MINE);
    cell(buffer, "id", 12);
    cell(buffer, "last", 10);
    cell(buffer, "blk", 4);
    append(buffer, "\n");
    uint8_t n = guard.trackedCount();
    for (uint8_t i = 0; i < n; ++i) {
        const char* id = guard.mineIdAt(i);
        if (id == nullptr) continue;
        cell(buffer, id, 12);
        cellN(buffer, static_cast<int32_t>(guard.lastCounterAt(i)), 10);
        cellN(buffer, guard.blockedAt(i) ? 1 : 0, 4);
        append(buffer, "\n");
    }
}

void GhostOutput::buildScan(GhostScanner& scanner, char* buffer) {
    buffer[0] = '\0';
    kvn(buffer, "aps", scanner.getWifiCount());
    kv(buffer, "block", scanner.lastScanBlocked() ? "1" : "0");
    cell(buffer, "ssid", 22);
    cell(buffer, "rssi", 5);
    cell(buffer, "ch", 3);
    cell(buffer, "enc", 5);
    append(buffer, "\n");
    uint8_t count = scanner.getWifiCount();
    for (uint8_t i = 0; i < count; ++i) {
        const WifiNetwork* net = scanner.getWifi(i);
        if (net == nullptr) continue;
        cell(buffer, net->ssid, 22);
        cellN(buffer, net->rssi, 5);
        cellN(buffer, net->channel, 3);
        cell(buffer, encName(net->encryption), 5);
        append(buffer, "\n");
    }
}

void GhostOutput::buildBackup(char* buffer) {
    buffer[0] = '\0';
    kv(buffer, "vault", "(detached)");
}

void GhostOutput::buildAlert(char* buffer) {
    buffer[0] = '\0';
    kv(buffer, "kind", "test");
    kv(buffer, "sink", "local");
    kv(buffer, "state", "fired");
}

void GhostOutput::buildStealth(GhostStealth& stealth, char* buffer) {
    buffer[0] = '\0';
    StealthInfo info = stealth.getInfo();
    kv(buffer, "game", info.gameMode ? "1" : "0");
    kv(buffer, "radio", info.gameMode ? "off" : "on");
}

void GhostOutput::buildPeek(GhostPeek& peek, char* buffer) {
    buffer[0] = '\0';
    cell(buffer, "id", 14);
    cell(buffer, "st", 4);
    cell(buffer, "ev", 10);
    append(buffer, "\n");
    uint8_t count = peek.getMineCount();
    for (uint8_t i = 0; i < count; ++i) {
        const MineInfo* mine = peek.getMine(i);
        if (mine == nullptr) continue;
        if (mine->mine_id[0] != '\0') cell(buffer, mine->mine_id, 14);
        else cellN(buffer, i, 14);
        cellN(buffer, mine->status, 4);
        cellN(buffer, static_cast<int32_t>(mine->lastEvent), 10);
        append(buffer, "\n");
    }
}

void GhostOutput::buildMineCheck(GhostPeek& peek, char* buffer) {
    buffer[0] = '\0';
    cell(buffer, "id", 12);
    cell(buffer, "sil", 4);
    cell(buffer, "blk", 4);
    cell(buffer, "rep", 4);
    cell(buffer, "trip", 4);
    append(buffer, "\n");
    uint8_t count = peek.getMineCount();
    for (uint8_t i = 0; i < count; ++i) {
        const MineInfo* mine = peek.getMine(i);
        if (mine == nullptr) continue;
        if (mine->mine_id[0] != '\0') cell(buffer, mine->mine_id, 12);
        else cellN(buffer, i, 12);
        cellN(buffer, mine->status == 0 ? 1 : 0, 4);
        cellN(buffer, mine->status == 1 ? 1 : 0, 4);
        cellN(buffer, mine->status == 2 ? 1 : 0, 4);
        cellN(buffer, mine->status == 3 ? 1 : 0, 4);
        append(buffer, "\n");
    }
}

void GhostOutput::buildDanger(char* buffer) {
    buffer[0] = '\0';
    kv(buffer, "psp", "passive");
    kv(buffer, "mines", "evaluate");
    kv(buffer, "phone", "minimal");
    kv(buffer, "laptop", "local");
    kv(buffer, "nas", "read-only");
    kv(buffer, "router", "informed");
    kv(buffer, "enroll", "off");
    kv(buffer, "backup", "manual");
    kv(buffer, "scan", "passive");
    kv(buffer, "replay", "on");
}

void GhostOutput::buildMineBlock(char* buffer) {
    buildMineBlock(0, buffer);
}

void GhostOutput::buildMineBlock(uint8_t blocked, char* buffer) {
    buffer[0] = '\0';
    kv(buffer, "action", "block");
    kv(buffer, "guard", "replay 32");
    append(buffer, "n        ");
    appendU32(buffer, blocked);
    append(buffer, "\n");
}

void GhostOutput::buildTime(uint32_t nowSec, char* buffer) {
    buffer[0] = '\0';
    append(buffer, "unix     ");
    appendU32(buffer, nowSec);
    append(buffer, "\n");
    kv(buffer, "anchor", "kernel");
    kv(buffer, "totp", "60-120s");
}

static uint32_t downRemainMs(uint32_t due_ms, uint32_t now_ms) {
    if (now_ms >= due_ms) return 0;
    return due_ms - now_ms;
}

static const char* downPhaseToken(DownStep step) {
    switch (step) {
        case DownStep::Idle: return "idle";
        case DownStep::RamSnapshot: return "ram";
        case DownStep::FinalSnapshot: return "final";
        case DownStep::NasFlushWait: return "nas";
        case DownStep::Kill: return "kill";
        case DownStep::StealthConceal: return "conceal";
        case DownStep::Stop: return "stop";
        case DownStep::StorageFlushWait: return "stor";
        case DownStep::StorageFlushRetry: return "retry";
        case DownStep::Done: return "done";
        default: return "?";
    }
}

void GhostOutput::buildGhostDown(char* buffer, const GhostDown& down,
                                 uint32_t now_ms, bool game) {
    buffer[0] = '\0';
    const bool active = down.isActive();
    GhostArming a = ghost_arming();
    if (active) kv(buffer, "down", "active");
    else if (a == GhostArming::Observe) kv(buffer, "down", "observe");
    else if (a == GhostArming::Armed) kv(buffer, "down", "idle");
    else kv(buffer, "down", "locked");

    kv(buffer, "phase", downPhaseToken(down.step()));

    DownStep st = down.step();
    if (!active) kv(buffer, "timer", "--");
    else if (st == DownStep::NasFlushWait) {
        kvn(buffer, "timer", static_cast<int32_t>(
            downRemainMs(down.nasDueMs(), now_ms)));
    } else if (st == DownStep::StorageFlushWait ||
               st == DownStep::StorageFlushRetry) {
        kvn(buffer, "timer", static_cast<int32_t>(
            downRemainMs(down.storageDueMs(), now_ms)));
    } else {
        kvn(buffer, "timer", 0);
    }

    kvn(buffer, "snap", down.snapshotCount());

    if (!active) kv(buffer, "nas", "--");
    else if (down.nasFlushTimedOut()) kv(buffer, "nas", "to");
    else if (st == DownStep::NasFlushWait) kv(buffer, "nas", "wait");
    else kv(buffer, "nas", "ok");

    if (!active) kv(buffer, "stor", "--");
    else if (down.storageFlushDone()) kv(buffer, "stor", "ok");
    else if (st == DownStep::StorageFlushRetry) kv(buffer, "stor", "retry");
    else kv(buffer, "stor", "wait");

    kv(buffer, "kill", down.killSent() ? "1" : "0");
    kv(buffer, "game", game ? "1" : "0");
    if (a == GhostArming::Observe) kv(buffer, "arm", "observe");
    else if (a == GhostArming::Armed) kv(buffer, "arm", "armed");
    else kv(buffer, "arm", "locked");
}

