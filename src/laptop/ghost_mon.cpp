// Host forensics: local vault.bin + forensic.log. No PSP, no new Event/Policy.

#include "ghost_core.h"
#include "alert.h"
#include "log_client.h"

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static const char VAULT_PATH[] = "/tmp/ghost_hive/vault.bin";
static const uint8_t VAULT_RAM_SLOTS = 64;

static bool get_u8(FILE* f, uint8_t* v) {
    return fread(v, 1, 1, f) == 1;
}

static bool get_u32(FILE* f, uint32_t* v) {
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return false;
    *v = static_cast<uint32_t>(b[0]) |
         (static_cast<uint32_t>(b[1]) << 8) |
         (static_cast<uint32_t>(b[2]) << 16) |
         (static_cast<uint32_t>(b[3]) << 24);
    return true;
}

static bool get_bytes(FILE* f, char* dst, uint16_t n) {
    return fread(dst, 1, n, f) == n;
}

static const char* ev_name(EventType t) {
    switch (t) {
        case EventType::ScanResult: return "Scan";
        case EventType::DeviceSeen: return "Seen";
        case EventType::DeviceLost: return "Lost";
        case EventType::ProfileUpdate: return "Prof";
        case EventType::AnomalyDetected: return "Anom";
        case EventType::PolicyViolation: return "PViol";
        case EventType::BackupWritten: return "Bak";
        case EventType::AlertSent: return "Alert";
        case EventType::Heartbeat: return "HBeat";
        case EventType::HeartbeatMiss: return "HMiss";
        case EventType::RoleChange: return "Role";
        case EventType::ConfigChange: return "Cfg";
        case EventType::GhostDownStart: return "GDStart";
        case EventType::GhostDownEnd: return "GDEnd";
        case EventType::PeekScan: return "Peek";
        case EventType::DangerModeEnter: return "Dng+";
        case EventType::DangerModeExit: return "Dng-";
        case EventType::MineEvent: return "Mine";
        case EventType::TelemetryUpdate: return "Telem";
        default: return "?";
    }
}

int main() {
    (void)umask(077);
    (void)mkdir("/tmp/ghost_hive", 0700);

    HiveAlert alert;
    alert.init();
    HiveLogClient logs;
    logs.init();

    FILE* vf = fopen(VAULT_PATH, "rb");
    uint8_t n = 0;
    uint8_t version = 0;
    bool loaded = false;
    if (vf != nullptr) {
        loaded = get_u8(vf, &version) && get_u8(vf, &n) && version == 1 &&
                 n <= VAULT_RAM_SLOTS;
    }

    FILE* out = fopen("/tmp/ghost_hive/forensic.log", "ab");
    printf("vault %u/64 file=%s\n", loaded ? n : 0, vf != nullptr ? "yes" : "no");
    if (out != nullptr) {
        fprintf(out, "vault %u file=%u\n", loaded ? n : 0, vf != nullptr ? 1 : 0);
    }

    if (loaded) {
        for (uint8_t i = 0; i < n; ++i) {
            Event e{};
            uint8_t type = 0;
            uint8_t sev = 0;
            uint8_t enc = 0;
            uint32_t sum = 0;
            e.source_device_id[0] = '\0';
            bool ok = get_u8(vf, &type) &&
                      get_bytes(vf, e.source_device_id, 32) &&
                      get_u32(vf, &e.timestamp) &&
                      get_bytes(vf, e.payload, 128) &&
                      get_u8(vf, &sev) &&
                      get_u32(vf, &sum) &&
                      get_u8(vf, &enc);
            if (!ok) break;
            e.source_device_id[31] = '\0';
            e.type = static_cast<EventType>(type);
            e.severity = static_cast<Severity>(sev);
            (void)alert.ingest(e);
            (void)logs.ingest(e);
            char hmac = (enc != 0) ? 'E' : 'P';
            if (e.payload[84] == 'I') hmac = 'I';
            printf("%s src=%.8s hmac=%c\n", ev_name(e.type), e.source_device_id,
                   hmac);
            if (out != nullptr) {
                fprintf(out, "%s src=%.32s hmac=%c ts=%u\n",
                        ev_name(e.type), e.source_device_id, hmac, e.timestamp);
            }
        }
    }
    if (vf != nullptr) fclose(vf);

    if (alert.danger()) printf("ALERT danger\n");
    if (alert.down()) printf("ALERT down\n");
    if (alert.kill()) printf("ALERT kill\n");
    if (!alert.showing()) printf("ALERT --\n");
    if (out != nullptr) {
        if (alert.danger()) fprintf(out, "ALERT danger\n");
        if (alert.down()) fprintf(out, "ALERT down\n");
        if (alert.kill()) fprintf(out, "ALERT kill\n");
        fclose(out);
    }
    (void)chmod("/tmp/ghost_hive/forensic.log", 0600);
    return 0;
}
