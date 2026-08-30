#include "p30_harness.h"

#include <stdio.h>

int main() {
    bool ok = true;
    (void)remove(VAULT_BIN_PATH);

    P30Hive h;
    p30_attach(h);
    if (!p30_bind_root(h)) ok = false;

    Device d{};
    p30_copy_id(d.id, "W");
    d.role = ROLE_WORKER;
    d.trust_level = 2;
    d.status = DeviceState::Pending;
    if (!h.reg.addDevice(d)) ok = false;
    uint8_t devices = h.reg.getDeviceCount();

    Event ev = p30_event(EventType::Heartbeat, "W", 1000, nullptr);
    (void)h.vault.signEvent(ev);
    if (!h.vault.store(ev, 1000)) ok = false;
    if (!h.vault.flush()) ok = false;

    FILE* f = fopen(VAULT_BIN_PATH, "rb+");
    if (f == nullptr) {
        printf("FAIL p30_vault\n");
        return 1;
    }
    if (fseek(f, 8, SEEK_SET) != 0) ok = false;
    int c = fgetc(f);
    if (c == EOF) ok = false;
    if (fseek(f, 8, SEEK_SET) != 0) ok = false;
    if (fputc(c ^ 0xFF, f) == EOF) ok = false;
    fclose(f);

    GhostVault bad;
    bad.attachKeys(&h.keys);
    uint8_t before = bad.getStoredCount();
    if (bad.load()) ok = false;
    if (!bad.safeMode()) ok = false;
    if (bad.getStoredCount() < before) ok = false;
    if (h.reg.getDeviceCount() != devices) ok = false;
    if (h.reg.getState("W") != DeviceState::Pending) ok = false;
    if (!p30_has_type(bad, EventType::PolicyViolation)) ok = false;

    char buf[OUTPUT_BUFFER_LEN];
    GhostOutput out;
    out.buildVault(bad, buf);
    if (!p30_contains(buf, "safemode") || !p30_contains(buf, "1")) ok = false;

    FILE* trunc = fopen(VAULT_BIN_PATH, "wb");
    if (trunc == nullptr) ok = false;
    else {
        uint8_t junk[3] = {1, 9, 0};
        (void)fwrite(junk, 1, 3, trunc);
        fclose(trunc);
    }
    GhostVault shortv;
    shortv.attachKeys(&h.keys);
    if (shortv.load()) ok = false;
    if (!shortv.safeMode()) ok = false;

    printf(ok ? "PASS p30_vault\n" : "FAIL p30_vault\n");
    return ok ? 0 : 1;
}
