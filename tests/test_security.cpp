#include "p30_harness.h"
#include "../src/psp/ghost_telemetry.h"
#include "../src/psp/transport/transport_frame.h"
#include "../src/laptop/kill.h"

static bool ok = true;

static void chk(bool v, const char* name) {
    if (v) printf("PASS %s\n", name);
    else {
        printf("FAIL %s\n", name);
        ok = false;
    }
}

static Event telem_ev(const char* id, uint32_t ts,
                      uint16_t ram, uint8_t cpu, uint8_t gpu,
                      uint16_t traf, uint8_t bat, uint16_t wifi) {
    Event ev{};
    ev.type = EventType::TelemetryUpdate;
    p30_copy_id(ev.source_device_id, id);
    ev.timestamp = ts;
    ev.severity = Severity::Critical;
    fillTelemetryPayload(&ev, ram, cpu, gpu, traf, bat, wifi);
    return ev;
}

static Event phone_telem(const char* id, uint32_t ts) {
    return telem_ev(id, ts, 128, 10, TELEM_ABSENT8, TELEM_ABSENT16, 80, 54);
}

int main() {
    {
        Registry reg;
        GhostVault vault;
        vault.init();
        DecisionPipeline pipe;
        pipe.attach(&reg, &vault);
        Device w{};
        p30_copy_id(w.id, "W");
        w.role = ROLE_WORKER;
        w.trust_level = 2;
        w.status = DeviceState::Pending;
        w.capability_mask = 0;
        chk(reg.addDevice(w), "pipe enroll");

        Event a = telem_ev("W", 100, 100, 10, 4, 8, 50, 12);
        chk(pipe.process(a, 100) == PipelineResult::Accepted, "replay first");
        chk(pipe.process(a, 200) == PipelineResult::Blocked, "replay same blob");
        const Device* dw = reg.getDevice("W");
        chk(dw != nullptr && dw->ram_mb == 100, "replay no rewrite");

        Event rewind = telem_ev("W", 50, 999, 99, 9, 9, 9, 9);
        chk(pipe.process(rewind, 250) == PipelineResult::Blocked, "ts rewind dense");
        dw = reg.getDevice("W");
        chk(dw != nullptr && dw->ram_mb == 100, "rewind no write");

        Event mag = telem_ev("W", 400, 200, 11, 5, 9, 51, 13);
        mag.payload[0] = static_cast<char>(0x41);
        uint32_t seen = dw->last_seen;
        chk(pipe.process(mag, 400) == PipelineResult::Rejected, "magic bypass");
        dw = reg.getDevice("W");
        chk(dw != nullptr && dw->last_seen == seen && dw->ram_mb == 100, "magic no last_seen");

        Event as_type{};
        p30_copy_id(as_type.source_device_id, "W");
        as_type.timestamp = 500;
        fillTelemetryPayload(&as_type, 777, 1, 1, 1, 1, 1);
        as_type.type = static_cast<EventType>(TELEM_MAGIC);
        chk(static_cast<uint8_t>(EventType::TelemetryUpdate) != TELEM_MAGIC, "magic not enum");
        (void)pipe.process(as_type, 500);
        dw = reg.getDevice("W");
        chk(dw != nullptr && dw->ram_mb == 100, "opcode type no telem write");

        Event pad = telem_ev("W", 600, 201, 12, 6, 10, 52, 14);
        pad.payload[10] = 1;
        chk(pipe.process(pad, 600) == PipelineResult::Rejected, "padding poison");

        Event over = telem_ev("W", 700, 202, 101, 6, 10, 52, 14);
        chk(pipe.process(over, 700) == PipelineResult::Rejected, "cpu >100");

        Event hb = p30_event(EventType::Heartbeat, "W", 800, nullptr);
        chk(pipe.process(hb, 800) == PipelineResult::Accepted, "hb ok");
        dw = reg.getDevice("W");
        chk(dw != nullptr && dw->capability_mask == 0, "cap spoof via hb");
        Device up = *dw;
        up.role = ROLE_KERNEL;
        up.capability_mask = 0xFFFFu;
        chk(!reg.updateDevice("W", up), "role upgrade blocked");
        dw = reg.getDevice("W");
        chk(dw != nullptr && dw->role == ROLE_WORKER && dw->capability_mask == 0,
            "role+cap held");

        Device ph{};
        p30_copy_id(ph.id, "P");
        ph.role = ROLE_PHONE;
        ph.trust_level = 1;
        ph.status = DeviceState::Pending;
        chk(reg.addDevice(ph), "phone enroll");
        Event spoof = telem_ev("P", 900, 100, 10, 4, 8, 50, 12);
        chk(pipe.process(spoof, 900) == PipelineResult::Rejected, "role layout spoof");
        chk(reg.getDevice("P")->ram_mb == TELEM_ABSENT16, "phone no worker fields");

        Device nas{};
        p30_copy_id(nas.id, "N");
        nas.role = ROLE_SAFE;
        nas.status = DeviceState::Pending;
        chk(reg.addDevice(nas), "nas enroll");
        Event ntel = telem_ev("N", 910, 1, 1, 1, 1, 1, 1);
        chk(pipe.process(ntel, 910) == PipelineResult::Rejected, "nas telem drop");

        Device mine{};
        p30_copy_id(mine.id, "M");
        mine.role = ROLE_MINE;
        mine.status = DeviceState::Pending;
        chk(reg.addDevice(mine), "mine enroll");
        Event mtel = telem_ev("M", 920, 1, 1, 1, 1, 1, 1);
        chk(pipe.process(mtel, 920) == PipelineResult::Rejected, "mine telem drop");

        uint32_t dense0 = pipe.telemDenseDrops("W");
        for (uint8_t i = 0; i < 20; ++i) {
            Event f = telem_ev("W", 110u + i, static_cast<uint16_t>(300u + i),
                               20, 4, 8, 50, 12);
            (void)pipe.process(f, 100);
        }
        chk(pipe.telemDenseDrops("W") >= dense0 + 20u, "telem flood counted");
        dw = reg.getDevice("W");
        chk(dw != nullptr && dw->ram_mb == 100, "flood no ram drift");

        Event ok2 = telem_ev("W", 1100, 400, 20, 8, 16, 60, 24);
        chk(pipe.process(ok2, 1101) == PipelineResult::Accepted, "after flood");
        chk(reg.getDevice("W")->ram_mb == 400, "post flood write");
    }

    {
        P30Hive h;
        p30_attach(h);
        chk(p30_bind_root(h), "bind");
        p30_enroll(h, "W", ROLE_WORKER, 2);
        p30_enroll(h, "P", ROLE_PHONE, 1);
        p30_enroll(h, "F", ROLE_SENSOR, 1);
        p30_enroll(h, "R", ROLE_ROUTER, 1);

        Event t1 = telem_ev("W", 2000, 512, 12, 4, 32, 87, 54);
        p30_inject(h, ROLE_WORKER, "W", t1, 2000, true);
        chk(h.reg.getDevice("W")->ram_mb == 512, "signed telem wire");
        chk(!h.xport.hiveFrozen(), "telem no down");

        p30_inject(h, ROLE_WORKER, "W", t1, 2000, true);
        chk(h.reg.getDevice("W")->ram_mb == 512, "hb-ts replay telem");
        chk(!h.xport.hiveFrozen(), "telem replay no freeze");

        Event bad = t1;
        bad.timestamp = 2100;
        fillTelemetryPayload(&bad, 600, 13, 5, 33, 86, 54);
        (void)h.vault.signEvent(bad);
        bad.payload[0] = static_cast<char>(0x00);
        p30_inject(h, ROLE_WORKER, "W", bad, 2100, false);
        chk(h.reg.getDevice("W")->ram_mb == 512, "hmac-i telem no write");
        chk(!h.xport.hiveFrozen(), "hmac-i no freeze");
        chk(!h.vault.frozen(), "hmac-i vault live");

        Event n2 = p30_event(EventType::Heartbeat, "W", 2200, "x");
        (void)h.vault.signEvent(n2);
        n2.payload[0] = 'Z';
        p30_inject(h, ROLE_WORKER, "W", n2, 2200, false);
        Event n3 = p30_event(EventType::Heartbeat, "W", 2300, "y");
        p30_inject(h, ROLE_WORKER, "W", n3, 2300, false);
        chk(!h.xport.hiveFrozen(), "hmac-i x3 no down");
        chk(p30_has_type(h.vault, EventType::PolicyViolation), "hmac-i alert path");

        Event hb0 = p30_event(EventType::Heartbeat, "W", 3000, nullptr);
        p30_inject(h, ROLE_WORKER, "W", hb0, 3000, true);
        uint32_t seen_hb = h.reg.getDevice("W")->last_seen;
        for (uint8_t i = 0; i < 12; ++i) {
            Event hb = p30_event(EventType::Heartbeat, "W", 3000, nullptr);
            p30_inject(h, ROLE_WORKER, "W", hb, 3000, true);
        }
        chk(h.reg.getDevice("W")->last_seen == seen_hb, "hb flood same ts");
        chk(!h.xport.hiveFrozen(), "hb flood no freeze");
        chk(h.reg.getState("W") != DeviceState::GhostDown, "hb flood no gd");

        Event pt = phone_telem("P", 4000);
        p30_inject(h, ROLE_PHONE, "P", pt, 4000, true);
        chk(h.reg.getDevice("P")->ram_mb == 128, "phone telem");
        chk(h.reg.getDevice("P")->gpu_percent == TELEM_ABSENT8, "phone gpu absent");
    }

    {
        P30Hive park;
        p30_attach(park);
        chk(p30_bind_root(park), "park bind");
        Event stranger = telem_ev("Z", 5000, 1, 1, 1, 1, 1, 1);
        p30_inject(park, ROLE_WORKER, "Z", stranger, 5000, true);
        const Device* z = park.reg.getDevice("Z");
        chk(z != nullptr && z->status == DeviceState::Pending, "ibss fake pending");
        chk(z->ram_mb == TELEM_ABSENT16, "pending no telem poison");
        chk(!park.xport.hiveFrozen(), "unknown no freeze");

        Event again = telem_ev("Z", 5100, 999, 50, 50, 50, 50, 50);
        p30_inject(park, ROLE_WORKER, "Z", again, 5100, true);
        chk(park.reg.getDevice("Z")->ram_mb == TELEM_ABSENT16, "pending flood still absent");
        chk(park.reg.pairDevice("Z"), "pair pending");
        Event live = telem_ev("Z", 5200, 256, 8, 2, 16, 40, 24);
        p30_inject(park, ROLE_WORKER, "Z", live, 5200, true);
        chk(park.reg.getDevice("Z")->ram_mb == 256, "after pair telem");
    }

    {
        P30Hive spoof;
        p30_attach(spoof);
        chk(p30_bind_root(spoof), "spoof bind");
        p30_enroll(spoof, "W", ROLE_WORKER, 2);
        p30_enroll(spoof, "F", ROLE_SENSOR, 1);
        Event fake = telem_ev("W", 6000, 1, 1, 1, 1, 1, 1);
        (void)spoof.vault.signEvent(fake);
        TransportFrame sf;
        transport_clear_frame(sf);
        sf.kind = TransportKind::EventFrame;
        sf.src_role = ROLE_SENSOR;
        sf.dst_role = ROLE_KERNEL;
        p30_copy_id(sf.src_id, "F");
        p30_copy_id(sf.dst_id, KERNEL_SOURCE_ID);
        sf.event = fake;
        p30_copy_id(sf.event.source_device_id, "W");
        sf.stamp = 6000;
        (void)spoof.wlan.toKernel(sf);
        spoof.xport.rx(6000);
        chk(spoof.xport.hiveFrozen() == false, "id/role mismatch no down");
        chk(spoof.reg.getDevice("W")->ram_mb == TELEM_ABSENT16, "spoof no telem");
    }

    {
        P30Hive mineh;
        p30_attach(mineh);
        chk(p30_bind_root(mineh), "mine bind");
        Mine mine;
        mine.init("M1");
        chk(mine.setTotpSeed(mineh.keys.totpSeed(), TOTP_SEED_LEN), "totp");
        chk(mineh.pipe.replay().setTotpSeed("M1", mineh.keys.totpSeed(), TOTP_SEED_LEN),
            "guard seed");
        MinePayload first{};
        chk(mine.send(&first, 7000), "mine send");
        p30_inject_mine(mineh, first, 7000, true);
        chk(!mineh.pipe.replay().isBlocked("M1"), "mine first ok");
        p30_inject_mine(mineh, first, 7090, true);
        chk(mineh.pipe.replay().isBlocked("M1"), "mine replay block");
        chk(mineh.xport.hiveFrozen() == false, "mine replay no down");
    }

    {
        P30Hive d;
        p30_attach(d);
        chk(p30_bind_root(d), "down bind");
        p30_enroll(d, "W", ROLE_WORKER, 2);
        p30_enroll(d, "N", ROLE_SAFE, 1);
        Event t = telem_ev("W", 8000, 100, 10, 4, 8, 50, 12);
        p30_inject(d, ROLE_WORKER, "W", t, 8000, true);
        chk(d.xport.enterHiveDown(8100), "kernel down");
        chk(d.xport.hiveFrozen(), "frozen");
        chk(d.vault.frozen(), "vault freeze");
        uint16_t ram = d.reg.getDevice("W")->ram_mb;
        Event t2 = telem_ev("W", 8200, 900, 90, 9, 9, 9, 9);
        p30_inject(d, ROLE_WORKER, "W", t2, 8200, true);
        chk(d.reg.getDevice("W")->ram_mb == ram, "down freeze drops telem");
        Event hb = p30_event(EventType::Heartbeat, "W", 8300, nullptr);
        p30_inject(d, ROLE_WORKER, "W", hb, 8300, true);
        chk(d.pipe.process(hb, 8300) == PipelineResult::Rejected, "pipe frozen reject");

        HiveKill killer;
        killer.attach(&d.keys);
        Event kreq{};
        chk(killer.fill(&kreq, "W", 8400, 0), "peer kill fill");
        p30_inject(d, ROLE_WORKER, "W", kreq, 8400, true);
        chk(d.xport.hiveFrozen(), "kill race still frozen");

        d.down.tick(8100 + NAS_FLUSH_TIMEOUT_SEC);
        chk(d.down.step() == DownStep::StorageFlushWait ||
            d.down.step() == DownStep::Done, "nas wait advanced");
        uint32_t due = 8100 + NAS_FLUSH_TIMEOUT_SEC + STORAGE_FLUSH_DELAY_SEC;
        d.down.tick(due);
        d.down.tick(due + STORAGE_FLUSH_DELAY_SEC);
        chk(d.down.step() == DownStep::Done || d.down.storageFlushDone() ||
            d.down.step() == DownStep::StorageFlushRetry,
            "flush no hang");
    }

    {
        uint8_t wire[TRANSPORT_WIRE_LEN];
        TransportFrame fr;
        transport_clear_frame(fr);
        fr.kind = TransportKind::EventFrame;
        fr.src_role = ROLE_WORKER;
        fr.dst_role = ROLE_KERNEL;
        p30_copy_id(fr.src_id, "W");
        p30_copy_id(fr.dst_id, KERNEL_SOURCE_ID);
        fr.event = telem_ev("W", 9000, 64, 3, 1, 2, 10, 6);
        GhostKeys keys;
        uint8_t kb[KEY_LEN];
        for (uint8_t i = 0; i < KEY_LEN; ++i) kb[i] = static_cast<uint8_t>(i + 9);
        chk(keys.provisionDerived(kb, KEY_LEN), "udp keys");
        GhostVault v;
        v.init();
        v.attachKeys(&keys);
        chk(v.signEvent(fr.event), "sign for udp");
        char mac127 = fr.event.payload[127];
        chk(mac127 != '\0', "hmac last nibble live");
        chk(transport_encode(fr, wire, TRANSPORT_WIRE_LEN), "encode");
        TransportFrame out;
        chk(!transport_decode(wire, 0, out), "udp len 0");
        chk(!transport_decode(wire, TRANSPORT_WIRE_LEN - 1, out), "udp short");
        chk(!transport_decode(wire, TRANSPORT_WIRE_LEN + 1, out), "udp long");
        chk(transport_decode(wire, TRANSPORT_WIRE_LEN, out), "udp exact");
        chk(out.event.payload[127] == mac127, "hmac byte survives decode");
        chk(v.verifyEvent(out.event), "verify after udp");

        for (uint16_t i = 0; i < TRANSPORT_WIRE_LEN; ++i) wire[i] = 0xFFu;
        TransportFrame junk;
        chk(transport_decode(wire, TRANSPORT_WIRE_LEN, junk), "junk length ok");
        chk(junk.src_id[31] == '\0', "id cap sidechannel");
    }

    {
        P30Hive ibss;
        p30_attach(ibss);
        chk(p30_bind_root(ibss), "kernel-role bind");
        TransportFrame kfake;
        transport_clear_frame(kfake);
        kfake.kind = TransportKind::EventFrame;
        kfake.src_role = ROLE_KERNEL;
        kfake.dst_role = ROLE_KERNEL;
        p30_copy_id(kfake.src_id, "K");
        Event ev = p30_event(EventType::Heartbeat, "K", 10000, nullptr);
        (void)ibss.vault.signEvent(ev);
        kfake.event = ev;
        kfake.stamp = 10000;
        (void)ibss.wlan.toKernel(kfake);
        ibss.xport.rx(10000);
        chk(!ibss.xport.hiveFrozen(), "kernel-role frame ignored");
        chk(ibss.reg.getDevice("K") == nullptr, "no kernel peer enroll");
    }

    printf(ok ? "PASS security\n" : "FAIL security\n");
    return ok ? 0 : 1;
}
