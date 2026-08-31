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
ARM64_BINS = ("ghost_phone", "ghost_family")
R3_SCRIPTS = (
    "devices.manifest", "wake_lan.py", "bind_serve.py",
    "wake/ghost_wake.py", "wake/ghost_wake.sh",
)


def main():
    bin_flat = os.path.join(DEPLOY, "bin")
    bin_amd64 = os.path.join(DEPLOY, "bin", "amd64")
    bin_arm64 = os.path.join(DEPLOY, "bin", "arm64")
    for d in (bin_flat, bin_amd64, bin_arm64,
              os.path.join(DEPLOY, "live"),
              os.path.join(DEPLOY, "ui"),
              os.path.join(DEPLOY, "wake"),
              STICK):
        os.makedirs(d, exist_ok=True)

    for b in BINS:
        src = "/tmp/" + b
        if not os.path.isfile(src):
            print("pack_stick: fehlt", src, "— make peers zuerst")
            return 1
        shutil.copy2(src, os.path.join(bin_flat, b))
        shutil.copy2(src, os.path.join(bin_amd64, b))

    arm64_dir = "/tmp/arm64"
    for b in ARM64_BINS:
        src = os.path.join(arm64_dir, b)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(bin_arm64, b))
            print("arm64:", b)
    if not any(os.path.isfile(os.path.join(bin_arm64, b)) for b in ARM64_BINS):
        print("Hinweis: make peers-arm64 fuer Termux arm64-Binaries")

    shutil.copy2(os.path.join(REPO, "scripts", "hive_gate.py"),
                 os.path.join(DEPLOY, "hive_gate.py"))

    for rel in R3_SCRIPTS:
        src = os.path.join(DEPLOY, rel)
        if os.path.isfile(src):
            dst = os.path.join(STICK, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)

    cfg = os.path.join(DEPLOY, "ghost.cfg")
    if os.path.isfile(cfg):
        shutil.copy2(cfg, os.path.join(STICK, "ghost.cfg"))

    manifest = os.path.join(DEPLOY, "devices.manifest")
    if os.path.isfile(manifest):
        shutil.copy2(manifest, os.path.join(STICK, "devices.manifest"))

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
    print("R3: ghost_wake auf Geraeten; wake_lan + bind_serve beim Gate")
    return 0


if __name__ == "__main__":
    sys.exit(main())
