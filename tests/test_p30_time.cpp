#include "p30_harness.h"

int main() {
    bool ok = true;
    P30Hive h;
    p30_attach(h);

    Device d{};
    p30_copy_id(d.id, "W");
    d.role = ROLE_WORKER;
    d.trust_level = 1;
    d.status = DeviceState::Pending;
    d.last_seen = 1000;
    if (!h.reg.addDevice(d)) ok = false;
    if (!h.reg.pairDevice("W")) ok = false;
    Device low = *h.reg.getDevice("W");
    low.trust_level = 1;
    low.last_seen = 1000;
    if (!h.reg.updateDevice("W", low)) ok = false;

    Event drift = p30_event(EventType::ConfigChange, "W", 9000, "time_drift");
    if (h.pipe.process(drift, 9000) != PipelineResult::Accepted) ok = false;
    const Device* w = h.reg.getDevice("W");
    if (w == nullptr || w->last_seen != 1000) ok = false;

    Device high = *h.reg.getDevice("W");
    high.trust_level = 2;
    high.last_seen = 1000;
    if (!h.reg.updateDevice("W", high)) ok = false;
    Event drift2 = p30_event(EventType::ConfigChange, "W", 9100, "time_drift");
    if (h.pipe.process(drift2, 9100) != PipelineResult::Accepted) ok = false;
    w = h.reg.getDevice("W");
    if (w == nullptr || w->last_seen != 9100) ok = false;

    if (h.pipe.policy().evaluate(drift) != PolicyAction::LogOnly) ok = false;
    if ((h.pipe.policy().extraFlags(drift) & PX_TIME_ANCHOR) == 0) ok = false;

    printf(ok ? "PASS p30_time\n" : "FAIL p30_time\n");
    return ok ? 0 : 1;
}
