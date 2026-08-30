#include "host_telem.h"

#include <stdio.h>

static uint64_t g_idle = 0;
static uint64_t g_total = 0;
static uint64_t g_bytes = 0;
static uint32_t g_byte_sec = 0;
static uint8_t g_have_cpu = 0;
static uint8_t g_have_net = 0;

static uint16_t clamp16(uint32_t v) {
    if (v > 65534u) return 65534;
    return static_cast<uint16_t>(v);
}

static uint8_t clamp_pct(uint32_t v) {
    if (v > 100u) return 100;
    return static_cast<uint8_t>(v);
}

static bool read_u32_after(const char* path, const char* key, uint32_t* out) {
    if (path == nullptr || key == nullptr || out == nullptr) return false;
    FILE* f = fopen(path, "rb");
    if (f == nullptr) return false;
    char line[256];
    bool ok = false;
    while (fgets(line, 256, f) != nullptr) {
        uint8_t i = 0;
        while (key[i] != '\0' && line[i] == key[i]) ++i;
        if (key[i] != '\0') continue;
        const char* p = line + i;
        while (*p == ' ' || *p == ':') ++p;
        uint32_t n = 0;
        uint8_t any = 0;
        while (*p >= '0' && *p <= '9') {
            any = 1;
            if (n > (0xFFFFFFFFu / 10u)) break;
            n = n * 10u + static_cast<uint32_t>(*p - '0');
            ++p;
        }
        if (any) {
            *out = n;
            ok = true;
            break;
        }
    }
    fclose(f);
    return ok;
}

static uint8_t sample_cpu() {
    FILE* f = fopen("/proc/stat", "rb");
    if (f == nullptr) return 0;
    char line[256];
    if (fgets(line, 256, f) == nullptr) {
        fclose(f);
        return 0;
    }
    fclose(f);
    const char* p = line;
    if (p[0] != 'c' || p[1] != 'p' || p[2] != 'u') return 0;
    p += 3;
    while (*p == ' ') ++p;
    uint64_t vals[8];
    uint8_t n = 0;
    while (n < 8) {
        uint64_t v = 0;
        uint8_t any = 0;
        while (*p >= '0' && *p <= '9') {
            any = 1;
            v = v * 10ull + static_cast<uint64_t>(*p - '0');
            ++p;
        }
        if (!any) break;
        vals[n++] = v;
        while (*p == ' ') ++p;
    }
    if (n < 4) return 0;
    uint64_t idle = vals[3];
    if (n > 4) idle += vals[4];
    uint64_t total = 0;
    for (uint8_t i = 0; i < n; ++i) total += vals[i];
    uint8_t pct = 0;
    if (g_have_cpu && total > g_total) {
        uint64_t dt = total - g_total;
        uint64_t di = idle - g_idle;
        if (dt > 0 && di <= dt) {
            pct = clamp_pct(static_cast<uint32_t>(((dt - di) * 100ull) / dt));
        }
    }
    g_idle = idle;
    g_total = total;
    g_have_cpu = 1;
    return pct;
}

static uint16_t sample_ram() {
    uint32_t total_kb = 0;
    uint32_t avail_kb = 0;
    if (!read_u32_after("/proc/meminfo", "MemTotal", &total_kb)) return 0;
    if (!read_u32_after("/proc/meminfo", "MemAvailable", &avail_kb)) {
        (void)read_u32_after("/proc/meminfo", "MemFree", &avail_kb);
    }
    if (avail_kb > total_kb) avail_kb = total_kb;
    uint32_t used_kb = total_kb - avail_kb;
    return clamp16(used_kb / 1024u);
}

static uint16_t sample_traffic(uint32_t now_sec) {
    FILE* f = fopen("/proc/net/dev", "rb");
    if (f == nullptr) return 0;
    char line[320];
    uint64_t sum = 0;
    while (fgets(line, 320, f) != nullptr) {
        char* p = line;
        while (*p == ' ') ++p;
        if (p[0] == 'l' && p[1] == 'o' && p[2] == ':') continue;
        char* col = p;
        while (*col != '\0' && *col != ':') ++col;
        if (*col != ':') continue;
        ++col;
        while (*col == ' ') ++col;
        uint64_t rx = 0;
        while (*col >= '0' && *col <= '9') {
            rx = rx * 10ull + static_cast<uint64_t>(*col - '0');
            ++col;
        }
        uint8_t skip = 0;
        while (skip < 7) {
            while (*col == ' ') ++col;
            while (*col >= '0' && *col <= '9') ++col;
            ++skip;
        }
        while (*col == ' ') ++col;
        uint64_t tx = 0;
        while (*col >= '0' && *col <= '9') {
            tx = tx * 10ull + static_cast<uint64_t>(*col - '0');
            ++col;
        }
        sum += rx + tx;
    }
    fclose(f);
    uint16_t kbps = 0;
    if (g_have_net && now_sec > g_byte_sec && sum >= g_bytes) {
        uint32_t dt = now_sec - g_byte_sec;
        if (dt == 0) dt = 1;
        uint64_t db = sum - g_bytes;
        kbps = clamp16(static_cast<uint32_t>((db * 8ull) / (dt * 1000ull)));
    }
    g_bytes = sum;
    g_byte_sec = now_sec;
    g_have_net = 1;
    return kbps;
}

static uint16_t sample_wifi() {
    uint32_t mb = 0;
    FILE* f = fopen("/sys/class/net/wlan0/speed", "rb");
    if (f != nullptr) {
        char buf[16];
        size_t n = fread(buf, 1, 15, f);
        fclose(f);
        buf[n] = '\0';
        uint32_t v = 0;
        uint8_t i = 0;
        while (buf[i] >= '0' && buf[i] <= '9') {
            v = v * 10u + static_cast<uint32_t>(buf[i] - '0');
            ++i;
        }
        mb = v;
    }
    return clamp16(mb);
}

static uint8_t sample_battery() {
    FILE* f = fopen("/sys/class/power_supply/BAT0/capacity", "rb");
    if (f == nullptr) return 0;
    char buf[8];
    size_t n = fread(buf, 1, 7, f);
    fclose(f);
    buf[n] = '\0';
    uint32_t v = 0;
    uint8_t i = 0;
    while (buf[i] >= '0' && buf[i] <= '9') {
        v = v * 10u + static_cast<uint32_t>(buf[i] - '0');
        ++i;
    }
    return clamp_pct(v);
}

bool host_telem_sample(uint32_t now_sec,
                       uint16_t* ram_mb,
                       uint8_t* cpu_percent,
                       uint8_t* gpu_percent,
                       uint16_t* traffic_kbps,
                       uint8_t* battery_percent,
                       uint16_t* wifi_mbit) {
    if (ram_mb == nullptr || cpu_percent == nullptr || gpu_percent == nullptr ||
        traffic_kbps == nullptr || battery_percent == nullptr ||
        wifi_mbit == nullptr) {
        return false;
    }
    *ram_mb = sample_ram();
    *cpu_percent = sample_cpu();
    *gpu_percent = 0;
    *traffic_kbps = sample_traffic(now_sec);
    *battery_percent = sample_battery();
    *wifi_mbit = sample_wifi();
    return true;
}

void host_telem_apply_role(uint8_t role,
                           uint16_t* ram_mb,
                           uint8_t* cpu_percent,
                           uint8_t* gpu_percent,
                           uint16_t* traffic_kbps,
                           uint8_t* battery_percent,
                           uint16_t* wifi_mbit) {
    if (ram_mb == nullptr || cpu_percent == nullptr || gpu_percent == nullptr ||
        traffic_kbps == nullptr || battery_percent == nullptr ||
        wifi_mbit == nullptr) {
        return;
    }
    if (role == ROLE_WORKER) return;
    if (role == ROLE_PHONE) {
        *gpu_percent = TELEM_ABSENT8;
        *traffic_kbps = TELEM_ABSENT16;
        return;
    }
    if (role == ROLE_SENSOR) {
        *ram_mb = TELEM_ABSENT16;
        *gpu_percent = TELEM_ABSENT8;
        *traffic_kbps = TELEM_ABSENT16;
        *wifi_mbit = TELEM_ABSENT16;
        return;
    }
    if (role == ROLE_ROUTER) {
        *ram_mb = TELEM_ABSENT16;
        *cpu_percent = TELEM_ABSENT8;
        *gpu_percent = TELEM_ABSENT8;
        *battery_percent = TELEM_ABSENT8;
        return;
    }
}
