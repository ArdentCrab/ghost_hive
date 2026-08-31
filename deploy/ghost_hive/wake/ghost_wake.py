#!/usr/bin/env python3
# Device-side wake listener (Termux / Linux). SPEC-v2 host peer only.
from __future__ import print_function

import os
import socket
import subprocess
import sys
import time

WAKE_PORT = 17469
BIND_PORT = 17470
BIND_DIR = os.environ.get("GHOST_BIND_DIR", "/tmp/ghost_hive")
BIND_PATH = os.path.join(BIND_DIR, "peer.bind")
WIRE_UDP = 17471


def parse_wake(msg):
    out = {}
    if not msg.startswith("GHST_WAKE"):
        return out
    for part in msg.split():
        if "=" in part:
            k, v = part.split("=", 1)
            out[k] = v
    return out


def fetch_bind(lan_ip, dev_id, bind_port=BIND_PORT):
    req = ("GHST_BIND id=%s\n" % dev_id).encode("ascii")
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(3.0)
    try:
        s.sendto(req, (lan_ip, bind_port))
        data, _ = s.recvfrom(256)
        if len(data) == 112:
            return data
    except (socket.error, socket.timeout):
        pass
    finally:
        s.close()
    return None


def write_bind(blob, epoch=None):
    os.makedirs(BIND_DIR, mode=0o700, exist_ok=True)
    with open(BIND_PATH, "wb") as f:
        f.write(blob)
    os.chmod(BIND_PATH, 0o600)
    ttl_path = BIND_PATH + ".ttl"
    if epoch is not None and int(epoch) > 0:
        exp = int(epoch)
    else:
        exp = int(time.time()) + 900
    with open(ttl_path, "w") as f:
        f.write("%d\n" % exp)
    os.chmod(ttl_path, 0o600)


def start_peer(binary, lan_ip, dev_id, extra_args=""):
    if not os.path.isfile(binary):
        print("ghost_wake: missing binary", binary)
        return 1
    cmd = [binary, lan_ip, dev_id]
    if extra_args:
        cmd.extend(extra_args.split())
    log = os.path.join(BIND_DIR, "logs", dev_id + ".log")
    os.makedirs(os.path.dirname(log), exist_ok=True)
    with open(log, "a") as lf:
        subprocess.Popen(cmd, stdout=lf, stderr=lf, close_fds=True)
    print("ghost_wake: started", binary, lan_ip, dev_id)
    return 0


def listen(dev_id, binary, extra_args=""):
    if dev_id == "" or binary == "":
        print("usage: ghost_wake.py <device_id> <binary> [extra args]")
        return 2
    print("ghost_wake: listening :%d id=%s" % (WAKE_PORT, dev_id))
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", WAKE_PORT))
    while True:
        data, _ = s.recvfrom(512)
        try:
            text = data.decode("ascii", errors="ignore").strip()
        except AttributeError:
            continue
        fields = parse_wake(text)
        if "lan" not in fields:
            continue
        lan = fields["lan"]
        bport = int(fields.get("bind_port", BIND_PORT))
        epoch = int(fields.get("epoch", "0") or "0")
        blob = fetch_bind(lan, dev_id, bport)
        if blob is None:
            print("ghost_wake: bind denied or timeout")
            continue
        write_bind(blob, epoch)
        start_peer(binary, lan, dev_id, extra_args)


def main():
    if len(sys.argv) < 3:
        print("ghost_wake.py <id> <binary> [extra args]")
        return 2
    dev_id = sys.argv[1]
    binary = sys.argv[2]
    extra = " ".join(sys.argv[3:]) if len(sys.argv) > 3 else ""
    try:
        return listen(dev_id, binary, extra)
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    sys.exit(main())
