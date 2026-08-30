#include "worker.h"
#include "ghost_telemetry.h"

Worker::Worker() : hasDeviceKey_(false) {
    id_[0] = '\0';
    for (uint8_t i = 0; i < KEY_LEN; ++i) deviceKey_[i] = 0;
}

void Worker::copyId(char* dst, const char* src) const {
    uint8_t i = 0;
    while (src[i] != '\0' && i < 31) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

void Worker::init(const char* deviceId) {
    setId(deviceId);
}

void Worker::setId(const char* id) {
    if (id == nullptr) {
        id_[0] = '\0';
        return;
    }
    copyId(id_, id);
}

bool Worker::sendEvent(const Event& event, EventQueue& queue) const {
    if (id_[0] == '\0') return false;
    Event out = event;
    copyId(out.source_device_id, id_);
    return queue.push(out);
}

bool Worker::analyze(const Event& event, Event& result) const {
    if (id_[0] == '\0') return false;
    result = event;
    copyId(result.source_device_id, id_);
    return true;
}

bool Worker::fillHeartbeat(Event* out, uint32_t now) const {
    if (out == nullptr || id_[0] == '\0') return false;
    out->type = EventType::Heartbeat;
    copyId(out->source_device_id, id_);
    out->timestamp = now;
    out->payload[0] = '\0';
    out->severity = Severity::Info;
    return true;
}

bool Worker::fillTelemetry(Event* out, uint32_t now,
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

bool Worker::fillAnalysis(Event* out, uint32_t now, const char* note) const {
    if (out == nullptr || id_[0] == '\0') return false;
    if (note == nullptr || note[0] == '\0') return false;
    out->type = EventType::AnomalyDetected;
    copyId(out->source_device_id, id_);
    out->timestamp = now;
    uint8_t i = 0;
    while (note[i] != '\0' && i < 127) {
        out->payload[i] = note[i];
        ++i;
    }
    out->payload[i] = '\0';
    out->severity = Severity::Warn;
    return true;
}

bool Worker::canDecide() const {
    return false;
}

bool Worker::provisionDeviceKey(const uint8_t* key, uint8_t len) {
    if (key == nullptr || len == 0 || len > KEY_LEN) return false;
    for (uint8_t i = 0; i < KEY_LEN; ++i) deviceKey_[i] = 0;
    for (uint8_t i = 0; i < len; ++i) deviceKey_[i] = key[i];
    hasDeviceKey_ = true;
    return true;
}

bool Worker::hasDeviceKey() const {
    return hasDeviceKey_;
}

uint8_t Worker::versionMajor() const { return WORKER_VERSION_MAJOR; }
uint8_t Worker::versionMinor() const { return WORKER_VERSION_MINOR; }
