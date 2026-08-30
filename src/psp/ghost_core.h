#ifndef GHOST_CORE_H
#define GHOST_CORE_H

// =====================================================
// Ghost Hive v1.7.1
// ghost-core
// Spec-Basis: §14, §12.1, §17–§22
// RAM-Budget ghost-core: 256 KB
// =====================================================

#include <stdint.h>

// ---------------------------------------------
// §8 Gerätezustände
// ---------------------------------------------
enum class DeviceState : uint8_t {
    Online,
    Degraded,
    Offline,
    Unknown,
    Suspected,
    Blocked,
    Pending,
    GhostDown,
    DangerMode,
    Silent
};

// ---------------------------------------------
// §6 Rechte (kein Enforcement hier, nur Abbildung)
// ---------------------------------------------
enum class Permission : uint8_t {
    Write,
    Read,
    Execute,
    Decide,
    Configure,
    Backup,
    Alert
};

// ---------------------------------------------
// §13 Capabilities (nur die Spec-Liste)
// ---------------------------------------------
enum class Capability : uint8_t {
    ScanWifi,
    ScanBt,
    ScanIr,
    Analyze,
    StoreLogs,
    Notify,
    Backup,
    Classify,
    RouterEvents,
    IrTx,
    IrRx,
    PassiveMonitor,
    ActiveMonitor,
    Heartbeat,
    HeartbeatReceive,
    Snapshot,
    Kill,
    Peek,
    MineScan,
    MineEvent,
    ReplayGuard
};

// ---------------------------------------------
// §14.2 Event
// ---------------------------------------------
enum class EventType : uint8_t {
    ScanResult,
    DeviceSeen,
    DeviceLost,
    ProfileUpdate,
    AnomalyDetected,
    PolicyViolation,
    BackupWritten,
    AlertSent,
    Heartbeat,
    HeartbeatMiss,
    RoleChange,
    ConfigChange,
    GhostDownStart,
    GhostDownEnd,
    PeekScan,
    DangerModeEnter,
    DangerModeExit,
    MineEvent,
    TelemetryUpdate
};

enum class Severity : uint8_t {
    Info,
    Warn,
    High,
    Critical
};

// ---------------------------------------------
// §5 / §19 Rollen-Codes (Priority-Engine-kompatibel)
// ---------------------------------------------
const uint8_t ROLE_WORKER = 1;
const uint8_t ROLE_PHONE  = 2;
const uint8_t ROLE_KERNEL = 3;
const uint8_t ROLE_SAFE   = 4;
const uint8_t ROLE_SENSOR = 5;
const uint8_t ROLE_ROUTER = 6;
const uint8_t ROLE_MINE   = 7;

// ---------------------------------------------
// §14.1 Device
// ---------------------------------------------
struct Device {
    char id[32];                 // 32-Byte Hex String
    uint8_t role;                // RoleCode
    uint16_t capability_mask;    // §13, bit-coded
    uint8_t trust_level;         // 0–3
    uint32_t last_seen;          // UNIX timestamp
    DeviceState status;
    uint8_t tag_mask;            // Tag-Bits
    uint16_t ram_mb;
    uint8_t cpu_percent;
    uint8_t gpu_percent;
    uint16_t traffic_kbps;
    uint8_t battery_percent;
    uint16_t wifi_mbit;
};

// PSP-Kernel ist kein Registry-Peer (§25). Heartbeat lokal, kein Auto-Enroll.
const char KERNEL_SOURCE_ID[8] = "kernel";

// Wire-IDs: ASCII in Device.id[32], Watch zeigt ≤8. Kein Hex (SPEC §14-Kommentar ist tot).
// 1 Kernel + 1 Worker + 1 Phone + 1 Router + 1 Safe + Sensoren + Minen (1–N, Host max 8).
const char HIVE_ID_WORKER[] = "W";
const char HIVE_ID_PHONE[] = "P";
const char HIVE_ID_ROUTER[] = "R";
const char HIVE_ID_SENSOR[] = "S";
const char HIVE_ID_FAMILY[] = "F";
const char HIVE_ID_SAFE[] = "N";
const char HIVE_ID_MINE_KERNEL[] = "X";
const char HIVE_ID_MINE_LAPTOP[] = "ML";
const char HIVE_ID_MINE_PHONE[] = "MP";
const char HIVE_ID_MINE_NAS[] = "MN";
const char HIVE_ID_MINE_ROUTER[] = "MR";
const char HIVE_ID_MINE_OS[] = "MO";
const char HIVE_ID_MINE_IOT[] = "MI";
const char HIVE_ID_MINE_BROWSER[] = "MB";

// ---------------------------------------------
// §14.2 Event
// ---------------------------------------------
struct Event {
    EventType type;
    char source_device_id[32];
    uint32_t timestamp;
    char payload[128];           // binary layout; HMAC-Hex at [88..127]
    Severity severity;
};

// ---------------------------------------------
// §14.3 Log
// ---------------------------------------------
struct LogEntry {
    char id[32];
    char event_id[32];
    char hash[64];               // SHA-256 Hex
    uint32_t stored_at;
    uint8_t retention_class;     // 0 short, 1 medium, 2 long
};

// ---------------------------------------------
// §14.4 Policy
// ---------------------------------------------
struct Policy {
    char id[16];
    char name[32];
    uint8_t scope;               // 0 device, 1 role, 2 global
    char condition[128];         // DSL
    uint8_t action;              // ActionCode
};

// ---------------------------------------------
// §14.5 Mine Payload (Replay-Guard)
// ---------------------------------------------
struct MinePayload {
    char mine_id[32];
    uint32_t counter;            // monoton
    uint32_t totp;               // Fenster 60–120s
    EventType event;
    uint32_t timestamp;
    char hash[64];
};

#endif // GHOST_CORE_H
