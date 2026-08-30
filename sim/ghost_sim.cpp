#include "registry.h"
#include "ghost_vault.h"
#include "ghost_keys.h"
#include "decision_pipeline.h"
#include "ghost_down.h"
#include "ghost_stealth.h"
#include "ghost_peek.h"
#include "ghost_ir.h"
#include "transport/ghost_transport.h"
#include "transport/medium_wlan.h"
#include "transport/medium_ir.h"
#include "ghost_crypto.h"
#include "lab_common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

struct SimHive {
    Registry reg;
    GhostVault vault;
    GhostKeys keys;
    DecisionPipeline pipe;
    GhostTransport xport;
    GhostDown down;
    GhostStealth stealth;
    GhostPeek peek;
    GhostIR ir;
    MediumWlan wlan;
    MediumIr mediumIr;
    uint8_t root_boot[KEY_LEN];
    uint32_t now;
    uint32_t now0;
    uint32_t time_factor;
    uint8_t ghost_mode;
    uint32_t pkt_n;
    struct timespec wall0;
    uint32_t game_n;
    uint8_t saw_game;
    LabSnap last;
    uint8_t uk_froze_latched;
};

static void sim_enroll(SimHive& h, const char* id, uint8_t role, uint8_t trust) {
    Device d{};
    lab_copy_id(d.id, id);
    d.role = role;
    d.trust_level = trust;
    d.status = DeviceState::Pending;
    d.last_seen = 0;
    (void)h.reg.addDevice(d);
    (void)h.reg.pairDevice(id);
    h.wlan.registerPeer(id, role);
}

static void sim_boot(SimHive& h) {
    h.pipe.attach(&h.reg, &h.vault);
    h.pipe.attachTransport(&h.xport);
    h.down.attach(&h.vault, &h.stealth);
    h.down.attachTransport(&h.xport);
    h.vault.attachTransport(&h.xport);
    h.xport.attach(&h.reg, &h.pipe, &h.vault, &h.down,
                   &h.peek, &h.ir, &h.wlan, &h.mediumIr);
    h.xport.setTerminalMode(true);
    h.stealth.enterTerminalMode();
    h.keys.initEmpty();
    uint8_t root[KEY_LEN];
    for (uint8_t i = 0; i < KEY_LEN; ++i) {
        root[i] = static_cast<uint8_t>(i + 1);
        h.root_boot[i] = root[i];
    }
    (void)h.keys.provisionRoot(root, KEY_LEN);
    (void)h.keys.provisionDerived(root, KEY_LEN);
    h.vault.attachKeys(&h.keys);
    sim_enroll(h, "W", ROLE_WORKER, 2);
    sim_enroll(h, "P", ROLE_PHONE, 1);
    sim_enroll(h, "R", ROLE_ROUTER, 1);
    sim_enroll(h, "N", ROLE_SAFE, 1);
    Device mine{};
    lab_copy_id(mine.id, "X");
    mine.role = ROLE_MINE;
    mine.status = DeviceState::Silent;
    (void)h.reg.addDevice(mine);
    if (h.keys.totpSeed() != nullptr) {
        (void)h.pipe.replay().setTotpSeed("X", h.keys.totpSeed(), TOTP_SEED_LEN);
    }
    h.now = 10000;
    Event hb{};
    hb.type = EventType::Heartbeat;
    lab_copy_id(hb.source_device_id, "W");
    hb.timestamp = h.now;
    hb.severity = Severity::Info;
    hb.payload[0] = 'b';
    hb.payload[1] = 'o';
    hb.payload[2] = 'o';
    hb.payload[3] = 't';
    hb.payload[4] = '\0';
    (void)h.vault.signEvent(hb);
    (void)h.vault.store(hb, h.now);
    uint8_t bind[GhostKeys::PEER_BIND_LEN];
    if (h.keys.exportPeerBind(bind, GhostKeys::PEER_BIND_LEN)) {
        FILE* f = fopen(lab_bind_path(), "wb");
        if (f != nullptr) {
            (void)fwrite(bind, 1, GhostKeys::PEER_BIND_LEN, f);
            fclose(f);
        }
    }
    for (uint8_t i = 0; i < GhostKeys::PEER_BIND_LEN; ++i) bind[i] = 0;
    for (uint8_t i = 0; i < KEY_LEN; ++i) root[i] = 0;
    h.game_n = 0;
    h.saw_game = 0;
    h.pkt_n = 0;
    lab_snap_clear(&h.last);
    h.uk_froze_latched = 0;
}

static uint32_t env_u32(const char* key, uint32_t def) {
    const char* v = getenv(key);
    if (v == nullptr || v[0] == '\0') return def;
    uint32_t n = 0;
    while (*v >= '0' && *v <= '9') {
        n = n * 10u + static_cast<uint32_t>(*v - '0');
        ++v;
    }
    return n != 0 ? n : def;
}

static void sim_clock_reset(SimHive& h) {
    clock_gettime(CLOCK_MONOTONIC, &h.wall0);
    h.now0 = h.now;
}

static void sim_advance(SimHive& h) {
    if (h.time_factor <= 1) return;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t ds = static_cast<int64_t>(ts.tv_sec) - static_cast<int64_t>(h.wall0.tv_sec);
    if (ds < 0) ds = 0;
    uint64_t extra = static_cast<uint64_t>(ds) * static_cast<uint64_t>(h.time_factor);
    uint64_t acc = static_cast<uint64_t>(h.now0) + extra;
    if (acc > 0xffffffffULL) acc = 0xffffffffULL;
    uint32_t nxt = static_cast<uint32_t>(acc);
    if (nxt > h.now) h.now = nxt;
}

static void sim_write_cfg(const SimHive& h) {
    char path[180];
    snprintf(path, sizeof(path), "%s/sim_cfg", lab_dir());
    FILE* f = fopen(path, "w");
    if (f == nullptr) return;
    fprintf(f, "ghost_mode=%u\nfactor=%u\nnow=%u\npkts=%u\n"
            "policies=GhostDownStart,PeerKill,MineEvent,terminal_mode\n"
            "bind=127.0.0.1:%u\n",
            static_cast<unsigned>(h.ghost_mode),
            h.time_factor, h.now, h.pkt_n,
            static_cast<unsigned>(lab_udp_port()));
    fclose(f);
}

static uint8_t sim_root_ok(const SimHive& h) {
    if (!h.keys.hasRoot()) return 0;
    for (uint8_t i = 0; i < KEY_LEN; ++i) {
        if (h.keys.root()[i] != h.root_boot[i]) return 0;
    }
    return 1;
}

static void sim_fill_snap(SimHive& h, LabSnap* s) {
    lab_snap_clear(s);
    s->root_ok = sim_root_ok(h);
    s->frozen = h.xport.hiveFrozen() ? 1 : 0;
    if (s->frozen) {
        snprintf(s->state, 24, "ghost_down");
    } else if (h.xport.dangerMode()) {
        snprintf(s->state, 24, "danger_mode");
    } else if (h.stealth.isGameMode()) {
        snprintf(s->state, 24, "game_mode");
    } else {
        snprintf(s->state, 24, "terminal_mode");
    }
    if (h.stealth.isGameMode()) {
        if (h.saw_game == 0) ++h.game_n;
        h.saw_game = 1;
    }
    s->game_n = h.game_n;
    s->snap_n = h.down.snapshotCount();
    s->snap_ran = h.down.isActive() ? 1 : 0;
    s->kill_sent = h.down.killSent() ? 1 : 0;
    s->now = h.now;
    ReplayGuard& rg = h.pipe.replay();
    if (rg.trackedCount() > 0) {
        s->last_counter = rg.lastCounterAt(0);
        uint32_t ts = rg.lastTimestampAt(0);
        s->last_totp_win = (h.now > ts) ? (h.now - ts) : 0;
    }
    s->hmac_ok = h.last.hmac_ok;
    s->accepted = h.last.accepted;
    s->unsigned_kill = h.last.unsigned_kill;
    s->uk_froze = h.uk_froze_latched;
    s->replay_attempt = h.last.replay_attempt;
    s->totp_out = h.last.totp_out;
    snprintf(s->last_policy, 24, "%s",
             h.last.last_policy[0] != '\0' ? h.last.last_policy : "drop");
    snprintf(s->kind, 16, "%s", h.last.kind[0] != '\0' ? h.last.kind : "-");
    snprintf(s->etype, 24, "%s", h.last.etype[0] != '\0' ? h.last.etype : "-");
    s->asan = 0;
    s->target_down = 0;
    s->recovered = 1;
    s->pid = static_cast<uint32_t>(getpid());
}

static uint32_t sim_last_seen(SimHive& h, const char* id) {
    const Device* d = h.reg.getDevice(id);
    if (d == nullptr) return 0;
    return d->last_seen;
}

static uint32_t sim_mine_counter(SimHive& h, const char* id) {
    ReplayGuard& rg = h.pipe.replay();
    for (uint8_t i = 0; i < rg.trackedCount(); ++i) {
        const char* mid = rg.mineIdAt(i);
        if (mid == nullptr) continue;
        uint8_t j = 0;
        bool same = true;
        while (j < 32) {
            if (mid[j] != id[j]) {
                same = false;
                break;
            }
            if (mid[j] == '\0') break;
            ++j;
        }
        if (same) return rg.lastCounterAt(i);
    }
    return 0;
}

static void sim_handle_frame(SimHive& h, const TransportFrame& frame) {
    uint8_t froze0 = h.xport.hiveFrozen() ? 1 : 0;
    LabSnap mark;
    lab_snap_clear(&mark);
    lab_kind_name(frame.kind, mark.kind, 16);
    if (frame.kind == TransportKind::MineFrame) {
        lab_etype_name(frame.mine.event, mark.etype, 24);
        bool mac = h.vault.verifyMine(frame.mine);
        mark.hmac_ok = mac ? 1 : 0;
        uint32_t prev = sim_mine_counter(h, frame.mine.mine_id);
        if (prev != 0 && frame.mine.counter <= prev) mark.replay_attempt = 1;
        ReplayGuard& rg = h.pipe.replay();
        uint32_t last_ts = 0;
        for (uint8_t i = 0; i < rg.trackedCount(); ++i) {
            const char* mid = rg.mineIdAt(i);
            if (mid != nullptr && mid[0] == frame.mine.mine_id[0]) {
                last_ts = rg.lastTimestampAt(i);
            }
        }
        if (last_ts != 0) {
            uint32_t diff = (h.now > last_ts) ? (h.now - last_ts) : 0;
            if (diff < TOTP_WINDOW_MIN_SEC || diff > TOTP_WINDOW_MAX_SEC) {
                mark.totp_out = 1;
            }
        }
        if (!mac) snprintf(mark.last_policy, 24, "hmac_i");
        uint32_t c0 = prev;
        h.xport.ingest(frame, h.now);
        uint32_t c1 = sim_mine_counter(h, frame.mine.mine_id);
        mark.accepted = (c1 > c0) ? 1 : 0;
        if (mac && mark.accepted) {
            lab_policy_name(static_cast<uint8_t>(
                h.pipe.policy().evaluate(Event{})), mark.last_policy, 24);
            snprintf(mark.last_policy, 24, "drop");
            if (frame.mine.event == EventType::MineEvent) {
                Event ev{};
                ev.type = EventType::MineEvent;
                lab_policy_name(static_cast<uint8_t>(h.pipe.policy().evaluate(ev)),
                                mark.last_policy, 24);
            }
        }
    } else {
        lab_etype_name(frame.event.type, mark.etype, 24);
        bool mac = h.vault.verifyEvent(frame.event);
        mark.hmac_ok = mac ? 1 : 0;
        if (frame.event.type == EventType::GhostDownStart ||
            frame.kind == TransportKind::KillFrame) {
            if (!mac) mark.unsigned_kill = 1;
        }
        const char* sid = frame.event.source_device_id;
        if (sid[0] == '\0') sid = frame.src_id;
        uint32_t seen0 = sim_last_seen(h, sid);
        if (seen0 != 0 && frame.event.timestamp <= seen0) mark.replay_attempt = 1;
        if (!mac) snprintf(mark.last_policy, 24, "hmac_i");
        else {
            lab_policy_name(static_cast<uint8_t>(h.pipe.policy().evaluate(frame.event)),
                            mark.last_policy, 24);
        }
        h.xport.ingest(frame, h.now);
        uint32_t seen1 = sim_last_seen(h, sid);
        mark.accepted = (seen1 > seen0) ? 1 : 0;
        if (!mac) mark.accepted = 0;
    }
    h.last = mark;
    /* SPEC-v1 Lab INV-07: latch only this ingest frame; sticky until sim_boot. */
    if (mark.unsigned_kill != 0 && froze0 == 0 && h.xport.hiveFrozen()) {
        h.uk_froze_latched = 1;
    }
    if (h.xport.hiveFrozen()) {
        h.down.tick(h.now + NAS_FLUSH_TIMEOUT_SEC);
    }
}

static void sim_on_udp(SimHive& h, const uint8_t* wire, uint16_t len,
                       const sockaddr_in& from) {
    uint32_t ip = ntohl(from.sin_addr.s_addr);
    if (ip != INADDR_LOOPBACK) return;
    ++h.pkt_n;
    sim_advance(h);
    TransportFrame frame;
    if (!transport_decode(wire, len, frame)) {
        lab_snap_clear(&h.last);
        h.last.hmac_ok = 0;
        h.last.accepted = 0;
        snprintf(h.last.last_policy, 24, "drop");
        snprintf(h.last.kind, 16, "bad_wire");
        return;
    }
    if (frame.stamp > h.now) h.now = frame.stamp;
    if (frame.event.timestamp > h.now) h.now = frame.event.timestamp;
    if (frame.mine.timestamp > h.now) h.now = frame.mine.timestamp;
    sim_handle_frame(h, frame);
}

static void sim_on_unix(SimHive& h, int cfd) {
    char buf[80];
    ssize_t n = recv(cfd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return;
    buf[n] = '\0';
    if (buf[0] == 'N' && buf[1] == 'O' && buf[2] == 'W') {
        uint32_t v = 0;
        const char* p = buf + 3;
        while (*p == ' ') ++p;
        while (*p >= '0' && *p <= '9') {
            v = v * 10u + static_cast<uint32_t>(*p - '0');
            ++p;
        }
        if (v > 0) {
            h.now = v;
            sim_clock_reset(h);
        }
    }
    sim_advance(h);
    if (buf[0] == 'C' && buf[1] == 'F' && buf[2] == 'G') {
        char cfg[160];
        snprintf(cfg, sizeof(cfg),
                 "CFG ghost_mode=%u factor=%u now=%u pkts=%u\n",
                 static_cast<unsigned>(h.ghost_mode), h.time_factor, h.now,
                 h.pkt_n);
        (void)send(cfd, cfg, strlen(cfg), 0);
        return;
    }
    LabSnap s;
    sim_fill_snap(h, &s);
    char line[LAB_SNAP_LINE];
    if (lab_snap_encode(s, line, LAB_SNAP_LINE)) {
        (void)send(cfd, line, strlen(line), 0);
    }
}

int main() {
    if (!lab_ensure_dir()) {
        printf("FAIL lab dir\n");
        return 1;
    }
    (void)unlink(lab_sock_path());
    (void)unlink(lab_ready_path());

    SimHive h;
    h.time_factor = env_u32("GHOST_LAB_TIME_FACTOR", 1);
    // ghost_mode is a lab-only flag (not a SPEC §9 standing state).
    // It keeps terminal_mode + full inbound policy surface on the sim UDP path.
    h.ghost_mode = static_cast<uint8_t>(env_u32("GHOST_LAB_GHOST_MODE", 0) != 0);
    sim_boot(h);
    sim_clock_reset(h);
    sim_write_cfg(h);
    if (h.ghost_mode) {
        h.xport.setTerminalMode(true);
        h.stealth.enterTerminalMode();
        printf("ghost-sim ghost_mode=1 factor=%u (sim-only, 127.0.0.1)\n",
               h.time_factor);
    }

    int udp = -1;
    if (!lab_udp_open(&udp, true)) {
        printf("FAIL udp 127.0.0.1:%u\n", static_cast<unsigned>(lab_udp_port()));
        return 1;
    }
    int flags = fcntl(udp, F_GETFL, 0);
    if (flags >= 0) (void)fcntl(udp, F_SETFL, flags | O_NONBLOCK);

    int us = socket(AF_UNIX, SOCK_STREAM, 0);
    if (us < 0) return 1;
    sockaddr_un un = {};
    un.sun_family = AF_UNIX;
    snprintf(un.sun_path, sizeof(un.sun_path), "%s", lab_sock_path());
    if (bind(us, reinterpret_cast<sockaddr*>(&un), sizeof(un)) != 0) {
        close(udp);
        close(us);
        return 1;
    }
    if (listen(us, 8) != 0) {
        close(udp);
        close(us);
        return 1;
    }
    int uf = fcntl(us, F_GETFL, 0);
    if (uf >= 0) (void)fcntl(us, F_SETFL, uf | O_NONBLOCK);

    FILE* ready = fopen(lab_ready_path(), "w");
    if (ready != nullptr) {
        fputs("ok\n", ready);
        fclose(ready);
    }
    printf("ghost-sim 127.0.0.1:%u sock=%s\n",
           static_cast<unsigned>(lab_udp_port()), lab_sock_path());
    fflush(stdout);

    uint32_t ticks = 0;
    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(udp, &rfds);
        FD_SET(us, &rfds);
        int maxfd = (udp > us) ? udp : us;
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        int r = select(maxfd + 1, &rfds, nullptr, nullptr, &tv);
        if (r < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (FD_ISSET(udp, &rfds)) {
            uint8_t wire[TRANSPORT_WIRE_LEN];
            sockaddr_in from = {};
            socklen_t flen = sizeof(from);
            ssize_t n = recvfrom(udp, wire, TRANSPORT_WIRE_LEN, 0,
                                 reinterpret_cast<sockaddr*>(&from), &flen);
            if (n > 0) {
                sim_on_udp(h, wire, static_cast<uint16_t>(n), from);
            }
        }
        if (FD_ISSET(us, &rfds)) {
            int cfd = accept(us, nullptr, nullptr);
            if (cfd >= 0) {
                sim_on_unix(h, cfd);
                close(cfd);
            }
        }
        sim_advance(h);
        h.xport.tick(h.now);
        h.vault.tick(h.now);
        h.down.tick(h.now);
        ++ticks;
        if ((ticks & 0x1f) == 0) sim_write_cfg(h);
    }
    close(udp);
    close(us);
    (void)unlink(lab_sock_path());
    return 0;
}
