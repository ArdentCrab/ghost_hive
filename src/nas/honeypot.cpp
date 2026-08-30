#include "honeypot.h"

NasHoneypot::NasHoneypot() {}

void NasHoneypot::init(const char* mineId) {
    mine_.init(mineId);
}

bool NasHoneypot::setTotpSeed(const uint8_t* seed, uint8_t len) {
    return mine_.setTotpSeed(seed, len);
}

bool NasHoneypot::onLockvogel(MinePayload* out, uint32_t now) {
    return mine_.sendTrip(out, now);
}

bool NasHoneypot::send(MinePayload* out, uint32_t now) {
    return mine_.send(out, now);
}

void NasHoneypot::freezeEvents() {
    mine_.freezeEvents();
}

bool NasHoneypot::recv(MinePayload* out) const {
    (void)out;
    return false;
}

bool NasHoneypot::canReceive() const {
    return false;
}

bool NasHoneypot::canAlert() const {
    return false;
}
