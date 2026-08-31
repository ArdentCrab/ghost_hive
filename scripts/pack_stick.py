#!/usr/bin/env python3
# Assemble deploy/ghost_hive stick folder (called from Makefile).
from __future__ import print_function

import os
import shutil
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DEPLOY = os.path.join(REPO, "deploy", "ghost_hive")
STICK = os.path.join(DEPLOY, "stick")
PSP_EBOOT = os.path.join(REPO, "src", "psp", "EBOOT.PBP")
BIND_CANDIDATES = [
    os.path.join(os.environ.get("HOME", ""), "Documents", "ghost_hive_peers", "k", "peer.bind"),
    "/mnt/c/Users/noahp/Documents/ghost_hive_peers/k/peer.bind",
]

BINS = (
    "ghost_laptop", "ghost_relay", "ghost_phone", "ghost_nas",
    "ghost_router", "ghost_family", "ghost_mines",
)


def main():
    os.makedirs(os.path.join(DEPLOY, "bin"), exist_ok=True)
    os.makedirs(os.path.join(DEPLOY, "live"), exist_ok=True)
    os.makedirs(os.path.join(DEPLOY, "ui"), exist_ok=True)
    os.makedirs(STICK, exist_ok=True)

    for b in BINS:
        src = "/tmp/" + b
        if not os.path.isfile(src):
            print("pack_stick: fehlt", src, "— make peers zuerst")
            return 1
        shutil.copy2(src, os.path.join(DEPLOY, "bin", b))

    shutil.copy2(os.path.join(REPO, "scripts", "hive_gate.py"),
                 os.path.join(DEPLOY, "hive_gate.py"))

    cfg = os.path.join(DEPLOY, "ghost.cfg")
    if os.path.isfile(cfg):
        shutil.copy2(cfg, os.path.join(STICK, "ghost.cfg"))

    if os.path.isfile(PSP_EBOOT):
        shutil.copy2(PSP_EBOOT, os.path.join(DEPLOY, "live", "EBOOT.PBP"))
        shutil.copy2(PSP_EBOOT, os.path.join(STICK, "EBOOT.PBP"))
    else:
        print("Hinweis: make eboot fuer EBOOT.PBP")

    for bind in BIND_CANDIDATES:
        if os.path.isfile(bind) and os.path.getsize(bind) == 112:
            shutil.copy2(bind, os.path.join(STICK, "peer.bind"))
            shutil.copy2(bind, os.path.join(DEPLOY, "peer.bind"))
            print("peer.bind -> stick/ (112 B, nicht fuer git)")
            break
    else:
        print("peer.bind nicht gefunden — nach PSP-Ingest fetch_bind.py")

    print("ghost:hive stick ready ->", DEPLOY)
    print("USB: gesamten Ordner ghost_hive nach E:\\ghost_hive kopieren")
    print("Start: hive_live.cmd  oder  python ghost_v2_final.py live")
    return 0


if __name__ == "__main__":
    sys.exit(main())
