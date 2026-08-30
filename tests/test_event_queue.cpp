#include <cstdio>
#include "../src/psp/event_queue.h"

int main() {
    EventQueue q;

    bool ok = true;

    Event e1{};
    e1.type = EventType::ScanResult;
    e1.source_device_id[0] = 'S';
    e1.source_device_id[1] = '\0';
    e1.severity = Severity::Info;

    Event e2 = e1;
    e2.type = EventType::Heartbeat;

    if (!q.push(e1)) ok = false;
    if (!q.push(e2)) ok = false;
    if (q.getSize() != 2) ok = false;

    Event out{};
    if (!q.pop(out)) ok = false;
    if (out.type != EventType::ScanResult) ok = false;

    if (!q.pop(out)) ok = false;
    if (out.type != EventType::Heartbeat) ok = false;

    if (!q.isEmpty()) ok = false;

    printf(ok ? "PASS event_queue\n" : "FAIL event_queue\n");
    return ok ? 0 : 1;
}
