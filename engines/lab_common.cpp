#include "lab_common.h"
#include "peer_keys.h"
#include "ghost_policy.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <sys/file.h>
#include <unistd.h>

bool lab_host_ok(const char* host) {
    if (host == nullptr) return false;
    return host[0] == '1' && host[1] == '2' && host[2] == '7' &&
           host[3] == '.' && host[4] == '0' && host[5] == '.' &&
           host[6] == '0' && host[7] == '.' && host[8] == '1' &&
           host[9] == '\0';
}

static uint16_t env_port(const char* key, uint16_t def) {
    const char* v = getenv(key);
    if (v == nullptr || v[0] == '\0') return def;
    uint32_t n = 0;
    while (*v >= '0' && *v <= '9') {
        n = n * 10u + static_cast<uint32_t>(*v - '0');
        ++v;
    }
    if (n == 0 || n > 65535u) return def;
    return static_cast<uint16_t>(n);
}

const char* lab_dir() {
    const char* d = getenv("GHOST_LAB_DIR");
    if (d != nullptr && d[0] != '\0') return d;
    return LAB_DIR;
}

uint16_t lab_udp_port() {
    return env_port("GHOST_LAB_PORT", LAB_PORT);
}

const char* lab_sock_path() {
    static char p[160];
    const char* e = getenv("GHOST_LAB_SOCK");
    if (e != nullptr && e[0] != '\0') return e;
    snprintf(p, sizeof(p), "%s/sim.sock", lab_dir());
    return p;
}

const char* lab_ready_path() {
    static char p[160];
    const char* e = getenv("GHOST_LAB_READY");
    if (e != nullptr && e[0] != '\0') return e;
    snprintf(p, sizeof(p), "%s/ready", lab_dir());
    return p;
}

const char* lab_bind_path() {
    static char p[160];
    snprintf(p, sizeof(p), "%s/peer.bind", lab_dir());
    return p;
}

bool lab_ensure_dir() {
    struct stat st;
    const char* d = lab_dir();
    if (stat(d, &st) == 0) return S_ISDIR(st.st_mode);
    return mkdir(d, 0755) == 0;
}

void lab_copy_id(char* dst, const char* src) {
    if (dst == nullptr) return;
    uint8_t i = 0;
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i < 31) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

bool lab_udp_open(int* fd, bool bind_loopback) {
    if (fd == nullptr) return false;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return false;
    int yes = 1;
    (void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (bind_loopback) {
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(lab_udp_port());
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            close(s);
            return false;
        }
    }
    *fd = s;
    return true;
}

bool lab_udp_send(int fd, const uint8_t* wire, uint16_t len) {
    if (fd < 0 || wire == nullptr || len == 0) return false;
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(lab_udp_port());
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ssize_t n = sendto(fd, wire, len, 0,
                       reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    return n == static_cast<ssize_t>(len);
}

bool lab_unix_cmd(const char* cmd, char* reply, uint16_t cap) {
    if (cmd == nullptr || reply == nullptr || cap < 8) return false;
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) return false;
    sockaddr_un un = {};
    un.sun_family = AF_UNIX;
    snprintf(un.sun_path, sizeof(un.sun_path), "%s", lab_sock_path());
    if (connect(s, reinterpret_cast<sockaddr*>(&un), sizeof(un)) != 0) {
        close(s);
        return false;
    }
    size_t clen = strlen(cmd);
    if (send(s, cmd, clen, 0) != static_cast<ssize_t>(clen)) {
        close(s);
        return false;
    }
    if (cmd[clen - 1] != '\n') {
        char nl = '\n';
        (void)send(s, &nl, 1, 0);
    }
    ssize_t n = recv(s, reply, cap - 1, 0);
    close(s);
    if (n <= 0) return false;
    reply[n] = '\0';
    return true;
}

bool lab_snap_now(LabSnap* s) {
    if (s == nullptr) return false;
    char line[LAB_SNAP_LINE];
    if (!lab_unix_cmd("SNAP", line, LAB_SNAP_LINE)) return false;
    return lab_snap_parse(line, s);
}

bool lab_set_now(uint32_t now) {
    char cmd[40];
    snprintf(cmd, sizeof(cmd), "NOW %u", now);
    char reply[LAB_SNAP_LINE];
    return lab_unix_cmd(cmd, reply, LAB_SNAP_LINE);
}

void lab_kind_name(TransportKind k, char* dst, uint8_t cap) {
    const char* n = "-";
    if (k == TransportKind::EventFrame) n = "EventFrame";
    else if (k == TransportKind::MineFrame) n = "MineFrame";
    else if (k == TransportKind::AckFrame) n = "AckFrame";
    else if (k == TransportKind::KillFrame) n = "KillFrame";
    else if (k == TransportKind::FlushFrame) n = "FlushFrame";
    lab_copy_id(dst, n);
    (void)cap;
}

void lab_etype_name(EventType t, char* dst, uint8_t cap) {
    const char* n = "-";
    switch (t) {
        case EventType::ScanResult: n = "ScanResult"; break;
        case EventType::DeviceSeen: n = "DeviceSeen"; break;
        case EventType::Heartbeat: n = "Heartbeat"; break;
        case EventType::AnomalyDetected: n = "AnomalyDetected"; break;
        case EventType::PolicyViolation: n = "PolicyViolation"; break;
        case EventType::GhostDownStart: n = "GhostDownStart"; break;
        case EventType::MineEvent: n = "MineEvent"; break;
        case EventType::AlertSent: n = "AlertSent"; break;
        default: n = "Other"; break;
    }
    uint8_t i = 0;
    while (n[i] != '\0' && i + 1 < cap) {
        dst[i] = n[i];
        ++i;
    }
    dst[i] = '\0';
}

void lab_policy_name(uint8_t action, char* dst, uint8_t cap) {
    const char* n = "drop";
    if (action == static_cast<uint8_t>(PolicyAction::LogOnly)) n = "log_only";
    else if (action == static_cast<uint8_t>(PolicyAction::Alert)) n = "alert";
    else if (action == static_cast<uint8_t>(PolicyAction::Backup)) n = "backup";
    else if (action == static_cast<uint8_t>(PolicyAction::Block)) n = "block";
    else if (action == static_cast<uint8_t>(PolicyAction::Kill)) n = "kill";
    else if (action == static_cast<uint8_t>(PolicyAction::GhostDown)) n = "ghost_down";
    uint8_t i = 0;
    while (n[i] != '\0' && i + 1 < cap) {
        dst[i] = n[i];
        ++i;
    }
    dst[i] = '\0';
}

bool lab_load_bind(GhostKeys* keys) {
    if (keys == nullptr) return false;
    FILE* f = fopen(lab_bind_path(), "rb");
    if (f == nullptr) return false;
    uint8_t buf[GhostKeys::PEER_BIND_LEN];
    size_t n = fread(buf, 1, GhostKeys::PEER_BIND_LEN, f);
    fclose(f);
    if (n != GhostKeys::PEER_BIND_LEN) return false;
    return keys->importPeerBind(buf, GhostKeys::PEER_BIND_LEN) && !keys->hasRoot();
}

bool lab_sign_event(GhostKeys& keys, Event& ev) {
    return peer_sign_event(keys, ev);
}

bool lab_sign_mine(GhostKeys& keys, MinePayload& mine) {
    return peer_sign_mine(keys, mine);
}

bool lab_encode_event(GhostKeys& keys, uint8_t role, const char* id,
                      Event ev, uint32_t stamp, uint8_t* wire, bool sign) {
    if (wire == nullptr) return false;
    if (sign) {
        if (!peer_sign_event(keys, ev)) return false;
    }
    TransportFrame frame;
    transport_clear_frame(frame);
    frame.kind = TransportKind::EventFrame;
    frame.src_role = role;
    frame.dst_role = ROLE_KERNEL;
    lab_copy_id(frame.src_id, id);
    lab_copy_id(frame.dst_id, KERNEL_SOURCE_ID);
    frame.event = ev;
    lab_copy_id(frame.event.source_device_id, id);
    frame.stamp = stamp;
    return transport_encode(frame, wire, TRANSPORT_WIRE_LEN);
}

bool lab_encode_kill(GhostKeys& keys, uint8_t role, const char* id,
                     Event ev, uint32_t stamp, uint8_t* wire, bool sign) {
    if (wire == nullptr) return false;
    if (sign) {
        if (!peer_sign_event(keys, ev)) return false;
    }
    TransportFrame frame;
    transport_clear_frame(frame);
    frame.kind = TransportKind::KillFrame;
    frame.src_role = role;
    frame.dst_role = ROLE_KERNEL;
    lab_copy_id(frame.src_id, id);
    lab_copy_id(frame.dst_id, KERNEL_SOURCE_ID);
    frame.event = ev;
    lab_copy_id(frame.event.source_device_id, id);
    frame.stamp = stamp;
    return transport_encode(frame, wire, TRANSPORT_WIRE_LEN);
}

bool lab_encode_mine(GhostKeys& keys, MinePayload mine, uint32_t stamp,
                     uint8_t* wire, bool sign) {
    if (wire == nullptr) return false;
    if (sign) {
        if (!peer_sign_mine(keys, mine)) return false;
    }
    TransportFrame frame;
    transport_clear_frame(frame);
    frame.kind = TransportKind::MineFrame;
    frame.src_role = ROLE_MINE;
    frame.dst_role = ROLE_KERNEL;
    lab_copy_id(frame.src_id, mine.mine_id);
    lab_copy_id(frame.dst_id, KERNEL_SOURCE_ID);
    frame.mine = mine;
    frame.stamp = stamp;
    return transport_encode(frame, wire, TRANSPORT_WIRE_LEN);
}

void lab_fill_event(Event* ev, EventType t, const char* id, uint32_t now,
                    const char* payload) {
    if (ev == nullptr) return;
    ev->type = t;
    lab_copy_id(ev->source_device_id, id);
    ev->timestamp = now;
    ev->severity = Severity::Info;
    ev->payload[0] = '\0';
    if (payload == nullptr) return;
    uint8_t i = 0;
    while (payload[i] != '\0' && i < 80) {
        ev->payload[i] = payload[i];
        ++i;
    }
    ev->payload[i] = '\0';
}

bool lab_hex40(const uint8_t* w, uint16_t n, char* out, uint16_t cap) {
    if (w == nullptr || out == nullptr) return false;
    const char* d = "0123456789abcdef";
    uint16_t max = n;
    if (max > 80) max = 80;
    if (cap < static_cast<uint16_t>(max * 2 + 1)) return false;
    for (uint16_t i = 0; i < max; ++i) {
        out[i * 2] = d[w[i] >> 4];
        out[i * 2 + 1] = d[w[i] & 0x0f];
    }
    out[max * 2] = '\0';
    return true;
}

bool lab_wait_ready(uint32_t ms) {
    uint32_t waited = 0;
    while (waited < ms) {
        struct stat st;
        if (stat(lab_ready_path(), &st) == 0) return true;
        usleep(20000);
        waited += 20;
    }
    return false;
}

bool lab_note_finding(const char* engine, const char* inv, const char* kind,
                      const char* payload_hex,
                      const LabSnap& before, const LabSnap& after) {
    char b[LAB_SNAP_LINE];
    char a[LAB_SNAP_LINE];
    if (!lab_snap_encode(before, b, LAB_SNAP_LINE)) return false;
    if (!lab_snap_encode(after, a, LAB_SNAP_LINE)) return false;
    for (uint16_t i = 0; b[i] != '\0'; ++i) {
        if (b[i] == '\n') b[i] = ' ';
    }
    for (uint16_t i = 0; a[i] != '\0'; ++i) {
        if (a[i] == '\n') a[i] = ' ';
    }
    char path[192];
    snprintf(path, sizeof(path), "%s/findings.jsonl", lab_dir());
    FILE* f = fopen(path, "a");
    if (f == nullptr) return false;
    (void)flock(fileno(f), LOCK_EX);
    fprintf(f,
            "{\"engine\":\"%s\",\"inv\":\"%s\",\"kind\":\"%s\","
            "\"payload_hex\":\"%s\",\"before\":\"%s\",\"after\":\"%s\"}\n",
            engine != nullptr ? engine : "-",
            inv != nullptr ? inv : "-",
            kind != nullptr ? kind : "invariant",
            payload_hex != nullptr ? payload_hex : "",
            b, a);
    (void)fflush(f);
    (void)flock(fileno(f), LOCK_UN);
    fclose(f);
    return true;
}

int lab_eval_send(const char* engine, const uint8_t* wire, uint16_t len, int udp) {
    LabSnap before;
    LabSnap after;
    if (!lab_snap_now(&before)) return -1;
    if (!lab_udp_send(udp, wire, len)) return -1;
    uint8_t i = 0;
    while (i < 20) {
        usleep(8000);
        if (!lab_snap_now(&after)) return -1;
        if (after.frozen != before.frozen ||
            after.kind[0] != '-' ||
            after.hmac_ok != before.hmac_ok ||
            after.now != before.now) {
            break;
        }
        ++i;
    }
    LabInvResult r = lab_check_pair(before, after);
    if (r.ok == 0) {
        char hex[200];
        hex[0] = '\0';
        (void)lab_hex40(wire, len, hex, 200);
        (void)lab_note_finding(engine, r.id, "invariant", hex, before, after);
        return 1;
    }
    return 0;
}
