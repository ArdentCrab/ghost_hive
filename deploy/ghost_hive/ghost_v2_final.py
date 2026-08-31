#!/usr/bin/env python3
# GHOST v2 Final — Whitelist ingest + peer.bind + live start (Windows/WSL).
from __future__ import print_function

import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import live_common as lc


def psp_root():
    if os.name != "nt":
        return None
    for d in "DEFGHIJ":
        root = d + ":\\PSP"
        if os.path.isdir(root):
            return d + ":\\"
    return None


def step_whitelist():
    wl = os.path.join(HERE, "root_config.json")
    if not os.path.isfile(wl):
        wl = os.path.join(HERE, "stick", "root_config.json")
    if not os.path.isfile(wl):
        print("root_config.json fehlt in", HERE)
        return 1
    root = psp_root()
    if root is None:
        print("PSP nicht per USB. XMB -> USB, dann erneut.")
        return 2
    dst_dir = os.path.join(root, "ghost_hive")
    os.makedirs(dst_dir, exist_ok=True)
    dst = os.path.join(dst_dir, "root_config.json")
    shutil.copy2(wl, dst)
    print("Whitelist ->", dst)
    print("Auswerfen. FastRecovery -> GHOST (ingest, Datei wird geloescht).")
    return 0


def step_bind():
    fetch = os.path.join(HERE, "fetch_bind.py")
    if not os.path.isfile(fetch):
        fetch = os.path.join(HERE, "stick", "fetch_bind.py")
    if os.path.isfile(fetch):
        subprocess.run([sys.executable, fetch], check=False)
    stick = lc.find_stick() or lc.stick_root()
    src = os.path.join(stick, "peer.bind")
    if not os.path.isfile(src) or os.path.getsize(src) != 112:
        print("peer.bind 112B fehlt auf Stick:", src)
        return 1
    if not lc.sync_bind_from_stick(stick):
        return 1
    print("peer.bind -> WSL", lc.WSL_TMP)
    return 0


def step_start(lan_ip=""):
    gate = os.path.join(HERE, "hive_gate.py")
    if os.path.isfile(gate):
        subprocess.run([sys.executable, gate, "once"], check=False)
    ap = os.path.join(HERE, "auto_peers.py")
    if os.path.isfile(ap):
        cmd = [sys.executable, ap, "start"]
        if lan_ip:
            cmd.extend(["--lan", lan_ip])
        return subprocess.call(cmd)
    print("auto_peers.py fehlt")
    return 1


def main():
    p = argparse.ArgumentParser()
    p.add_argument("step", nargs="?", default="auto",
                   choices=["auto", "whitelist", "bind", "start", "live"])
    p.add_argument("--lan", default="")
    args = p.parse_args()
    print("=== Ghost:Hive v2 FINAL ===")
    if args.step == "whitelist":
        return step_whitelist()
    if args.step == "bind":
        return step_bind()
    if args.step == "start":
        return step_start(args.lan)
    if args.step == "live":
        rc = step_bind()
        if rc != 0:
            return rc
        return step_start(args.lan)
    rc = step_whitelist()
    if rc == 2:
        return 2
    if rc != 0:
        return rc
    print("Naechster:  python ghost_v2_final.py bind")
    print("Dann:       python ghost_v2_final.py live --lan <laptop-ip>")
    return 0


if __name__ == "__main__":
    sys.exit(main())
