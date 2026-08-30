#ifndef GHOST_DATA_H
#define GHOST_DATA_H

// =====================================================
// Ghost Hive v1.7.1
// Anzeige-/Scan-Daten
// Spec-Basis: §12, §13, §14, §24
// Nur Felder, die Module später real befüllen.
// =====================================================

#include <stdint.h>

struct WifiNetwork {
    char ssid[33];
    char bssid[18];
    int8_t rssi;
    uint8_t channel;
    uint8_t encryption;     // 0=none, 1=WEP, 2=WPA, 3=WPA2, 4=WPA3
};

struct MineInfo {
    char mine_id[32];
    uint8_t status;         // 0=silent, 1=blocked, 2=replay, 3=triggered
    uint32_t lastEvent;
};

struct HeartbeatInfo {
    uint32_t lastBeat;
    uint8_t missCount;
};

struct StealthInfo {
    uint8_t gameMode;
    uint8_t invisible;
};

struct DeviceInfo {
    char id[32];
    uint8_t role;
    uint8_t status;
    uint32_t lastSeen;
};

#endif // GHOST_DATA_H
