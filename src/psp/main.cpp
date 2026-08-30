// =====================================================
// Ghost Hive v1.7.1
// PSP-Einstiegspunkt — Phase E Sensor / Safe / Mine
// Spec-Basis: §10, §11, §12, §22, §23, §24, §25, §28, §32, §33, §42, §44
// =====================================================

#include "ghost_core.h"
#include "registry.h"
#include "event_queue.h"
#include "ghost_terminal.h"
#include "ghost_keys.h"
#include "ghost_vault.h"
#include "decision_pipeline.h"
#include "psp_time.h"
#include "psp_input.h"
#include "ghost_ir.h"
#include "root_config.h"
#include "ghost_wrap.h"
#include "hive_net.h"
#include "ghost_arm.h"
#include "transport/ghost_transport.h"
#include "transport/medium_wlan.h"
#include "transport/medium_ir.h"
#include "transport/sensor_transport.h"
#include "transport/safe_transport.h"
#include "transport/mine_transport.h"
#include "../phone/sensor.h"
#include "../nas/safe.h"
#include "../mine/mine.h"
#if defined(GHOST_MINI_WATCH) && !defined(__PSP__)
#include "../laptop/host_telem.h"
#include "../laptop/worker.h"
#include "ghost_telemetry.h"
#endif

#include <stdio.h>
#include <time.h>

#if defined(__PSP__)
#include <pspkernel.h>
#include <pspdebug.h>
#include <psprtc.h>
#include <pspdisplay.h>
#include <psppower.h>
#include <pspwlan.h>
#include <pspiofilemgr.h>

PSP_MODULE_INFO("GhostHive", 0, 1, 7);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_MAIN_THREAD_STACK_SIZE_KB(1024);
PSP_HEAP_SIZE_KB(20480); // §24: RAM ≤ 24 MB; Kernel-Heap 20480 KB

static int hive_exit_callback(int arg1, int arg2, void* common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    sceKernelExitGame();
    return 0;
}

static int hive_callback_thread(SceSize args, void* argp) {
    (void)args;
    (void)argp;
    int cbid = sceKernelCreateCallback("exit", hive_exit_callback, nullptr);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void hive_setup_callbacks() {
    SceUID th = sceKernelCreateThread("cb", hive_callback_thread, 0x11, 0xFA0, 0, nullptr);
    if (th >= 0) {
        sceKernelStartThread(th, 0, nullptr);
    }
}
#endif

#if !defined(__PSP__)
#include <sys/stat.h>
#endif

// §33: Root nur PSP-intern. JSON-Ingest ist root_config.json, nie Root-Bytes.
#if defined(__PSP__)
static const char ROOT_KEY_PATH[] = "ms0:/ghost_hive/k/root.key";
static const char ROOT_RESET_PATH[] = "ms0:/ghost_hive/k/root.reset";
static const char PEER_BIND_PATH[] = "ms0:/ghost_hive/k/peer.bind";
#else
static const char ROOT_KEY_PATH[] = "/tmp/ghost_hive_psp/root.key";
static const char ROOT_RESET_PATH[] = "/tmp/ghost_hive_psp/root.reset";
static const char PEER_BIND_PATH[] = "/tmp/ghost_hive/peer.bind";
#endif

static void hive_log(const char* line) {
#if defined(__PSP__)
    (void)line;
#else
    fprintf(stderr, "%s\n", line);
#endif
}

static void hive_u32_to_dec(uint32_t v, char* out) {
    char tmp[11];
    uint8_t t = 0;
    if (v == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    while (v > 0 && t < 10) {
        tmp[t++] = static_cast<char>('0' + (v % 10));
        v /= 10;
    }
    uint8_t o = 0;
    while (t > 0) out[o++] = tmp[--t];
    out[o] = '\0';
}

static void logRamBudget(uint32_t usedBytes) {
    const uint32_t cap = 24u * 1024u * 1024u;
    char used[12];
    char capb[12];
    hive_u32_to_dec(usedBytes, used);
    hive_u32_to_dec(cap, capb);
    char line[96];
    uint8_t i = 0;
    const char* p = "RAM budget ";
    while (p[i] != '\0' && i < 80) {
        line[i] = p[i];
        ++i;
    }
    uint8_t k = 0;
    while (used[k] != '\0' && i < 90) line[i++] = used[k++];
    line[i++] = '/';
    k = 0;
    while (capb[k] != '\0' && i < 94) line[i++] = capb[k++];
    line[i] = '\0';
    hive_log(line);
#if defined(__PSP__)
    const uint32_t heap = 20480u * 1024u;
    char heapb[12];
    hive_u32_to_dec(heap, heapb);
    i = 0;
    p = "RAM heap ";
    while (p[i] != '\0' && i < 80) {
        line[i] = p[i];
        ++i;
    }
    k = 0;
    while (heapb[k] != '\0' && i < 90) line[i++] = heapb[k++];
    line[i++] = '/';
    k = 0;
    while (capb[k] != '\0' && i < 94) line[i++] = capb[k++];
    line[i] = '\0';
    hive_log(line);
    uint32_t freeb = static_cast<uint32_t>(sceKernelTotalFreeMemSize());
    char fb[12];
    hive_u32_to_dec(freeb, fb);
    char fline[64];
    i = 0;
    p = "RAM free ";
    while (p[i] != '\0') {
        fline[i] = p[i];
        ++i;
    }
    k = 0;
    while (fb[k] != '\0' && i < 62) fline[i++] = fb[k++];
    fline[i] = '\0';
    hive_log(fline);
#endif
}

static uint32_t hive_now_ms() {
#if defined(__PSP__)
    u64 tick = 0;
    sceRtcGetCurrentTick(&tick);
    u32 tickres = sceRtcGetTickResolution();
    if (tickres == 0) tickres = 1000000;
    return static_cast<uint32_t>((static_cast<uint64_t>(tick) * 1000ull) /
                                 static_cast<uint64_t>(tickres));
#else
    return psp_now_ms();
#endif
}

static uint32_t hive_now_sec() {
#if defined(__PSP__)
    ScePspDateTime dt;
    dt.year = 0;
    dt.month = 0;
    dt.day = 0;
    dt.hour = 0;
    dt.minute = 0;
    dt.second = 0;
    dt.microsecond = 0;
    if (sceRtcGetCurrentClockLocalTime(&dt) == 0) {
        time_t t = 0;
        if (sceRtcGetTime_t(&dt, &t) == 0) {
            return static_cast<uint32_t>(t);
        }
    }
    u64 tick = 0;
    sceRtcGetCurrentTick(&tick);
    u32 tickres = sceRtcGetTickResolution();
    if (tickres == 0) tickres = 1000000;
    return static_cast<uint32_t>(static_cast<uint64_t>(tick) /
                                 static_cast<uint64_t>(tickres));
#else
    return psp_now_sec();
#endif
}

static void hive_wlan_off() {
    hive_net_down();
}

static bool hive_wlan_on() {
    return hive_net_up();
}

static void hive_apply_low_power(bool on) {
#if defined(__PSP__)
    if (on) {
        (void)scePowerSetClockFrequency(111, 111, 55);
        hive_wlan_off();
    } else {
        (void)scePowerSetClockFrequency(222, 222, 111);
    }
#else
    (void)on;
#endif
}

static void ingestIrAsSensor(GhostIR& irHw, Sensor& sensor, EventQueue& queue,
                             GhostVault& vault, uint32_t nowSec) {
    if (!irHw.ready()) return;
    uint8_t cmd = 0;
    if (!irHw.takeRx(&cmd)) return;
    Event ev{};
    if (!sensor.fillScan(&ev, nowSec)) return;
    const char* tag = "ir_signal_unknown";
    uint8_t i = 0;
    while (tag[i] != '\0' && i < 80) {
        ev.payload[i] = tag[i];
        ++i;
    }
    ev.payload[i++] = ':';
    const char* hex = "0123456789abcdef";
    ev.payload[i++] = hex[(cmd >> 4) & 0x0f];
    ev.payload[i++] = hex[cmd & 0x0f];
    ev.payload[i] = '\0';
    (void)vault.signEvent(ev);
    (void)sensor.sendScanResult(ev, queue);
}

static void copyKernelId(char* dst) {
    uint8_t i = 0;
    while (KERNEL_SOURCE_ID[i] != '\0' && i < 31) {
        dst[i] = KERNEL_SOURCE_ID[i];
        ++i;
    }
    dst[i] = '\0';
}

static void copyId(char* dst, const char* src) {
    uint8_t i = 0;
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i < 31) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static bool enqueueKernelHeartbeat(EventQueue& queue, uint32_t nowSec) {
    Event ev{};
    ev.type = EventType::Heartbeat;
    ev.severity = Severity::Info;
    ev.timestamp = nowSec;
    copyKernelId(ev.source_device_id);
    ev.payload[0] = '\0';
    return queue.push(ev);
}

static bool enrollPending(Registry& registry, const char* id, uint8_t role, uint8_t trust) {
    Device d{};
    copyId(d.id, id);
    d.role = role;
    d.trust_level = trust;
    d.status = DeviceState::Pending;
    if (!registry.addDevice(d)) return false;
    return registry.pairDevice(id);
}

static bool enrollSilentMine(Registry& registry, const char* id) {
    Device d{};
    copyId(d.id, id);
    d.role = ROLE_MINE;
    d.trust_level = 0;
    d.status = DeviceState::Silent;
    return registry.addDevice(d);
}

// §33: Device/Log/Session/Mine/TOTP deterministisch aus Root (HMAC-SHA1).
static bool bindDerivedSlots(GhostKeys& keys) {
    if (!keys.hasRoot()) return false;
    return keys.provisionDerived(keys.root(), KEY_LEN);
}

static void ensureKernelDirs() {
#if defined(__PSP__)
    (void)sceIoMkdir("ms0:/ghost_hive", 0777);
    (void)sceIoMkdir("ms0:/ghost_hive/k", 0777);
#else
    (void)mkdir("/tmp/ghost_hive", 0755);
    (void)mkdir("/tmp/ghost_hive_psp", 0755);
#endif
}

static bool readInternalRoot(uint8_t* out) {
    FILE* f = fopen(ROOT_KEY_PATH, "rb");
    if (f == nullptr) return false;
    uint8_t blob[ROOT_WRAP_LEN];
    size_t n = fread(blob, 1, ROOT_WRAP_LEN, f);
    int extra = fgetc(f);
    fclose(f);
    if (n != ROOT_WRAP_LEN || extra != EOF) return false;
    bool ok = ghost_wrap_reveal(blob, out);
    for (uint16_t i = 0; i < ROOT_WRAP_LEN; ++i) blob[i] = 0;
    return ok;
}

static bool writeInternalRoot(const uint8_t* root) {
    if (root == nullptr) return false;
    ensureKernelDirs();
    uint8_t blob[ROOT_WRAP_LEN];
    if (!ghost_wrap_protect(root, blob)) return false;
    FILE* f = fopen(ROOT_KEY_PATH, "wb");
    if (f == nullptr) {
        for (uint16_t i = 0; i < ROOT_WRAP_LEN; ++i) blob[i] = 0;
        return false;
    }
    bool ok = fwrite(blob, 1, ROOT_WRAP_LEN, f) == ROOT_WRAP_LEN;
    if (fclose(f) != 0) ok = false;
    for (uint16_t i = 0; i < ROOT_WRAP_LEN; ++i) blob[i] = 0;
    return ok;
}

static bool writePeerBindHost(const GhostKeys& keys) {
    ensureKernelDirs();
    uint8_t buf[GhostKeys::PEER_BIND_LEN];
    if (!keys.exportPeerBind(buf, GhostKeys::PEER_BIND_LEN)) return false;
    FILE* f = fopen(PEER_BIND_PATH, "wb");
    if (f == nullptr) {
        for (uint8_t i = 0; i < GhostKeys::PEER_BIND_LEN; ++i) buf[i] = 0;
        return false;
    }
    bool ok = fwrite(buf, 1, GhostKeys::PEER_BIND_LEN, f) == GhostKeys::PEER_BIND_LEN;
    if (fclose(f) != 0) ok = false;
    for (uint8_t i = 0; i < GhostKeys::PEER_BIND_LEN; ++i) buf[i] = 0;
    return ok;
}

static bool physicalResetMarker() {
    FILE* f = fopen(ROOT_RESET_PATH, "rb");
    if (f == nullptr) return false;
    fclose(f);
    (void)remove(ROOT_RESET_PATH);
    return true;
}

// §32 / §33: Root intern erzeugen/halten. Kein USB-Import, keine Stick-Rotation.
static bool physicalProvision(GhostKeys& keys) {
    ensureKernelDirs();
    if (physicalResetMarker()) {
        keys.physicalReset();
        (void)remove(ROOT_KEY_PATH);
    }

    if (keys.hasRoot()) {
        if (!keys.hasTotpSeed()) {
            if (!bindDerivedSlots(keys)) return false;
            (void)writePeerBindHost(keys);
        }
        return true;
    }

    uint8_t root[KEY_LEN];
    if (readInternalRoot(root)) {
        bool ok = keys.provisionRoot(root, KEY_LEN);
        for (uint8_t i = 0; i < KEY_LEN; ++i) root[i] = 0;
        if (!ok) return false;
        if (!bindDerivedSlots(keys)) return false;
        (void)writePeerBindHost(keys);
        return true;
    }
    for (uint8_t i = 0; i < KEY_LEN; ++i) root[i] = 0;

    if (!keys.generateRoot()) return false;
    if (!writeInternalRoot(keys.root())) return false;
    if (!bindDerivedSlots(keys)) return false;
    (void)writePeerBindHost(keys);
    return true;
}

static bool registerRegistryPeers(Registry& registry, MediumWlan& wlan) {
    uint8_t n = registry.getDeviceCount();
    for (uint8_t i = 0; i < n; ++i) {
        DeviceInfo info = registry.getDeviceInfo(i);
        if (info.id[0] == '\0') continue;
        wlan.registerPeer(info.id, info.role);
    }
    return true;
}

static void writeKillFlag(const GhostKeys& keys) {
    // §33 / §43: Kill nur mit Root. Signatur als Datei, Snapshot kommt vorher.
    if (!keys.hasRoot()) return;
#if defined(__PSP__)
    (void)sceIoMkdir("ms0:/ghost_hive", 0777);
    FILE* f = fopen("ms0:/ghost_hive/kill.flag", "wb");
    if (f == nullptr) return;
    const char tag[5] = {'k', 'i', 'l', 'l', '\n'};
    (void)fwrite(tag, 1, 5, f);
    fclose(f);
#else
    struct stat st;
    if (stat(HIVE_PERSIST_DIR, &st) != 0) {
        if (mkdir(HIVE_PERSIST_DIR, 0755) != 0) return;
    } else if (!S_ISDIR(st.st_mode)) {
        return;
    }
    FILE* f = fopen("/tmp/ghost_hive/kill.flag", "wb");
    if (f == nullptr) return;
    const char tag[5] = {'k', 'i', 'l', 'l', '\n'};
    (void)fwrite(tag, 1, 5, f);
    fclose(f);
#endif
}

int main() {
#if defined(__PSP__)
    hive_setup_callbacks();
    sceDisplaySetMode(0, 480, 272);
    pspDebugScreenInitEx(nullptr, PSP_DISPLAY_PIXEL_FORMAT_8888, 1);
    pspDebugScreenClear();
    pspDebugScreenSetBackColor(0x00000000);
    pspDebugScreenSetTextColor(0x0000FF00);
#endif

    // §32: Terminal-Mode, Root intern, Registry, warten.
    GhostKeys keys;
    keys.initEmpty();

    Registry registry;
    registry.init();

    EventQueue queue;
    queue.init();

    GhostVault vault;
    vault.init();
    vault.attachKeys(&keys);

    (void)physicalProvision(keys);
    if (keys.hasRoot()) {
        (void)vault.load();
    }

    DecisionPipeline pipeline;
    pipeline.attach(&registry, &vault);

    GhostTerminal terminal;
    terminal.init(&registry, &queue);
    terminal.attach(&vault, &pipeline);
    psp_input_init();

    GhostIR irHw;
    irHw.init();
    MediumWlan wlan;
    MediumIr irMedium;
    irMedium.attach(&irHw);

    GhostTransport transport;
    transport.attach(&registry, &pipeline, &vault, &terminal.down(),
                     &terminal.peek(), &irHw, &wlan, &irMedium);
    pipeline.attachTransport(&transport);
    vault.attachTransport(&transport);
    terminal.attachTransport(&transport);
    terminal.down().attachTransport(&transport);
    terminal.peek().attachTransport(&transport);

    Sensor sensor;
    sensor.init(HIVE_ID_SENSOR);
    SensorTransport sensorLink;
    sensorLink.init(HIVE_ID_SENSOR);
#if defined(__PSP__)
    sensorLink.attach(&wlan, nullptr);
#else
    sensorLink.attach(&wlan, &irMedium);
#endif

    Safe safe;
    safe.init(HIVE_ID_SAFE);
    SafeTransport safeLink;
    safeLink.init(HIVE_ID_SAFE);
#if defined(__PSP__)
    safeLink.attach(&wlan, nullptr);
#else
    safeLink.attach(&wlan, &irMedium);
#endif

    Mine mine;
    mine.init(HIVE_ID_MINE_KERNEL);
    MineTransport mineLink;
    mineLink.init(HIVE_ID_MINE_KERNEL);
#if defined(__PSP__)
    mineLink.attach(&wlan, nullptr);
#else
    mineLink.attach(&wlan, &irMedium);
#endif

    uint32_t ramUsed =
        static_cast<uint32_t>(sizeof(keys) + sizeof(registry) + sizeof(queue) +
                              sizeof(vault) + sizeof(pipeline) + sizeof(terminal) +
                              sizeof(irHw) + sizeof(wlan) + sizeof(irMedium) +
                              sizeof(transport) + sizeof(sensor) + sizeof(sensorLink) +
                              sizeof(safe) + sizeof(safeLink) + sizeof(mine) +
                              sizeof(mineLink));
    logRamBudget(ramUsed);

    bool peersUp = false;
    bool wlanUp = false;
    bool wlanTried = false;
    uint32_t wlanRetrySec = 0;
    bool lowApplied = false;
    uint32_t lastResetPoll = 0;
    uint32_t lastBeat = 0;
#if !defined(__PSP__)
    uint32_t lastSensor = 0;
    uint32_t lastMine = 0;
#endif
    uint32_t lastWifiScan = 0;
    uint32_t bootSec = 0;
#if defined(GHOST_MINI_WATCH) && !defined(__PSP__)
    Worker miniWorker;
    miniWorker.init(HIVE_ID_WORKER);
    uint32_t lastMiniTelem = 0;
#endif

    while (terminal.isRunning()) {
        uint32_t nowMs = hive_now_ms();
        uint32_t nowSec = hive_now_sec();
        if (bootSec == 0) bootSec = nowSec;

        terminal.tick(nowMs);
        terminal.draw();
#if defined(GHOST_MINI_WATCH) && !defined(__PSP__)
        {
            static uint32_t mini_last = 0;
            if (mini_last == 0 || (nowMs - mini_last) >= 500u) {
                mini_last = nowMs;
                terminal.dumpWatchStacked(stdout);
                fflush(stdout);
            }
        }
#endif
        /* §32/§33: Root einmal, Reset-Marker höchstens 1 Hz. Nicht jedes Tick FAT. */
        if (!keys.hasRoot()) {
            (void)physicalProvision(keys);
        } else if (lastResetPoll != nowSec) {
            lastResetPoll = nowSec;
            if (physicalResetMarker()) {
                keys.physicalReset();
                (void)remove(ROOT_KEY_PATH);
                (void)physicalProvision(keys);
                peersUp = false;
            }
        }

        // §40: Funk tot sobald Game-Mode/Down aktiv (self-start, nicht erst nach Kill).
        bool freeze = transport.hiveFrozen() || terminal.down().isActive();
        bool lowPower = transport.kernelDown();
        bool radioQuiet = terminal.stealth().isGameMode() ||
                          terminal.stealth().isInvisible() ||
                          lowPower;
        bool terminalOn = terminal.scanner().terminalMode() && !radioQuiet;

        if (freeze) {
            mine.freezeEvents();
            safe.setWriteLock(true);
        }

        if (terminal.down().isActive() &&
            terminal.down().peekAllowed() &&
            !transport.kernelDown()) {
            if (terminal.down().killSent()) {
                writeKillFlag(keys);
            }
            transport.setKernelDown(true);
            lowPower = true;
            radioQuiet = true;
        }

        if (lowPower != lowApplied) {
            hive_apply_low_power(lowPower);
            lowApplied = lowPower;
        }

        if (keys.hasRoot() && !peersUp) {
            (void)sensor.provisionDeviceKey(keys.device(), KEY_LEN);
            (void)safe.provisionDeviceKey(keys.device(), KEY_LEN);
            if (!root_config_ingest(ROOT_CONFIG_PATH, registry)) {
                (void)enrollPending(registry, HIVE_ID_WORKER, ROLE_WORKER, 2);
                (void)enrollPending(registry, HIVE_ID_PHONE, ROLE_PHONE, 1);
                (void)enrollPending(registry, HIVE_ID_ROUTER, ROLE_ROUTER, 1);
                (void)enrollPending(registry, HIVE_ID_SENSOR, ROLE_SENSOR, 1);
                (void)enrollPending(registry, HIVE_ID_FAMILY, ROLE_SENSOR, 1);
                (void)enrollPending(registry, HIVE_ID_SAFE, ROLE_SAFE, 1);
            }
            (void)registerRegistryPeers(registry, wlan);
            if (mine.setTotpSeed(keys.totpSeed(), TOTP_SEED_LEN)) {
                (void)pipeline.replay().setTotpSeed(HIVE_ID_MINE_KERNEL, keys.totpSeed(),
                                                    TOTP_SEED_LEN);
            }
            if (registry.getDevice(HIVE_ID_MINE_KERNEL) == nullptr) {
                (void)enrollSilentMine(registry, HIVE_ID_MINE_KERNEL);
            }
            pipeline.heartbeat().send(HIVE_ID_MINE_KERNEL, nowSec);
            peersUp = true;
        }

        if (!keys.hasRoot()) {
            psp_sleep_ms(10);
            continue;
        }

        if (radioQuiet) {
            if (wlanUp) {
                hive_wlan_off();
                wlanUp = false;
            }
        } else if (terminalOn && !wlanUp && (nowSec - bootSec) >= 3u) {
            if (!wlanTried || (wlanRetrySec != 0 && nowSec >= wlanRetrySec)) {
                wlanTried = true;
                wlanRetrySec = 0;
                if (hive_wlan_on()) {
                    wlanUp = wlan.listenKernel(GHOST_UDP_PORT);
                    if (!wlanUp) wlanRetrySec = nowSec + 30u;
                } else {
                    wlanRetrySec = nowSec + 30u;
                }
            }
        }

        uint32_t beatIv = lowPower
            ? static_cast<uint32_t>(HEARTBEAT_INTERVAL_SEC) * 3u
            : HEARTBEAT_INTERVAL_SEC;
        if (lastBeat == 0 || (nowSec - lastBeat) >= beatIv) {
            enqueueKernelHeartbeat(queue, nowSec);
            lastBeat = nowSec;
        }

        transport.setTerminalMode(terminalOn);

        // §23 WLAN/BT-Scan 30s. Nur Terminal-Mode, nie Game-Mode (§2.2). Buffer sofort frei (§12.2).
        if (terminalOn && !radioQuiet && !freeze && (nowSec - bootSec) >= 8u) {
            if (lastWifiScan == 0 ||
                (nowSec - lastWifiScan) >= HEARTBEAT_INTERVAL_SEC) {
                GhostScanner& sc = terminal.scanner();
                (void)sc.scanWifi();
                (void)sc.scanBluetooth();
                sc.releaseBuffer();
                lastWifiScan = nowSec;
            }
        }

        const Device* worker = registry.findByRole(ROLE_WORKER);
        bool workerOnline = (worker != nullptr && worker->status == DeviceState::Online);
        sensor.setLowPower(lowPower || !workerOnline);
        mine.setLowPower(lowPower || !workerOnline);
        mine.setPeekAllowed(terminalOn && transport.peekWindowOpen(nowSec));

        if (terminalOn && !radioQuiet) {
            if (!freeze) {
#if !defined(__PSP__)
                /* Host-Sim: Sensor/Mine Loopback. PSP-Kernel sendet das nicht —
                   IR/WLAN toKernel landet im eigenen rx → sofort enterHiveDown. */
                if (lastSensor == 0 || (nowSec - lastSensor) >= sensor.intervalSec()) {
                    Event sf{};
                    if (sensor.fillHeartbeat(&sf, nowSec)) {
                        (void)vault.signEvent(sf);
                        (void)sensorLink.send(sf, nowSec);
                    }
                    Event scan{};
                    if (sensor.fillScan(&scan, nowSec)) {
                        (void)vault.signEvent(scan);
                        (void)sensorLink.send(scan, nowSec);
                    }
                    lastSensor = nowSec;
                }
                sensorLink.tick(nowSec);
                (void)sensor.recv(nullptr);

                if (lastMine == 0 || (nowSec - lastMine) >= mine.intervalSec()) {
                    MinePayload mp{};
                    if (mine.send(&mp, nowSec)) {
                        (void)vault.signMine(mp);
                        (void)mineLink.send(mp, nowSec);
                    }
                    lastMine = nowSec;
                }
                MinePayload ignored{};
                (void)mineLink.recv(ignored);
#endif
                ingestIrAsSensor(irHw, sensor, queue, vault, nowSec);
            }
            transport.rx(nowSec);
        }

#if defined(GHOST_MINI_WATCH) && !defined(__PSP__)
        if (peersUp &&
            (lastMiniTelem == 0 || (nowSec - lastMiniTelem) >= TELEM_INTERVAL_SEC)) {
            uint16_t ram = 0, traf = 0, wifi = 0;
            uint8_t cpu = 0, gpu = 0, bat = 0;
            if (host_telem_sample(nowSec, &ram, &cpu, &gpu, &traf, &bat, &wifi)) {
                host_telem_apply_role(ROLE_WORKER, &ram, &cpu, &gpu, &traf, &bat,
                                      &wifi);
                Event te{};
                if (miniWorker.fillTelemetry(&te, nowSec, ram, cpu, gpu, traf, bat,
                                             wifi)) {
                    (void)vault.signEvent(te);
                    (void)queue.push(te);
                    lastMiniTelem = nowSec;
                }
            }
        }
#endif

        pipeline.drain(queue, nowSec);
        pipeline.heartbeat().tick(nowSec, &registry);

        freeze = transport.hiveFrozen() || terminal.down().isActive();
        radioQuiet = terminal.stealth().isGameMode() ||
                     terminal.stealth().isInvisible() ||
                     transport.kernelDown();
        if (freeze) {
            mine.freezeEvents();
            safe.setWriteLock(true);
        }
        if (radioQuiet && wlanUp) {
            hive_wlan_off();
            wlanUp = false;
        }

        if (!radioQuiet && !safe.writeLocked()) {
            Event backup{};
            while (safeLink.poll(backup)) {
                (void)safe.ingestBackup(backup);
            }
            safeLink.tick(nowSec);
        }

        vault.tick(nowSec);
        if (!transport.kernelDown()) {
            transport.tick(nowSec);
        }

        psp_sleep_ms(lowPower ? 50u : 33u);
    }

#if defined(__PSP__)
    sceKernelExitGame();
#endif
    return 0;
}

