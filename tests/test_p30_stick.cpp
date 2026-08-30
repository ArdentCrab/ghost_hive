#include "p30_harness.h"

#include <stdio.h>

int main() {
    bool ok = true;
    (void)remove(VAULT_BIN_PATH);

    P30Hive h;
    p30_attach(h);
    if (!p30_bind_root(h)) ok = false;

    Event ev = p30_event(EventType::Heartbeat, "W", 1000, nullptr);
    (void)h.vault.signEvent(ev);
    if (!h.vault.store(ev, 1000)) ok = false;
    if (!h.vault.flush()) ok = false;

    GhostVault stolen;
    GhostKeys empty;
    empty.initEmpty();
    stolen.attachKeys(&empty);
    if (stolen.load()) ok = false;
    if (stolen.getStoredCount() != 0) ok = false;
    Event plain{};
    if (stolen.copyPlain(0, &plain)) ok = false;
    if (stolen.snapshot(&plain, 1) != 0) ok = false;

    GhostVault derived;
    GhostKeys peer;
    peer.initEmpty();
    uint8_t root[KEY_LEN];
    for (uint8_t i = 0; i < KEY_LEN; ++i) root[i] = static_cast<uint8_t>(i + 1);
    if (!peer.provisionDerived(root, KEY_LEN)) ok = false;
    if (peer.hasRoot()) ok = false;
    derived.attachKeys(&peer);
    if (derived.load()) ok = false;
    if (derived.getStoredCount() != 0) ok = false;

    printf(ok ? "PASS p30_stick\n" : "FAIL p30_stick\n");
    return ok ? 0 : 1;
}
