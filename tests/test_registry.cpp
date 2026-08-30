#include <cstdio>
#include "../src/psp/registry.h"

int main() {
    Registry reg;

    Device a{};
    a.id[0] = 'A';
    a.id[1] = '\0';
    a.role = 1;
    a.trust_level = 2;
    a.status = DeviceState::Unknown;

    Device b{};
    b.id[0] = 'B';
    b.id[1] = '\0';
    b.role = 4;
    b.trust_level = 1;
    b.status = DeviceState::Unknown;

    bool ok = true;

    if (!reg.addDevice(a)) ok = false;
    {
        const Device* fa = reg.getDevice("A");
        if (fa == nullptr || fa->ram_mb != 0xFFFFu || fa->cpu_percent != 0xFFu) ok = false;
    }
    if (!reg.addDevice(b)) ok = false;
    if (reg.getDeviceCount() != 2) ok = false;

    const Device* found = reg.getDevice("A");
    if (found == nullptr) ok = false;

    if (!reg.setState("A", DeviceState::Pending)) ok = false;
    if (reg.getState("A") != DeviceState::Pending) ok = false;

    if (!reg.removeDevice("B")) ok = false;
    if (reg.getDevice("B") != nullptr) ok = false;

    Device online{};
    online.id[0] = 'C';
    online.id[1] = '\0';
    online.role = ROLE_SENSOR;
    online.trust_level = 2;
    online.status = DeviceState::Online;
    if (!reg.addDevice(online)) ok = false;
    if (reg.getState("C") != DeviceState::Pending) ok = false;
    if (!reg.pairDevice("C")) ok = false;
    if (reg.getState("C") != DeviceState::Online) ok = false;
    if (!reg.setState("C", DeviceState::Silent)) ok = false;
    if (!reg.setState("C", DeviceState::Unknown)) ok = false;
    if (!reg.setState("C", DeviceState::Pending)) ok = false;
    if (!reg.pairDevice("C")) ok = false;

    Device kernel{};
    kernel.id[0] = 'K';
    kernel.id[1] = '\0';
    kernel.role = ROLE_KERNEL;
    kernel.status = DeviceState::Pending;
    if (reg.addDevice(kernel)) ok = false;

    Device worker2{};
    worker2.id[0] = 'D';
    worker2.id[1] = '\0';
    worker2.role = ROLE_WORKER;
    worker2.status = DeviceState::Pending;
    if (reg.addDevice(worker2)) ok = false;

    Device safe1{};
    safe1.id[0] = 'N';
    safe1.id[1] = '\0';
    safe1.role = ROLE_SAFE;
    safe1.status = DeviceState::Pending;
    if (!reg.addDevice(safe1)) ok = false;

    Device safe2{};
    safe2.id[0] = 'M';
    safe2.id[1] = '\0';
    safe2.role = ROLE_SAFE;
    safe2.status = DeviceState::Pending;
    if (reg.addDevice(safe2)) ok = false;

    for (uint8_t i = 0; i < 7; ++i) {
        Device s{};
        s.id[0] = 'S';
        s.id[1] = static_cast<char>('0' + i);
        s.id[2] = '\0';
        s.role = ROLE_SENSOR;
        s.status = DeviceState::Pending;
        if (!reg.addDevice(s)) ok = false;
    }
    Device extraSensor{};
    extraSensor.id[0] = 'S';
    extraSensor.id[1] = '9';
    extraSensor.id[2] = '\0';
    extraSensor.role = ROLE_SENSOR;
    extraSensor.status = DeviceState::Pending;
    if (reg.addDevice(extraSensor)) ok = false;

    Device mine{};
    mine.id[0] = 'X';
    mine.id[1] = '\0';
    mine.role = ROLE_MINE;
    mine.status = DeviceState::Silent;
    if (!reg.addDevice(mine)) ok = false;

    if (!reg.pairDevice("A")) ok = false;
    if (!reg.updateTelemetry("A", 512, 10, 4, 32, 80, 54, 9)) ok = false;
    {
        Device z{};
        z.trust_level = 2;
        z.status = DeviceState::Online;
        z.last_seen = 99;
        z.ram_mb = 0;
        z.cpu_percent = 0;
        z.gpu_percent = 0;
        z.traffic_kbps = 0;
        z.battery_percent = 0;
        z.wifi_mbit = 0;
        if (!reg.updateDevice("A", z)) ok = false;
        const Device* ta = reg.getDevice("A");
        if (ta == nullptr || ta->ram_mb != 512 || ta->cpu_percent != 10) ok = false;
        if (ta == nullptr || ta->last_seen != 99) ok = false;
    }
    if (!reg.updateTelemetry("A", 0, 0, 0, 0, 0, 0, 10)) ok = false;
    {
        const Device* ta = reg.getDevice("A");
        if (ta == nullptr || ta->ram_mb != 0 || ta->cpu_percent != 0) ok = false;
    }
    reg.applyGlobalDown();
    if (reg.getState("A") != DeviceState::GhostDown) ok = false;
    if (reg.getState("C") != DeviceState::Silent) ok = false;

    printf(ok ? "PASS registry\n" : "FAIL registry\n");
    return ok ? 0 : 1;
}
