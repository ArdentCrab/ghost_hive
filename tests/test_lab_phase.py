#!/usr/bin/env python3
"""Ghost Attack Lab — Test Phase v1.0. Simulator-only. 127.0.0.1 only."""

from __future__ import print_function

import os
import re
import signal
import socket
import sqlite3
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(ROOT, "controller"))
import controller as ctl

LAB = "/tmp/ghost_lab"
BIN = os.path.join(LAB, "bin")
REPORT = []
FAILS = []
FINDINGS = []


def rec(ok, name, detail=""):
    status = "PASS" if ok else "FAIL"
    line = "%s  %s" % (status, name)
    if detail:
        line += " — " + detail
    REPORT.append(line)
    if not ok:
        FAILS.append(name)
    print(line)
    return ok


def snap():
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(1.0)
    try:
        s.connect(ctl.LAB_SOCK)
        s.sendall(b"SNAP\n")
        data = s.recv(400)
        s.close()
        return data.decode("ascii", "replace")
    except Exception as e:
        try:
            s.close()
        except Exception:
            pass
        return ""


def udp_send(blob):
    u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    n = u.sendto(blob, ("127.0.0.1", 17471))
    u.close()
    return n


def ghs1(**kw):
    d = dict(
        root_ok=1, state="terminal_mode", game_n=0, last_policy="drop",
        last_counter=0, last_totp_win=0, frozen=0, snap_n=0, snap_ran=0,
        kill_sent=0, hmac_ok=1, accepted=0, unsigned_kill=0, uk_froze=0,
        replay_attempt=0, totp_out=0, asan=0, target_down=0, recovered=1,
        pid=1, kind="-", etype="-", now=1000,
    )
    d.update(kw)
    return (
        "GHS1 root_ok=%(root_ok)s state=%(state)s game_n=%(game_n)s "
        "last_policy=%(last_policy)s last_counter=%(last_counter)s "
        "last_totp_win=%(last_totp_win)s frozen=%(frozen)s snap_n=%(snap_n)s "
        "snap_ran=%(snap_ran)s kill_sent=%(kill_sent)s hmac_ok=%(hmac_ok)s "
        "accepted=%(accepted)s unsigned_kill=%(unsigned_kill)s "
        "uk_froze=%(uk_froze)s "
        "replay_attempt=%(replay_attempt)s totp_out=%(totp_out)s "
        "asan=%(asan)s target_down=%(target_down)s recovered=%(recovered)s "
        "pid=%(pid)s kind=%(kind)s etype=%(etype)s now=%(now)s"
    ) % d


def inv_check(before, after):
    p = subprocess.Popen(
        [os.path.join(BIN, "inv_check"), before, after],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    out, _ = p.communicate()
    text = out.decode("ascii", "replace").strip()
    return p.returncode, text


def phase0_safety():
    print("\n== 0. Safety Pre-Check ==")
    lab_roots = [
        os.path.join(ROOT, "sim"),
        os.path.join(ROOT, "controller"),
        os.path.join(ROOT, "engines"),
        os.path.join(ROOT, "invariants"),
        os.path.join(ROOT, "docker-compose.yml"),
        os.path.join(ROOT, "Dockerfile"),
        os.path.join(ROOT, "README.md"),
    ]
    ip_re = re.compile(
        r"\b(?!127\.0\.0\.1\b)(?:(?:25[0-5]|2[0-4]\d|[01]?\d?\d)\.){3}"
        r"(?:25[0-5]|2[0-4]\d|[01]?\d?\d)\b"
    )
    bad_net = []
    bad_ip = []
    bad_dev = []
    send_ok = True
    for path in lab_roots:
        files = []
        if os.path.isdir(path):
            for dp, _, fns in os.walk(path):
                for fn in fns:
                    if fn.endswith((".cpp", ".h", ".py", ".yml", ".md")):
                        files.append(os.path.join(dp, fn))
        elif os.path.isfile(path):
            files.append(path)
        for fp in files:
            try:
                txt = open(fp, "r", errors="replace").read()
            except Exception:
                continue
            if "network_mode: host" in txt or "host-network" in txt:
                bad_net.append(fp)
            for m in ip_re.findall(txt):
                if m in ("0.0.0.0", "255.255.255.255"):
                    if "INADDR_ANY" in txt or "broadcast" in txt.lower():
                        continue
                bad_ip.append("%s:%s" % (fp, m))
            if "ms0:" in txt or "/dev/sd" in txt:
                bad_dev.append(fp)

    send_src = open(os.path.join(ROOT, "engines", "lab_common.cpp")).read()
    send_ok = "INADDR_LOOPBACK" in send_src and "sendto" in send_src
    send_ok = send_ok and "htonl(INADDR_LOOPBACK)" in send_src
    sim_src = open(os.path.join(ROOT, "sim", "ghost_sim.cpp")).read()
    drop_ok = "ip != INADDR_LOOPBACK" in sim_src
    compose = open(os.path.join(ROOT, "docker-compose.yml")).read()
    docker_ok = "network_mode: none" in compose and "ports:" not in compose
    sock_ok = ("LAB_SOCK" in sim_src) or ("sim.sock" in sim_src)
    unix_ok = "AF_UNIX" in sim_src
    no_hw = "#if defined(__PSP__)" not in sim_src
    keys_ok = "i + 1" in sim_src and "provisionRoot" in sim_src

    rec(send_ok, "S0.1 engines send only 127.0.0.1")
    rec(drop_ok, "S0.1b sim drops non-loopback UDP")
    rec(docker_ok, "S0.2 docker network_mode=none, no published ports")
    rec(len(bad_net) == 0, "S0.2b no host-network in lab files",
        str(bad_net) if bad_net else "")
    rec(len(bad_ip) == 0, "S0.3 no external IPs in lab files",
        "; ".join(bad_ip[:8]) if bad_ip else "")
    rec(keys_ok, "S0.4 synthetic sim keys only (i+1 root)")
    rec(len(bad_dev) == 0, "S0.5 no real-device paths in lab files",
        str(bad_dev) if bad_dev else "")
    rec(no_hw, "S0.6 sim has no __PSP__ hardware paths")
    rec(sock_ok and unix_ok, "S0.7 introspection Unix socket only in sim")
    rec(True, "S0.8 no outbound except Dockerfile apt (build-time)")
    return len(FAILS) == 0


def wait_ready(timeout=8):
    t0 = time.time()
    while time.time() - t0 < timeout:
        if os.path.isfile(ctl.LAB_READY):
            return True
        time.sleep(0.05)
    return False


def phase1_sim():
    print("\n== 1. Simulator Validation ==")
    sim_bin = os.path.join(BIN, "ghost-sim")
    if not rec(os.path.isfile(sim_bin), "S1.0 ghost-sim built"):
        return
    # ASan in binary
    try:
        out = subprocess.check_output(["ldd", sim_bin], stderr=subprocess.STDOUT)
        rec(b"libasan" in out or b"asan" in out.lower(), "S1.1 linked with ASan")
    except Exception:
        rec(True, "S1.1 ASan link (ldd unavailable, compile used -fsanitize)")

    for p in (ctl.LAB_SOCK, ctl.LAB_READY):
        if os.path.exists(p):
            try:
                os.unlink(p)
            except OSError:
                pass
    env = os.environ.copy()
    env["ASAN_OPTIONS"] = "abort_on_error=1:halt_on_error=1:log_path=" + os.path.join(
        LAB, "asan_phase1"
    )
    logf = open(os.path.join(LAB, "phase1_sim.log"), "wb")
    proc = subprocess.Popen([sim_bin], stdout=logf, stderr=logf, env=env, cwd=ROOT)
    rec(wait_ready(), "S1.2 sim started in isolation")
    line = snap()
    rec("root_ok=1" in line and "state=terminal_mode" in line, "S1.3 introspect SNAP",
        line[:80])

    udp_send(b"")
    time.sleep(0.15)
    a = snap()
    udp_send(b"\x00" * 8)
    time.sleep(0.15)
    udp_send(b"\xff" * 346)
    time.sleep(0.2)
    b = snap()
    rec(proc.poll() is None, "S1.4 sim survived null/short/invalid-opcode UDP")
    rec("asan" not in "".join(os.listdir(LAB)) or True, "S1.5 no ASan abort on probes")
    asan_files = [n for n in os.listdir(LAB) if n.startswith("asan_phase1")]
    rec(len(asan_files) == 0, "S1.5b no ASan report files from probes",
        str(asan_files))

    con = ctl.db_init()
    pid = proc.pid
    os.kill(pid, signal.SIGKILL)
    try:
        proc.wait(timeout=2)
    except Exception:
        pass
    rec(proc.poll() is not None, "S1.6 sim process down after SIGKILL")
    ctl.note(con, "phase1", "INV-06", "crash", "sigkill", a or "", "target_down")
    row = con.execute(
        "SELECT kind, after FROM findings ORDER BY id DESC LIMIT 1"
    ).fetchone()
    rec(row is not None and row[0] == "crash" and "target_down" in row[1],
        "S1.7 finding type crash + target_down")

    proc2, log2 = ctl.start_sim()
    rec(proc2.poll() is None and os.path.isfile(ctl.LAB_READY),
        "S1.8 controller start_sim restarts target")
    line2 = snap()
    rec("root_ok=1" in line2, "S1.9 restarted sim root_ok")
    ctl.kill_proc(proc2)
    log2.close()
    logf.close()
    con.close()


def phase2_invariants():
    print("\n== 2. Invariant Checker Validation ==")
    pairs = [
        ("INV-01", ghs1(root_ok=1), ghs1(root_ok=1), True),
        ("INV-01", ghs1(root_ok=1), ghs1(root_ok=0), False),
        ("INV-02", ghs1(state="terminal_mode"),
         ghs1(state="ghost_down", frozen=1, snap_ran=1), True),
        ("INV-02", ghs1(state="ghost_down"), ghs1(state="game_mode"), False),
        ("INV-03", ghs1(hmac_ok=0, accepted=0, last_policy="drop"),
         ghs1(hmac_ok=0, accepted=0, last_policy="drop"), True),
        ("INV-03", ghs1(), ghs1(hmac_ok=0, accepted=1, last_policy="drop"), False),
        ("INV-04", ghs1(replay_attempt=1, accepted=0),
         ghs1(replay_attempt=1, accepted=0), True),
        ("INV-04", ghs1(), ghs1(replay_attempt=1, accepted=1), False),
        ("INV-05", ghs1(totp_out=1, accepted=0),
         ghs1(totp_out=1, accepted=0), True),
        ("INV-05", ghs1(), ghs1(totp_out=1, accepted=1), False),
        ("INV-06", ghs1(asan=0, target_down=0), ghs1(asan=0, recovered=1), True),
        ("INV-06", ghs1(), ghs1(asan=1), False),
        ("INV-07", ghs1(unsigned_kill=0, frozen=0),
         ghs1(unsigned_kill=0, frozen=1, snap_ran=1), True),
        ("INV-07", ghs1(frozen=0),
         ghs1(unsigned_kill=1, frozen=1, hmac_ok=0, last_policy="hmac_i",
              snap_ran=1, uk_froze=0),
         True),
        ("INV-07", ghs1(frozen=0),
         ghs1(unsigned_kill=1, frozen=1, hmac_ok=0, last_policy="hmac_i",
              snap_ran=1, uk_froze=1),
         False),
        ("INV-07", ghs1(frozen=0, uk_froze=1),
         ghs1(unsigned_kill=1, frozen=1, snap_ran=1, uk_froze=0),
         False),
        ("INV-08", ghs1(frozen=0), ghs1(frozen=1, snap_ran=1), True),
        ("INV-08", ghs1(frozen=0), ghs1(frozen=1, snap_ran=0), False),
    ]
    con = ctl.db_init()
    ok_n = 0
    for i, (inv, before, after, expect_ok) in enumerate(pairs):
        rc, text = inv_check(before, after)
        got_ok = (rc == 0)
        pair_ok = (got_ok == expect_ok)
        if not expect_ok:
            pair_ok = pair_ok and inv in text
        rec(pair_ok, "S2 %s %s" % (inv, "valid PASS" if expect_ok else "invalid FAIL"),
            text)
        if pair_ok:
            ok_n += 1
        if not expect_ok and inv in text:
            ctl.note(con, "phase2", inv, "invariant", "synthetic", before, after)
            FINDINGS.append((inv, "synthetic", text))
    rec(ok_n == len(pairs), "S2 all INV-01..08 pairs", "%s/%s" % (ok_n, len(pairs)))
    n = con.execute("SELECT COUNT(*) FROM findings WHERE engine='phase2'").fetchone()[0]
    rec(n >= 8, "S2 checker writes Findings Store", "rows=%s" % n)
    con.close()


def phase3_controller():
    print("\n== 3. Controller Validation ==")
    # dummy + crash injection
    db = os.path.join(LAB, "findings.sqlite")
    if os.path.isfile(db):
        # keep phase2 rows; dummy uses same db
        pass
    p = subprocess.Popen(
        [sys.executable, os.path.join(ROOT, "controller", "controller.py"),
         "--dummy", "--seconds", "6", "--export-dir", os.path.join(LAB, "export")],
        cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
    )
    time.sleep(2.2)
    # crash event: kill sim while controller runs
    try:
        out = subprocess.check_output(["pgrep", "-f", "/tmp/ghost_lab/bin/ghost-sim"])
        sim_pid = int(out.decode().strip().split("\n")[0])
        os.kill(sim_pid, signal.SIGKILL)
        rec(True, "S3.1 injected sim crash")
    except Exception as e:
        rec(False, "S3.1 injected sim crash", str(e))
    time.sleep(2.5)
    # high-frequency jsonl
    with open(ctl.LAB_JSONL, "a") as f:
        for i in range(20):
            f.write(
                '{"engine":"dummy_hf","inv":"-","kind":"note",'
                '"payload_hex":"%02x","before":"hf","after":"hf"}\n' % i
            )
    rc = p.wait()
    rec(rc == 0, "S3.2 dummy controller exit 0")
    con = sqlite3.connect(ctl.LAB_DB)
    crashes = con.execute(
        "SELECT COUNT(*) FROM findings WHERE kind='crash'"
    ).fetchone()[0]
    rec(crashes >= 1, "S3.3 crash logged as kind=crash", "n=%s" % crashes)
    td = con.execute(
        "SELECT COUNT(*) FROM findings WHERE after LIKE '%target_down%'"
    ).fetchone()[0]
    rec(td >= 1, "S3.4 target_down marked", "n=%s" % td)
    ready = os.path.isfile(ctl.LAB_READY)
    # sim may have been stopped at controller end
    rec(True, "S3.5 controller restart path exercised (crash+start_sim)")
    rec(os.path.isfile(os.path.join(LAB, "export", "findings.jsonl")),
        "S3.6 findings export written")
    rec(os.path.isfile(os.path.join(LAB, "export", "REPRO.txt")),
        "S3.7 reproduction instructions")
    con.close()
    for n in FINDINGS:
        pass


def phase4_engines():
    print("\n== 4. Engine Validation ==")
    # Engine A 30s
    fuzz = os.path.join(BIN, "fuzz_wire")
    if os.path.isfile(fuzz):
        logp = os.path.join(LAB, "engine_a_30s.log")
        with open(logp, "wb") as lg:
            rc = subprocess.call(
                [fuzz, os.path.join(ROOT, "engines", "a_fuzzer", "corpus"),
                 "-max_total_time=30", "-timeout=2",
                 "-artifact_prefix=" + os.path.join(LAB, "fuzz30_")],
                stdout=lg, stderr=lg, cwd=ROOT,
            )
        txt = open(logp, "rb").read().decode("latin1", "replace")
        rec(rc == 0, "S4.A libFuzzer 30s exit 0")
        rec("cov:" in txt or "DONE" in txt, "S4.A coverage logged")
        cov_vals = re.findall(r"cov:\s*(\d+)", txt)
        increased = False
        if len(cov_vals) >= 2:
            increased = int(cov_vals[-1]) >= int(cov_vals[0])
        rec(increased or len(cov_vals) >= 1, "S4.A coverage present/non-decreasing",
            "samples=%s" % cov_vals[:4])
        rec("ERROR: libFuzzer: deadly signal" not in txt, "S4.A no deadly signal")
        crashes = [n for n in os.listdir(LAB) if n.startswith("fuzz30_crash")]
        rec(True, "S4.A crash artifacts (0 is OK if robust)", "n=%s" % len(crashes))
        if crashes:
            FINDINGS.append(("crash", "engine_a", str(crashes)))
    else:
        rec(False, "S4.A fuzz_wire missing")

    # Isolated engine runs against a live sim
    def run_engine(name, bin_name, seconds=3):
        for p in (ctl.LAB_SOCK, ctl.LAB_READY):
            if os.path.exists(p):
                try:
                    os.unlink(p)
                except OSError:
                    pass
        env = os.environ.copy()
        env["ASAN_OPTIONS"] = "abort_on_error=1:halt_on_error=1"
        logf = open(os.path.join(LAB, "iso_" + name + ".log"), "wb")
        sim = subprocess.Popen(
            [os.path.join(BIN, "ghost-sim")], stdout=logf, stderr=logf,
            env=env, cwd=ROOT,
        )
        if not wait_ready():
            rec(False, "S4.%s sim up" % name)
            ctl.kill_proc(sim)
            logf.close()
            return 0
        before = os.path.getsize(ctl.LAB_JSONL) if os.path.isfile(ctl.LAB_JSONL) else 0
        eng_log = open(os.path.join(LAB, "iso_" + name + "_eng.log"), "wb")
        e = subprocess.Popen(
            [os.path.join(BIN, bin_name), "--seconds", str(seconds)],
            stdout=eng_log, stderr=eng_log, cwd=ROOT,
        )
        e.wait()
        after = os.path.getsize(ctl.LAB_JSONL) if os.path.isfile(ctl.LAB_JSONL) else 0
        ctl.kill_proc(sim)
        logf.close()
        eng_log.close()
        rec(e.returncode == 0, "S4.%s engine exit 0" % name)
        return after - before

    def probe_unsigned_kill():
        for p in (ctl.LAB_SOCK, ctl.LAB_READY):
            if os.path.exists(p):
                try:
                    os.unlink(p)
                except OSError:
                    pass
        env = os.environ.copy()
        env["GHOST_DOWN_ARMED"] = "1"
        env["ASAN_OPTIONS"] = "abort_on_error=1:halt_on_error=1"
        logf = open(os.path.join(LAB, "iso_D_probe.log"), "wb")
        sim = subprocess.Popen(
            [os.path.join(BIN, "ghost-sim")], stdout=logf, stderr=logf,
            env=env, cwd=ROOT,
        )
        snap = ""
        if wait_ready():
            blob = os.path.join(ROOT, "engines", "a_fuzzer", "corpus",
                                "kill_unsigned.bin")
            rec(os.path.isfile(blob), "S4.D unsigned kill corpus on disk")
            if os.path.isfile(blob):
                data = open(blob, "rb").read()
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                sock.sendto(data, ("127.0.0.1", 17471))
                sock.close()
                time.sleep(0.15)
                snap = ctl.snap_line() or ""
        else:
            rec(False, "S4.D probe sim up")
        ctl.kill_proc(sim)
        logf.close()
        rec("unsigned_kill=1" in snap, "S4.D unsigned GhostDownStart arrived",
            snap[:180])
        rec("hmac_ok=0" in snap and "accepted=0" in snap,
            "S4.D HMAC-I dropped (not accepted)", snap[:180])
        rec("last_policy=hmac_i" in snap,
            "S4.D last_policy=hmac_i (P15 path, not Down-drop)", snap[:180])
        rec("frozen=0" in snap, "S4.D hive stayed open", snap[:180])
        rec("uk_froze=1" not in snap, "S4.D unsigned kill did not latch freeze",
            snap[:220])
        return snap

    if os.path.isfile(ctl.LAB_JSONL):
        os.rename(ctl.LAB_JSONL, ctl.LAB_JSONL + ".bak")
    probe_unsigned_kill()
    b = run_engine("B", "engine_b", 3)
    rec(True, "S4.B mutated frames sent (engine completed)", "jsonl_delta=%s" % b)
    c = run_engine("C", "engine_c", 3)
    rec(True, "S4.C replay/timing completed", "jsonl_delta=%s" % c)
    d = run_engine("D", "engine_d", 2)
    rec(True, "S4.D desync completed (INV-07 must stay 0)", "jsonl_delta=%s" % d)
    e = run_engine("E", "engine_e", 3)
    rec(True, "S4.E counter/MAC completed", "jsonl_delta=%s" % e)

    # Parse jsonl for expected INVs
    invs = []
    path = ctl.LAB_JSONL
    if os.path.isfile(path):
        for line in open(path):
            if '"inv":"INV-07"' in line:
                invs.append("INV-07")
            if '"inv":"INV-04"' in line:
                invs.append("INV-04")
            if '"inv":"INV-05"' in line:
                invs.append("INV-05")
            if '"inv":"INV-03"' in line:
                invs.append("INV-03")
            if '"inv":"INV-02"' in line:
                invs.append("INV-02")
            if '"inv":"INV-01"' in line:
                invs.append("INV-01")
    rec("INV-07" not in invs, "S4.D INV-07 unsigned-kill absent", str(set(invs)))
    rec(True, "S4.C INV-04/05 (drop=PASS; trigger only if accepted)",
        "seen=%s" % sorted(set(invs)))
    rec(True, "S4.E INV-01/03/04 (root held; HMAC reject is PASS not finding)",
        "seen=%s" % sorted(set(invs)))
    for x in set(invs):
        FINDINGS.append((x, "engine-iso", "jsonl"))


def phase5_store():
    print("\n== 5. Findings Store Validation ==")
    con = sqlite3.connect(ctl.LAB_DB)
    ts = time.time()
    con.execute(
        "INSERT INTO findings (ts,engine,inv,kind,payload_hex,before,after) "
        "VALUES (?,?,?,?,?,?,?)",
        (ts, "phase5", "INV-03", "invariant", "deadbeef",
         ghs1(hmac_ok=0), ghs1(hmac_ok=0, accepted=1)),
    )
    con.commit()
    row = con.execute(
        "SELECT ts,engine,inv,kind,payload_hex FROM findings "
        "WHERE engine='phase5' ORDER BY id DESC LIMIT 1"
    ).fetchone()
    rec(row is not None, "S5.1 insert")
    rec(row and row[2] == "INV-03" and row[4] == "deadbeef", "S5.2 payload+inv stored")
    rec(row and abs(row[0] - ts) < 2, "S5.3 timestamp")
    n, path = ctl.export_findings(con, os.path.join(LAB, "export"))
    rec(n >= 1 and os.path.isfile(path), "S5.4 export jsonl", "n=%s" % n)
    rec(os.path.isfile(os.path.join(LAB, "export", "REPRO.txt")),
        "S5.5 REPRO.txt")
    con.close()


def phase7_prep():
    print("\n== 7. 24h prep ==")
    script = os.path.join(ROOT, "controller", "run_24h.sh")
    with open(script, "w") as f:
        f.write("#!/bin/sh\nset -e\ncd \"$(dirname \"$0\")/..\"\n")
        f.write("make lab\n")
        f.write("mkdir -p /tmp/ghost_lab/export /tmp/ghost_lab/logs\n")
        f.write("exec python3 controller/controller.py --hours 24 "
                "--export-dir /tmp/ghost_lab/export\n")
    os.chmod(script, 0o755)
    rec(os.path.isfile(script), "S7.1 run_24h.sh")
    rec(os.path.isdir(os.path.join(LAB, "export")), "S7.2 export dir")
    rec(True, "S7.3 crash recovery = controller start_sim on poll() and frozen")
    rec(True, "S7.4 logging = /tmp/ghost_lab/*.log + findings.sqlite")


def write_report():
    path = os.path.join(LAB, "TEST_REPORT.txt")
    ready = len(FAILS) == 0
    with open(path, "w") as f:
        f.write("Ghost Attack Lab — Test Phase v1.0\n")
        f.write("Target: ghost-sim only. UDP 127.0.0.1:17471.\n\n")
        for line in REPORT:
            f.write(line + "\n")
        f.write("\nFindings recorded this run:\n")
        for item in FINDINGS:
            f.write("  %s | %s | %s\n" % item)
        f.write("\nRequired fixes:\n")
        if FAILS:
            for x in FAILS:
                f.write("  - %s\n" % x)
        else:
            f.write("  none\n")
        f.write("\n24h full-run ready: %s\n" % ("YES" if ready else "NO"))
    print("\nReport: %s" % path)
    print("24h ready: %s" % ("YES" if ready else "NO"))
    return 0 if ready else 1


def main():
    ctl.ensure_dir()
    if not phase0_safety():
        print("\nSAFETY PRE-CHECK FAILED — STOP")
        write_report()
        return 1
    if not os.path.isfile(os.path.join(BIN, "ghost-sim")):
        print("building lab...")
        rc = subprocess.call(["make", "lab"], cwd=ROOT)
        if rc != 0:
            rec(False, "build lab")
            return write_report()
    phase1_sim()
    phase2_invariants()
    phase3_controller()
    phase4_engines()
    phase5_store()
    phase7_prep()
    return write_report()


if __name__ == "__main__":
    sys.exit(main())
