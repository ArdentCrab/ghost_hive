#include <cstdio>
#include "../src/psp/ghost_policy.h"

int main() {
    GhostPolicy policy;
    policy.initDefaults();

    Event ev{};
    ev.type = EventType::HeartbeatMiss;
    ev.severity = Severity::High;
    ev.source_device_id[0] = 'W';
    ev.source_device_id[1] = '\0';

    PolicyAction action = policy.evaluate(ev);
    bool ok = (action == PolicyAction::Alert || action == PolicyAction::Backup);

    Event hmac{};
    hmac.type = EventType::PolicyViolation;
    hmac.severity = Severity::High;
    hmac.payload[0] = 'h';
    hmac.payload[1] = 'm';
    hmac.payload[2] = 'a';
    hmac.payload[3] = 'c';
    hmac.payload[4] = '_';
    hmac.payload[5] = 'i';
    hmac.payload[6] = '\0';
    ok = ok && (policy.evaluate(hmac) == PolicyAction::Alert);
    ok = ok && (policy.ruleCount() == 16);

    Event telem{};
    telem.type = EventType::TelemetryUpdate;
    telem.severity = Severity::Info;
    telem.source_device_id[0] = 'W';
    telem.source_device_id[1] = '\0';
    ok = ok && (policy.evaluate(telem) == PolicyAction::LogOnly);
    ok = ok && (policy.extraFlags(telem) == 0);

    printf(ok ? "PASS policy\n" : "FAIL policy\n");
    return ok ? 0 : 1;
}
