#include "iot_mine.h"

IotMine::IotMine() {}

void IotMine::init(const char* mineId) {
    mine_.init(mineId);
}

bool IotMine::setTotpSeed(const uint8_t* seed, uint8_t len) {
    return mine_.setTotpSeed(seed, len);
}

bool IotMine::send(MinePayload* out, uint32_t now) {
    return mine_.send(out, now);
}

void IotMine::freezeEvents() {
    mine_.freezeEvents();
}

bool IotMine::recv(MinePayload* out) const {
    (void)out;
    return false;
}

bool IotMine::canReceive() const {
    return false;
}
