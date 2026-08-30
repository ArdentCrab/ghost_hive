#include "ghost_telemetry.h"
#include "ghost_vault.h"

void device_telem_clear(Device* d) {
    if (d == nullptr) return;
    d->ram_mb = TELEM_ABSENT16;
    d->cpu_percent = TELEM_ABSENT8;
    d->gpu_percent = TELEM_ABSENT8;
    d->traffic_kbps = TELEM_ABSENT16;
    d->battery_percent = TELEM_ABSENT8;
    d->wifi_mbit = TELEM_ABSENT16;
}

static uint16_t rd16(const char* p) {
    return static_cast<uint16_t>(static_cast<uint8_t>(p[0])) |
           (static_cast<uint16_t>(static_cast<uint8_t>(p[1])) << 8);
}

static void wr16(char* p, uint16_t v) {
    p[0] = static_cast<char>(v & 0xFFu);
    p[1] = static_cast<char>((v >> 8) & 0xFFu);
}

static bool pct_ok(uint8_t v) {
    return v <= 100 || v == TELEM_ABSENT8;
}

bool parseTelemetryFields(const Event& event,
                          uint16_t* ram_mb,
                          uint8_t* cpu_percent,
                          uint8_t* gpu_percent,
                          uint16_t* traffic_kbps,
                          uint8_t* battery_percent,
                          uint16_t* wifi_mbit) {
    if (event.payload[0] != static_cast<char>(TELEM_MAGIC)) return false;
    if (ram_mb != nullptr) *ram_mb = rd16(event.payload + 1);
    if (cpu_percent != nullptr) *cpu_percent = static_cast<uint8_t>(event.payload[3]);
    if (gpu_percent != nullptr) *gpu_percent = static_cast<uint8_t>(event.payload[4]);
    if (traffic_kbps != nullptr) *traffic_kbps = rd16(event.payload + 5);
    if (battery_percent != nullptr) {
        *battery_percent = static_cast<uint8_t>(event.payload[7]);
    }
    if (wifi_mbit != nullptr) *wifi_mbit = rd16(event.payload + 8);
    return true;
}

bool validateTelemetryPayload(const Event& event, uint8_t role) {
    if (event.type != EventType::TelemetryUpdate) return false;
    if (static_cast<uint8_t>(event.payload[0]) != TELEM_MAGIC) return false;
    for (uint8_t i = 10; i < VAULT_MAC_OFF; ++i) {
        if (event.payload[i] != 0) return false;
    }
    uint16_t ram = 0, traf = 0, wifi = 0;
    uint8_t cpu = 0, gpu = 0, bat = 0;
    if (!parseTelemetryFields(event, &ram, &cpu, &gpu, &traf, &bat, &wifi)) {
        return false;
    }
    if (!pct_ok(cpu) || !pct_ok(gpu) || !pct_ok(bat)) return false;
    bool ram_a = ram == TELEM_ABSENT16;
    bool cpu_a = cpu == TELEM_ABSENT8;
    bool gpu_a = gpu == TELEM_ABSENT8;
    bool tr_a = traf == TELEM_ABSENT16;
    bool bat_a = bat == TELEM_ABSENT8;
    bool wi_a = wifi == TELEM_ABSENT16;

    if (role == ROLE_WORKER) {
        if (ram_a || cpu_a || gpu_a || tr_a || bat_a || wi_a) return false;
        return true;
    }
    if (role == ROLE_PHONE) {
        if (ram_a || cpu_a || bat_a || wi_a) return false;
        if (!gpu_a || !tr_a) return false;
        return true;
    }
    if (role == ROLE_SENSOR) {
        if (cpu_a || bat_a) return false;
        if (!ram_a || !gpu_a || !tr_a || !wi_a) return false;
        return true;
    }
    if (role == ROLE_ROUTER) {
        if (tr_a || wi_a) return false;
        if (!ram_a || !cpu_a || !gpu_a || !bat_a) return false;
        return true;
    }
    return false;
}

void fillTelemetryPayload(Event* event,
                          uint16_t ram_mb,
                          uint8_t cpu_percent,
                          uint8_t gpu_percent,
                          uint16_t traffic_kbps,
                          uint8_t battery_percent,
                          uint16_t wifi_mbit) {
    if (event == nullptr) return;
    for (uint8_t i = 0; i < 128; ++i) event->payload[i] = 0;
    event->payload[0] = static_cast<char>(TELEM_MAGIC);
    wr16(event->payload + 1, ram_mb);
    event->payload[3] = static_cast<char>(cpu_percent);
    event->payload[4] = static_cast<char>(gpu_percent);
    wr16(event->payload + 5, traffic_kbps);
    event->payload[7] = static_cast<char>(battery_percent);
    wr16(event->payload + 8, wifi_mbit);
}
