#include <cstdio>
#include "../src/laptop/tetact.h"

int main() {
    bool ok = true;
    TetactState st;
    tetact_init(st);
    Event ev{};
    TetactKind k = tetact_watch(st, 1000, &ev);
    if (k != TETACT_NONE) ok = false;
    k = tetact_watch(st, 100, &ev);
    if (k != TETACT_TAMPER) ok = false;
    if (ev.type != EventType::AnomalyDetected) ok = false;
    tetact_set_source(&ev, "W");
    if (ev.source_device_id[0] != 'W') ok = false;

    TetactState st2;
    tetact_init(st2);
    k = tetact_poll(st2, 2000, &ev);
    if (k == TETACT_NONE) ok = false;
    if (k == TETACT_SCAN && ev.type != EventType::ScanResult &&
        ev.type != EventType::DeviceSeen) {
        ok = false;
    }

    printf(ok ? "PASS tetact\n" : "FAIL tetact\n");
    return ok ? 0 : 1;
}
