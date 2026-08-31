#!/usr/bin/env python3
# keyd-Lite: 112-byte peer.bind for whitelisted device IDs (LAN UDP, gate window only).
from __future__ import print_function

import os
import socket
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import live_common as lc

BIND_PORT_DEFAULT = 17470
BIND_PATH = lc.WSL_TMP + "/peer.bind"
RATE_MAX_PER_MIN = 5


def log_deny(reason, addr, dev_id=""):
    ip = addr[0] if addr else "?"
    if dev_id:
        sys.stderr.write("bind_serve: DENY %s id=%s ip=%s\n" % (reason, dev_id, ip))
    else:
        sys.stderr.write("bind_serve: DENY %s ip=%s\n" % (reason, ip))
    sys.stderr.flush()


def load_whitelist(stick_root=None):
    """IDs from devices.manifest only. No default fallback."""
    if stick_root is None:
        stick_root = lc.find_stick() or HERE
    manifest = lc.manifest_path(stick_root)
    if not os.path.isfile(manifest):
        return None
    devices = lc.load_manifest_devices(stick_root)
    if not devices:
        return None
    ids = set()
    for d in devices:
        dev_id = (d.get("id") or "").strip()
        if dev_id:
            ids.add(dev_id)
    if not ids:
        return None
    return ids


class RateLimit(object):
    def __init__(self, max_per_min):
        self.max_per_min = max_per_min
        self.hits = {}

    def allow(self, ip):
        now = time.time()
        window = self.hits.setdefault(ip, [])
        window[:] = [t for t in window if now - t < 60.0]
        if len(window) >= self.max_per_min:
            return False
        window.append(now)
        return True


def handle_request(data, whitelist, bind_blob, addr, rate):
    ip = addr[0]
    if not rate.allow(ip):
        log_deny("rate", addr)
        return b"DENY\n"
    try:
        text = data.decode("ascii", errors="ignore").strip()
    except AttributeError:
        text = ""
    if not text.startswith("GHST_BIND"):
        log_deny("proto", addr)
        return b"DENY\n"
    dev_id = ""
    for part in text.split():
        if part.startswith("id="):
            dev_id = part[3:].strip()
    if dev_id == "" or dev_id not in whitelist:
        log_deny("whitelist", addr, dev_id)
        return b"DENY\n"
    return bind_blob


def serve(bind_port=None):
    cp = lc.load_cfg()
    if bind_port is None:
        bind_port = cp.getint("gate", "bind_port", fallback=BIND_PORT_DEFAULT)
    window_sec = cp.getint("gate", "bind_ttl_sec", fallback=900)
    stick = lc.find_stick() or HERE
    manifest = lc.manifest_path(stick)
    if not os.path.isfile(manifest):
        print("bind_serve: devices.manifest required at", manifest)
        return 1
    whitelist = load_whitelist(stick)
    if not whitelist:
        print("bind_serve: no device ids in devices.manifest")
        return 1
    if not os.path.isfile(BIND_PATH) or os.path.getsize(BIND_PATH) != 112:
        print("bind_serve: no peer.bind at", BIND_PATH)
        return 1
    with open(BIND_PATH, "rb") as f:
        bind_blob = f.read(112)
    if len(bind_blob) != 112:
        return 1
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", bind_port))
    s.settimeout(1.0)
    rate = RateLimit(RATE_MAX_PER_MIN)
    deadline = time.time() + window_sec
    print("bind_serve: :%d whitelist=%d window=%ds" %
          (bind_port, len(whitelist), window_sec))
    while time.time() < deadline:
        try:
            data, addr = s.recvfrom(256)
        except socket.timeout:
            continue
        resp = handle_request(data, whitelist, bind_blob, addr, rate)
        s.sendto(resp, addr)
    s.close()
    print("bind_serve: window closed after %ds" % window_sec)
    return 0


def main():
    port = BIND_PORT_DEFAULT
    if len(sys.argv) > 1:
        port = int(sys.argv[1])
    try:
        return serve(port)
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    sys.exit(main())
