#include "os_mine.h"

OsMine::OsMine() {}

void OsMine::init(const char* mineId) {
    mine_.init(mineId);
}

bool OsMine::setTotpSeed(const uint8_t* seed, uint8_t len) {
    return mine_.setTotpSeed(seed, len);
}

bool OsMine::onSuspiciousProcess(MinePayload* out, uint32_t now) {
    return mine_.sendTrip(out, now);
}

bool OsMine::onSuspiciousFile(MinePayload* out, uint32_t now) {
    return mine_.sendTrip(out, now);
}

bool OsMine::send(MinePayload* out, uint32_t now) {
    return mine_.send(out, now);
}

void OsMine::freezeEvents() {
    mine_.freezeEvents();
}

bool OsMine::recv(MinePayload* out) const {
    (void)out;
    return false;
}

bool OsMine::canReceive() const {
    return false;
}
