#include "peer_keys.h"
#include "ghost_crypto.h"

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#if defined(__linux__)
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif

static const uint8_t PEER_AUTH_OFF = 84;
static const uint8_t PEER_MAC_OFF = 88;
static const uint8_t PEER_MAC_HEX = 40;
static const char BIND_PREFIX[] = "/tmp/ghost_hive/";
static const char BIND_NAME[] = "peer.bind";

static bool peer_key_zero(const uint8_t* key, uint8_t len) {
    if (key == nullptr || len == 0) return true;
    for (uint8_t i = 0; i < len; ++i) {
        if (key[i] != 0) return false;
    }
    return true;
}

void peer_os_harden() {
    (void)umask(077);
#if defined(__linux__)
    (void)prctl(PR_SET_DUMPABLE, 0);
#endif
}

static bool path_has_dotdot(const char* p) {
    uint8_t i = 0;
    while (p[i] != '\0') {
        if (p[i] == '.' && p[i + 1] == '.') return true;
        ++i;
    }
    return false;
}

static bool path_has_scheme(const char* p) {
    uint8_t i = 0;
    while (p[i] != '\0') {
        if (p[i] == ':' && p[i + 1] == '/' && p[i + 2] == '/') return true;
        ++i;
    }
    return false;
}

bool peer_bind_path_ok(const char* bindPath) {
    if (bindPath == nullptr || bindPath[0] == '\0') return false;
    if (path_has_scheme(bindPath)) return false;
    if (path_has_dotdot(bindPath)) return false;
    uint8_t i = 0;
    while (BIND_PREFIX[i] != '\0') {
        if (bindPath[i] != BIND_PREFIX[i]) break;
        ++i;
    }
    bool hive = (BIND_PREFIX[i] == '\0');
    if (!hive) {
        const char* lab = "/tmp/ghost_lab/";
        i = 0;
        while (lab[i] != '\0') {
            if (bindPath[i] != lab[i]) return false;
            ++i;
        }
    }
    uint8_t n = 0;
    while (BIND_NAME[n] != '\0') {
        if (bindPath[i + n] != BIND_NAME[n]) return false;
        ++n;
    }
    return bindPath[i + n] == '\0';
}

static bool peer_bind_ttl_ok(const char* bindPath) {
    char ttlp[64];
    uint8_t n = 0;
    while (bindPath[n] != '\0' && n < 48) {
        ttlp[n] = bindPath[n];
        ++n;
    }
    const char* suf = ".ttl";
    uint8_t s = 0;
    while (suf[s] != '\0' && n < 63) ttlp[n++] = suf[s++];
    ttlp[n] = '\0';
    FILE* f = fopen(ttlp, "rb");
    if (f == nullptr) return true;
    unsigned long exp = 0;
    int got = fscanf(f, "%lu", &exp);
    fclose(f);
    if (got != 1) return false;
    time_t now = time(nullptr);
    if (now < 0) return false;
    return static_cast<unsigned long>(now) <= exp;
}

bool peer_bind_keys(GhostKeys& keys, const char* bindPath) {
    if (!peer_bind_path_ok(bindPath)) return false;
    if (!peer_bind_ttl_ok(bindPath)) return false;
    FILE* f = fopen(bindPath, "rb");
    if (f == nullptr) return false;
    uint8_t buf[GhostKeys::PEER_BIND_LEN];
    size_t n = fread(buf, 1, GhostKeys::PEER_BIND_LEN, f);
    int extra = fgetc(f);
    fclose(f);
    (void)chmod(bindPath, 0600);
    if (n != GhostKeys::PEER_BIND_LEN || extra != EOF) {
        for (uint8_t i = 0; i < GhostKeys::PEER_BIND_LEN; ++i) buf[i] = 0;
        return false;
    }
    if (peer_key_zero(buf, KEY_LEN)) {
        for (uint8_t i = 0; i < GhostKeys::PEER_BIND_LEN; ++i) buf[i] = 0;
        return false;
    }
    bool ok = keys.importPeerBind(buf, GhostKeys::PEER_BIND_LEN);
    for (uint8_t i = 0; i < GhostKeys::PEER_BIND_LEN; ++i) buf[i] = 0;
    return ok && !keys.hasRoot();
}

bool peer_bind_from_keyd(GhostKeys& keys) {
#if !defined(__linux__)
    (void)keys;
    return false;
#else
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
    struct sockaddr_un addr;
    for (uint16_t i = 0; i < sizeof(addr); ++i) {
        reinterpret_cast<uint8_t*>(&addr)[i] = 0;
    }
    addr.sun_family = AF_UNIX;
    uint8_t p = 0;
    while (PEER_KEYD_SOCK[p] != '\0' && p < 107) {
        addr.sun_path[p] = PEER_KEYD_SOCK[p];
        ++p;
    }
    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return false;
    }
    const char req[4] = {'B', 'I', 'N', 'D'};
    if (write(fd, req, 4) != 4) {
        close(fd);
        return false;
    }
    uint8_t buf[GhostKeys::PEER_BIND_LEN];
    uint8_t ttlb[4];
    ssize_t n = read(fd, buf, GhostKeys::PEER_BIND_LEN);
    ssize_t t = read(fd, ttlb, 4);
    close(fd);
    if (n != GhostKeys::PEER_BIND_LEN || t != 4) {
        for (uint8_t i = 0; i < GhostKeys::PEER_BIND_LEN; ++i) buf[i] = 0;
        return false;
    }
    uint32_t exp = static_cast<uint32_t>(ttlb[0]) |
                   (static_cast<uint32_t>(ttlb[1]) << 8) |
                   (static_cast<uint32_t>(ttlb[2]) << 16) |
                   (static_cast<uint32_t>(ttlb[3]) << 24);
    time_t now = time(nullptr);
    if (now < 0 || static_cast<uint32_t>(now) > exp) {
        for (uint8_t i = 0; i < GhostKeys::PEER_BIND_LEN; ++i) buf[i] = 0;
        return false;
    }
    bool ok = keys.importPeerBind(buf, GhostKeys::PEER_BIND_LEN);
    for (uint8_t i = 0; i < GhostKeys::PEER_BIND_LEN; ++i) buf[i] = 0;
    return ok && !keys.hasRoot();
#endif
}

bool peer_load_keys(GhostKeys& keys, const char* bindPath) {
    peer_os_harden();
    (void)mkdir("/tmp/ghost_hive", 0700);
    if (peer_bind_from_keyd(keys)) return true;
    return peer_bind_keys(keys, bindPath);
}

static bool peer_session_event(const Event& event) {
    switch (event.type) {
        case EventType::ScanResult:
        case EventType::ProfileUpdate:
        case EventType::AnomalyDetected:
        case EventType::PolicyViolation:
        case EventType::BackupWritten:
        case EventType::AlertSent:
            return true;
        default:
            return false;
    }
}

static void peer_put_u32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}

static void peer_hex20(const uint8_t in[20], char out[41]) {
    const char* d = "0123456789abcdef";
    for (uint8_t i = 0; i < 20; ++i) {
        out[i * 2] = d[in[i] >> 4];
        out[i * 2 + 1] = d[in[i] & 0x0f];
    }
    out[40] = '\0';
}

bool peer_sign_event(const GhostKeys& keys, Event& event) {
    if (!keys.hasDevice() || !keys.hasSession()) return false;
    for (uint8_t i = PEER_AUTH_OFF; i < 128; ++i) event.payload[i] = 0;
    uint8_t msg[160];
    uint32_t n = 0;
    msg[n++] = 0;
    msg[n++] = static_cast<uint8_t>(event.type);
    for (uint8_t i = 0; i < 32; ++i) msg[n++] = static_cast<uint8_t>(event.source_device_id[i]);
    peer_put_u32(msg + n, event.timestamp);
    n += 4;
    msg[n++] = static_cast<uint8_t>(event.severity);
    for (uint8_t i = 0; i < PEER_MAC_OFF; ++i) {
        msg[n++] = static_cast<uint8_t>(event.payload[i]);
    }
    const uint8_t* key = peer_session_event(event) ? keys.session() : keys.device();
    uint8_t mac[20];
    ghost_hmac_sha1(key, KEY_LEN, msg, n, mac);
    char hex[41];
    peer_hex20(mac, hex);
    for (uint8_t i = 0; i < PEER_MAC_HEX; ++i) {
        event.payload[PEER_MAC_OFF + i] = hex[i];
    }
    return true;
}

bool peer_verify_event(const GhostKeys& keys, const Event& event) {
    if (!keys.hasDevice() || !keys.hasSession()) return false;
    uint8_t hexn = 0;
    while (hexn < PEER_MAC_HEX) {
        char c = event.payload[PEER_MAC_OFF + hexn];
        bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!ok) return false;
        ++hexn;
    }
    Event work = event;
    char got[41];
    for (uint8_t i = 0; i < PEER_MAC_HEX; ++i) {
        got[i] = work.payload[PEER_MAC_OFF + i];
        work.payload[PEER_MAC_OFF + i] = 0;
    }
    got[40] = '\0';
    uint8_t msg[160];
    uint32_t n = 0;
    msg[n++] = 0;
    msg[n++] = static_cast<uint8_t>(work.type);
    for (uint8_t i = 0; i < 32; ++i) {
        msg[n++] = static_cast<uint8_t>(work.source_device_id[i]);
    }
    peer_put_u32(msg + n, work.timestamp);
    n += 4;
    msg[n++] = static_cast<uint8_t>(work.severity);
    for (uint8_t i = 0; i < PEER_MAC_OFF; ++i) {
        msg[n++] = static_cast<uint8_t>(work.payload[i]);
    }
    const uint8_t* key = peer_session_event(work) ? keys.session() : keys.device();
    uint8_t mac[20];
    ghost_hmac_sha1(key, KEY_LEN, msg, n, mac);
    char hex[41];
    peer_hex20(mac, hex);
    uint8_t diff = 0;
    for (uint8_t i = 0; i < PEER_MAC_HEX; ++i) {
        diff = static_cast<uint8_t>(diff | (static_cast<uint8_t>(got[i]) ^
                                           static_cast<uint8_t>(hex[i])));
    }
    return diff == 0;
}

bool peer_sign_mine(const GhostKeys& keys, MinePayload& mine) {
    if (!keys.hasMine() || !keys.hasTotpSeed()) return false;
    uint8_t msg[80];
    uint32_t n = 0;
    msg[n++] = 1;
    for (uint8_t i = 0; i < 32; ++i) msg[n++] = static_cast<uint8_t>(mine.mine_id[i]);
    peer_put_u32(msg + n, mine.counter);
    n += 4;
    peer_put_u32(msg + n, mine.totp);
    n += 4;
    msg[n++] = static_cast<uint8_t>(mine.event);
    peer_put_u32(msg + n, mine.timestamp);
    n += 4;
    uint8_t mac[20];
    ghost_hmac_sha1(keys.mine(), KEY_LEN, msg, n, mac);
    char hex[41];
    peer_hex20(mac, hex);
    for (uint8_t i = 0; i < 64; ++i) mine.hash[i] = 0;
    for (uint8_t i = 0; i < PEER_MAC_HEX; ++i) mine.hash[i] = hex[i];
    return true;
}
