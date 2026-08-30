"""Ghost Attack Lab controller. Sim-only. UDP only to 127.0.0.1."""

from __future__ import print_function

import argparse
import os
import signal
import sqlite3
import subprocess
import sys
import time

LAB_DIR = "/tmp/ghost_lab"
LAB_BIN = os.path.join(LAB_DIR, "bin")
LAB_SOCK = os.path.join(LAB_DIR, "sim.sock")
LAB_READY = os.path.join(LAB_DIR, "ready")
LAB_DB = os.path.join(LAB_DIR, "findings.sqlite")
LAB_JSONL = os.path.join(LAB_DIR, "findings.jsonl")
INV_CHECK = os.path.join(LAB_BIN, "inv_check")
SIM = os.path.join(LAB_BIN, "ghost-sim")


def ensure_dir():
    if not os.path.isdir(LAB_DIR):
        os.makedirs(LAB_DIR, 0o755)


def db_init():
    con = sqlite3.connect(LAB_DB)
    con.execute(
        "CREATE TABLE IF NOT EXISTS findings ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "ts REAL,"
        "engine TEXT,"
        "inv TEXT,"
        "kind TEXT,"
        "payload_hex TEXT,"
        "before TEXT,"
        "after TEXT)"
    )
    con.commit()
    return con


def ingest_jsonl(con):
    if not os.path.isfile(LAB_JSONL):
        return 0
    n = 0
    with open(LAB_JSONL, "r") as f:
        lines = f.readlines()
    # jsonl written by engines; import new lines via file size marker
    for line in lines:
        line = line.strip()
        if not line:
            continue
        engine = "-"
        inv = "-"
        kind = "invariant"
        payload = ""
        before = ""
        after = ""
        def grab(key):
            k = '"' + key + '":"'
            i = line.find(k)
            if i < 0:
                return ""
            i += len(k)
            j = line.find('"', i)
            if j < 0:
                return ""
            return line[i:j]
        engine = grab("engine") or engine
        inv = grab("inv") or inv
        kind = grab("kind") or kind
        payload = grab("payload_hex")
        before = grab("before")
        after = grab("after")
        con.execute(
            "INSERT INTO findings (ts,engine,inv,kind,payload_hex,before,after) "
            "VALUES (?,?,?,?,?,?,?)",
            (time.time(), engine, inv, kind, payload, before, after),
        )
        n += 1
    con.commit()
    if n:
        os.rename(LAB_JSONL, LAB_JSONL + ".ingested")
    return n


def note(con, engine, inv, kind, payload, before, after):
    con.execute(
        "INSERT INTO findings (ts,engine,inv,kind,payload_hex,before,after) "
        "VALUES (?,?,?,?,?,?,?)",
        (time.time(), engine, inv, kind, payload, before, after),
    )
    con.commit()


SIM_ENV = {}


def start_sim():
    if os.path.exists(LAB_SOCK):
        try:
            os.unlink(LAB_SOCK)
        except OSError:
            pass
    if os.path.exists(LAB_READY):
        try:
            os.unlink(LAB_READY)
        except OSError:
            pass
    env = os.environ.copy()
    env.update(SIM_ENV)
    env.pop("GHOST_OS_HALT", None)
    # Attack lab always fires real Ghost Down. MVP lock is PSP/host kernel only.
    env["GHOST_DOWN_ARMED"] = "1"
    env.setdefault("GHOST_LAB_TIME_FACTOR", "1")
    env.setdefault("GHOST_LAB_GHOST_MODE", "0")
    env["ASAN_OPTIONS"] = "abort_on_error=1:halt_on_error=0:log_path=" + os.path.join(
        LAB_DIR, "asan"
    )
    env["UBSAN_OPTIONS"] = "print_stacktrace=1:halt_on_error=0"
    out = open(os.path.join(LAB_DIR, "sim.log"), "ab")
    proc = subprocess.Popen(
        [SIM],
        stdout=out,
        stderr=out,
        env=env,
        cwd=os.getcwd(),
    )
    deadline = time.time() + 8
    while time.time() < deadline:
        if os.path.isfile(LAB_READY) and proc.poll() is None:
            return proc, out
        if proc.poll() is not None:
            return proc, out
        time.sleep(0.05)
    return proc, out


def unix_cmd(cmd):
    import socket
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(0.5)
    try:
        s.connect(LAB_SOCK)
        s.sendall(cmd.encode("ascii") + b"\n")
        data = s.recv(400)
        s.close()
        return data.decode("ascii", "replace")
    except Exception:
        try:
            s.close()
        except Exception:
            pass
        return ""


def snap_line():
    return unix_cmd("SNAP")


def cfg_line():
    line = unix_cmd("CFG")
    if line:
        return line
    path = os.path.join(LAB_DIR, "sim_cfg")
    if not os.path.isfile(path):
        return ""
    try:
        with open(path, "r") as f:
            return " ".join(x.strip() for x in f.readlines() if x.strip())
    except Exception:
        return ""


def snap_field(line, key):
    if not line:
        return ""
    k = key + "="
    i = line.find(k)
    if i < 0:
        return ""
    i += len(k)
    j = line.find(" ", i)
    val = line[i:j] if j >= 0 else line[i:]
    return val.strip().strip("\n")


def append_log(path, line):
    with open(path, "a") as f:
        f.write(line)
        if not line.endswith("\n"):
            f.write("\n")


def last_cov_line(log_path):
    if not os.path.isfile(log_path):
        return ""
    last = ""
    try:
        with open(log_path, "rb") as f:
            f.seek(0, os.SEEK_END)
            sz = f.tell()
            f.seek(max(0, sz - 65536), os.SEEK_SET)
            text = f.read().decode("ascii", "replace")
        for ln in text.splitlines():
            if "cov:" in ln or "COVERED_FUNCS" in ln or "PASS fuzz" in ln:
                last = ln
    except Exception:
        return last
    return last


def run_inv(before, after):
    if not before or not after:
        return "OK"
    p = subprocess.Popen(
        [INV_CHECK, before.strip(), after.strip()],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    out, _ = p.communicate()
    text = out.decode("ascii", "replace").strip()
    return text if text else "FAIL parse"


def engine_argv(kind, remain):
    corpus = "engines/a_fuzzer/corpus"
    if kind == "engine_b":
        return [os.path.join(LAB_BIN, "engine_b"), "--seconds", str(remain)]
    if kind == "engine_c":
        return [os.path.join(LAB_BIN, "engine_c"), "--seconds", str(remain)]
    if kind == "engine_d":
        return [os.path.join(LAB_BIN, "engine_d"), "--seconds", str(remain)]
    if kind == "engine_e":
        return [os.path.join(LAB_BIN, "engine_e"), "--seconds", str(remain)]
    if kind == "fuzz":
        return [
            os.path.join(LAB_BIN, "fuzz_wire"),
            corpus,
            "-max_total_time=" + str(remain),
            "-timeout=2",
            "-artifact_prefix=" + os.path.join(LAB_DIR, "fuzz_"),
        ]
    if kind == "smoke_loop":
        fuzz = os.path.join(LAB_BIN, "fuzz_smoke")
        return [
            sys.executable, "-c",
            "import subprocess,sys,time\n"
            "end=time.time()+int(sys.argv[1])\n"
            "cmd=sys.argv[2:]\n"
            "while time.time()<end:\n"
            " subprocess.call(cmd)\n",
            str(remain), fuzz, corpus,
        ]
    if kind == "smoke":
        return [os.path.join(LAB_BIN, "fuzz_smoke"), corpus]
    return []


def start_engines(seconds, accelerated=False):
    kinds = []
    for name in ("engine_b", "engine_c", "engine_d", "engine_e"):
        if os.path.isfile(os.path.join(LAB_BIN, name)):
            kinds.append(name)
    if accelerated:
        if os.path.isfile(os.path.join(LAB_BIN, "fuzz_wire")):
            kinds.append("fuzz")
        elif os.path.isfile(os.path.join(LAB_BIN, "fuzz_smoke")):
            kinds.append("smoke_loop")
    else:
        if os.path.isfile(os.path.join(LAB_BIN, "fuzz_smoke")):
            kinds.append("smoke")
    procs = []
    logs = []
    metas = []
    for kind in kinds:
        argv = engine_argv(kind, seconds)
        if not argv:
            continue
        if kind != "smoke_loop" and not os.path.isfile(argv[0]):
            continue
        logname = "engine_a.log" if kind in ("fuzz", "smoke", "smoke_loop") else kind + ".log"
        lg = open(os.path.join(LAB_DIR, logname), "ab")
        p = subprocess.Popen(argv, stdout=lg, stderr=lg)
        procs.append(p)
        logs.append(lg)
        metas.append({"kind": kind, "log": logname, "restarts": 0})
    return procs, logs, metas


def respawn_engines(procs, logs, metas, remain, accelerated):
    if not accelerated:
        return
    remain = max(1, int(remain))
    for i, meta in enumerate(metas):
        if procs[i].poll() is None:
            continue
        argv = engine_argv(meta["kind"], remain)
        if not argv:
            continue
        try:
            logs[i].close()
        except Exception:
            pass
        logs[i] = open(os.path.join(LAB_DIR, meta["log"]), "ab")
        procs[i] = subprocess.Popen(argv, stdout=logs[i], stderr=logs[i])
        meta["restarts"] += 1


def kill_proc(proc):
    if proc is None:
        return
    if proc.poll() is not None:
        return
    proc.send_signal(signal.SIGTERM)
    try:
        proc.wait(timeout=2)
    except Exception:
        proc.kill()


def asan_hit():
    for name in os.listdir(LAB_DIR):
        if name.startswith("asan"):
            return True
    return False


def export_findings(con, dest_dir):
    if not os.path.isdir(dest_dir):
        os.makedirs(dest_dir, 0o755)
    out_jsonl = os.path.join(dest_dir, "findings.jsonl")
    n = 0
    with open(out_jsonl, "w") as f:
        for row in con.execute(
            "SELECT ts,engine,inv,kind,payload_hex,before,after FROM findings "
            "ORDER BY id"
        ):
            ts, engine, inv, kind, payload, before, after = row
            before = (before or "").replace("\\", "\\\\").replace('"', "'")
            after = (after or "").replace("\\", "\\\\").replace('"', "'")
            payload = (payload or "").replace('"', "")
            f.write(
                '{"ts":%s,"engine":"%s","inv":"%s","kind":"%s",'
                '"payload_hex":"%s","before":"%s","after":"%s"}\n'
                % (ts, engine, inv, kind, payload, before, after)
            )
            n += 1
    readme = os.path.join(dest_dir, "REPRO.txt")
    with open(readme, "w") as f:
        f.write("Replay a payload: hex-decode payload_hex, pad/trunc to 346 bytes,\n")
        f.write("UDP send to 127.0.0.1:17471 only. Never any other address.\n")
        f.write("Rows: %s\n" % n)
        factor = SIM_ENV.get("GHOST_LAB_TIME_FACTOR", os.environ.get("GHOST_LAB_TIME_FACTOR", "1"))
        gm = SIM_ENV.get("GHOST_LAB_GHOST_MODE", os.environ.get("GHOST_LAB_GHOST_MODE", "0"))
        f.write("sim_time_factor=%s ghost_mode=%s (simulator only)\n" % (factor, gm))
        f.write("Policies exercised: GhostDownStart, PeerKill/KillFrame, MineEvent, PeekScan/terminal_mode.\n")
    return n, out_jsonl


def write_status(path, text):
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        f.write(text)
    os.rename(tmp, path)


def main():
    global SIM_ENV
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=int, default=0)
    ap.add_argument("--hours", type=float, default=0)
    ap.add_argument("--no-engines", action="store_true")
    ap.add_argument("--dummy", action="store_true")
    ap.add_argument("--accelerated", action="store_true")
    ap.add_argument("--time-factor", type=int, default=0)
    ap.add_argument("--export-dir", default="/tmp/ghost_lab/export")
    args = ap.parse_args()
    total = args.seconds
    if args.hours > 0:
        total = int(args.hours * 3600)
    if total <= 0:
        total = 12

    factor = args.time_factor
    if factor <= 0:
        try:
            factor = int(os.environ.get("GHOST_LAB_TIME_FACTOR", "1"))
        except ValueError:
            factor = 1
    if factor <= 0:
        factor = 1
    if args.accelerated and factor < 2:
        factor = 200
    SIM_ENV = {
        "GHOST_LAB_TIME_FACTOR": str(factor),
        "GHOST_LAB_GHOST_MODE": "1" if args.accelerated else os.environ.get(
            "GHOST_LAB_GHOST_MODE", "0"),
    }
    if args.accelerated:
        SIM_ENV["GHOST_LAB_GHOST_MODE"] = "1"

    ensure_dir()
    if not os.path.isdir(args.export_dir):
        os.makedirs(args.export_dir, 0o755)
    with open(os.path.join(LAB_DIR, "controller.pid"), "w") as f:
        f.write(str(os.getpid()) + "\n")

    if not os.path.isfile(SIM):
        print("FAIL missing " + SIM + " (make lab)")
        return 1
    con = db_init()
    sim, simlog = start_sim()
    if sim.poll() is not None:
        note(con, "controller", "INV-06", "crash", "", "", "sim_exit_boot")
        print("FAIL sim did not start")
        return 1

    engines, elogs, metas = [], [], []
    if args.dummy:
        for name, delay in (("dummy_fast", "0.05"), ("dummy_slow", "1.5"),
                            ("dummy_hang", "8")):
            lg = open(os.path.join(LAB_DIR, name + ".log"), "ab")
            p = subprocess.Popen(
                [sys.executable, "-c",
                 "import time,sys; time.sleep(float(sys.argv[1]))", delay],
                stdout=lg, stderr=lg,
            )
            engines.append(p)
            elogs.append(lg)
        crashy = subprocess.Popen(
            [sys.executable, "-c", "import sys; sys.exit(2)"],
        )
        engines.append(crashy)
    elif not args.no_engines:
        engines, elogs, metas = start_engines(total, args.accelerated)
    t0 = time.time()
    last_snap = snap_line()
    last_state = snap_field(last_snap, "state")
    prev_uk = snap_field(last_snap, "unsigned_kill")
    prev_bypass = False
    batches = 0
    ingested_n = 0
    crashes = 0
    freeze_restarts = 0
    freeze_hold = 0
    thru_path = os.path.join(args.export_dir, "throughput.log")
    stime_path = os.path.join(args.export_dir, "sim_time.log")
    cov_path = os.path.join(args.export_dir, "coverage.log")
    proof_path = os.path.join(args.export_dir, "proof_snaps.log")
    if args.accelerated:
        append_log(thru_path, "# real_s batches findings ingested pkts crashes freeze_restarts")
        append_log(stime_path, "# real_s sim_now factor ghost_mode pkts cfg")
        append_log(cov_path, "# libFuzzer / fuzz_smoke coverage lines")
        append_log(proof_path, "# proof-of-hack snapshots (sim-only)")
        sim_hours = (total * factor) / 3600.0
        append_log(stime_path, "# plan real_s=%s factor=%s sim_hours=%.1f (24h*200=4800)" %
                   (total, factor, sim_hours))

    while time.time() - t0 < total:
        time.sleep(1.0)
        batches += 1
        elapsed = time.time() - t0
        remain = max(1, int(total - elapsed))
        ingested = ingest_jsonl(con)
        ingested_n += ingested
        now_snap = snap_line()
        if last_snap and now_snap:
            verdict = run_inv(last_snap, now_snap)
            if verdict.startswith("FAIL"):
                note(con, "controller", verdict[5:].strip(), "invariant",
                     "", last_snap, now_snap)
                if args.accelerated:
                    append_log(proof_path, "real_s=%.0f kind=invariant %s\nbefore=%s\nafter=%s" %
                               (elapsed, verdict.strip(), last_snap.strip(), now_snap.strip()))
        now_state = snap_field(now_snap, "state")
        if args.accelerated and now_state and now_state != last_state:
            append_log(proof_path, "real_s=%.0f kind=state_transition %s -> %s snap=%s" %
                       (elapsed, last_state or "-", now_state, (now_snap or "").strip()))
        if args.accelerated and now_snap:
            uk = snap_field(now_snap, "unsigned_kill")
            if uk == "1" and prev_uk != "1":
                append_log(proof_path, "real_s=%.0f kind=kill_attempt snap=%s" %
                           (elapsed, now_snap.strip()))
            prev_uk = uk or prev_uk
            bypass = (snap_field(now_snap, "hmac_ok") == "0" and
                      snap_field(now_snap, "accepted") == "1")
            if bypass and not prev_bypass:
                append_log(proof_path, "real_s=%.0f kind=bypass_attempt snap=%s" %
                           (elapsed, now_snap.strip()))
            prev_bypass = bypass
        last_state = now_state or last_state
        last_snap = now_snap if now_snap else last_snap

        if sim.poll() is not None:
            crashes += 1
            note(con, "controller", "INV-06", "crash", "", last_snap or "",
                 "target_down")
            try:
                simlog.close()
            except Exception:
                pass
            kill_proc(sim)
            sim, simlog = start_sim()
            last_snap = snap_line()
            last_state = snap_field(last_snap, "state")
            prev_uk = snap_field(last_snap, "unsigned_kill")
        elif "frozen=1" in (now_snap or ""):
            # Real defense: dwell in Down so other engines hit a frozen hive,
            # then respawn. Expected Down is not an INV finding.
            freeze_hold += 1
            if freeze_hold >= 3:
                freeze_restarts += 1
                freeze_hold = 0
                kill_proc(sim)
                sim, simlog = start_sim()
                last_snap = snap_line()
                last_state = snap_field(last_snap, "state")
                prev_uk = snap_field(last_snap, "unsigned_kill")
        else:
            freeze_hold = 0
        if asan_hit():
            note(con, "controller", "INV-06", "asan", "", last_snap, now_snap)
        respawn_engines(engines, elogs, metas, remain, args.accelerated)

        if args.accelerated and (batches % 60 == 0):
            cfg = cfg_line()
            cfg_body = cfg.replace("CFG ", "")
            pkts = snap_field(cfg_body, "pkts") or "0"
            nowv = snap_field(cfg_body, "now") or snap_field(now_snap, "now") or "0"
            nfind = con.execute("SELECT COUNT(*) FROM findings").fetchone()[0]
            append_log(thru_path, "%.0f %s %s %s %s %s %s" %
                       (elapsed, batches, nfind, ingested_n, pkts, crashes, freeze_restarts))
            append_log(stime_path, "%.0f %s %s %s %s %s" %
                       (elapsed, nowv, factor, SIM_ENV.get("GHOST_LAB_GHOST_MODE", "0"),
                        pkts, cfg.strip()))
            cov = last_cov_line(os.path.join(LAB_DIR, "engine_a.log"))
            if cov:
                append_log(cov_path, "%.0f %s" % (elapsed, cov))
            export_findings(con, args.export_dir)
            eng_alive = sum(1 for p in engines if p.poll() is None)
            write_status(os.path.join(LAB_DIR, "PHASE_A_STATUS.txt"),
                         "elapsed_s=%.0f remain_s=%s batches=%s findings=%s "
                         "crashes=%s freeze_restarts=%s engines_alive=%s/%s "
                         "sim_now=%s factor=%s ghost_mode=%s\nlast_snap=%s\n" %
                         (elapsed, remain, batches, nfind, crashes, freeze_restarts,
                          eng_alive, len(engines), nowv, factor,
                          SIM_ENV.get("GHOST_LAB_GHOST_MODE", "0"),
                          (last_snap or "").strip()))

    for p in engines:
        kill_proc(p)
    kill_proc(sim)
    ingest_jsonl(con)
    n = con.execute("SELECT COUNT(*) FROM findings").fetchone()[0]
    exported, export_path = export_findings(con, args.export_dir)
    if args.accelerated:
        cfg = cfg_line()
        append_log(proof_path, "final snap=%s cfg=%s" %
                   ((last_snap or "").strip(), cfg.strip()))
        cov = last_cov_line(os.path.join(LAB_DIR, "engine_a.log"))
        if cov:
            append_log(cov_path, "final %s" % cov)
        write_status(os.path.join(LAB_DIR, "PHASE_A_STATUS.txt"),
                     "DONE elapsed_s=%.0f findings=%s crashes=%s freeze_restarts=%s\n" %
                     (time.time() - t0, n, crashes, freeze_restarts))
    con.close()
    for lg in elogs:
        lg.close()
    simlog.close()
    print("PASS lab_controller seconds=%s findings=%s batches=%s" %
          (total, n, batches))
    print("store=%s jsonl=%s export=%s rows=%s" %
          (LAB_DB, LAB_JSONL, export_path, exported))
    return 0


if __name__ == "__main__":
    sys.exit(main())
