#include <cstdio>
#include "../src/psp/ghost_core.h"
#include "../src/psp/registry.h"
#include "../src/psp/event_queue.h"
#include "../src/psp/ghost_policy.h"
#include "../src/psp/context_engine.h"
#include "../src/psp/priority_engine.h"
#include "../src/psp/replay_guard.h"
#include "../src/psp/decision_pipeline.h"
#include "../src/psp/ghost_scanner.h"
#include "../src/psp/ghost_stealth.h"
#include "../src/psp/ghost_peek.h"
#include "../src/psp/ghost_output.h"
#include "../src/psp/ghost_terminal.h"
#include "../src/mine/mine.h"
#include "../src/phone/sensor.h"
#include "../src/psp/ghost_telemetry.h"

int main() {
    bool ok = true;

    // §25 Topologie-Budget
    if (MAX_DEVICES != 32) ok = false;
    if (CMD_COUNT != 12) ok = false;
    if (REPLAY_WINDOW_PER_MINE != 64) ok = false;
    if (MAX_TRACKED_MINES != 32) ok = false;
    if (MAX_WORKERS != 1) ok = false;
    if (MAX_SAFES != 1) ok = false;
    if (MAX_PHONES != 1) ok = false;
    if (MAX_ROUTERS != 1) ok = false;
    if (MAX_SENSORS != 8) ok = false;
    if (HOST_MINE_SLOTS != 8) ok = false;
    if (MINE_INTERVAL_SEC < TOTP_WINDOW_MIN_SEC) ok = false;
    if (MINE_INTERVAL_SEC > TOTP_WINDOW_MAX_SEC) ok = false;
    if (SENSOR_INTERVAL_SEC < 60 || SENSOR_INTERVAL_SEC > 300) ok = false;
    if (TELEM_INTERVAL_SEC < 1 || TELEM_INTERVAL_SEC > 5) ok = false;

    char ids[HOST_MINE_SLOTS][32];
    uint8_t n = 0;
    if (!mine_arg_id("mine=ML1", ids[0])) ok = false;
    if (!mine_push_id(ids, &n, HOST_MINE_SLOTS, "ML1")) ok = false;
    if (!mine_push_id(ids, &n, HOST_MINE_SLOTS, "ML2")) ok = false;
    if (n != 2) ok = false;
    if (!mine_push_id(ids, &n, HOST_MINE_SLOTS, "ML1")) ok = false;
    if (n != 2) ok = false;

    // §9 / Registry
    Registry reg;
    reg.clear();

    Device d{};
    d.id[0] = 'W';
    d.id[1] = '\0';
    d.role = 1;
    d.trust_level = 2;
    d.status = DeviceState::Unknown;

    if (!reg.addDevice(d)) ok = false;
    if (!reg.setState("W", DeviceState::Pending)) ok = false;
    if (reg.getState("W") != DeviceState::Pending) ok = false;
    if (reg.setState("W", DeviceState::Blocked)) ok = false;

    // §21 Context
    ContextEngine ctx;
    if (ctx.compute("home", "", true, true) != ContextMode::Home) ok = false;
    if (ctx.compute(nullptr, "", false, false) != ContextMode::Offline) ok = false;

    // §19 Priority
    PriorityEngine prio;
    if (prio.compute(1, 2, true, true) != Priority::Worker) ok = false;
    if (prio.compute(3, 3, true, true) != Priority::Psp) ok = false;

    // §17 Policy
    GhostPolicy policy;
    policy.initDefaults();
    Event ev{};
    ev.type = EventType::HeartbeatMiss;
    ev.severity = Severity::High;
    ev.source_device_id[0] = 'W';
    ev.source_device_id[1] = '\0';
    PolicyAction action = policy.evaluate(ev);
    if (action != PolicyAction::Alert && action != PolicyAction::Backup) ok = false;
    if (policy.ruleCount() != 16) ok = false;

    // §15 Replay-Guard
    ReplayGuard guard;
    MinePayload p{};
    p.mine_id[0] = 'M';
    p.mine_id[1] = '\0';
    p.counter = 1;
    p.totp = 100;
    if (!guard.check(p, 1000)) ok = false;
    if (guard.check(p, 1120)) ok = false;

    // §18 Pipeline
    GhostVault vault;
    vault.init();
    DecisionPipeline pipe;
    pipe.attach(&reg, &vault);
    Event hb{};
    hb.type = EventType::Heartbeat;
    hb.source_device_id[0] = 'W';
    hb.source_device_id[1] = '\0';
    hb.timestamp = 1000;
    if (pipe.process(hb, 1000) != PipelineResult::Accepted) ok = false;
    if (vault.getStoredCount() != 1) ok = false;

    EventQueue pipeQ;
    Event queued = hb;
    queued.timestamp = 1001;
    if (!pipeQ.push(queued)) ok = false;
    pipe.drain(pipeQ, 1001);
    if (!pipeQ.isEmpty()) ok = false;
    if (vault.getStoredCount() != 2) ok = false;

    Event bad{};
    if (pipe.process(bad, 1000) != PipelineResult::Rejected) ok = false;

    // Scanner + Output zusammen
    GhostScanner scanner;
    scanner.setTerminalMode(true);
    scanner.scanWifi();
    if (scanner.getWifiCount() != 0) ok = false;
    if (scanner.getWifi(0) != nullptr) ok = false;
    if (scanner.lastScanBlocked()) ok = false;

    scanner.setTerminalMode(false);
    scanner.scanWifi();
    if (!scanner.lastScanBlocked()) ok = false;
    if (scanner.getWifiCount() != 0) ok = false;

    GhostStealth stealth;
    stealth.enterInvisibleMode();
    if (!stealth.isInvisible()) ok = false;

    GhostPeek peek;
    peek.perform();
    if (peek.getMineCount() != 0) ok = false;

    char buf[OUTPUT_BUFFER_LEN];
    GhostOutput out;
    out.buildStatus(buf);
    if (buf[0] == '\0') ok = false;

    out.buildScan(scanner, buf);
    if (buf[0] == '\0') ok = false;

    GhostDown gdown;
    out.buildGhostDown(buf, gdown, 0, false);
    if (buf[0] == '\0') ok = false;
    {
        const char* p = buf;
        uint8_t hit = 0;
        const char* keys[6] = {"phase", "timer", "snap", "nas", "stor", "kill"};
        for (uint8_t k = 0; k < 6; ++k) {
            const char* q = p;
            const char* n = keys[k];
            while (*q != '\0') {
                uint8_t i = 0;
                while (n[i] != '\0' && q[i] == n[i]) ++i;
                if (n[i] == '\0') {
                    ++hit;
                    break;
                }
                ++q;
            }
        }
        if (hit != 6) ok = false;
    }
    if (gdown.nasDueMs() != 0 || gdown.storageDueMs() != 0) ok = false;

    out.buildPeek(peek, buf);
    if (buf[0] == '\0') ok = false;

    EventQueue q;
    if (!q.isEmpty()) ok = false;

    printf(ok ? "PASS spec\n" : "FAIL spec\n");
    return ok ? 0 : 1;
}
