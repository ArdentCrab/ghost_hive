#include "tetact.h"

#include <stdio.h>

static void tet_zero_event(Event* out) {
    if (out == nullptr) return;
    out->type = EventType::ScanResult;
    out->source_device_id[0] = '\0';
    out->timestamp = 0;
    out->payload[0] = '\0';
    out->severity = Severity::Info;
}

static void tet_tag(Event* out, const char* tag, EventType type, Severity sev) {
    tet_zero_event(out);
    out->type = type;
    out->severity = sev;
    if (tag == nullptr) return;
    uint8_t i = 0;
    while (tag[i] != '\0' && i < 127) {
        out->payload[i] = tag[i];
        ++i;
    }
    out->payload[i] = '\0';
}

static uint16_t tet_arp_view(uint32_t* hashOut) {
    uint32_t h = 2166136261u;
    uint16_t count = 0;
    FILE* f = fopen("/proc/net/arp", "rb");
    if (f == nullptr) {
        if (hashOut != nullptr) *hashOut = 0;
        return 0;
    }
    char line[160];
    bool first = true;
    while (fgets(line, 160, f) != nullptr) {
        if (first) {
            first = false;
            continue;
        }
        if (line[0] == '\0') continue;
        ++count;
        uint8_t i = 0;
        while (line[i] != '\0' && i < 80) {
            h ^= static_cast<uint8_t>(line[i]);
            h *= 16777619u;
            ++i;
        }
    }
    fclose(f);
    if (hashOut != nullptr) *hashOut = h;
    return count;
}

static bool tet_tracer() {
    FILE* f = fopen("/proc/self/status", "rb");
    if (f == nullptr) return false;
    char line[128];
    bool hit = false;
    while (fgets(line, 128, f) != nullptr) {
        const char* k = "TracerPid:";
        uint8_t i = 0;
        bool match = true;
        while (k[i] != '\0') {
            if (line[i] != k[i]) {
                match = false;
                break;
            }
            ++i;
        }
        if (!match) continue;
        while (line[i] == ' ' || line[i] == '\t') ++i;
        if (line[i] != '0') hit = true;
        else if (line[i + 1] >= '0' && line[i + 1] <= '9') hit = true;
        break;
    }
    fclose(f);
    return hit;
}

static uint16_t tet_proc_count() {
    FILE* f = fopen("/proc/loadavg", "rb");
    if (f == nullptr) return 0;
    char line[80];
    if (fgets(line, 80, f) == nullptr) {
        fclose(f);
        return 0;
    }
    fclose(f);
    uint8_t i = 0;
    uint8_t slashes = 0;
    while (line[i] != '\0') {
        if (line[i] == '/') ++slashes;
        ++i;
    }
    if (slashes < 1) return 0;
    i = 0;
    uint8_t seen = 0;
    while (line[i] != '\0' && seen < 3) {
        if (line[i] == ' ') ++seen;
        ++i;
    }
    while (line[i] != '\0' && line[i] != '/') ++i;
    if (line[i] != '/') return 0;
    ++i;
    uint16_t n = 0;
    while (line[i] >= '0' && line[i] <= '9' && n < 10000) {
        n = static_cast<uint16_t>(n * 10 + (line[i] - '0'));
        ++i;
    }
    return n;
}

static bool tet_wifi_line(char* dst, uint8_t max) {
    if (dst == nullptr || max == 0) return false;
    dst[0] = '\0';
    FILE* f = fopen("/proc/net/wireless", "rb");
    if (f == nullptr) return false;
    char line[128];
    uint8_t skip = 0;
    bool ok = false;
    while (fgets(line, 128, f) != nullptr) {
        if (skip < 2) {
            ++skip;
            continue;
        }
        uint8_t i = 0;
        while (line[i] == ' ' || line[i] == '\t') ++i;
        uint8_t n = 0;
        dst[0] = 'w';
        dst[1] = 'i';
        dst[2] = 'f';
        dst[3] = 'i';
        dst[4] = ':';
        n = 5;
        while (line[i] != '\0' && line[i] != ':' && n + 1 < max) {
            dst[n++] = line[i++];
        }
        dst[n] = '\0';
        ok = n > 5;
        break;
    }
    fclose(f);
    return ok;
}

static bool tet_bt_present() {
    FILE* f = fopen("/sys/class/bluetooth/hci0/address", "rb");
    if (f == nullptr) return false;
    fclose(f);
    return true;
}

void tetact_init(TetactState& st) {
    st.lastNow = 0;
    st.arpCount = 0;
    st.lastArpCount = 0;
    st.arpHash = 0;
    st.scanPhase = 0;
    st.ready = false;
}

void tetact_set_source(Event* out, const char* id) {
    if (out == nullptr) return;
    uint8_t i = 0;
    if (id == nullptr) {
        out->source_device_id[0] = '\0';
        return;
    }
    while (id[i] != '\0' && i < 31) {
        out->source_device_id[i] = id[i];
        ++i;
    }
    out->source_device_id[i] = '\0';
}

TetactKind tetact_watch(TetactState& st, uint32_t now, Event* out) {
    if (out == nullptr) return TETACT_NONE;
    tet_zero_event(out);
    out->timestamp = now;
    if (!st.ready) {
        st.lastNow = now;
        st.ready = true;
        return TETACT_NONE;
    }
    if (st.lastNow != 0) {
        uint32_t dt = (now > st.lastNow) ? (now - st.lastNow) : (st.lastNow - now);
        if (now + TETACT_TIME_WARP_SEC < st.lastNow ||
            dt > (TETACT_TIME_WARP_SEC * 4u)) {
            st.lastNow = now;
            tet_tag(out, "tamper:time", EventType::AnomalyDetected, Severity::Critical);
            out->timestamp = now;
            return TETACT_TAMPER;
        }
    }
    st.lastNow = now;
    if (tet_tracer()) {
        tet_tag(out, "tamper:ptrace", EventType::AnomalyDetected, Severity::Critical);
        out->timestamp = now;
        return TETACT_TAMPER;
    }
    return TETACT_NONE;
}

TetactKind tetact_poll(TetactState& st, uint32_t now, Event* out) {
    if (out == nullptr) return TETACT_NONE;
    tet_zero_event(out);
    out->timestamp = now;

    uint32_t arpHash = 0;
    uint16_t arp = tet_arp_view(&arpHash);

    TetactKind watch = tetact_watch(st, now, out);
    if (watch == TETACT_TAMPER) return TETACT_TAMPER;

    if (st.arpCount == 0 && st.lastArpCount == 0 && arp > 0 && st.scanPhase == 0) {
        st.arpCount = arp;
        st.lastArpCount = arp;
        st.arpHash = arpHash;
    }

    if (arp > st.lastArpCount && st.lastArpCount != 0) {
        st.lastArpCount = arp;
        st.arpCount = arp;
        st.arpHash = arpHash;
        tet_tag(out, "net:new_device", EventType::DeviceSeen, Severity::Warn);
        out->timestamp = now;
        return TETACT_SEEN;
    }
    if (st.lastArpCount == 0) st.lastArpCount = arp;
    st.arpCount = arp;
    st.arpHash = arpHash;

    uint8_t phase = st.scanPhase;
    st.scanPhase = static_cast<uint8_t>((st.scanPhase + 1) % 3);
    if (phase == 0) {
        char tag[80];
        if (tet_wifi_line(tag, 80)) {
            tet_tag(out, tag, EventType::ScanResult, Severity::Info);
        } else {
            tet_tag(out, "os:wlan", EventType::ScanResult, Severity::Info);
        }
        out->timestamp = now;
        return TETACT_SCAN;
    }
    if (phase == 1) {
        char tag[32];
        tag[0] = 'a';
        tag[1] = 'p';
        tag[2] = 'p';
        tag[3] = ':';
        tag[4] = 'p';
        tag[5] = 'r';
        tag[6] = 'o';
        tag[7] = 'c';
        tag[8] = ':';
        uint16_t pc = tet_proc_count();
        uint8_t n = 9;
        if (pc == 0) {
            tag[n++] = '0';
        } else {
            char tmp[6];
            uint8_t t = 0;
            while (pc > 0 && t < 5) {
                tmp[t++] = static_cast<char>('0' + (pc % 10));
                pc = static_cast<uint16_t>(pc / 10);
            }
            while (t > 0) tag[n++] = tmp[--t];
        }
        tag[n] = '\0';
        tet_tag(out, tag, EventType::ScanResult, Severity::Info);
        out->timestamp = now;
        return TETACT_SCAN;
    }
    if (tet_bt_present()) {
        tet_tag(out, "net:bt", EventType::ScanResult, Severity::Info);
    } else {
        tet_tag(out, "net:arp", EventType::ScanResult, Severity::Info);
    }
    out->timestamp = now;
    return TETACT_SCAN;
}
