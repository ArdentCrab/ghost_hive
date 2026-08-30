#ifndef WORKER_H
#define WORKER_H

// =====================================================
// Ghost Hive v1.7.1 — Worker 1.0
// Spec-Basis: §3, §5, §7, §11, §25, §33
// Analysiert. Entscheidet nie. Blockiert nie.
// =====================================================

#include "ghost_core.h"
#include "event_queue.h"
#include "ghost_keys.h"

const uint8_t WORKER_VERSION_MAJOR = 1;
const uint8_t WORKER_VERSION_MINOR = 0;

class Worker {
public:
    Worker();

    void init(const char* deviceId);
    void setId(const char* id);

    bool sendEvent(const Event& event, EventQueue& queue) const;
    bool analyze(const Event& event, Event& result) const;
    bool fillHeartbeat(Event* out, uint32_t now) const;
    bool fillTelemetry(Event* out, uint32_t now,
                       uint16_t ram_mb, uint8_t cpu_percent, uint8_t gpu_percent,
                       uint16_t traffic_kbps, uint8_t battery_percent,
                       uint16_t wifi_mbit) const;
    bool fillAnalysis(Event* out, uint32_t now, const char* note) const;

    bool canDecide() const;
    bool provisionDeviceKey(const uint8_t* key, uint8_t len);
    bool hasDeviceKey() const;

    uint8_t versionMajor() const;
    uint8_t versionMinor() const;

private:
    char id_[32];
    uint8_t deviceKey_[KEY_LEN];
    bool hasDeviceKey_;

    void copyId(char* dst, const char* src) const;
};

#endif // WORKER_H
