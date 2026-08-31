#!/usr/bin/env python3
# LAN wake broadcast when Hive gate opens (SPEC-v2 host-only).
from __future__ import print_function

import argparse
import os
import socket
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import live_common as lc

WAKE_PORT_DEFAULT = 17469


def send_wake(lan_ip=None, count=3):
    cp = lc.load_cfg()
    wake_port = cp.getint("gate", "wake_port", fallback=WAKE_PORT_DEFAULT)
    bind_port = cp.getint("gate", "bind_port", fallback=17470)
    kernel = lc.kernel_ip(cp)
    if lan_ip is None or lan_ip == "":
        lan_ip = lc.detect_lan_ip()
    epoch = int(time.time()) + cp.getint("gate", "bind_ttl_sec", fallback=900)
    msg = ("GHST_WAKE v2 kernel=%s lan=%s relay_port=%d bind_port=%d epoch=%d\n" %
           (kernel, lan_ip, lc.GHOST_UDP, bind_port, epoch))
    data = msg.encode("ascii")
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    try:
        for _ in range(count):
            s.sendto(data, ("<broadcast>", wake_port))
            s.sendto(data, ("255.255.255.255", wake_port))
    finally:
        s.close()
    print("wake_lan: sent to :%d  lan=%s kernel=%s" % (wake_port, lan_ip, kernel))
    return 0


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--lan", default="")
    p.add_argument("--count", type=int, default=3)
    args = p.parse_args()
    return send_wake(args.lan, args.count)


if __name__ == "__main__":
    sys.exit(main())
