#include "sensor.h"
#include "ghost_telemetry.h"

Sensor::Sensor() : hasDeviceKey_(false), lowPower_(false) {
    id_[0] = '\0';
    for (uint8_t i = 0; i < KEY_LEN; ++i) deviceKey_[i] = 0;
}

void Sensor::copyId(char* dst, const char* src) const {
    uint8_t i = 0;
    while (src[i] != '\0' && i < 31) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

void Sensor::init(const char* deviceId) {
    setId(deviceId);
}

void Sensor::setId(const char* id) {
    if (id == nullptr) {
        id_[0] = '\0';
        return;
    }
    copyId(id_, id);
}

bool Sensor::sendScanResult(const Event& event, EventQueue& queue) const {
    if (id_[0] == '\0') return false;
    Event out = event;
    copyId(out.source_device_id, id_);
    out.type = EventType::ScanResult;
    return queue.push(out);
}

bool Sensor::sendHeartbeat(const Event& event, EventQueue& queue) const {
    if (id_[0] == '\0') return false;
    Event out = event;
    copyId(out.source_device_id, id_);
    out.type = EventType::Heartbeat;
    return queue.push(out);
}

bool Sensor::sendHeartbeat(EventQueue& queue, uint32_t now) const {
    Event out{};
    if (!fillHeartbeat(&out, now)) return false;
    return queue.push(out);
}

bool Sensor::fillScan(Event* out, uint32_t now) const {
    if (out == nullptr || id_[0] == '\0') return false;
    out->type = EventType::ScanResult;
    copyId(out->source_device_id, id_);
    out->timestamp = now;
    out->payload[0] = '\0';
    out->severity = Severity::Info;
    return true;
}

bool Sensor::fillHeartbeat(Event* out, uint32_t now) const {
    if (out == nullptr || id_[0] == '\0') return false;
    out->type = EventType::Heartbeat;
    copyId(out->source_device_id, id_);
    out->timestamp = now;
    out->payload[0] = '\0';
    out->severity = Severity::Info;
    return true;
}

bool Sensor::fillTelemetry(Event* out, uint32_t now,
                           uint16_t ram_mb, uint8_t cpu_percent, uint8_t gpu_percent,
                           uint16_t traffic_kbps, uint8_t battery_percent,
                           uint16_t wifi_mbit) const {
    if (out == nullptr || id_[0] == '\0') return false;
    out->type = EventType::TelemetryUpdate;
    copyId(out->source_device_id, id_);
    out->timestamp = now;
    out->severity = Severity::Info;
    fillTelemetryPayload(out, ram_mb, cpu_percent, gpu_percent,
                         traffic_kbps, battery_percent, wifi_mbit);
    return true;
}

bool Sensor::canWrite() const {
    return false;
}

bool Sensor::recv(Event* out) const {
    (void)out;
    return false;
}

void Sensor::setLowPower(bool on) {
    lowPower_ = on;
}

bool Sensor::lowPower() const {
    return lowPower_;
}

uint32_t Sensor::intervalSec() const {
    return lowPower_ ? SENSOR_LOWPOWER_SEC : SENSOR_INTERVAL_SEC;
}

bool Sensor::provisionDeviceKey(const uint8_t* key, uint8_t len) {
    if (key == nullptr || len == 0 || len > KEY_LEN) return false;
    for (uint8_t i = 0; i < KEY_LEN; ++i) deviceKey_[i] = 0;
    for (uint8_t i = 0; i < len; ++i) deviceKey_[i] = key[i];
    hasDeviceKey_ = true;
    return true;
}

bool Sensor::hasDeviceKey() const {
    return hasDeviceKey_;
}

uint8_t Sensor::versionMajor() const { return SENSOR_VERSION_MAJOR; }
uint8_t Sensor::versionMinor() const { return SENSOR_VERSION_MINOR; }
