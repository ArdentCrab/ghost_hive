#include "browser_mine.h"

BrowserMine::BrowserMine() {}

void BrowserMine::init(const char* mineId) {
    mine_.init(mineId);
}

bool BrowserMine::setTotpSeed(const uint8_t* seed, uint8_t len) {
    return mine_.setTotpSeed(seed, len);
}

bool BrowserMine::onSuspiciousUrl(MinePayload* out, uint32_t now) {
    return mine_.sendTrip(out, now);
}

bool BrowserMine::send(MinePayload* out, uint32_t now) {
    return mine_.send(out, now);
}

void BrowserMine::freezeEvents() {
    mine_.freezeEvents();
}

bool BrowserMine::recv(MinePayload* out) const {
    (void)out;
    return false;
}

bool BrowserMine::canReceive() const {
    return false;
}
