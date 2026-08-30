#include "watch_hud.h"
#include "ghost_terminal.h"
#include "psp_input.h"
#include "registry.h"
#include "event_queue.h"
#include "ghost_vault.h"
#include "decision_pipeline.h"
#include "ghost_telemetry.h"
#include "replay_guard.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int fail = 0;

static void chk(bool ok, const char* name) {
    if (ok) printf("PASS %s\n", name);
    else {
        printf("FAIL %s\n", name);
        fail = 1;
    }
}

static uint64_t ns_now() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull +
           static_cast<uint64_t>(ts.tv_nsec);
}

static void src_clear(GhostOutput::WatchSrc* s) {
    memset(s, 0, sizeof(*s));
}

static void tap(GhostTerminal& t, Key k, uint32_t* now) {
    psp_push_key(k);
    *now += 10;
    t.tick(*now);
}

int main() {
    Registry reg;
    EventQueue eq;
    GhostVault vault;
    vault.init();
    DecisionPipeline pipe;
    pipe.attach(&reg, &vault);
    GhostOutput::WatchSrc src;
    src_clear(&src);
    src.registry = &reg;
    src.replay = &pipe.replay();
    src.heartbeat = &pipe.heartbeat();
    src.pipeline = &pipe;

    chk(!watch_danger_headline(src), "empty registry quiet");

    Device pend{};
    pend.id[0] = 'W';
    pend.id[1] = '\0';
    pend.role = ROLE_WORKER;
    pend.status = DeviceState::Pending;
    device_telem_clear(&pend);
    chk(reg.addDevice(pend), "enroll pending");
    chk(!watch_danger_headline(src), "pending no telemAbsent");

    Device w{};
    w.id[0] = 'P';
    w.id[1] = '\0';
    w.role = ROLE_PHONE;
    w.status = DeviceState::Online;
    w.last_seen = 50;
    w.trust_level = 2;
    device_telem_clear(&w);
    chk(reg.addDevice(w), "enroll phone");
    chk(reg.pairDevice("P"), "pair phone online");
    Device* pd = const_cast<Device*>(reg.getDevice("P"));
    chk(pd != nullptr, "phone ptr");
    if (pd != nullptr) pd->last_seen = 50;
    device_telem_clear(pd);
    uint64_t t0 = ns_now();
    bool ta = watch_danger_headline(src);
    uint64_t t1 = ns_now();
    chk(ta, "telemAbsent headline");
    printf("TIME telemAbsent_to_headline_ns=%llu\n",
           static_cast<unsigned long long>(t1 - t0));

    ReplayGuard* g = &pipe.replay();
    MinePayload p1{};
    p1.mine_id[0] = 'M';
    p1.mine_id[1] = '\0';
    p1.counter = 1;
    p1.totp = 1;
    t0 = ns_now();
    chk(g->check(p1, 1000), "mine first");
    chk(!g->check(p1, 1001), "mine replay reject");
    g->blockMine("M", reg);
    t1 = ns_now();
    chk(watch_danger_headline(src), "replay blk headline");
    printf("TIME replay_to_headline_ns=%llu\n",
           static_cast<unsigned long long>(t1 - t0));

    GhostScanner scan;
    src.scanner = &scan;
    WifiNetwork nets[2];
    memset(nets, 0, sizeof(nets));
    const char* ss = "GHSTHIVE";
    for (uint8_t i = 0; i < 8; ++i) {
        nets[0].ssid[i] = ss[i];
        nets[1].ssid[i] = ss[i];
    }
    nets[0].rssi = -40;
    nets[0].channel = 6;
    nets[1].rssi = -70;
    nets[1].channel = 11;
    nets[1].encryption = 3;
    scan.setTerminalMode(true);
    t0 = ns_now();
    chk(scan.loadWifiSnapshot(nets, 2), "twin inject");
    t1 = ns_now();
    chk(watch_danger_headline(src), "twin headline");
    printf("TIME twin_snapshot_to_headline_ns=%llu\n",
           static_cast<unsigned long long>(t1 - t0));

    pipe.heartbeat().send("P", 100);
    t0 = ns_now();
    pipe.heartbeat().tick(129, &reg);
    chk(pipe.heartbeat().getMissCount("P") == 0, "hb 29s still 0");
    pipe.heartbeat().tick(130, &reg);
    t1 = ns_now();
    chk(pipe.heartbeat().getMissCount("P") >= 1, "hb 30s miss>=1");
    chk(watch_danger_headline(src), "hbmiss headline");
    printf("TIME hb_tick_ns=%llu hb_alarm_after_sec=30\n",
           static_cast<unsigned long long>(t1 - t0));

    Event ev{};
    ev.type = EventType::TelemetryUpdate;
    ev.source_device_id[0] = 'P';
    ev.source_device_id[1] = '\0';
    ev.timestamp = 200;
    fillTelemetryPayload(&ev, 512, 12, TELEM_ABSENT8, TELEM_ABSENT16, 87, 54);
    chk(pipe.process(ev, 200) == PipelineResult::Accepted, "telem first");
    ev.timestamp = 200;
    t0 = ns_now();
    PipelineResult r2 = pipe.process(ev, 200);
    t1 = ns_now();
    chk(r2 != PipelineResult::Accepted, "telem dense drop");
    chk(pipe.telemDenseDrops("P") >= 1, "drop count");
    chk(watch_danger_headline(src), "drop headline");
    printf("TIME dense_telem_drop_ns=%llu\n",
           static_cast<unsigned long long>(t1 - t0));

    GhostTerminal term;
    term.init(&reg, &eq);
    term.attach(&vault, &pipe);
    psp_input_init();
    uint32_t now = 1000;
    term.tick(now);
    tap(term, Key::R, &now);
    chk(term.watchPage() == 1, "kernel page");
    psp_set_combo_held(true);
    term.tick(now);
    now += 2999;
    term.tick(now);
    chk(!term.down().isActive(), "hold 2999ms no down");
    now += 1;
    term.tick(now);
    chk(term.down().isActive(), "hold 3000ms down");
    printf("TIME hold_down_ms=3000\n");
    psp_set_combo_held(false);
    chk(!term.down().isActive() || term.mode() == TermMode::Watch, "down stays watch");

    printf("LOOP_MS=10 SCAN_SEC=30 HB_INTERVAL_SEC=30 TELEM_MIN_GAP_SEC=1 HOLD_MS=3000\n");
    printf("DETECTS=replay_blk,twin_ssid,hbmiss,telem_drop,telemAbsent_online,blocked,danger\n");
    printf("NOT_IN_HEADLINE=drift,ibss_flag,peer_id_dup,mac_spoof,xap_alone,funk_channel\n");

    if (fail) {
        printf("FAIL test_alarm_latency\n");
        return 1;
    }
    printf("PASS test_alarm_latency\n");
    return 0;
}
