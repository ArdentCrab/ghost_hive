#include "router_mine.h"

RouterMine::RouterMine() {}

void RouterMine::init(const char* mineId) {
    mine_.init(mineId);
}

bool RouterMine::setTotpSeed(const uint8_t* seed, uint8_t len) {
    return mine_.setTotpSeed(seed, len);
}

bool RouterMine::onDecoyHit(MinePayload* out, uint32_t now) {
    return mine_.sendTrip(out, now);
}

bool RouterMine::send(MinePayload* out, uint32_t now) {
    return mine_.send(out, now);
}

void RouterMine::freezeEvents() {
    mine_.freezeEvents();
}

bool RouterMine::recv(MinePayload* out) const {
    (void)out;
    return false;
}

bool RouterMine::canReceive() const {
    return false;
}
