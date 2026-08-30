#include <cstdio>
#include <sys/stat.h>
#include "../src/psp/root_config.h"
#include "../src/psp/ghost_keys.h"

static void writeFile(const char* path, const char* body) {
    FILE* f = fopen(path, "wb");
    if (f == nullptr) return;
    uint16_t i = 0;
    while (body[i] != '\0') ++i;
    fwrite(body, 1, i, f);
    fclose(f);
}

int main() {
    bool ok = true;
    (void)mkdir("/tmp/ghost_hive", 0755);
    (void)remove(ROOT_CONFIG_PATH);

    const char* good =
        "{"
        "\"hive_members\":["
        "{\"id\":\"W\",\"type\":\"worker\"},"
        "{\"id\":\"P\",\"type\":\"phone\"},"
        "{\"id\":\"N\",\"type\":\"nas\"}"
        "],"
        "\"kill_policy\":{\"require_signatures\":true}"
        "}";
    writeFile(ROOT_CONFIG_PATH, good);
    Registry reg;
    reg.clear();
    if (!root_config_ingest(ROOT_CONFIG_PATH, reg)) ok = false;
    if (reg.getDevice("W") == nullptr || reg.getDevice("W")->role != ROLE_WORKER) {
        ok = false;
    }
    if (reg.getState("W") != DeviceState::Online) ok = false;
    if (reg.getDevice("P") == nullptr) ok = false;
    if (reg.getDevice("N") == nullptr || reg.getDevice("N")->role != ROLE_SAFE) {
        ok = false;
    }
    FILE* gone = fopen(ROOT_CONFIG_PATH, "rb");
    if (gone != nullptr) {
        fclose(gone);
        ok = false;
    }

    const char* secret =
        "{\"hive_members\":[{\"id\":\"W\",\"type\":\"worker\"}],"
        "\"root_key\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}";
    writeFile(ROOT_CONFIG_PATH, secret);
    Registry reg2;
    reg2.clear();
    if (root_config_ingest(ROOT_CONFIG_PATH, reg2)) ok = false;

    const char* nosig =
        "{\"hive_members\":[{\"id\":\"W\",\"type\":\"worker\"}],"
        "\"kill_policy\":{\"require_signatures\":false}}";
    writeFile(ROOT_CONFIG_PATH, nosig);
    Registry reg3;
    reg3.clear();
    if (root_config_ingest(ROOT_CONFIG_PATH, reg3)) ok = false;

    GhostKeys k;
    k.initEmpty();
    if (!k.generateRoot()) ok = false;
    if (!k.hasRoot()) ok = false;
    if (!k.provisionDerived(k.root(), KEY_LEN)) ok = false;
    uint8_t bind[GhostKeys::PEER_BIND_LEN];
    if (!k.exportPeerBind(bind, GhostKeys::PEER_BIND_LEN)) ok = false;
    GhostKeys peer;
    peer.initEmpty();
    if (!peer.importPeerBind(bind, GhostKeys::PEER_BIND_LEN)) ok = false;
    if (peer.hasRoot()) ok = false;
    if (!peer.hasDevice() || !peer.hasSession() || !peer.hasMine()) ok = false;
    for (uint8_t i = 0; i < GhostKeys::PEER_BIND_LEN; ++i) bind[i] = 0;

    printf(ok ? "PASS root_config\n" : "FAIL root_config\n");
    return ok ? 0 : 1;
}
