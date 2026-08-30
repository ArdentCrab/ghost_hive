#include "../src/laptop/peer_halt.h"
#include "../src/laptop/peer_keys.h"
#include "../src/psp/ghost_keys.h"
#include <stdio.h>

static bool g_ok = true;

static void chk(bool c) {
    if (!c) g_ok = false;
}

int main() {
    peer_halt_reset();
    chk(!peer_halt_dead());
    chk(peer_halt_can_tx());

    Event hb{};
    hb.type = EventType::Heartbeat;
    chk(!peer_halt_is_kill(hb));
    Event danger{};
    danger.type = EventType::DangerModeEnter;
    chk(!peer_halt_is_kill(danger));

    Event kill{};
    kill.type = EventType::GhostDownStart;
    chk(peer_halt_is_kill(kill));

    peer_halt_run(ROLE_WORKER, "W");
    chk(peer_halt_dead());
    chk(!peer_halt_can_tx());
    chk(peer_halt_has_marker("W"));

    peer_halt_reset();
    peer_halt_run(ROLE_PHONE, "P");
    chk(peer_halt_has_marker("P"));
    chk(!peer_halt_can_tx());

    peer_halt_reset();
    peer_halt_run(ROLE_SENSOR, "F");
    chk(peer_halt_has_marker("F"));

    peer_halt_reset();
    peer_halt_run(ROLE_ROUTER, "R");
    chk(peer_halt_has_marker("R"));

    peer_halt_reset();
    peer_halt_run(ROLE_SAFE, "N");
    chk(peer_halt_has_marker("N"));

    peer_halt_reset();
    peer_halt_run(ROLE_KERNEL, "K");
    chk(!peer_halt_dead());
    chk(peer_halt_can_tx());
    chk(!peer_halt_has_marker("K"));

    peer_halt_reset();
    peer_halt_run(ROLE_MINE, "X");
    chk(!peer_halt_dead());
    chk(!peer_halt_has_marker("X"));

    GhostKeys keys;
    keys.initEmpty();
    uint8_t root[KEY_LEN];
    for (uint8_t i = 0; i < KEY_LEN; ++i) root[i] = static_cast<uint8_t>(i + 3);
    chk(keys.provisionRoot(root, KEY_LEN));
    chk(keys.provisionDerived(root, KEY_LEN));
    for (uint8_t i = 0; i < KEY_LEN; ++i) root[i] = 0;

    Event unsigned_kill{};
    unsigned_kill.type = EventType::GhostDownStart;
    unsigned_kill.source_device_id[0] = 'k';
    unsigned_kill.source_device_id[1] = 'e';
    unsigned_kill.source_device_id[2] = 'r';
    unsigned_kill.source_device_id[3] = 'n';
    unsigned_kill.source_device_id[4] = 'e';
    unsigned_kill.source_device_id[5] = 'l';
    unsigned_kill.source_device_id[6] = '\0';
    unsigned_kill.timestamp = 50;
    unsigned_kill.severity = Severity::Critical;
    chk(!peer_halt_authorized(keys, unsigned_kill));

    Event peer_kill = unsigned_kill;
    peer_kill.source_device_id[0] = 'W';
    peer_kill.source_device_id[1] = '\0';
    chk(peer_sign_event(keys, peer_kill));
    chk(!peer_halt_authorized(keys, peer_kill));

    Event kernel_kill = unsigned_kill;
    chk(peer_sign_event(keys, kernel_kill));
    chk(peer_halt_authorized(keys, kernel_kill));

    printf(g_ok ? "PASS peer_halt\n" : "FAIL peer_halt\n");
    return g_ok ? 0 : 1;
}
