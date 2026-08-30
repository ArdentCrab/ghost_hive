#include "peer_keys.h"
#include "ghost_keys.h"

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static int fail = 0;

static void chk(bool ok, const char* name) {
    if (ok) printf("PASS %s\n", name);
    else {
        printf("FAIL %s\n", name);
        fail = 1;
    }
}

int main() {
    chk(!peer_bind_path_ok("https://evil/peer.bind"), "reject https");
    chk(!peer_bind_path_ok("s3://bucket/peer.bind"), "reject s3");
    chk(!peer_bind_path_ok("/tmp/ghost_hive/../peer.bind"), "reject dotdot");
    chk(!peer_bind_path_ok("/tmp/ghost_hive/other.bind"), "reject name");
    chk(peer_bind_path_ok("/tmp/ghost_hive/peer.bind"), "allow hive bind");
    chk(peer_bind_path_ok("/tmp/ghost_lab/peer.bind"), "allow lab bind");

    (void)mkdir("/tmp/ghost_hive", 0700);
    GhostKeys src;
    src.initEmpty();
    chk(src.generateRoot(), "root");
    chk(src.provisionDerived(src.root(), KEY_LEN), "derive");
    uint8_t buf[GhostKeys::PEER_BIND_LEN];
    chk(src.exportPeerBind(buf, GhostKeys::PEER_BIND_LEN), "export");
    FILE* f = fopen("/tmp/ghost_hive/peer.bind", "wb");
    chk(f != nullptr, "open bind");
    if (f != nullptr) {
        chk(fwrite(buf, 1, GhostKeys::PEER_BIND_LEN, f) == GhostKeys::PEER_BIND_LEN,
            "write bind");
        fclose(f);
        (void)chmod("/tmp/ghost_hive/peer.bind", 0600);
    }
    GhostKeys dst;
    dst.initEmpty();
    chk(peer_bind_keys(dst, "/tmp/ghost_hive/peer.bind"), "import bind");
    chk(!dst.hasRoot(), "no root on peer");
    chk(dst.hasDevice(), "has device");

    GhostKeys bad;
    bad.initEmpty();
    chk(!peer_bind_keys(bad, "http://x/peer.bind"), "no cloud import");

    if (fail) {
        printf("FAIL test_opsec\n");
        return 1;
    }
    printf("PASS test_opsec\n");
    return 0;
}
