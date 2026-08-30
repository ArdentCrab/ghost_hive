#include "net_sensor.h"

NetSensor::NetSensor() : sensor_(nullptr) {}

void NetSensor::attach(Sensor* sensor) {
    sensor_ = sensor;
}

bool NetSensor::putTag(Event* out, const char* tag) const {
    if (out == nullptr || tag == nullptr) return false;
    uint8_t i = 0;
    while (tag[i] != '\0' && i < 80) {
        out->payload[i] = tag[i];
        ++i;
    }
    out->payload[i] = '\0';
    return true;
}

bool NetSensor::fillPortScan(Event* out, uint32_t now) const {
    if (sensor_ == nullptr) return false;
    if (!sensor_->fillScan(out, now)) return false;
    return putTag(out, "port_scan");
}

bool NetSensor::fillNewDevice(Event* out, uint32_t now) const {
    if (sensor_ == nullptr) return false;
    if (!sensor_->fillScan(out, now)) return false;
    out->type = EventType::DeviceSeen;
    return putTag(out, "router_new_client");
}

bool NetSensor::fillSuspiciousFlow(Event* out, uint32_t now) const {
    if (sensor_ == nullptr) return false;
    if (!sensor_->fillScan(out, now)) return false;
    out->type = EventType::AnomalyDetected;
    out->severity = Severity::Warn;
    return putTag(out, "flow");
}

bool NetSensor::canConfigure() const {
    return false;
}

bool NetSensor::canWrite() const {
    return sensor_ != nullptr ? sensor_->canWrite() : false;
}
