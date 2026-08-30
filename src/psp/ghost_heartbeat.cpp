#include "ghost_heartbeat.h"

GhostHeartbeat::GhostHeartbeat() {
    init();
}

void GhostHeartbeat::init() {
    count_ = 0;
    for (uint8_t i = 0; i < 32; ++i) {
        entries_[i].device_id[0] = '\0';
        entries_[i].last_beat = 0;
        entries_[i].miss_count = 0;
    }
}

void GhostHeartbeat::copyId(char* dst, const char* src) {
    uint8_t i = 0;
    while (src[i] != '\0' && i < 31) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

void GhostHeartbeat::send(const char* deviceId, uint32_t now) {
    update(deviceId, now);
}

void GhostHeartbeat::update(const char* deviceId, uint32_t timestamp) {
    if (deviceId == nullptr || deviceId[0] == '\0') return;

    int16_t index = findIndex(deviceId);

    if (index >= 0) {
        entries_[index].last_beat = timestamp;
        entries_[index].miss_count = 0;
        return;
    }

    if (count_ < 32) {
        Entry& entry = entries_[count_];
        copyId(entry.device_id, deviceId);
        entry.last_beat = timestamp;
        entry.miss_count = 0;
        ++count_;
    }
}

void GhostHeartbeat::tick(uint32_t now, Registry* registry) {
    for (uint8_t i = 0; i < count_; ++i) {
        Entry& e = entries_[i];
        if (now <= e.last_beat) {
            e.miss_count = 0;
            continue;
        }
        uint32_t gap = now - e.last_beat;
        uint8_t misses = static_cast<uint8_t>(gap / HEARTBEAT_INTERVAL_SEC);
        e.miss_count = misses;

        if (registry == nullptr || misses == 0) continue;

        const Device* d = registry->getDevice(e.device_id);
        if (d == nullptr) continue;

        if (d->role == ROLE_SENSOR && d->status == DeviceState::Online) {
            registry->setState(e.device_id, DeviceState::Degraded);
        }

        if (d->role == ROLE_WORKER && d->status == DeviceState::Online) {
            registry->setState(e.device_id, DeviceState::Degraded);
        }

        if (d->role == ROLE_MINE && misses >= HEARTBEAT_MISS_LIMIT) {
            if (d->status == DeviceState::Silent) {
                registry->setState(e.device_id, DeviceState::Suspected);
            } else if (d->status == DeviceState::Unknown) {
                registry->setState(e.device_id, DeviceState::Suspected);
            }
        }
    }
}

void GhostHeartbeat::checkAll(uint32_t now, Registry& registry) {
    tick(now, &registry);
}

bool GhostHeartbeat::isAlive(const char* deviceId, uint32_t now) const {
    int16_t index = findIndex(deviceId);
    if (index < 0) return false;
    return (now - entries_[index].last_beat) <= HEARTBEAT_INTERVAL_SEC;
}

uint8_t GhostHeartbeat::getMissCount(const char* deviceId) const {
    int16_t index = findIndex(deviceId);
    if (index < 0) return 0;
    return entries_[index].miss_count;
}

HeartbeatInfo GhostHeartbeat::getInfo(const char* deviceId) const {
    HeartbeatInfo info{};
    int16_t index = findIndex(deviceId);
    if (index < 0) return info;

    info.lastBeat = entries_[index].last_beat;
    info.missCount = entries_[index].miss_count;
    return info;
}

uint8_t GhostHeartbeat::trackedCount() const {
    return count_;
}

const char* GhostHeartbeat::idAt(uint8_t index) const {
    if (index >= count_) return nullptr;
    return entries_[index].device_id;
}

int16_t GhostHeartbeat::findIndex(const char* deviceId) const {
    if (deviceId == nullptr) return -1;
    for (uint8_t i = 0; i < count_; ++i) {
        bool same = true;
        for (uint8_t j = 0; j < 32; ++j) {
            if (entries_[i].device_id[j] != deviceId[j]) {
                same = false;
                break;
            }
            if (deviceId[j] == '\0') break;
        }
        if (same) return static_cast<int16_t>(i);
    }
    return -1;
}
