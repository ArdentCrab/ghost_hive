#include "peer_halt.h"
#include "peer_keys.h"

#include <stdio.h>

#if !defined(__PSP__) && !defined(PSP_BUILD)
#include <sys/stat.h>
#ifdef GHOST_OS_HALT
#include <unistd.h>
#endif
#endif

static bool g_dead = false;

static void copy_id(char* dst, const char* src) {
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

static const char* tag_for_role(uint8_t role) {
    if (role == ROLE_WORKER) return "worker";
    if (role == ROLE_PHONE) return "phone";
    if (role == ROLE_SENSOR) return "sensor";
    if (role == ROLE_ROUTER) return "router";
    if (role == ROLE_SAFE) return "safe";
    return "peer";
}

bool peer_halt_is_kill(const Event& event) {
    return event.type == EventType::GhostDownStart;
}

bool peer_halt_authorized(const GhostKeys& keys, const Event& event) {
    if (!peer_halt_is_kill(event)) return false;
    const char* kid = KERNEL_SOURCE_ID;
    uint8_t i = 0;
    while (kid[i] != '\0') {
        if (event.source_device_id[i] != kid[i]) return false;
        ++i;
    }
    if (event.source_device_id[i] != '\0') return false;
    return peer_verify_event(keys, event);
}

bool peer_halt_dead() {
    return g_dead;
}

bool peer_halt_can_tx() {
    return !g_dead;
}

void peer_halt_reset() {
    g_dead = false;
}

bool peer_halt_has_marker(const char* id) {
    if (id == nullptr || id[0] == '\0') return false;
#if defined(__PSP__) || defined(PSP_BUILD)
    (void)id;
    return g_dead;
#else
    char path[64];
    path[0] = '\0';
    const char* pfx = "/tmp/ghost_hive/halt.";
    uint8_t i = 0;
    while (pfx[i] != '\0' && i < 48) {
        path[i] = pfx[i];
        ++i;
    }
    uint8_t j = 0;
    while (id[j] != '\0' && i < 62) {
        path[i++] = id[j++];
    }
    path[i] = '\0';
    FILE* f = fopen(path, "rb");
    if (f == nullptr) return false;
    fclose(f);
    return true;
#endif
}

void peer_halt_run(uint8_t role, const char* id) {
    // §43: nie gegen PSP. Minen haben keinen RX.
    if (role == ROLE_KERNEL) return;
    if (role == ROLE_MINE) return;
    g_dead = true;

#if defined(__PSP__) || defined(PSP_BUILD)
    (void)id;
    return;
#else
    struct stat st;
    if (stat("/tmp/ghost_hive", &st) != 0) {
        (void)mkdir("/tmp/ghost_hive", 0755);
    }

    char path[64];
    path[0] = '\0';
    const char* pfx = "/tmp/ghost_hive/halt.";
    uint8_t i = 0;
    while (pfx[i] != '\0' && i < 48) {
        path[i] = pfx[i];
        ++i;
    }
    char nid[32];
    copy_id(nid, id);
    uint8_t j = 0;
    while (nid[j] != '\0' && i < 62) {
        path[i++] = nid[j++];
    }
    path[i] = '\0';

    FILE* f = fopen(path, "wb");
    if (f != nullptr) {
        const char* tag = tag_for_role(role);
        uint8_t n = 0;
        while (tag[n] != '\0') ++n;
        (void)fwrite(tag, 1, n, f);
        (void)fwrite("\n", 1, 1, f);
        if (role == ROLE_ROUTER) {
            const char ports[6] = {'p', 'o', 'r', 't', 's', '\n'};
            (void)fwrite(ports, 1, 6, f);
        }
        if (role == ROLE_SAFE) {
            const char shares[7] = {'s', 'h', 'a', 'r', 'e', 's', '\n'};
            (void)fwrite(shares, 1, 7, f);
        }
        fclose(f);
    }

#ifdef GHOST_OS_HALT
    if (role == ROLE_WORKER || role == ROLE_PHONE) {
        execlp("shutdown", "shutdown", "-h", "now", static_cast<char*>(0));
    }
#endif
#endif
}
