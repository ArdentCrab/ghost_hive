#include "registry.h"
#include "ghost_telemetry.h"

Registry::Registry() : remove_hook_(nullptr), remove_ctx_(nullptr) {
    init();
}

void Registry::setRemoveHook(RemoveHook fn, void* ctx) {
    remove_hook_ = fn;
    remove_ctx_ = ctx;
}

void Registry::notifyRemoved(const char* id) {
    if (remove_hook_ == nullptr || id == nullptr || id[0] == '\0') return;
    remove_hook_(remove_ctx_, id);
}

void Registry::init() {
    count_ = 0;
    for (uint8_t i = 0; i < MAX_DEVICES; ++i) {
        devices_[i].id[0] = '\0';
        devices_[i].role = 0;
        devices_[i].capability_mask = 0;
        devices_[i].trust_level = 0;
        devices_[i].last_seen = 0;
        devices_[i].status = DeviceState::Unknown;
        devices_[i].tag_mask = 0;
        device_telem_clear(&devices_[i]);
    }
}

void Registry::clear() {
    for (uint8_t i = 0; i < MAX_DEVICES; ++i) {
        if (devices_[i].id[0] != '\0') notifyRemoved(devices_[i].id);
    }
    init();
}

bool Registry::addDevice(const Device& device) {
    if (isFull()) return false;
    if (device.id[0] == '\0') return false;
    if (getDevice(device.id) != nullptr) return false;

    // §2.2 / §44: kein Auto-Enroll als online
    Device copy = device;
    if (copy.status == DeviceState::Online ||
        copy.status == DeviceState::DangerMode ||
        copy.status == DeviceState::GhostDown) {
        copy.status = DeviceState::Pending;
    }

    // §9: Kernel nicht als Peer einschleusen
    if (copy.role == ROLE_KERNEL) return false;
    if (!isRoleAllowed(copy.role)) return false;
    device_telem_clear(&copy);

    for (uint8_t i = 0; i < MAX_DEVICES; ++i) {
        if (devices_[i].id[0] == '\0') {
            devices_[i] = copy;
            ++count_;
            return true;
        }
    }
    return false;
}

bool Registry::updateDevice(const char* id, const Device& device) {
    Device* target = findDevice(id);
    if (target == nullptr) return false;
    if (isRoleUpgrade(target->role, device.role)) return false;

    Device copy = device;
    for (uint8_t i = 0; i < 32; ++i) {
        copy.id[i] = target->id[i];
        if (target->id[i] == '\0') break;
    }
    copy.role = target->role;
    copy.ram_mb = target->ram_mb;
    copy.cpu_percent = target->cpu_percent;
    copy.gpu_percent = target->gpu_percent;
    copy.traffic_kbps = target->traffic_kbps;
    copy.battery_percent = target->battery_percent;
    copy.wifi_mbit = target->wifi_mbit;
    *target = copy;
    return true;
}

bool Registry::removeDevice(const char* id) {
    Device* target = findDevice(id);
    if (target == nullptr) return false;
    notifyRemoved(id);
    target->id[0] = '\0';
    --count_;
    return true;
}

const Device* Registry::getDevice(const char* id) const {
    for (uint8_t i = 0; i < MAX_DEVICES; ++i) {
        if (devices_[i].id[0] != '\0' && isSameId(devices_[i].id, id)) {
            return &devices_[i];
        }
    }
    return nullptr;
}

uint8_t Registry::getDeviceCount() const {
    return count_;
}

DeviceInfo Registry::getDeviceInfo(uint8_t index) const {
    DeviceInfo info{};
    uint8_t seen = 0;
    for (uint8_t i = 0; i < MAX_DEVICES; ++i) {
        if (devices_[i].id[0] == '\0') continue;
        if (seen == index) {
            const Device& d = devices_[i];
            uint8_t n = 0;
            while (d.id[n] != '\0' && n < 31) {
                info.id[n] = d.id[n];
                ++n;
            }
            info.id[n] = '\0';
            info.role = d.role;
            info.status = static_cast<uint8_t>(d.status);
            info.lastSeen = d.last_seen;
            return info;
        }
        ++seen;
    }
    return info;
}

bool Registry::setState(const char* id, DeviceState state) {
    Device* target = findDevice(id);
    if (target == nullptr) return false;
    if (!isTransitionAllowed(target->status, state)) return false;
    target->status = state;
    return true;
}

DeviceState Registry::getState(const char* id) const {
    const Device* target = getDevice(id);
    if (target == nullptr) return DeviceState::Unknown;
    return target->status;
}

bool Registry::pairDevice(const char* id) {
    // §32: Pairing nur pending → online
    return setState(id, DeviceState::Online);
}

bool Registry::blockDevice(const char* id) {
    // §9 / §15 / §20: unknown|silent → suspected → blocked
    Device* target = findDevice(id);
    if (target == nullptr) return false;

    if (target->status == DeviceState::Silent) {
        if (!setState(id, DeviceState::Suspected)) return false;
    }
    if (target->status == DeviceState::Unknown) {
        if (!setState(id, DeviceState::Suspected)) return false;
    }
    if (target->status == DeviceState::Suspected) {
        return setState(id, DeviceState::Blocked);
    }
    return target->status == DeviceState::Blocked;
}

const Device* Registry::findByRole(uint8_t role) const {
    for (uint8_t i = 0; i < MAX_DEVICES; ++i) {
        if (devices_[i].id[0] != '\0' && devices_[i].role == role) {
            return &devices_[i];
        }
    }
    return nullptr;
}

uint8_t Registry::countByRole(uint8_t role) const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < MAX_DEVICES; ++i) {
        if (devices_[i].id[0] != '\0' && devices_[i].role == role) ++n;
    }
    return n;
}

uint8_t Registry::getRoleCount(uint8_t role) const {
    return countByRole(role);
}

bool Registry::isRoleAllowed(uint8_t role) const {
    switch (role) {
        case ROLE_WORKER:
            return countByRole(ROLE_WORKER) < MAX_WORKERS;
        case ROLE_SAFE:
            return countByRole(ROLE_SAFE) < MAX_SAFES;
        case ROLE_SENSOR:
            return countByRole(ROLE_SENSOR) < MAX_SENSORS;
        case ROLE_PHONE:
            return countByRole(ROLE_PHONE) < MAX_PHONES;
        case ROLE_ROUTER:
            return countByRole(ROLE_ROUTER) < MAX_ROUTERS;
        case ROLE_MINE:
            return !isFull();
        default:
            return false;
    }
}

bool Registry::isFull() const {
    return count_ >= MAX_DEVICES;
}

Device* Registry::findDevice(const char* id) {
    for (uint8_t i = 0; i < MAX_DEVICES; ++i) {
        if (devices_[i].id[0] != '\0' && isSameId(devices_[i].id, id)) {
            return &devices_[i];
        }
    }
    return nullptr;
}

bool Registry::isSameId(const char* a, const char* b) const {
    uint8_t i = 0;
    while (i < 32) {
        if (a[i] != b[i]) return false;
        if (a[i] == '\0') return true;
        ++i;
    }
    return true;
}

bool Registry::isRoleUpgrade(uint8_t from, uint8_t to) const {
    if (from == to) return false;
    if (to == ROLE_KERNEL) return true;
    if (from == ROLE_SENSOR && to == ROLE_WORKER) return true;
    if (from == ROLE_MINE && (to == ROLE_WORKER || to == ROLE_KERNEL)) return true;
    if (from == ROLE_SAFE && to == ROLE_KERNEL) return true;
    return false;
}

bool Registry::updateTelemetry(const char* id,
                               uint16_t ram_mb,
                               uint8_t cpu_percent,
                               uint8_t gpu_percent,
                               uint16_t traffic_kbps,
                               uint8_t battery_percent,
                               uint16_t wifi_mbit,
                               uint32_t last_seen) {
    Device* t = findDevice(id);
    if (t == nullptr) return false;
    // 0 is a measurement. Absent only via 0xFFFF/0xFF; updateDevice never copies telem.
    t->ram_mb = ram_mb;
    t->cpu_percent = cpu_percent;
    t->gpu_percent = gpu_percent;
    t->traffic_kbps = traffic_kbps;
    t->battery_percent = battery_percent;
    t->wifi_mbit = wifi_mbit;
    t->last_seen = last_seen;
    return true;
}

void Registry::applyGlobalDown() {
    // Worker: Online → Degraded → ghost_down. Sensor/Phone: active → silent (§9).
    // Safe/Router: Online → Degraded (Write-Lock / Events-only liegen in den Rollen).
    for (uint8_t i = 0; i < MAX_DEVICES; ++i) {
        if (devices_[i].id[0] == '\0') continue;
        const char* id = devices_[i].id;
        uint8_t role = devices_[i].role;
        DeviceState st = devices_[i].status;

        if (role == ROLE_WORKER) {
            if (st == DeviceState::Online) {
                (void)setState(id, DeviceState::Degraded);
                st = devices_[i].status;
            }
            if (st == DeviceState::Degraded) {
                (void)setState(id, DeviceState::GhostDown);
            }
            continue;
        }

        if (role == ROLE_SENSOR || role == ROLE_PHONE) {
            if (st == DeviceState::Online || st == DeviceState::Degraded) {
                (void)setState(id, DeviceState::Silent);
            }
            continue;
        }

        if (role == ROLE_SAFE || role == ROLE_ROUTER) {
            if (st == DeviceState::Online) {
                (void)setState(id, DeviceState::Degraded);
            }
        }
    }
}

bool Registry::isTransitionAllowed(DeviceState from, DeviceState to) const {
    switch (from) {
        case DeviceState::Online:
            return to == DeviceState::Degraded || to == DeviceState::Silent;
        case DeviceState::Degraded:
            return to == DeviceState::Offline ||
                   to == DeviceState::GhostDown ||
                   to == DeviceState::Silent;
        case DeviceState::Offline:
            return to == DeviceState::Pending;
        case DeviceState::Pending:
            return to == DeviceState::Online;
        case DeviceState::Unknown:
            return to == DeviceState::Suspected || to == DeviceState::Pending;
        case DeviceState::Suspected:
            return to == DeviceState::Blocked || to == DeviceState::Online;
        case DeviceState::Blocked:
            return false;
        case DeviceState::GhostDown:
            return to == DeviceState::DangerMode;
        case DeviceState::DangerMode:
            return to == DeviceState::Online;
        case DeviceState::Silent:
            return to == DeviceState::Unknown || to == DeviceState::Suspected;
        default:
            return false;
    }
}
