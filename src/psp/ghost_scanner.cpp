#include "ghost_scanner.h"
#include "hive_net.h"

#if defined(__PSP__) || defined(PSP_BUILD)
#include <pspwlan.h>
#include <pspnet.h>
#include <psptypes.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#define GHOST_PSP_HW 1
#endif

#ifdef GHOST_PSP_HW
extern "C" {
int sceNet_lib_5216CBF5(const char* name);
int sceNet_lib_7BA3ED91(const char* name, void* type, u32* size, void* buf, u32* unk);
int sceNet_lib_D2422E4D(const char* name);
}

#define PspWlanInitScan sceNet_lib_5216CBF5
#define PspWlanScanAps  sceNet_lib_7BA3ED91
#define PspWlanTermScan sceNet_lib_D2422E4D

const uint32_t PSP_WLAN_NOT_READY = 0x80410D0Eu;
const uint16_t PSP_WLAN_RAW_LEN = 0xA80;

struct PspWlanBss {
    void* pNext;
    uint8_t bssid[6];
    char channel;
    uint8_t namesize;
    char name[32];
    uint32_t bsstype;
    uint32_t beaconperiod;
    uint32_t dtimperiod;
    uint32_t timestamp;
    uint32_t localtime;
    uint16_t atim;
    uint16_t capabilities;
    uint8_t rate[8];
    uint16_t rssi;
    uint8_t sizepad[6];
} __attribute__((packed));

static uint8_t s_raw_scan_[PSP_WLAN_RAW_LEN];
static bool s_net_ready_ = false;

static void psp_zero(uint8_t* p, uint16_t n) {
    for (uint16_t i = 0; i < n; ++i) p[i] = 0;
}

static void psp_copy_ssid(char* dst, const char* src, uint8_t n) {
    uint8_t i = 0;
    uint8_t max = n;
    if (max > 32) max = 32;
    while (i < max && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static void psp_format_bssid(char* dst, const uint8_t* mac) {
    const char* hex = "0123456789ABCDEF";
    uint8_t o = 0;
    for (uint8_t i = 0; i < 6; ++i) {
        dst[o++] = hex[(mac[i] >> 4) & 0x0F];
        dst[o++] = hex[mac[i] & 0x0F];
        if (i < 5) dst[o++] = ':';
    }
    dst[o] = '\0';
}

static void psp_free_raw() {
    psp_zero(s_raw_scan_, PSP_WLAN_RAW_LEN);
}

static bool psp_net_up() {
    if (s_net_ready_) return true;
    (void)sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    (void)sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
    (void)sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
    s_net_ready_ = true;
    return true;
}

static bool psp_attach_radio() {
    if (sceWlanGetSwitchState() == 0) return false;
    int ret = sceWlanDevAttach();
    if (ret == static_cast<int>(PSP_WLAN_NOT_READY)) return false;
    if (ret < 0) return false;
    return true;
}

static uint8_t psp_fill_networks(WifiNetwork* out, uint8_t max, uint32_t raw_bytes) {
    uint8_t n = 0;
    uint16_t entry = static_cast<uint16_t>(sizeof(PspWlanBss));
    if (entry == 0 || raw_bytes < entry) return 0;
    uint32_t found = raw_bytes / entry;
    if (found > max) found = max;

    const PspWlanBss* list = reinterpret_cast<const PspWlanBss*>(s_raw_scan_);
    for (uint32_t i = 0; i < found; ++i) {
        const PspWlanBss& ap = list[i];
        bool empty = true;
        for (uint8_t b = 0; b < 6; ++b) {
            if (ap.bssid[b] != 0) empty = false;
        }
        if (empty && ap.namesize == 0 && ap.name[0] == '\0') continue;

        psp_copy_ssid(out[n].ssid, ap.name, ap.namesize);
        psp_format_bssid(out[n].bssid, ap.bssid);
        int16_t rssi = static_cast<int16_t>(ap.rssi);
        if (rssi > 127) rssi = 127;
        if (rssi < -128) rssi = -128;
        out[n].rssi = static_cast<int8_t>(rssi);
        out[n].channel = static_cast<uint8_t>(ap.channel);
        out[n].encryption = (ap.capabilities & (1u << 4)) ? 1 : 0;
        ++n;
        if (n >= max) break;
    }
    return n;
}

static uint8_t psp_scan_bss(WifiNetwork* out, uint8_t max) {
    if (!psp_net_up()) return 0;
    if (!psp_attach_radio()) return 0;

    psp_free_raw();

    if (PspWlanInitScan("wlan") < 0) {
        PspWlanTermScan("wlan");
        return 0;
    }

    uint8_t type[0x4C];
    psp_zero(type, 0x4C);
    for (uint8_t ch = 1; ch < 0x0F; ++ch) {
        type[0x09 + ch] = ch;
    }
    type[0x3C] = 1;
    uint32_t min_rssi = 1;
    uint32_t max_rssi = 100;
    type[0x44] = static_cast<uint8_t>(min_rssi);
    type[0x45] = static_cast<uint8_t>(min_rssi >> 8);
    type[0x46] = static_cast<uint8_t>(min_rssi >> 16);
    type[0x47] = static_cast<uint8_t>(min_rssi >> 24);
    type[0x48] = static_cast<uint8_t>(max_rssi);
    type[0x49] = static_cast<uint8_t>(max_rssi >> 8);
    type[0x4A] = static_cast<uint8_t>(max_rssi >> 16);
    type[0x4B] = static_cast<uint8_t>(max_rssi >> 24);

    u32 size = PSP_WLAN_RAW_LEN;
    u32 unk = 0;
    int ret = PspWlanScanAps("wlan", type, &size, s_raw_scan_, &unk);
    uint8_t count = 0;
    if (ret >= 0) {
        count = psp_fill_networks(out, max, size);
    }

    PspWlanTermScan("wlan");
    psp_free_raw();
    return count;
}
#endif

static void copy_wifi_net(WifiNetwork* dst, const WifiNetwork* src) {
    uint8_t i = 0;
    while (i < 33) {
        dst->ssid[i] = src->ssid[i];
        ++i;
    }
    i = 0;
    while (i < 18) {
        dst->bssid[i] = src->bssid[i];
        ++i;
    }
    dst->rssi = src->rssi;
    dst->channel = src->channel;
    dst->encryption = src->encryption;
}

static bool ssid_is_hive(const char* ssid) {
    const char* h = HIVE_IBSS_SSID;
    uint8_t i = 0;
    if (ssid == nullptr) return false;
    while (h[i] != '\0') {
        if (ssid[i] != h[i]) return false;
        ++i;
    }
    return ssid[i] == '\0';
}

GhostScanner::GhostScanner() {
    init();
}

void GhostScanner::clearHud() {
    hud_n_ = 0;
    hud_hive_n_ = 0;
    hud_foreign_ = 0;
    uint8_t* p = reinterpret_cast<uint8_t*>(&hud_[0]);
    for (uint8_t i = 0; i < sizeof(hud_); ++i) p[i] = 0;
}

void GhostScanner::snapshotHud() {
    hud_n_ = (wifiCount_ > 2) ? 2 : wifiCount_;
    hud_hive_n_ = 0;
    hud_foreign_ = 0;
    for (uint8_t i = 0; i < wifiCount_; ++i) {
        if (ssid_is_hive(wifi_[i].ssid)) ++hud_hive_n_;
        else ++hud_foreign_;
        if (i < 2) copy_wifi_net(&hud_[i], &wifi_[i]);
    }
}

void GhostScanner::init() {
    wifiCount_ = 0;
    btCount_ = 0;
    terminalMode_ = false;
    lastScanBlocked_ = false;
    clearBuffer();
    clearHud();
}

void GhostScanner::setTerminalMode(bool on) {
    terminalMode_ = on;
    if (!on) {
        releaseBuffer();
    }
}

bool GhostScanner::terminalMode() const {
    return terminalMode_;
}

bool GhostScanner::lastScanBlocked() const {
    return lastScanBlocked_;
}

void GhostScanner::clearBuffer() {
    for (uint8_t n = 0; n < SCAN_BUFFER_SIZE; ++n) {
        uint8_t* p = reinterpret_cast<uint8_t*>(&wifi_[n]);
        for (uint8_t i = 0; i < sizeof(WifiNetwork); ++i) p[i] = 0;
    }
}

void GhostScanner::clearBuffers() {
    releaseBuffer();
}

void GhostScanner::releaseBuffer() {
    clearBuffer();
    wifiCount_ = 0;
    btCount_ = 0;
#ifdef GHOST_PSP_HW
    psp_free_raw();
#endif
}

bool GhostScanner::scanWifi() {
    lastScanBlocked_ = false;
    if (!terminalMode_) {
        lastScanBlocked_ = true;
        releaseBuffer();
        clearHud();
        return false;
    }

    clearBuffer();
    wifiCount_ = 0;

#ifdef GHOST_PSP_HW
    if (hive_net_ready()) {
        /* IBSS already up (SPEC-v1 GHSTHIVE). Infrastructure BSS-scan would tear it. */
        wifiCount_ = 1;
        uint8_t i = 0;
        const char* ss = HIVE_IBSS_SSID;
        while (ss[i] != '\0' && i < 32) {
            wifi_[0].ssid[i] = ss[i];
            ++i;
        }
        wifi_[0].ssid[i] = '\0';
        wifi_[0].bssid[0] = '\0';
        wifi_[0].rssi = 0;
        wifi_[0].channel = 0;
        wifi_[0].encryption = 0;
    } else {
        wifiCount_ = psp_scan_bss(wifi_, SCAN_BUFFER_SIZE);
    }
#else
    (void)0;
#endif

    snapshotHud();
    return true;
}

bool GhostScanner::scanBluetooth() {
    lastScanBlocked_ = false;
    if (!terminalMode_) {
        lastScanBlocked_ = true;
        btCount_ = 0;
        return false;
    }
    // §12.2 scan_bt im bestehenden ghost-scanner. PSP-1004 FAT hat kein BT-Radio:
    // Slot läuft, Buffer frei, 0 Ergebnisse. Keine erfundenen Geräte.
    btCount_ = 0;
    return true;
}

bool GhostScanner::scanIr() {
    lastScanBlocked_ = false;
    if (!terminalMode_) {
        lastScanBlocked_ = true;
        return false;
    }
    return true;
}

uint8_t GhostScanner::getWifiCount() const {
    return wifiCount_;
}

uint8_t GhostScanner::getBtCount() const {
    return btCount_;
}

const WifiNetwork* GhostScanner::getWifi(uint8_t index) const {
    if (index >= wifiCount_) return nullptr;
    return &wifi_[index];
}

uint8_t GhostScanner::hudWifiCount() const {
    return hud_n_;
}

const WifiNetwork* GhostScanner::hudWifi(uint8_t index) const {
    if (index >= hud_n_) return nullptr;
    return &hud_[index];
}

uint8_t GhostScanner::hudHiveSsidCount() const {
    return hud_hive_n_;
}

uint8_t GhostScanner::hudForeignCount() const {
    return hud_foreign_;
}

bool GhostScanner::loadWifiSnapshot(const WifiNetwork* nets, uint8_t n) {
    if (!terminalMode_) return false;
    clearBuffer();
    wifiCount_ = 0;
    if (nets != nullptr) {
        uint8_t max = n;
        if (max > SCAN_BUFFER_SIZE) max = SCAN_BUFFER_SIZE;
        for (uint8_t i = 0; i < max; ++i) {
            copy_wifi_net(&wifi_[i], &nets[i]);
            ++wifiCount_;
        }
    }
    snapshotHud();
    return true;
}
