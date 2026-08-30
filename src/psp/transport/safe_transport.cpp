#include "safe_transport.h"

SafeTransport::SafeTransport() {
}

void SafeTransport::init(const char* deviceId) {
    link_.init(deviceId, ROLE_SAFE);
}

void SafeTransport::attach(MediumWlan* wlan, MediumIr* ir) {
    link_.attach(wlan, ir);
}

bool SafeTransport::send(const Event& event, uint32_t now) {
    return link_.send(event, now);
}

bool SafeTransport::poll(Event& out) {
    return link_.poll(out);
}

void SafeTransport::tick(uint32_t now) {
    link_.tick(now);
}

bool SafeTransport::lastAcked() const {
    return link_.lastAcked();
}
