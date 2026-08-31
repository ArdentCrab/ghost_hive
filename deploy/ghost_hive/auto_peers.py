#!/usr/bin/env python3
# Auto-start Hive peers (SPEC-v2). R2 local WSL + R3 remote via wake_lan.
from __future__ import print_function

import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import live_common as lc

PEERS_FALLBACK = (
    ("relay", "ghost_relay", "{kernel}", "local"),
    ("worker", "ghost_laptop", "{kernel} W", "local"),
    ("phone", "ghost_phone", "{lan} P", "local"),
    ("nas", "ghost_nas", "{lan} N", "local"),
    ("router", "ghost_router", "{lan} R", "local"),
    ("family", "ghost_family", "{lan} F", "local"),
    ("mines", "ghost_mines", "{lan} browser id=ML", "local"),
)


def peer_rows(stick):
    devices = lc.load_manifest_devices(stick)
    if not devices:
        return PEERS_FALLBACK
    rows = []
    for d in devices:
        rows.append((d["name"], d["binary"], d["args"], d["host"]))
    return rows


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
    rows = peer_rows(stick)
    local = [r for r in rows if r[3] == "local"]
    remote = [r for r in rows if r[3] == "remote"]
    print("auto_peers: kernel=%s lan=%s stick=%s local=%d remote=%d" %
          (kernel, lan_ip, stick, len(local), len(remote)))
    ok = 0
    for name, bin_name, fmt, host in local:
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
    for name, _, _, _ in remote:
        print("  %s: remote (ghost_wake on device)" % name)
    print("auto_peers: %d/%d local up" % (ok, len(local)))
    return 0 if ok == len(local) else 1


def stop_all():
    stick = lc.find_stick() or lc.stick_root()
    rows = peer_rows(stick)
    lc.stop_daemon("bind_serve")
    for name, _, _, host in reversed(rows):
        if host == "local":
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
    rows = peer_rows(stick or lc.stick_root())
    print("kernel=%s lan=%s stick=%s bind=%s" % (
        kernel, lan, stick or "none", lc.bind_ok(stick) if stick else lc.bind_ok()))
    for name, _, _, host in rows:
        if host == "remote":
            print("  %-8s remote" % name)
            continue
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
