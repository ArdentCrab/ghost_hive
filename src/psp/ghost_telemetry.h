#ifndef GHOST_TELEMETRY_H
#define GHOST_TELEMETRY_H

#include "ghost_core.h"

const uint8_t TELEM_MAGIC = 0x42;
const uint8_t TELEM_ABSENT8 = 0xFF;
const uint16_t TELEM_ABSENT16 = 0xFFFFu;
const uint8_t TELEM_INTERVAL_SEC = 2;

void device_telem_clear(Device* d);
bool validateTelemetryPayload(const Event& event, uint8_t role);
bool parseTelemetryFields(const Event& event,
                          uint16_t* ram_mb,
                          uint8_t* cpu_percent,
                          uint8_t* gpu_percent,
                          uint16_t* traffic_kbps,
                          uint8_t* battery_percent,
                          uint16_t* wifi_mbit);
void fillTelemetryPayload(Event* event,
                          uint16_t ram_mb,
                          uint8_t cpu_percent,
                          uint8_t gpu_percent,
                          uint16_t traffic_kbps,
                          uint8_t battery_percent,
                          uint16_t wifi_mbit);

#endif
