#!/usr/bin/env python3
# keyd-Lite: serve 112-byte peer.bind to whitelisted device IDs (LAN UDP, no root).
from __future__ import print_function

import json
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


def load_whitelist(stick_root=None):
    ids = set()
    paths = []
    if stick_root:
        paths.append(os.path.join(stick_root, "root_config.json"))
    paths.append(os.path.join(HERE, "root_config.json"))
    paths.append(os.path.join(HERE, "stick", "root_config.json"))
    for p in paths:
        if not os.path.isfile(p):
            continue
        try:
            with open(p, "r") as f:
                doc = json.load(f)
            for m in doc.get("hive_members", []):
                mid = m.get("id", "")
                if mid:
                    ids.add(mid)
        except (IOError, ValueError):
            pass
    if not ids:
        ids = set(["W", "P", "R", "N", "F", "S", "X", "ML", "MP", "MN", "MR"])
    return ids


def handle_request(data, whitelist, bind_blob):
    try:
        text = data.decode("ascii", errors="ignore").strip()
    except AttributeError:
        text = ""
    if not text.startswith("GHST_BIND"):
        return b"DENY\n"
    parts = text.split()
    dev_id = ""
    for part in parts:
        if part.startswith("id="):
            dev_id = part[3:].strip()
    if dev_id == "" or dev_id not in whitelist:
        return b"DENY\n"
    return bind_blob


def serve(bind_port=None):
    cp = lc.load_cfg()
    if bind_port is None:
        bind_port = cp.getint("gate", "bind_port", fallback=BIND_PORT_DEFAULT)
    stick = lc.find_stick() or HERE
    whitelist = load_whitelist(stick)
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
    print("bind_serve: :%d whitelist=%d ids" % (bind_port, len(whitelist)))
    while True:
        data, addr = s.recvfrom(256)
        resp = handle_request(data, whitelist, bind_blob)
        if resp == bind_blob:
            s.sendto(resp, addr)
        else:
            s.sendto(resp, addr)


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
