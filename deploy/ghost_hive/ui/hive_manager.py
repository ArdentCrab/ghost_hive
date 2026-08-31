#!/usr/bin/env python3
# Hive Manager — live status dashboard (host-only, SPEC-v2).
from __future__ import print_function

import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
if ROOT not in sys.path:
    sys.path.insert(0, ROOT)

import live_common as lc

PEER_NAMES = ("relay", "worker", "phone", "nas", "router", "family", "mines")


def clear():
    if os.name == "nt":
        os.system("cls")
    else:
        sys.stdout.write("\033[2J\033[H")
        sys.stdout.flush()


def draw():
    cp = lc.load_cfg()
    hz = cp.getfloat("manager", "refresh_hz", fallback=20.0)
    kernel = lc.kernel_ip(cp)
    lan = lc.detect_lan_ip()
    stick = lc.find_stick()
    bind = lc.bind_ok(stick) if stick else lc.bind_ok(lc.stick_root())

    lines = []
    lines.append("ghost:hive  LIVE MANAGER  [GHv2]  SPEC-v2")
    lines.append("-" * 56)
    lines.append("Kernel  %s:%d  IBSS %s" % (
        kernel, lc.GHOST_UDP, cp.get("kernel", "ssid", fallback="GHSTHIVE")))
    lines.append("LAN     %s  Stick  %s  bind  %s" % (
        lan, stick or "(local stick/)", "OK 112B" if bind else "MISSING"))
    lines.append("-" * 56)
    lines.append("Peers:")
    up = 0
    for name in PEER_NAMES:
        run = lc.pid_running(name)
        if run:
            up += 1
        lines.append("  %-8s %s" % (name, "RUN" if run else "...."))
    lines.append("  (%d/%d up)" % (up, len(PEER_NAMES)))
    lines.append("-" * 56)
    lines.append("Logs (worker):")
    tail = lc.tail_log("worker", 4)
    if tail:
        for ln in tail.splitlines():
            lines.append("  " + ln[:52])
    else:
        lines.append("  (no log yet)")
    lines.append("-" * 56)
    lines.append("Ctrl+C exit | hive_live.cmd = full start | make live = build stick")
    return lines, hz


def main():
    try:
        while True:
            clear()
            lines, hz = draw()
            for ln in lines:
                print(ln)
            sys.stdout.flush()
            time.sleep(1.0 / hz if hz > 0 else 0.05)
    except KeyboardInterrupt:
        print("\nmanager exit")
        return 0


if __name__ == "__main__":
    sys.exit(main())
