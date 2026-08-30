#ifndef SAFE_H
#define SAFE_H

// =====================================================
// Ghost Hive v1.7.1 — Safe 1.0
// Spec-Basis: §3, §7, §20, §25, §28, §33
// Sichert. Alarmiert nie.
// =====================================================

#include "ghost_core.h"
#include "event_queue.h"
#include "ghost_keys.h"

const uint8_t SAFE_VERSION_MAJOR = 1;
const uint8_t SAFE_VERSION_MINOR = 0;
const uint8_t SAFE_SLOTS = 32;

class Safe {
public:
    Safe();

    void init(const char* deviceId);

    bool store(const Event& event);
    bool storeBackup(const Event& event, EventQueue& queue);
    bool writeIndex(const char* index, char* output, uint8_t maxLen) const;

    bool isFull() const;
    uint8_t storedCount() const;
    bool canAlert() const;
    void setWriteLock(bool on);
    bool writeLocked() const;
    bool ingestBackup(const Event& event);
    bool provisionDeviceKey(const uint8_t* key, uint8_t len);
    bool hasDeviceKey() const;

    uint8_t versionMajor() const;
    uint8_t versionMinor() const;

private:
    char id_[32];
    Event ram_[SAFE_SLOTS];
    uint8_t count_;
    uint8_t deviceKey_[KEY_LEN];
    bool hasDeviceKey_;
    bool writeLock_;
};

#endif // SAFE_H
