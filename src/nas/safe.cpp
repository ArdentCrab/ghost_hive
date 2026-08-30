#include "safe.h"

Safe::Safe() : count_(0), hasDeviceKey_(false), writeLock_(false) {
    id_[0] = '\0';
    for (uint8_t i = 0; i < KEY_LEN; ++i) deviceKey_[i] = 0;
}

void Safe::init(const char* deviceId) {
    uint8_t i = 0;
    if (deviceId == nullptr) {
        id_[0] = '\0';
        return;
    }
    while (deviceId[i] != '\0' && i < 31) {
        id_[i] = deviceId[i];
        ++i;
    }
    id_[i] = '\0';
}

bool Safe::store(const Event& event) {
    if (writeLock_) return false;
    if (isFull()) return false;
    ram_[count_] = event;
    ++count_;
    return true;
}

bool Safe::storeBackup(const Event& event, EventQueue& queue) {
    if (!store(event)) return false;

    Event out = event;
    uint8_t i = 0;
    while (id_[i] != '\0' && i < 31) {
        out.source_device_id[i] = id_[i];
        ++i;
    }
    out.source_device_id[i] = '\0';
    out.type = EventType::BackupWritten;
    return queue.push(out);
}

bool Safe::writeIndex(const char* index, char* output, uint8_t maxLen) const {
    if (index == nullptr || output == nullptr || maxLen == 0) return false;
    uint8_t i = 0;
    while (index[i] != '\0' && i < (maxLen - 1)) {
        output[i] = index[i];
        ++i;
    }
    output[i] = '\0';
    return true;
}

bool Safe::isFull() const {
    return count_ >= SAFE_SLOTS;
}

uint8_t Safe::storedCount() const {
    return count_;
}

bool Safe::canAlert() const {
    return false;
}

void Safe::setWriteLock(bool on) {
    writeLock_ = on;
}

bool Safe::writeLocked() const {
    return writeLock_;
}

bool Safe::ingestBackup(const Event& event) {
    return store(event);
}

bool Safe::provisionDeviceKey(const uint8_t* key, uint8_t len) {
    if (key == nullptr || len == 0 || len > KEY_LEN) return false;
    for (uint8_t i = 0; i < KEY_LEN; ++i) deviceKey_[i] = 0;
    for (uint8_t i = 0; i < len; ++i) deviceKey_[i] = key[i];
    hasDeviceKey_ = true;
    return true;
}

bool Safe::hasDeviceKey() const {
    return hasDeviceKey_;
}

uint8_t Safe::versionMajor() const { return SAFE_VERSION_MAJOR; }
uint8_t Safe::versionMinor() const { return SAFE_VERSION_MINOR; }
