// Host-only keyd: holds root locally, issues 112-byte peer.bind (no root).
// Unix socket only. No TCP, no cloud.

#include "ghost_keys.h"
#include "peer_keys.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

static const char ROOT_PATH[] = "/tmp/ghost_hive/keyd.root";
static const char BIND_PATH[] = "/tmp/ghost_hive/peer.bind";
static const char TTL_PATH[] = "/tmp/ghost_hive/peer.bind.ttl";

static void zero(uint8_t* p, uint8_t n) {
    for (uint8_t i = 0; i < n; ++i) p[i] = 0;
}

static bool write_bytes(const char* path, const uint8_t* b, uint8_t n, mode_t mode) {
    FILE* f = fopen(path, "wb");
    if (f == nullptr) return false;
    size_t w = fwrite(b, 1, n, f);
    fclose(f);
    (void)chmod(path, mode);
    return w == n;
}

static bool load_or_make_root(GhostKeys& keys) {
    uint8_t raw[KEY_LEN];
    FILE* f = fopen(ROOT_PATH, "rb");
    if (f != nullptr) {
        size_t n = fread(raw, 1, KEY_LEN, f);
        int extra = fgetc(f);
        fclose(f);
        if (n != KEY_LEN || extra != EOF) {
            zero(raw, KEY_LEN);
            return false;
        }
        bool ok = keys.provisionRoot(raw, KEY_LEN) &&
                  keys.provisionDerived(raw, KEY_LEN);
        zero(raw, KEY_LEN);
        return ok;
    }
    if (!keys.generateRoot()) return false;
    if (!write_bytes(ROOT_PATH, keys.root(), KEY_LEN, 0600)) return false;
    return keys.provisionDerived(keys.root(), KEY_LEN);
}

static uint32_t issue_ttl() {
    time_t now = time(nullptr);
    if (now < 0) return 0;
    return static_cast<uint32_t>(now) + PEER_BIND_TTL_SEC;
}

static bool drop_bind(const GhostKeys& keys, uint32_t exp) {
    uint8_t buf[GhostKeys::PEER_BIND_LEN];
    if (!keys.exportPeerBind(buf, GhostKeys::PEER_BIND_LEN)) return false;
    bool ok = write_bytes(BIND_PATH, buf, GhostKeys::PEER_BIND_LEN, 0600);
    zero(buf, GhostKeys::PEER_BIND_LEN);
    if (!ok) return false;
    FILE* t = fopen(TTL_PATH, "wb");
    if (t == nullptr) return false;
    fprintf(t, "%u\n", exp);
    fclose(t);
    (void)chmod(TTL_PATH, 0600);
    return true;
}

int main() {
    peer_os_harden();
    (void)mkdir("/tmp/ghost_hive", 0700);
    GhostKeys keys;
    keys.initEmpty();
    if (!load_or_make_root(keys)) return 1;
    uint32_t exp = issue_ttl();
    if (exp == 0 || !drop_bind(keys, exp)) return 1;

    (void)unlink(PEER_KEYD_SOCK);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return 1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, PEER_KEYD_SOCK, sizeof(addr.sun_path) - 1);
    if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return 1;
    }
    (void)chmod(PEER_KEYD_SOCK, 0600);
    if (listen(fd, 2) != 0) {
        close(fd);
        return 1;
    }

    while (true) {
        int c = accept(fd, nullptr, nullptr);
        if (c < 0) continue;
        char req[4];
        ssize_t r = read(c, req, 4);
        if (r == 4 && req[0] == 'B' && req[1] == 'I' && req[2] == 'N' && req[3] == 'D') {
            exp = issue_ttl();
            (void)drop_bind(keys, exp);
            uint8_t buf[GhostKeys::PEER_BIND_LEN];
            if (keys.exportPeerBind(buf, GhostKeys::PEER_BIND_LEN)) {
                uint8_t ttlb[4];
                ttlb[0] = static_cast<uint8_t>(exp);
                ttlb[1] = static_cast<uint8_t>(exp >> 8);
                ttlb[2] = static_cast<uint8_t>(exp >> 16);
                ttlb[3] = static_cast<uint8_t>(exp >> 24);
                (void)write(c, buf, GhostKeys::PEER_BIND_LEN);
                (void)write(c, ttlb, 4);
                zero(buf, GhostKeys::PEER_BIND_LEN);
            }
        }
        close(c);
    }
}
