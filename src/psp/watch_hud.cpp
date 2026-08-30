#include "ghost_output.h"
#include "watch_hud.h"
#include "hive_net.h"
#include "ghost_arm.h"
#include "ghost_keys.h"
#include "ghost_telemetry.h"
#include "decision_pipeline.h"
#include "transport/transport_frame.h"

#include <stdio.h>
#include <time.h>

#if defined(__PSP__)
#include <psppower.h>
#include <pspwlan.h>
#endif

#if defined(__PSP__)
static const char BIND_PATH[] = "ms0:/ghost_hive/k/peer.bind";
#else
static const char BIND_PATH[] = "/tmp/ghost_hive/peer.bind";
#endif

bool watch_bind_ok() {
    FILE* f = fopen(BIND_PATH, "rb");
    if (f == nullptr) return false;
    bool ok = false;
    if (fseek(f, 0, SEEK_END) == 0) {
        long sz = ftell(f);
        if (sz == static_cast<long>(GhostKeys::PEER_BIND_LEN)) ok = true;
    }
    fclose(f);
    return ok;
}

static void fmt_clock(uint32_t sec, char* dst) {
    time_t t = static_cast<time_t>(sec);
    struct tm tmv;
    if (gmtime_r(&t, &tmv) == nullptr) {
        dst[0] = '-';
        dst[1] = '-';
        dst[2] = '\0';
        return;
    }
    int y = tmv.tm_year + 1900;
    int mo = tmv.tm_mon + 1;
    dst[0] = static_cast<char>('0' + ((y / 1000) % 10));
    dst[1] = static_cast<char>('0' + ((y / 100) % 10));
    dst[2] = static_cast<char>('0' + ((y / 10) % 10));
    dst[3] = static_cast<char>('0' + (y % 10));
    dst[4] = '-';
    dst[5] = static_cast<char>('0' + (mo / 10));
    dst[6] = static_cast<char>('0' + (mo % 10));
    dst[7] = '-';
    dst[8] = static_cast<char>('0' + (tmv.tm_mday / 10));
    dst[9] = static_cast<char>('0' + (tmv.tm_mday % 10));
    dst[10] = ' ';
    dst[11] = static_cast<char>('0' + (tmv.tm_hour / 10));
    dst[12] = static_cast<char>('0' + (tmv.tm_hour % 10));
    dst[13] = ':';
    dst[14] = static_cast<char>('0' + (tmv.tm_min / 10));
    dst[15] = static_cast<char>('0' + (tmv.tm_min % 10));
    dst[16] = '\0';
}

static void fmt_hhmm(uint32_t sec, char* dst) {
    uint32_t sod = sec % 86400u;
    uint32_t hh = sod / 3600u;
    uint32_t mm = (sod % 3600u) / 60u;
    dst[0] = static_cast<char>('0' + (hh / 10u));
    dst[1] = static_cast<char>('0' + (hh % 10u));
    dst[2] = ':';
    dst[3] = static_cast<char>('0' + (mm / 10u));
    dst[4] = static_cast<char>('0' + (mm % 10u));
    dst[5] = '\0';
}

static const char* roleCap(uint8_t role) {
    switch (role) {
        case ROLE_WORKER: return "Worker";
        case ROLE_PHONE: return "Phone";
        case ROLE_KERNEL: return "Kernel";
        case ROLE_SAFE: return "Safe";
        case ROLE_SENSOR: return "Sensor";
        case ROLE_ROUTER: return "Router";
        case ROLE_MINE: return "Mine";
        default: return "?";
    }
}

static const char* downPhase(DownStep step, bool active) {
    if (!active) return "idle";
    switch (step) {
        case DownStep::Idle: return "idle";
        case DownStep::StealthConceal:
        case DownStep::Stop:
        case DownStep::Done:
            return "freeze";
        case DownStep::RamSnapshot:
        case DownStep::FinalSnapshot:
            return "snapshot";
        case DownStep::NasFlushWait:
        case DownStep::StorageFlushWait:
        case DownStep::StorageFlushRetry:
            return "flush";
        case DownStep::Kill:
            return "kill";
        default:
            return "idle";
    }
}

static const char* downView(const GhostDown& down) {
    if (down.isActive()) return "active";
    GhostArming a = ghost_arming();
    if (a == GhostArming::Observe) return "observe";
    if (a == GhostArming::Armed) return "idle";
    return "locked";
}

static const char* armView() {
    GhostArming a = ghost_arming();
    if (a == GhostArming::Observe) return "observe";
    if (a == GhostArming::Armed) return "armed";
    return "locked";
}

static uint8_t hbMissSum(GhostHeartbeat* hb) {
    if (hb == nullptr) return 0;
    uint8_t n = hb->trackedCount();
    uint8_t s = 0;
    for (uint8_t i = 0; i < n; ++i) {
        const char* id = hb->idAt(i);
        if (id == nullptr) continue;
        s = static_cast<uint8_t>(s + hb->getMissCount(id));
    }
    return s;
}

static uint8_t replayBlk(ReplayGuard* g) {
    if (g == nullptr) return 0;
    uint8_t n = g->trackedCount();
    uint8_t b = 0;
    for (uint8_t i = 0; i < n; ++i) {
        if (g->blockedAt(i)) ++b;
    }
    return b;
}

static uint8_t alertCount(EventQueue* q) {
    if (q == nullptr) return 0;
    uint8_t n = q->getSize();
    uint8_t c = 0;
    for (uint8_t i = 0; i < n; ++i) {
        const Event* e = q->peek(i);
        if (e != nullptr && e->type == EventType::AlertSent) ++c;
    }
    return c;
}

static bool anyDanger(Registry* r) {
    if (r == nullptr) return false;
    uint8_t n = r->getDeviceCount();
    for (uint8_t i = 0; i < n; ++i) {
        DeviceInfo info = r->getDeviceInfo(i);
        if (static_cast<DeviceState>(info.status) == DeviceState::DangerMode) {
            return true;
        }
    }
    return false;
}

static void workerSync(Registry* r, char* dst) {
    dst[0] = '-';
    dst[1] = '-';
    dst[2] = '\0';
    if (r == nullptr) return;
    const Device* w = r->findByRole(ROLE_WORKER);
    if (w == nullptr || w->trust_level < 2) return;
    dst[0] = 't'; dst[1] = 'r'; dst[2] = 'u'; dst[3] = 's';
    dst[4] = 't'; dst[5] = '>'; dst[6] = '='; dst[7] = '2';
    dst[8] = '\0';
}

static void workerDrift(Registry* r, uint32_t now_sec, char* dst) {
    dst[0] = '-';
    dst[1] = '-';
    dst[2] = '\0';
    if (r == nullptr) return;
    const Device* w = r->findByRole(ROLE_WORKER);
    if (w == nullptr || w->trust_level < 2) return;
    int32_t d = static_cast<int32_t>(w->last_seen) - static_cast<int32_t>(now_sec);
    char tmp[16];
    uint8_t n = 0;
    uint32_t mag;
    if (d < 0) {
        tmp[n++] = '-';
        mag = static_cast<uint32_t>(-d);
    } else {
        tmp[n++] = '+';
        mag = static_cast<uint32_t>(d);
    }
    if (mag == 0) tmp[n++] = '0';
    else {
        char dig[11];
        uint8_t k = 0;
        while (mag > 0 && k < 10) {
            dig[k++] = static_cast<char>('0' + (mag % 10));
            mag /= 10;
        }
        while (k > 0) tmp[n++] = dig[--k];
    }
    tmp[n++] = 's';
    tmp[n] = '\0';
    uint8_t i = 0;
    while (tmp[i] != '\0' && i < 15) {
        dst[i] = tmp[i];
        ++i;
    }
    dst[i] = '\0';
}

static const char* stShort(DeviceState st) {
    switch (st) {
        case DeviceState::Online: return "ok";
        case DeviceState::Pending: return "pend";
        case DeviceState::Degraded: return "deg";
        case DeviceState::Offline: return "off";
        case DeviceState::Blocked: return "blk";
        case DeviceState::Silent: return "sil";
        case DeviceState::GhostDown: return "down";
        case DeviceState::DangerMode: return "dng";
        case DeviceState::Suspected: return "sus";
        case DeviceState::Unknown: return "?";
        default: return "?";
    }
}

static void put_u8(char* dst, uint8_t* n, uint8_t max, uint8_t v) {
    if (v == 0) {
        if (*n + 1 < max) dst[(*n)++] = '0';
        return;
    }
    char dig[4];
    uint8_t d = 0;
    uint8_t x = v;
    while (x > 0 && d < 3) {
        dig[d++] = static_cast<char>('0' + (x % 10));
        x = static_cast<uint8_t>(x / 10);
    }
    while (d > 0 && *n + 1 < max) dst[(*n)++] = dig[--d];
}

static void put_i8(char* dst, uint8_t* n, uint8_t max, int8_t v) {
    if (v < 0) {
        if (*n + 1 < max) dst[(*n)++] = '-';
        if (v == static_cast<int8_t>(-128)) {
            put_u8(dst, n, max, 128);
            return;
        }
        v = static_cast<int8_t>(-v);
    }
    put_u8(dst, n, max, static_cast<uint8_t>(v));
}

static void put_hex16(char* dst, uint8_t* n, uint8_t max, uint16_t v) {
    const char* h = "0123456789abcdef";
    if (*n + 4 >= max) return;
    dst[(*n)++] = h[(v >> 12) & 0x0f];
    dst[(*n)++] = h[(v >> 8) & 0x0f];
    dst[(*n)++] = h[(v >> 4) & 0x0f];
    dst[(*n)++] = h[v & 0x0f];
}

static void put_hex8(char* dst, uint8_t* n, uint8_t max, uint8_t v) {
    const char* h = "0123456789abcdef";
    if (*n + 2 >= max) return;
    dst[(*n)++] = h[(v >> 4) & 0x0f];
    dst[(*n)++] = h[v & 0x0f];
}

static void hiveCensus(Registry* r, uint8_t* n, uint8_t* pend, uint8_t* on,
                       uint8_t* blk, uint8_t* sil) {
    *n = 0;
    *pend = 0;
    *on = 0;
    *blk = 0;
    *sil = 0;
    if (r == nullptr) return;
    *n = r->getDeviceCount();
    for (uint8_t i = 0; i < *n; ++i) {
        DeviceInfo inf = r->getDeviceInfo(i);
        DeviceState st = static_cast<DeviceState>(inf.status);
        if (st == DeviceState::Pending) ++(*pend);
        else if (st == DeviceState::Online) ++(*on);
        else if (st == DeviceState::Blocked) ++(*blk);
        else if (st == DeviceState::Silent) ++(*sil);
    }
}

static void hiveRoles(Registry* r, char* dst) {
    uint8_t p = 0;
    const uint8_t roles[6] = {
        ROLE_WORKER, ROLE_PHONE, ROLE_SENSOR, ROLE_ROUTER, ROLE_SAFE, ROLE_MINE
    };
    const char tags[6] = { 'W', 'P', 'S', 'R', 'N', 'M' };
    for (uint8_t i = 0; i < 6; ++i) {
        if (i > 0 && p < 46) dst[p++] = ' ';
        if (p < 46) dst[p++] = tags[i];
        uint8_t c = r != nullptr ? r->getRoleCount(roles[i]) : 0;
        put_u8(dst, &p, 47, c);
    }
    dst[p] = '\0';
}

static uint32_t telemDropSum(const GhostOutput::WatchSrc& src) {
    if (src.pipeline == nullptr || src.registry == nullptr) return 0;
    uint32_t s = 0;
    uint8_t n = src.registry->getDeviceCount();
    for (uint8_t i = 0; i < n; ++i) {
        DeviceInfo inf = src.registry->getDeviceInfo(i);
        s += src.pipeline->telemDenseDrops(inf.id);
    }
    return s;
}

static void put_str(char* dst, uint8_t* n, uint8_t max, const char* s) {
    if (s == nullptr) return;
    uint8_t k = 0;
    while (s[k] != '\0' && *n + 1 < max) dst[(*n)++] = s[k++];
}

static const char* enc_lab(uint8_t e) {
    switch (e) {
        case 0: return "open";
        case 1: return "wep";
        case 2: return "wpa";
        case 3: return "wpa2";
        case 4: return "wpa3";
        default: return "?";
    }
}

static bool ssid_hive(const char* ssid) {
    const char* h = HIVE_IBSS_SSID;
    uint8_t i = 0;
    if (ssid == nullptr) return false;
    while (h[i] != '\0') {
        if (ssid[i] != h[i]) return false;
        ++i;
    }
    return ssid[i] == '\0';
}

static void fill_ap_line(char* dst, const WifiNetwork* net, bool twin) {
    uint8_t n = 0;
    if (net == nullptr) {
        dst[0] = '-';
        dst[1] = '-';
        dst[2] = '\0';
        return;
    }
    uint8_t s = 0;
    while (net->ssid[s] != '\0' && n < 12) dst[n++] = net->ssid[s++];
    put_str(dst, &n, 48, " r=");
    put_i8(dst, &n, 48, net->rssi);
    put_str(dst, &n, 48, " ch=");
    put_u8(dst, &n, 48, net->channel);
    put_str(dst, &n, 48, " ");
    put_str(dst, &n, 48, enc_lab(net->encryption));
    put_str(dst, &n, 48, " twin=");
    put_str(dst, &n, 48, (twin && ssid_hive(net->ssid)) ? "yes" : "no");
    dst[n] = '\0';
}

static void put_u32(char* dst, uint8_t* n, uint8_t max, uint32_t v) {
    if (v == 0) {
        if (*n + 1 < max) dst[(*n)++] = '0';
        return;
    }
    char dig[11];
    uint8_t d = 0;
    while (v > 0 && d < 10) {
        dig[d++] = static_cast<char>('0' + (v % 10));
        v /= 10;
    }
    while (d > 0 && *n + 1 < max) dst[(*n)++] = dig[--d];
}

static const char* evShort(EventType t) {
    switch (t) {
        case EventType::ScanResult: return "Scan";
        case EventType::DeviceSeen: return "Seen";
        case EventType::DeviceLost: return "Lost";
        case EventType::ProfileUpdate: return "Prof";
        case EventType::AnomalyDetected: return "Anom";
        case EventType::PolicyViolation: return "PViol";
        case EventType::BackupWritten: return "Bak";
        case EventType::AlertSent: return "Alert";
        case EventType::Heartbeat: return "HBeat";
        case EventType::HeartbeatMiss: return "HMiss";
        case EventType::RoleChange: return "Role";
        case EventType::ConfigChange: return "Cfg";
        case EventType::GhostDownStart: return "GDStart";
        case EventType::GhostDownEnd: return "GDEnd";
        case EventType::PeekScan: return "Peek";
        case EventType::DangerModeEnter: return "Dng+";
        case EventType::DangerModeExit: return "Dng-";
        case EventType::MineEvent: return "Mine";
        case EventType::TelemetryUpdate: return "Telem";
        default: return "?";
    }
}

static uint8_t stRank(DeviceState st) {
    switch (st) {
        case DeviceState::DangerMode: return 0;
        case DeviceState::GhostDown: return 1;
        case DeviceState::Blocked: return 2;
        case DeviceState::Silent: return 3;
        case DeviceState::Suspected: return 4;
        case DeviceState::Degraded: return 5;
        case DeviceState::Offline: return 6;
        case DeviceState::Pending: return 7;
        case DeviceState::Unknown: return 8;
        case DeviceState::Online: return 9;
        default: return 10;
    }
}

static const char* hiveHead(Registry* r) {
    if (r == nullptr || r->getDeviceCount() == 0) return "empty";
    uint8_t n = r->getDeviceCount();
    uint8_t worst = 9;
    for (uint8_t i = 0; i < n; ++i) {
        DeviceInfo inf = r->getDeviceInfo(i);
        uint8_t rk = stRank(static_cast<DeviceState>(inf.status));
        if (rk < worst) worst = rk;
    }
    if (worst == 0) return "danger";
    if (worst == 1) return "down";
    if (worst == 2) return "block";
    if (worst <= 6) return "warn";
    if (worst == 7) return "pend";
    return "ok";
}

static void fmt_age(uint32_t last, uint32_t now_sec, char* dst) {
    if (last == 0) {
        dst[0] = '-';
        dst[1] = '-';
        dst[2] = '\0';
        return;
    }
    uint32_t a = (now_sec >= last) ? (now_sec - last) : 0;
    uint8_t n = 0;
    if (a == 0) dst[n++] = '0';
    else {
        char dig[11];
        uint8_t d = 0;
        uint32_t x = a;
        while (x > 0 && d < 10) {
            dig[d++] = static_cast<char>('0' + (x % 10));
            x /= 10;
        }
        while (d > 0 && n < 14) dst[n++] = dig[--d];
    }
    dst[n++] = 's';
    dst[n] = '\0';
}

static void fillLast(const GhostOutput::WatchSrc& src, char* dst) {
    dst[0] = '-';
    dst[1] = '-';
    dst[2] = '\0';
    if (src.events == nullptr) return;
    uint8_t n = src.events->getSize();
    if (n == 0) return;
    const Event* e = src.events->peek(static_cast<uint8_t>(n - 1u));
    if (e == nullptr) return;
    uint8_t p = 0;
    put_str(dst, &p, 47, evShort(e->type));
    if (p < 46) dst[p++] = ' ';
    uint8_t k = 0;
    while (e->source_device_id[k] != '\0' && k < 8 && p < 46) {
        dst[p++] = e->source_device_id[k++];
    }
    dst[p] = '\0';
}

static void fillAlert(const GhostOutput::WatchSrc& src, char* dst) {
    dst[0] = '-';
    dst[1] = '-';
    dst[2] = '\0';
    if (src.down != nullptr && src.down->isActive()) {
        dst[0] = 'd';
        dst[1] = 'o';
        dst[2] = 'w';
        dst[3] = 'n';
        dst[4] = '\0';
        return;
    }
    if (src.registry != nullptr) {
        uint8_t n = src.registry->getDeviceCount();
        for (uint8_t pass = 0; pass < 2; ++pass) {
            for (uint8_t i = 0; i < n; ++i) {
                DeviceInfo inf = src.registry->getDeviceInfo(i);
                DeviceState st = static_cast<DeviceState>(inf.status);
                bool hit = (pass == 0 && st == DeviceState::DangerMode) ||
                           (pass == 1 && st == DeviceState::Blocked);
                if (!hit) continue;
                uint8_t p = 0;
                put_str(dst, &p, 47, pass == 0 ? "danger " : "block ");
                uint8_t k = 0;
                while (inf.id[k] != '\0' && k < 8 && p < 46) dst[p++] = inf.id[k++];
                dst[p] = '\0';
                return;
            }
        }
    }
    if (src.hmac_alert != 0) {
        dst[0] = 'h';
        dst[1] = 'm';
        dst[2] = 'a';
        dst[3] = 'c';
        dst[4] = '-';
        dst[5] = 'i';
        dst[6] = '\0';
        return;
    }
    if (src.vault != nullptr && src.vault->safeMode()) {
        dst[0] = 'v';
        dst[1] = 'a';
        dst[2] = 'u';
        dst[3] = 'l';
        dst[4] = 't';
        dst[5] = '-';
        dst[6] = 's';
        dst[7] = 'a';
        dst[8] = 'f';
        dst[9] = 'e';
        dst[10] = '\0';
        return;
    }
}

static void fill_alert_line(const GhostOutput::WatchSrc& src, char* dst) {
    fillAlert(src, dst);
    uint8_t n = 0;
    while (dst[n] != '\0' && n < 46) ++n;
    put_str(dst, &n, 47, " last=");
    char last[48];
    fillLast(src, last);
    put_str(dst, &n, 47, last);
    dst[n] = '\0';
}

static void fill_down_combo(char* dst, const GhostDown* down, Registry* r,
                            bool engine_active) {
    uint8_t n = 0;
    if (engine_active) {
        put_str(dst, &n, 47, "active ");
        put_str(dst, &n, 47, down != nullptr ? downPhase(down->step(), true) : "idle");
    } else {
        put_str(dst, &n, 47, down != nullptr ? downView(*down) : "locked");
        put_str(dst, &n, 47, " ");
        put_str(dst, &n, 47,
                down != nullptr ? downPhase(down->step(), false) : "idle");
    }
    put_str(dst, &n, 47, " kill=");
    put_str(dst, &n, 47, (down != nullptr && down->killSent()) ? "yes" : "no");
    put_str(dst, &n, 47, " danger=");
    put_str(dst, &n, 47, anyDanger(r) ? "on" : "off");
    dst[n] = '\0';
}

static void fill_vault_combo(char* dst, GhostVault* vault) {
    uint8_t n = 0;
    uint8_t stored = vault != nullptr ? vault->getStoredCount() : 0;
    put_u8(dst, &n, 47, stored);
    put_str(dst, &n, 47, "/64 safe=");
    put_str(dst, &n, 47, (vault != nullptr && vault->safeMode()) ? "yes" : "no");
    dst[n] = '\0';
}

static void fill_sdk_clock(char* dst) {
#if defined(__PSP__)
    int mhz = scePowerGetCpuClockFrequency();
    uint8_t n = 0;
    if (mhz <= 0) {
        dst[0] = '-';
        dst[1] = '-';
        dst[2] = '\0';
        return;
    }
    put_u32(dst, &n, 47, static_cast<uint32_t>(mhz));
    put_str(dst, &n, 47, "MHz");
    dst[n] = '\0';
#else
    dst[0] = '-';
    dst[1] = '-';
    dst[2] = '\0';
#endif
}

static void fill_sdk_battery(char* dst) {
#if defined(__PSP__)
    int pct = scePowerGetBatteryLifePercent();
    int temp = scePowerGetBatteryTemp();
    uint8_t n = 0;
    if (pct < 0) {
        dst[0] = '-';
        dst[1] = '-';
        dst[2] = '\0';
        return;
    }
    put_u32(dst, &n, 47, static_cast<uint32_t>(pct));
    put_str(dst, &n, 47, "%");
    if (temp > 0) {
        put_str(dst, &n, 47, " ");
        put_u32(dst, &n, 47, static_cast<uint32_t>(temp));
        put_str(dst, &n, 47, "C");
    }
    if (scePowerIsBatteryCharging() != 0) put_str(dst, &n, 47, " Charging");
    dst[n] = '\0';
#else
    dst[0] = '-';
    dst[1] = '-';
    dst[2] = '\0';
#endif
}

static const char* fill_sdk_wlan(void) {
#if defined(__PSP__)
    return (sceWlanGetSwitchState() != 0) ? "ON" : "OFF";
#else
    return "--";
#endif
}

static void telem_lab16(GhostOutput* o, char* buffer, const char* lab, uint16_t v,
                        uint16_t abs, const char* unit) {
    if (v == abs) {
        o->wlab(buffer, lab, "--");
        return;
    }
    char buf[24];
    uint8_t n = 0;
    put_u32(buf, &n, 23, v);
    put_str(buf, &n, 23, unit);
    buf[n] = '\0';
    o->wlab(buffer, lab, buf);
}

static void telem_lab8(GhostOutput* o, char* buffer, const char* lab, uint8_t v,
                       const char* unit) {
    if (v == TELEM_ABSENT8) {
        o->wlab(buffer, lab, "--");
        return;
    }
    char buf[16];
    uint8_t n = 0;
    put_u8(buf, &n, 15, v);
    put_str(buf, &n, 15, unit);
    buf[n] = '\0';
    o->wlab(buffer, lab, buf);
}

static void writeTitle(GhostOutput* o, char* buffer, uint8_t page, bool down) {
    const char* pname = "Hive";
    if (page == 1) pname = "Kernel";
    else if (page == 2) pname = "Net";
    else if (page == 3) pname = "Peer";
    char line[49];
    uint8_t n = 0;
    const char* a = "[GHv2] PAGE:";
    uint8_t i = 0;
    while (a[i] != '\0' && n < 48) line[n++] = a[i++];
    i = 0;
    while (pname[i] != '\0' && n < 48) line[n++] = pname[i++];
    if (n < 48) line[n++] = ' ';
    const char* b = "STATE:";
    i = 0;
    while (b[i] != '\0' && n < 48) line[n++] = b[i++];
    const char* st = down ? "Down" : "Watch";
    i = 0;
    while (st[i] != '\0' && n < 48) line[n++] = st[i++];
    while (n < 48) line[n++] = ' ';
    line[n] = '\0';
    o->wraw(buffer, line);
}

static void writeSep(GhostOutput* o, char* buffer) {
    char line[49];
    for (uint8_t i = 0; i < 48; ++i) line[i] = '-';
    line[48] = '\0';
    o->wraw(buffer, line);
}

static void pageHive(GhostOutput* o, char* buffer, const GhostOutput::WatchSrc& src) {
    o->wlab(buffer, "Hive", hiveHead(src.registry));
    uint8_t pn = 0, pe = 0, on = 0, bl = 0, si = 0;
    hiveCensus(src.registry, &pn, &pe, &on, &bl, &si);
    char peers[48];
    uint8_t p = 0;
    put_u8(peers, &p, 47, pn);
    put_str(peers, &p, 47, "/");
    put_u8(peers, &p, 47, MAX_DEVICES);
    put_str(peers, &p, 47, " on=");
    put_u8(peers, &p, 47, on);
    put_str(peers, &p, 47, " pend=");
    put_u8(peers, &p, 47, pe);
    put_str(peers, &p, 47, " blk=");
    put_u8(peers, &p, 47, bl);
    put_str(peers, &p, 47, " sil=");
    put_u8(peers, &p, 47, si);
    peers[p] = '\0';
    o->wlab(buffer, "Peers", peers);
    char roles[48];
    hiveRoles(src.registry, roles);
    o->wlab(buffer, "Roles", roles);
    char al[48];
    fillAlert(src, al);
    o->wlab(buffer, "Alert", al);
    o->wlabn(buffer, "HMAC-I", src.hmac_i);
    o->wlabn(buffer, "Ack", src.ack_n);
    o->wlabn(buffer, "Drop", static_cast<int32_t>(telemDropSum(src)));
    o->wlabn(buffer, "HBMiss", hbMissSum(src.heartbeat));
    char last[48];
    fillLast(src, last);
    o->wlab(buffer, "Last", last);
    char tbuf[20];
    fmt_clock(src.now_sec, tbuf);
    o->wlab(buffer, "Time", tbuf);
    if (src.registry == nullptr) return;
    uint8_t n = src.registry->getDeviceCount();
    if (n == 0) return;
    uint8_t ord[32];
    for (uint8_t i = 0; i < n; ++i) ord[i] = i;
    for (uint8_t i = 1; i < n; ++i) {
        uint8_t key = ord[i];
        DeviceInfo ka = src.registry->getDeviceInfo(key);
        uint8_t kr = stRank(static_cast<DeviceState>(ka.status));
        int8_t j = static_cast<int8_t>(i - 1);
        while (j >= 0) {
            DeviceInfo ja = src.registry->getDeviceInfo(ord[static_cast<uint8_t>(j)]);
            uint8_t jr = stRank(static_cast<DeviceState>(ja.status));
            if (jr <= kr) break;
            ord[static_cast<uint8_t>(j + 1)] = ord[static_cast<uint8_t>(j)];
            --j;
        }
        ord[static_cast<uint8_t>(j + 1)] = key;
    }
    uint8_t show = n;
    bool extra = false;
    if (n > 11) {
        show = 10;
        extra = true;
    }
    for (uint8_t i = 0; i < show; ++i) {
        DeviceInfo inf = src.registry->getDeviceInfo(ord[i]);
        char line[49];
        uint8_t q = 0;
        uint8_t k = 0;
        while (inf.id[k] != '\0' && k < 8 && q < 47) {
            line[q++] = inf.id[k++];
        }
        while (k < 8 && q < 47) {
            line[q++] = ' ';
            ++k;
        }
        if (q < 47) line[q++] = ' ';
        put_str(line, &q, 48, roleCap(inf.role));
        if (q < 47) line[q++] = ' ';
        put_str(line, &q, 48, stShort(static_cast<DeviceState>(inf.status)));
        put_str(line, &q, 48, " age=");
        char age[16];
        fmt_age(inf.lastSeen, src.now_sec, age);
        put_str(line, &q, 48, age);
        line[q] = '\0';
        o->wraw(buffer, line);
    }
    if (extra) {
        char more[16];
        uint8_t m = 0;
        more[m++] = '+';
        put_u8(more, &m, 15, static_cast<uint8_t>(n - 10u));
        more[m] = '\0';
        o->wraw(buffer, more);
    }
}

static void pageKernelDown(GhostOutput* o, char* buffer, const GhostOutput::WatchSrc& src) {
    char dc[48];
    fill_down_combo(dc, src.down, src.registry, true);
    o->wlab(buffer, "Down", dc);
    o->wlabn(buffer, "Timer", static_cast<int32_t>(src.down_elapsed_ms));
    o->wlab(buffer, "Arming", armView());
    char vs[48];
    fill_vault_combo(vs, src.vault);
    o->wlab(buffer, "Vault", vs);
    o->wlabn(buffer, "Snap", src.down != nullptr ? src.down->snapshotCount() : 0);
    bool flush_done = src.down != nullptr && src.down->storageFlushDone();
    o->wlab(buffer, "Flush", flush_done ? "done" : "pending");
    uint8_t pc = src.policy != nullptr ? src.policy->ruleCount() : 0;
    o->wlab(buffer, "Policy", (pc == 15 || pc == 16) ? "ok" : "error");
    o->wlab(buffer, "GameLook", "off");
    {
        char rl[48];
        uint8_t n = 0;
        uint8_t rc = src.replay != nullptr ? src.replay->trackedCount() : 0;
        put_u8(rl, &n, 47, rc);
        put_str(rl, &n, 47, " ack=");
        put_u8(rl, &n, 47, src.ack_bud);
        put_str(rl, &n, 47, "/8");
        rl[n] = '\0';
        o->wlab(buffer, "Replay", rl);
    }
    o->wlab(buffer, "Hive", hiveHead(src.registry));
    char al[48];
    fill_alert_line(src, al);
    o->wlab(buffer, "Alert", al);
}

static void pageKernel(GhostOutput* o, char* buffer, const GhostOutput::WatchSrc& src) {
    if (src.down != nullptr && src.down->isActive()) {
        pageKernelDown(o, buffer, src);
        return;
    }
    o->wlab(buffer, "PSP", "1004");
    char clk[24];
    fill_sdk_clock(clk);
    o->wlab(buffer, "Clock", clk);
    char bat[32];
    fill_sdk_battery(bat);
    o->wlab(buffer, "Battery", bat);
    o->wlab(buffer, "WLAN", fill_sdk_wlan());
    o->wlab(buffer, "IBSS", hive_net_ready() ? "GHSTHIVE (ON)" : "GHSTHIVE (OFF)");
    o->wlab(buffer, "Radio", hive_net_ready() ? "on" : "off");
    o->wlab(buffer, "Kernel", src.running ? "active" : "inactive");
    o->wlab(buffer, "Arming", armView());
    uint8_t stored = src.vault != nullptr ? src.vault->getStoredCount() : 0;
    {
        char vs[16];
        uint8_t n = 0;
        put_u8(vs, &n, 15, stored);
        put_str(vs, &n, 15, "/64");
        vs[n] = '\0';
        o->wlab(buffer, "Vault", vs);
    }
    o->wlab(buffer, "Safe", (src.vault != nullptr && src.vault->safeMode()) ? "yes" : "no");
    o->wlab(buffer, "Keys", (src.vault != nullptr && src.vault->keysAttached()) ? "yes" : "no");
    o->wlab(buffer, "Auth", (src.vault != nullptr && src.vault->authBound()) ? "yes" : "no");
    o->wlab(buffer, "Bind", src.bind_ok ? "yes" : "no");
    o->wlab(buffer, "Down", src.down != nullptr ? downView(*src.down) : "locked");
    o->wlab(buffer, "Phase",
            src.down != nullptr ? downPhase(src.down->step(), src.down->isActive())
                                : "idle");
    o->wlabn(buffer, "Snap", src.down != nullptr ? src.down->snapshotCount() : 0);
    bool flush_done = src.down != nullptr && src.down->storageFlushDone();
    o->wlab(buffer, "Flush", flush_done ? "done" : "pending");
    o->wlab(buffer, "Kill", (src.down != nullptr && src.down->killSent()) ? "yes" : "no");
    uint8_t pc = src.policy != nullptr ? src.policy->ruleCount() : 0;
    o->wlab(buffer, "Policy", pc == 16 ? "ok" : "error");
    bool peek_on = src.down != nullptr && src.down->peekAllowed();
    o->wlab(buffer, "Peek", peek_on ? "on" : "off");
    o->wlabn(buffer, "HMAC-I", src.hmac_i);
}

static void pageNet(GhostOutput* o, char* buffer, const GhostOutput::WatchSrc& src) {
    o->wlab(buffer, "IBSS", hive_net_ready() ? "GHSTHIVE (ON)" : "GHSTHIVE (OFF)");
    o->wlabn(buffer, "UDP", static_cast<int32_t>(GHOST_UDP_PORT));
    o->wlab(buffer, "Pin", HIVE_IBSS_SSID);
    o->wlabn(buffer, "ReplayW", static_cast<int32_t>(REPLAY_WINDOW_PER_MINE));
    {
        char ab[16];
        uint8_t n = 0;
        put_u8(ab, &n, 15, src.ack_bud);
        if (n < 14) ab[n++] = '/';
        put_u8(ab, &n, 15, ACK_BUDGET_PER_SEC);
        ab[n] = '\0';
        o->wlab(buffer, "AckBd", ab);
    }
    o->wlab(buffer, "IP", "10.17.47.1");
    uint8_t aps = 0;
    uint8_t hive_n = 0;
    uint8_t xap = 0;
    if (src.scanner != nullptr) {
        aps = src.scanner->hudWifiCount();
        hive_n = src.scanner->hudHiveSsidCount();
        xap = src.scanner->hudForeignCount();
        if (aps == 0) {
            aps = src.scanner->getWifiCount();
            hive_n = 0;
            xap = 0;
            for (uint8_t i = 0; i < aps; ++i) {
                const WifiNetwork* net = src.scanner->getWifi(i);
                if (net == nullptr) continue;
                if (ssid_hive(net->ssid)) ++hive_n;
                else ++xap;
            }
        }
    }
    bool twin = hive_n >= 2;
    o->wlabn(buffer, "Scan", aps);
    o->wlab(buffer, "Twin", twin ? "yes" : "no");
    o->wlabn(buffer, "XAP", xap);
    {
        char ap[49];
        const WifiNetwork* n0 = nullptr;
        if (src.scanner != nullptr) {
            n0 = src.scanner->hudWifi(0);
            if (n0 == nullptr) n0 = src.scanner->getWifi(0);
        }
        fill_ap_line(ap, n0, twin);
        o->wlab(buffer, "Ap1", ap);
    }
    {
        char ap[49];
        const WifiNetwork* n1 = nullptr;
        if (src.scanner != nullptr) {
            n1 = src.scanner->hudWifi(1);
            if (n1 == nullptr) n1 = src.scanner->getWifi(1);
        }
        fill_ap_line(ap, n1, twin);
        o->wlab(buffer, "Ap2", ap);
    }
    uint8_t mines = src.registry != nullptr ? src.registry->getRoleCount(ROLE_MINE) : 0;
    o->wlabn(buffer, "Ack", src.ack_n);
    o->wlabn(buffer, "Mines", mines);
    uint8_t rc = src.replay != nullptr ? src.replay->trackedCount() : 0;
    if (replayBlk(src.replay)) {
        char rline[48];
        uint8_t n = 0;
        put_u8(rline, &n, 47, rc);
        put_str(rline, &n, 47, " blk=yes");
        rline[n] = '\0';
        o->wlab(buffer, "Replay", rline);
    } else {
        o->wlabn(buffer, "Replay", rc);
    }
    o->wlabn(buffer, "Alerts", alertCount(src.events));
    o->wlabn(buffer, "HBMiss", hbMissSum(src.heartbeat));
    char drift[16];
    char ws[12];
    workerDrift(src.registry, src.now_sec, drift);
    workerSync(src.registry, ws);
    o->wlab(buffer, "Drift", drift);
    o->wlab(buffer, "WorkerSync", ws);
    char last[48];
    fillLast(src, last);
    o->wlab(buffer, "Last", last);
    char tbuf[20];
    fmt_clock(src.now_sec, tbuf);
    o->wlab(buffer, "Time", tbuf);
}

static void append_u16(char* dst, uint8_t* n, uint8_t max, uint16_t v) {
    char dig[6];
    uint8_t d = 0;
    if (v == 0) {
        if (*n + 1 < max) dst[(*n)++] = '0';
        return;
    }
    while (v > 0 && d < 5) {
        dig[d++] = static_cast<char>('0' + (v % 10));
        v = static_cast<uint16_t>(v / 10);
    }
    while (d > 0 && *n + 1 < max) dst[(*n)++] = dig[--d];
}

static void pagePeer(GhostOutput* o, char* buffer, const GhostOutput::WatchSrc& src) {
    uint8_t n = src.registry != nullptr ? src.registry->getDeviceCount() : 0;
    uint8_t focus = src.focus;
    if (n == 0) {
        o->wlab(buffer, "idx", "0/0");
        return;
    }
    if (focus >= n) focus = 0;
    DeviceInfo info = src.registry->getDeviceInfo(focus);
    const Device* d = src.registry->getDevice(info.id);
    char line[49];
    uint8_t p = 0;
    put_str(line, &p, 48, "id=");
    uint8_t k = 0;
    while (info.id[k] != '\0' && p < 20) line[p++] = info.id[k++];
    put_str(line, &p, 48, " role=");
    put_str(line, &p, 48, roleCap(info.role));
    line[p] = '\0';
    char lab[8];
    lab[0] = 'D';
    lab[1] = 'e';
    lab[2] = 'v';
    uint8_t num = static_cast<uint8_t>(focus + 1u);
    if (num >= 10) {
        lab[3] = static_cast<char>('0' + (num / 10));
        lab[4] = static_cast<char>('0' + (num % 10));
        lab[5] = '\0';
    } else {
        lab[3] = static_cast<char>('0' + num);
        lab[4] = '\0';
    }
    o->wlab(buffer, lab, line);
    DeviceState st = static_cast<DeviceState>(info.status);
    const char* sn = (st == DeviceState::Online) ? "ok" : nullptr;
    if (sn == nullptr) {
        switch (st) {
            case DeviceState::Degraded: sn = "degraded"; break;
            case DeviceState::Offline: sn = "offline"; break;
            case DeviceState::Unknown: sn = "unknown"; break;
            case DeviceState::Suspected: sn = "suspect"; break;
            case DeviceState::Blocked: sn = "blocked"; break;
            case DeviceState::Pending: sn = "pending"; break;
            case DeviceState::GhostDown: sn = "down"; break;
            case DeviceState::DangerMode: sn = "danger"; break;
            case DeviceState::Silent: sn = "silent"; break;
            default: sn = "?"; break;
        }
    }
    o->wlab(buffer, "Status", sn);
    o->wlabn(buffer, "Trust", d != nullptr ? d->trust_level : 0);
    if (info.lastSeen == 0) o->wlab(buffer, "Last", "--");
    else {
        char hm[8];
        fmt_hhmm(info.lastSeen, hm);
        o->wlab(buffer, "Last", hm);
    }
    char age[16];
    fmt_age(info.lastSeen, src.now_sec, age);
    o->wlab(buffer, "Age", age);
    uint32_t drops = 0;
    if (src.pipeline != nullptr) drops = src.pipeline->telemDenseDrops(info.id);
    o->wlabn(buffer, "Drop", static_cast<int32_t>(drops));
    {
        char cap[8];
        uint8_t cn = 0;
        put_hex16(cap, &cn, 8, d != nullptr ? d->capability_mask : 0);
        cap[cn] = '\0';
        o->wlab(buffer, "Cap", cap);
    }
    {
        char tag[6];
        uint8_t tn = 0;
        put_hex8(tag, &tn, 6, d != nullptr ? d->tag_mask : 0);
        tag[tn] = '\0';
        o->wlab(buffer, "Tag", tag);
    }
    if (src.heartbeat == nullptr) o->wlab(buffer, "HB", "--");
    else o->wlabn(buffer, "HB", src.heartbeat->getMissCount(info.id));
    uint16_t ram = d != nullptr ? d->ram_mb : TELEM_ABSENT16;
    uint8_t cpu = d != nullptr ? d->cpu_percent : TELEM_ABSENT8;
    uint8_t gpu = d != nullptr ? d->gpu_percent : TELEM_ABSENT8;
    uint16_t traf = d != nullptr ? d->traffic_kbps : TELEM_ABSENT16;
    uint8_t bat = d != nullptr ? d->battery_percent : TELEM_ABSENT8;
    uint16_t wifi = d != nullptr ? d->wifi_mbit : TELEM_ABSENT16;
    telem_lab16(o, buffer, "RAM", ram, TELEM_ABSENT16, "MB");
    telem_lab8(o, buffer, "CPU", cpu, "%");
    telem_lab8(o, buffer, "GPU", gpu, "%");
    telem_lab16(o, buffer, "Traffic", traf, TELEM_ABSENT16, "KB/s");
    telem_lab8(o, buffer, "Battery", bat, "%");
    telem_lab16(o, buffer, "WiFi", wifi, TELEM_ABSENT16, "Mbit/s");
    char idx[12];
    uint8_t in = 0;
    append_u16(idx, &in, 12, static_cast<uint16_t>(focus + 1u));
    if (in + 1 < 12) idx[in++] = '/';
    append_u16(idx, &in, 12, n);
    idx[in] = '\0';
    o->wlab(buffer, "idx", idx);
}

static bool telemAbsentOnline(Registry* r) {
    if (r == nullptr) return false;
    uint8_t n = r->getDeviceCount();
    for (uint8_t i = 0; i < n; ++i) {
        DeviceInfo inf = r->getDeviceInfo(i);
        uint8_t role = inf.role;
        if (role != ROLE_WORKER && role != ROLE_PHONE && role != ROLE_SENSOR &&
            role != ROLE_ROUTER) {
            continue;
        }
        if (static_cast<DeviceState>(inf.status) != DeviceState::Online) continue;
        const Device* d = r->getDevice(inf.id);
        if (d == nullptr || d->last_seen == 0) continue;
        if (d->ram_mb == TELEM_ABSENT16 && d->cpu_percent == TELEM_ABSENT8 &&
            d->gpu_percent == TELEM_ABSENT8 && d->traffic_kbps == TELEM_ABSENT16 &&
            d->battery_percent == TELEM_ABSENT8 && d->wifi_mbit == TELEM_ABSENT16) {
            return true;
        }
    }
    return false;
}

bool watch_danger_headline(const GhostOutput::WatchSrc& src) {
    if (src.down != nullptr && src.down->isActive()) return false;
    uint8_t pn = 0, pe = 0, on = 0, bl = 0, si = 0;
    hiveCensus(src.registry, &pn, &pe, &on, &bl, &si);
    (void)pn;
    (void)pe;
    (void)on;
    (void)si;
    if (bl != 0) return true;
    if (anyDanger(src.registry)) return true;
    if (replayBlk(src.replay) != 0) return true;
    if (hbMissSum(src.heartbeat) != 0) return true;
    if (telemDropSum(src) != 0) return true;
    if (telemAbsentOnline(src.registry)) return true;
    uint8_t hive_n = 0;
    if (src.scanner != nullptr) {
        hive_n = src.scanner->hudHiveSsidCount();
        if (hive_n < 2) {
            uint8_t aps = src.scanner->hudWifiCount();
            if (aps == 0) aps = src.scanner->getWifiCount();
            hive_n = 0;
            for (uint8_t i = 0; i < aps; ++i) {
                const WifiNetwork* net = src.scanner->getWifi(i);
                if (net != nullptr && ssid_hive(net->ssid)) ++hive_n;
            }
        }
    }
    return hive_n >= 2;
}

bool watch_headline_alarm(const GhostOutput::WatchSrc& src) {
    return watch_danger_headline(src);
}

void watch_fill_alert(const GhostOutput::WatchSrc& src, char* dst) {
    fillAlert(src, dst);
}

void GhostOutput::buildWatchPage(char* buffer, const WatchSrc& src) {
    buffer[0] = '\0';
    bool down = src.down != nullptr && src.down->isActive();
    writeTitle(this, buffer, src.page, down);
    writeSep(this, buffer);
    if (src.page == 0) pageHive(this, buffer, src);
    else if (src.page == 1) pageKernel(this, buffer, src);
    else if (src.page == 2) pageNet(this, buffer, src);
    else pagePeer(this, buffer, src);
}
