#ifndef SENSOR_H
#define SENSOR_H

// =====================================================
// Ghost Hive v1.7.1 — Sensor 1.0
// Spec-Basis: §2.2, §3, §7, §25
// Scannt. Schreibt nie.
// =====================================================

#include "ghost_core.h"
#include "event_queue.h"
#include "ghost_keys.h"

const uint8_t SENSOR_VERSION_MAJOR = 1;
const uint8_t SENSOR_VERSION_MINOR = 0;
// §23 Sensor-Scan 60–300s. Mehr Sicht = mehr Sensor-Instanzen (≤8), nicht schneller als 60s.
const uint32_t SENSOR_INTERVAL_SEC = 60;
const uint32_t SENSOR_LOWPOWER_SEC = 180;

class Sensor {
public:
    Sensor();

    void init(const char* deviceId);
    void setId(const char* id);

    bool sendScanResult(const Event& event, EventQueue& queue) const;
    bool sendHeartbeat(const Event& event, EventQueue& queue) const;
    bool sendHeartbeat(EventQueue& queue, uint32_t now) const;
    bool fillScan(Event* out, uint32_t now) const;
    bool fillHeartbeat(Event* out, uint32_t now) const;
    bool fillTelemetry(Event* out, uint32_t now,
                       uint16_t ram_mb, uint8_t cpu_percent, uint8_t gpu_percent,
                       uint16_t traffic_kbps, uint8_t battery_percent,
                       uint16_t wifi_mbit) const;

    bool canWrite() const;
    bool recv(Event* out) const;
    void setLowPower(bool on);
    bool lowPower() const;
    uint32_t intervalSec() const;
    bool provisionDeviceKey(const uint8_t* key, uint8_t len);
    bool hasDeviceKey() const;

    uint8_t versionMajor() const;
    uint8_t versionMinor() const;

private:
    char id_[32];
    uint8_t deviceKey_[KEY_LEN];
    bool hasDeviceKey_;
    bool lowPower_;

    void copyId(char* dst, const char* src) const;
};

#endif // SENSOR_H
