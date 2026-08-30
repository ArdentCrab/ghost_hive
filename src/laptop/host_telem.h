#ifndef HOST_TELEM_H
#define HOST_TELEM_H

#include "ghost_core.h"
#include "ghost_telemetry.h"

bool host_telem_sample(uint32_t now_sec,
                       uint16_t* ram_mb,
                       uint8_t* cpu_percent,
                       uint8_t* gpu_percent,
                       uint16_t* traffic_kbps,
                       uint8_t* battery_percent,
                       uint16_t* wifi_mbit);

void host_telem_apply_role(uint8_t role,
                           uint16_t* ram_mb,
                           uint8_t* cpu_percent,
                           uint8_t* gpu_percent,
                           uint16_t* traffic_kbps,
                           uint8_t* battery_percent,
                           uint16_t* wifi_mbit);

#endif
