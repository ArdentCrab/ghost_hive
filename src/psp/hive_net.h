#ifndef HIVE_NET_H
#define HIVE_NET_H

// SPEC-v1 AMEND §22a — IBSS + static kernel IPv4. Not a new module.
// 1.7.3 §2.2: never WPA2/WPA3 client, never Infrastructure-STA.

#include <stdint.h>

const char HIVE_IBSS_SSID[9] = "GHSTHIVE";
const uint32_t HIVE_KERNEL_IPV4 = 0x0A112F01u; /* 10.17.47.1 */
const uint32_t HIVE_NETMASK_IPV4 = 0xFFFFFF00u;

bool hive_net_up();
void hive_net_down();
bool hive_net_ready();

#endif
