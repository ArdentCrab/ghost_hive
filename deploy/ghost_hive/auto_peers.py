#!/usr/bin/env python3
# Auto-start all Hive peers (SPEC-v2). Host-only — no PSP module.
from __future__ import print_function

import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import live_common as lc


PEERS = (
    ("relay", "ghost_relay", "{kernel}"),
    ("worker", "ghost_laptop", "{kernel} W"),
    ("phone", "ghost_phone", "{lan} P"),
    ("nas", "ghost_nas", "{lan} N"),
    ("router", "ghost_router", "{lan} R"),
    ("family", "ghost_family", "{lan} F"),
    ("mines", "ghost_mines", "{lan} browser id=ML"),
)


def require_ready(stick):
    if not lc.sync_bins_from_stick(stick):
        print("auto_peers: bin/ghost_laptop oder ghost_relay fehlt. make live")
        return False
    if not lc.sync_bind_from_stick(stick):
        print("auto_peers: peer.bind fehlt oder != 112 Byte auf Stick")
        return False
    return True


def start_all(lan_ip=None):
    stick = lc.find_stick() or lc.stick_root()
    if not require_ready(stick):
        return 1
    cp = lc.load_cfg()
    kernel = lc.kernel_ip(cp)
    if lan_ip is None or lan_ip == "":
        lan_ip = lc.detect_lan_ip()
    print("auto_peers: kernel=%s lan=%s stick=%s" % (kernel, lan_ip, stick))
    ok = 0
    for name, bin_name, fmt in PEERS:
        if lc.pid_running(name):
            print("  %s: already running" % name)
            ok += 1
            continue
        args = fmt.format(kernel=kernel, lan=lan_ip)
        cmd = "/tmp/%s %s" % (bin_name, args)
        if lc.start_daemon(name, cmd):
            print("  %s: started" % name)
            ok += 1
        else:
            print("  %s: FAILED" % name)
    print("auto_peers: %d/%d up" % (ok, len(PEERS)))
    return 0 if ok == len(PEERS) else 1


def stop_all():
    for name, _, _ in reversed(PEERS):
        lc.stop_daemon(name)
    lc.wsl("pkill -f '/tmp/ghost_' 2>/dev/null || true", check=False)
    lc.wsl("rm -f %s/peer.bind" % lc.WSL_TMP, check=False)
    print("auto_peers: stopped")
    return 0


def status():
    cp = lc.load_cfg()
    kernel = lc.kernel_ip(cp)
    lan = lc.detect_lan_ip()
    stick = lc.find_stick()
    print("kernel=%s lan=%s stick=%s bind=%s" % (
        kernel, lan, stick or "none", lc.bind_ok(stick) if stick else lc.bind_ok()))
    for name, _, _ in PEERS:
        mark = "UP" if lc.pid_running(name) else "down"
        print("  %-8s %s" % (name, mark))
    return 0


def main():
    p = argparse.ArgumentParser(description="Ghost:Hive auto peers")
    p.add_argument("cmd", nargs="?", default="start",
                   choices=["start", "stop", "status", "restart"])
    p.add_argument("--lan", default="", help="Laptop LAN IP (auto-detect if omitted)")
    args = p.parse_args()
    if args.cmd == "stop":
        return stop_all()
    if args.cmd == "status":
        return status()
    if args.cmd == "restart":
        stop_all()
        return start_all(args.lan)
    return start_all(args.lan)


if __name__ == "__main__":
    sys.exit(main())
