#ifndef PSP_TIME_H
#define PSP_TIME_H

// =====================================================
// Ghost Hive v1.7.1
// Zeitquelle
// Spec-Basis: §23, §31, §44
// PSP-Tick / Host-Uhr. Kein NTP. Kein Fake-Inkrement.
// =====================================================

#include <stdint.h>

uint32_t psp_now_ms();
uint32_t psp_now_sec();
void psp_sleep_ms(uint32_t ms);

#endif // PSP_TIME_H
