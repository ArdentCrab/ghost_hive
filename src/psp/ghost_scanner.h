#ifndef GHOST_SCANNER_H
#define GHOST_SCANNER_H

// =====================================================
// Ghost Hive v1.7.1
// Scanner — Phase A
// Spec-Basis: §2.2, §10, §12.2, §44
// Aktiv nur Terminal-Mode. Nie WPA2/WPA3-Client, nie Associate.
// PSP: BSS-Scan über sceNet_lib (wlanscan), Host: 0 Netze.
// =====================================================

#include "ghost_core.h"
#include "ghost_data.h"

const uint8_t SCAN_BUFFER_SIZE = 32;

class GhostScanner {
public:
    GhostScanner();

    void init();
    void setTerminalMode(bool on);
    bool terminalMode() const;
    bool lastScanBlocked() const;

    bool scanWifi();
    bool scanBluetooth();
    bool scanIr();

    void clearBuffers();
    void releaseBuffer();

    uint8_t getWifiCount() const;
    const WifiNetwork* getWifi(uint8_t index) const;
    uint8_t getBtCount() const;

    /* HUD keeps 2 APs after releaseBuffer() (§12.2 free raw/wifi slots). */
    uint8_t hudWifiCount() const;
    const WifiNetwork* hudWifi(uint8_t index) const;
    uint8_t hudHiveSsidCount() const;
    uint8_t hudForeignCount() const;
    bool loadWifiSnapshot(const WifiNetwork* nets, uint8_t n);

private:
    WifiNetwork wifi_[SCAN_BUFFER_SIZE];
    uint8_t wifiCount_;
    uint8_t btCount_;
    bool terminalMode_;
    bool lastScanBlocked_;
    WifiNetwork hud_[2];
    uint8_t hud_n_;
    uint8_t hud_hive_n_;
    uint8_t hud_foreign_;

    void clearBuffer();
    void snapshotHud();
    void clearHud();
};

#endif // GHOST_SCANNER_H
