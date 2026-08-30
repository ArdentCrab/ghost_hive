#!/usr/bin/env python3
"""INV-07 per-frame latch. Isolated UDP port — does not touch a live /tmp/ghost_lab run."""

from __future__ import print_function

import os
import socket
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ISO = "/tmp/ghost_inv07"
BIN = os.path.join(ISO, "bin")
SOCK = os.path.join(ISO, "sim.sock")
READY = os.path.join(ISO, "ready")
PORT = 17472
CORPUS = os.path.join(ROOT, "engines", "a_fuzzer", "corpus")
FAILS = []


def rec(ok, name, detail=""):
    line = "%s  %s" % ("PASS" if ok else "FAIL", name)
    if detail:
        line += " — " + detail[:200]
    print(line)
    if not ok:
        FAILS.append(name)
    return ok


def env():
    e = os.environ.copy()
    e["GHOST_LAB_DIR"] = ISO
    e["GHOST_LAB_PORT"] = str(PORT)
    e["GHOST_LAB_SOCK"] = SOCK
    e["GHOST_LAB_READY"] = READY
    e["GHOST_DOWN_ARMED"] = "1"
    e["GHOST_LAB_GHOST_MODE"] = "1"
    e["ASAN_OPTIONS"] = "abort_on_error=1:halt_on_error=1"
    return e


def snap():
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(1.0)
    try:
        s.connect(SOCK)
        s.sendall(b"SNAP\n")
        return s.recv(480).decode("ascii", "replace")
    except Exception:
        return ""
    finally:
        s.close()


def send(path):
    data = open(path, "rb").read()
    u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    u.sendto(data, ("127.0.0.1", PORT))
    u.close()


def wait_ready():
    for _ in range(50):
        if os.path.isfile(READY):
            return True
        time.sleep(0.05)
    return False


def kv(line, key):
    token = key + "="
    i = line.find(token)
    if i < 0:
        return ""
    v = line[i + len(token):]
    return v.split()[0] if v else ""


def main():
    os.makedirs(BIN, exist_ok=True)
    for p in (SOCK, READY):
        try:
            os.unlink(p)
        except OSError:
            pass

    sim_bin = os.path.join(BIN, "ghost-sim")
    rec(os.path.isfile(sim_bin), "isolated ghost-sim built")
    uk = os.path.join(CORPUS, "kill_unsigned.bin")
    mine = os.path.join(CORPUS, "mine.bin")
    rec(os.path.isfile(uk), "kill_unsigned.bin")
    rec(os.path.isfile(mine), "mine.bin")

    logf = open(os.path.join(ISO, "sim.log"), "wb")
    sim = subprocess.Popen([sim_bin], stdout=logf, stderr=logf, env=env(), cwd=ROOT)
    if not wait_ready():
        rec(False, "isolated sim ready")
        sim.kill()
        logf.close()
        return 1

    send(uk)
    time.sleep(0.2)
    s1 = snap()
    rec(kv(s1, "unsigned_kill") == "1", "A unsigned_kill=1", s1)
    rec(kv(s1, "hmac_ok") == "0", "A hmac_ok=0", s1)
    rec(kv(s1, "accepted") == "0", "A accepted=0", s1)
    rec(kv(s1, "frozen") == "0", "A frozen=0 (isolated HMAC-I)", s1)
    rec(kv(s1, "uk_froze") == "0", "A uk_froze=0", s1)

    # Real Down: same mine counter twice → replay_guard (§15) → enterHiveDown.
    send(mine)
    time.sleep(0.2)
    send(mine)
    time.sleep(0.25)
    s2 = snap()
    rec(kv(s2, "frozen") == "0", "B mine replay no auto-down", s2)
    rec(kv(s2, "uk_froze") == "0", "B real freeze is not uk_froze", s2)

    send(uk)
    time.sleep(0.2)
    s3 = snap()
    rec(kv(s3, "frozen") == "0", "C still not frozen", s3)
    rec(kv(s3, "unsigned_kill") == "1", "C last frame unsigned kill", s3)
    rec(kv(s3, "uk_froze") == "0", "C smear: uk last ≠ uk caused freeze", s3)

    sim.terminate()
    try:
        sim.wait(timeout=2)
    except Exception:
        sim.kill()
    logf.close()

    if FAILS:
        print("FAIL test_inv07_frame")
        return 1
    print("PASS test_inv07_frame")
    return 0


if __name__ == "__main__":
    sys.exit(main())
