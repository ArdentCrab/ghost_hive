#include "worker_transport.h"

WorkerTransport::WorkerTransport() {
}

void WorkerTransport::init(const char* deviceId) {
    link_.init(deviceId, ROLE_WORKER);
}

void WorkerTransport::attach(MediumWlan* wlan, MediumIr* ir) {
    link_.attach(wlan, ir);
}

bool WorkerTransport::send(const Event& event, uint32_t now) {
    return link_.send(event, now);
}

bool WorkerTransport::poll(Event& out) {
    return link_.poll(out);
}

void WorkerTransport::tick(uint32_t now) {
    link_.tick(now);
}

bool WorkerTransport::lastAcked() const {
    return link_.lastAcked();
}
