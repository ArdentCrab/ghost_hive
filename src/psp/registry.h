#ifndef REGISTRY_H
#define REGISTRY_H

// =====================================================
// Ghost Hive v1.7.1
// Registry — P3 HART
// Spec-Basis: §9, §12.1, §25, §32
// 1 Kernel (PSP, kein Peer), 1 Worker, 1 Safe,
// 1 Phone, 1 Router, 1–8 Sensoren, 1–N Minen
// =====================================================

#include "ghost_core.h"
#include "ghost_data.h"

const uint8_t MAX_DEVICES = 32;
const uint8_t MAX_WORKERS = 1;
const uint8_t MAX_SAFES = 1;
const uint8_t MAX_SENSORS = 8;
const uint8_t MAX_PHONES = 1;
const uint8_t MAX_ROUTERS = 1;

class Registry {
public:
    Registry();

    void init();
    void clear();

    bool addDevice(const Device& device);
    bool updateDevice(const char* id, const Device& device);
    bool removeDevice(const char* id);

    const Device* getDevice(const char* id) const;
    uint8_t getDeviceCount() const;
    DeviceInfo getDeviceInfo(uint8_t index) const;
    uint8_t getRoleCount(uint8_t role) const;

    bool setState(const char* id, DeviceState state);
    DeviceState getState(const char* id) const;

    bool pairDevice(const char* id);
    bool blockDevice(const char* id);
    const Device* findByRole(uint8_t role) const;
    uint8_t countByRole(uint8_t role) const;

    // §8 / §9 / Noah: Ghost Down ist global. Kein neuer Lock-State.
    void applyGlobalDown();
    bool updateTelemetry(const char* id,
                         uint16_t ram_mb,
                         uint8_t cpu_percent,
                         uint8_t gpu_percent,
                         uint16_t traffic_kbps,
                         uint8_t battery_percent,
                         uint16_t wifi_mbit,
                         uint32_t last_seen);

    bool isFull() const;

    typedef void (*RemoveHook)(void* ctx, const char* id);
    void setRemoveHook(RemoveHook fn, void* ctx);

private:
    Device devices_[MAX_DEVICES];
    uint8_t count_;
    RemoveHook remove_hook_;
    void* remove_ctx_;
    void notifyRemoved(const char* id);

    Device* findDevice(const char* id);
    bool isSameId(const char* a, const char* b) const;
    bool isTransitionAllowed(DeviceState from, DeviceState to) const;
    bool isRoleUpgrade(uint8_t from, uint8_t to) const;
    bool isRoleAllowed(uint8_t role) const;
};

#endif // REGISTRY_H
