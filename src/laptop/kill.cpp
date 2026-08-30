#include "kill.h"
#include "peer_keys.h"

HiveKill::HiveKill() : keys_(nullptr) {}

void HiveKill::attach(GhostKeys* keys) {
    keys_ = keys;
}

bool HiveKill::fill(Event* out, const char* sourceId, uint32_t now) const {
    return fill(out, sourceId, now, 0);
}

bool HiveKill::fill(Event* out, const char* sourceId, uint32_t now,
                    uint32_t snapshotRef) const {
    if (out == nullptr || sourceId == nullptr || sourceId[0] == '\0') return false;
    out->type = EventType::GhostDownStart;
    uint8_t i = 0;
    while (sourceId[i] != '\0' && i < 31) {
        out->source_device_id[i] = sourceId[i];
        ++i;
    }
    out->source_device_id[i] = '\0';
    out->timestamp = now;
    out->severity = Severity::Critical;
    out->payload[0] = 'k';
    out->payload[1] = 'i';
    out->payload[2] = 'l';
    out->payload[3] = 'l';
    i = 4;
    if (snapshotRef > 0) {
        out->payload[i++] = ':';
        uint32_t n = snapshotRef;
        char tmp[11];
        uint8_t t = 0;
        while (n > 0 && t < 10) {
            tmp[t++] = static_cast<char>('0' + (n % 10));
            n /= 10;
        }
        while (t > 0 && i < 80) out->payload[i++] = tmp[--t];
    }
    out->payload[i] = '\0';
    return true;
}

bool HiveKill::sign(Event& event) const {
    if (keys_ == nullptr) return false;
    if (event.type != EventType::GhostDownStart) return false;
    return peer_sign_event(*keys_, event);
}

bool HiveKill::canWriteFlag() const {
    return false;
}
