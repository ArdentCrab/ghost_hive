#include <cstdio>
#include "../src/psp/ghost_core.h"
#include "../src/psp/registry.h"
#include "../src/psp/event_queue.h"
#include "../src/psp/ghost_policy.h"
#include "../src/psp/replay_guard.h"
#include "../src/psp/decision_pipeline.h"
#include "../src/psp/ghost_vault.h"
#include "../src/psp/ghost_keys.h"
#include "../src/psp/ghost_down.h"
#include "../src/psp/ghost_stealth.h"
#include "../src/psp/ghost_heartbeat.h"
#include "../src/psp/ghost_peek.h"
#include "../src/psp/ghost_scanner.h"
#include "../src/psp/ghost_crypto.h"
#include "../src/psp/ghost_output.h"
#include "../src/laptop/worker.h"
#include "../src/phone/sensor.h"
#include "../src/nas/safe.h"
#include "../src/mine/mine.h"

static bool contains(const char* hay, const char* needle) {
    if (hay == nullptr || needle == nullptr || needle[0] == '\0') return false;
    uint16_t i = 0;
    while (hay[i] != '\0') {
        uint16_t j = 0;
        while (needle[j] != '\0' && hay[i + j] == needle[j]) ++j;
        if (needle[j] == '\0') return true;
        ++i;
    }
    return false;
}

int main() {
    bool ok = true;

    uint8_t abc_sha[20];
    const uint8_t abc[3] = {'a', 'b', 'c'};
    ghost_sha1(abc, 3, abc_sha);
    const uint8_t expect[20] = {
        0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a,
        0xba, 0x3e, 0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c,
        0x9c, 0xd0, 0xd8, 0x9d
    };
    for (uint8_t i = 0; i < 20; ++i) {
        if (abc_sha[i] != expect[i]) ok = false;
    }

    // §36 nur PSP
    Registry reg;
    EventQueue q;
    GhostVault vault;
    DecisionPipeline pipe;
    pipe.attach(&reg, &vault);
    Event hb{};
    hb.type = EventType::Heartbeat;
    hb.source_device_id[0] = 'P';
    hb.source_device_id[1] = '\0';
    hb.timestamp = 1000;
    if (pipe.process(hb, 1000) != PipelineResult::Accepted) ok = false;
    if (reg.findByRole(ROLE_WORKER) != nullptr) ok = false;

    // §36 neues Gerät / kein Auto-Enroll
    Device newbie{};
    newbie.id[0] = 'W';
    newbie.id[1] = '\0';
    newbie.role = ROLE_WORKER;
    newbie.trust_level = 2;
    newbie.status = DeviceState::Online;
    if (!reg.addDevice(newbie)) ok = false;
    if (reg.getState("W") != DeviceState::Pending) ok = false;
    if (!reg.pairDevice("W")) ok = false;
    if (reg.getState("W") != DeviceState::Online) ok = false;

    // §36 Worker fällt aus / Heartbeat-Verlust
    pipe.heartbeat().send("W", 2000);
    pipe.heartbeat().tick(2031, &reg);
    if (pipe.heartbeat().getMissCount("W") == 0) ok = false;
    if (reg.getState("W") != DeviceState::Degraded) ok = false;

    // §36 NAS voll → Vault
    Safe nas;
    Event blob{};
    blob.type = EventType::BackupWritten;
    blob.source_device_id[0] = 'N';
    blob.source_device_id[1] = '\0';
    for (uint8_t i = 0; i < SAFE_SLOTS; ++i) {
        if (!nas.store(blob)) ok = false;
    }
    if (!nas.isFull()) ok = false;
    if (nas.store(blob)) ok = false;
    if (nas.canAlert()) ok = false;
    nas.setWriteLock(true);
    if (nas.store(blob)) ok = false;
    if (!nas.writeLocked()) ok = false;
    uint8_t before = vault.getStoredCount();
    Event full{};
    full.type = EventType::BackupWritten;
    full.source_device_id[0] = 'N';
    full.source_device_id[1] = '\0';
    full.payload[0] = 'n';
    // payload nas_full
    const char* nf = "nas_full";
    uint8_t pi = 0;
    while (nf[pi] != '\0') {
        full.payload[pi] = nf[pi];
        ++pi;
    }
    full.payload[pi] = '\0';
    if (pipe.process(full, 2100) != PipelineResult::Accepted) ok = false;
    if (vault.getStoredCount() <= before) ok = false;

    // §36 Registry kaputt / Kaltstart vermeiden
    GhostKeys keys;
    keys.initEmpty();
    uint8_t kbyte[KEY_LEN];
    for (uint8_t i = 0; i < KEY_LEN; ++i) kbyte[i] = static_cast<uint8_t>(i + 1);
    if (!keys.provisionRoot(kbyte, KEY_LEN)) ok = false;
    reg.clear();
    if (reg.getDeviceCount() != 0) ok = false;
    if (!keys.hasRoot()) ok = false;

    GhostStealth stealth;
    stealth.enterTerminalMode();
    GhostDown down;
    down.attach(&vault, &stealth);
    down.execute(3000);
    if (!stealth.isGameMode()) ok = false;
    if (stealth.isInvisible()) ok = false;
    down.tick(3005);
    if (!down.nasFlushTimedOut()) ok = false;
    if (!down.peekAllowed()) ok = false;
    if (!down.coldStartAvoided()) ok = false;
    if (!stealth.isGameMode()) ok = false;
    down.tick(3035);
    if (!down.storageFlushDone()) ok = false;

    GhostPeek peek;
    peek.ingestGuard(pipe.replay());
    if (peek.coldStart()) ok = false;

    // §36 Zeitdrift
    GhostPolicy policy;
    policy.initDefaults();
    Event drift{};
    drift.type = EventType::ConfigChange;
    drift.source_device_id[0] = 'W';
    drift.source_device_id[1] = '\0';
    const char* td = "time_drift";
    pi = 0;
    while (td[pi] != '\0') {
        drift.payload[pi] = td[pi];
        ++pi;
    }
    drift.payload[pi] = '\0';
    if (policy.evaluate(drift) != PolicyAction::LogOnly) ok = false;
    if ((policy.extraFlags(drift) & PX_TIME_ANCHOR) == 0) ok = false;

    // §36 Danger Mode
    Event danger{};
    danger.type = EventType::DangerModeEnter;
    danger.source_device_id[0] = 'P';
    danger.source_device_id[1] = '\0';
    if (policy.evaluate(danger) != PolicyAction::LogOnly) ok = false;
    if ((policy.extraFlags(danger) & PX_PASSIVE) == 0) ok = false;

    // §36 Final Backup
    uint8_t flushed = vault.getStoredCount();
    vault.tick(4000);
    if (vault.getStoredCount() != flushed) ok = false;

    // §36 Mine kritisch
    Event mc{};
    mc.type = EventType::MineEvent;
    mc.severity = Severity::Critical;
    mc.source_device_id[0] = 'M';
    mc.source_device_id[1] = '\0';
    mc.payload[0] = '1';
    mc.payload[1] = ':';
    mc.payload[2] = '1';
    mc.payload[3] = '\0';
    PipelineResult pr = pipe.process(mc, 5000);
    if (pr != PipelineResult::Accepted && pr != PipelineResult::Blocked) ok = false;
    if (policy.evaluate(mc) != PolicyAction::Alert) ok = false;

    // §36 Mine still
    Event silent{};
    silent.type = EventType::DeviceLost;
    silent.source_device_id[0] = 'M';
    silent.source_device_id[1] = '\0';
    const char* ms = "mine_silent";
    pi = 0;
    while (ms[pi] != '\0') {
        silent.payload[pi] = ms[pi];
        ++pi;
    }
    silent.payload[pi] = '\0';
    if (policy.evaluate(silent) != PolicyAction::LogOnly) ok = false;
    if ((policy.extraFlags(silent) & PX_CHECK_MINE) == 0) ok = false;

    Device mineDev{};
    mineDev.id[0] = 'X';
    mineDev.id[1] = '\0';
    mineDev.role = ROLE_MINE;
    mineDev.status = DeviceState::Silent;
    if (!reg.addDevice(mineDev)) ok = false;
    pipe.heartbeat().send("X", 6000);
    pipe.heartbeat().tick(6090, &reg);
    if (reg.getState("X") != DeviceState::Suspected) ok = false;

    // Replay + TOTP
    uint8_t seed[TOTP_SEED_LEN];
    for (uint8_t i = 0; i < TOTP_SEED_LEN; ++i) seed[i] = static_cast<uint8_t>(i + 3);
    Mine mine;
    mine.setId("M1");
    if (!mine.setTotpSeed(seed, TOTP_SEED_LEN)) ok = false;
    if (mine.canReceive()) ok = false;

    ReplayGuard guard;
    if (!guard.setTotpSeed("M1", seed, TOTP_SEED_LEN)) ok = false;
    MinePayload mp{};
    if (!mine.send(&mp, 7000)) ok = false;
    if (!guard.check(mp, 7000)) ok = false;
    if (guard.check(mp, 7090)) ok = false;

    MinePayload mp2{};
    if (!mine.send(&mp2, 7090)) ok = false;
    if (!guard.check(mp2, 7090)) ok = false;
    uint32_t held = mine.counter();
    mine.freezeEvents();
    MinePayload mp3{};
    if (mine.send(&mp3, 8000)) ok = false;
    if (mine.counter() != held) ok = false;

    // Hive 1.0 versions
    Worker worker;
    worker.setId("W");
    Sensor sensor;
    sensor.setId("S");
    if (worker.versionMajor() != 1) ok = false;
    if (sensor.versionMajor() != 1) ok = false;
    if (nas.versionMajor() != 1) ok = false;
    if (mine.versionMajor() != 1) ok = false;
    if (sensor.canWrite()) ok = false;

    // Scanner Game-Mode
    GhostScanner scanner;
    scanner.setTerminalMode(false);
    if (scanner.scanWifi()) ok = false;
    if (scanner.scanBluetooth()) ok = false;
    scanner.setTerminalMode(true);
    if (!scanner.scanBluetooth()) ok = false;
    if (scanner.getBtCount() != 0) ok = false;

    // Views §35 — echte Werte, keine Deko
    char buf[OUTPUT_BUFFER_LEN];
    GhostOutput out;
    out.buildDevices(reg, buf);
    if (buf[0] == '\0') ok = false;
    if (!contains(buf, "X")) ok = false;
    out.buildEvents(q, buf);
    if (buf[0] == '\0') ok = false;
    out.buildHeartbeat(pipe.heartbeat(), "W", buf);
    if (buf[0] == '\0') ok = false;
    out.buildHeartbeat(pipe.heartbeat(), reg, buf);
    if (buf[0] == '\0') ok = false;
    out.buildVault(vault, buf);
    if (buf[0] == '\0') ok = false;
    if (!contains(buf, "crc")) ok = false;
    out.buildPolicy(policy, buf);
    if (buf[0] == '\0') ok = false;
    if (!contains(buf, "P01")) ok = false;
    if (!contains(buf, "P14")) ok = false;
    if (!contains(buf, "P15")) ok = false;
    out.buildPolicyView(policy, buf);
    if (buf[0] == '\0') ok = false;
    out.buildPeek(peek, buf);
    if (buf[0] == '\0') ok = false;
    out.buildMines(guard, buf);
    if (buf[0] == '\0') ok = false;
    if (!contains(buf, "M1")) ok = false;
    out.buildReplay(guard, buf);
    if (buf[0] == '\0') ok = false;
    if (!contains(buf, "M1")) ok = false;
    out.buildStatus(buf, reg, q, vault, stealth);
    if (buf[0] == '\0') ok = false;
    if (!contains(buf, "1.7.3")) ok = false;

    // P4 FINAL — Pairing, Queue→Vault, Heartbeat-Miss, Policy-Flags
    Registry registry;
    EventQueue queue;
    GhostVault p4vault;
    GhostHeartbeat heartbeat;
    GhostStealth p4stealth;
    GhostPolicy p4policy;
    p4policy.initDefaults();
    if (registry.getDeviceCount() != 0) ok = false;

    Device dev{};
    dev.id[0] = 'A';
    dev.id[1] = '\0';
    dev.role = ROLE_WORKER;
    dev.status = DeviceState::Pending;
    if (!registry.addDevice(dev)) ok = false;
    if (!registry.pairDevice("A")) ok = false;
    if (registry.getState("A") != DeviceState::Online) ok = false;

    Event ev{};
    ev.type = EventType::Heartbeat;
    ev.source_device_id[0] = 'A';
    ev.source_device_id[1] = '\0';
    ev.severity = Severity::Info;
    if (!queue.push(ev)) ok = false;
    out.buildEvents(queue, buf);
    if (!contains(buf, "Heartbeat")) ok = false;
    if (!contains(buf, "A")) ok = false;

    Event popped{};
    if (!queue.pop(popped)) ok = false;
    if (!p4vault.store(popped)) ok = false;
    if (p4vault.getStoredCount() != 1) ok = false;
    if (p4vault.checksum() == 0) ok = false;

    heartbeat.update("A", 0);
    heartbeat.checkAll(31, registry);
    if (registry.getState("A") != DeviceState::Degraded) ok = false;

    Event miss{};
    miss.type = EventType::HeartbeatMiss;
    miss.source_device_id[0] = 'A';
    miss.source_device_id[1] = '\0';
    miss.severity = Severity::Warn;
    uint8_t actions = p4policy.evaluate(miss, "degraded");
    if (actions == 0) ok = false;

    out.buildStatus(buf, registry, queue, p4vault, p4stealth);
    if (buf[0] == '\0') ok = false;
    if (!contains(buf, "devices  1")) ok = false;
    if (!contains(buf, "vault    1")) ok = false;
    out.buildDevices(registry, buf);
    if (!contains(buf, "A")) ok = false;
    out.buildHeartbeat(heartbeat, registry, buf);
    if (!contains(buf, "A")) ok = false;
    out.buildVault(p4vault, buf);
    if (!contains(buf, "entries  1")) ok = false;

    printf(ok ? "PASS p36\n" : "FAIL p36\n");
    return ok ? 0 : 1;
}
