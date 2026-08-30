#include "sensor_transport.h"

SensorTransport::SensorTransport() {
}

void SensorTransport::init(const char* deviceId) {
    link_.init(deviceId, ROLE_SENSOR);
}

void SensorTransport::attach(MediumWlan* wlan, MediumIr* ir) {
    link_.attach(wlan, ir);
}

bool SensorTransport::send(const Event& event, uint32_t now) {
    return link_.send(event, now);
}

bool SensorTransport::poll(Event& out) {
    (void)out;
    return false;
}

void SensorTransport::tick(uint32_t now) {
    link_.tick(now);
}

bool SensorTransport::lastAcked() const {
    return link_.lastAcked();
}
