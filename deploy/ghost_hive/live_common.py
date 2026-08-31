# Shared helpers for Ghost:Hive live orchestration (SPEC-v2, host-only).
from __future__ import print_function

import configparser
import os
import socket
import subprocess
import sys
import time

STICK_DIRNAME = "ghost_hive"
WSL_TMP = "/tmp/ghost_hive"
KERNEL_DEFAULT = "10.17.47.1"
GHOST_UDP = 17471

HERE = os.path.dirname(os.path.abspath(__file__))


def cfg_path():
    for p in (
        os.path.join(HERE, "ghost.cfg"),
        os.path.join(HERE, "stick", "ghost.cfg"),
    ):
        if os.path.isfile(p):
            return p
    return os.path.join(HERE, "ghost.cfg")


def load_cfg():
    cp = configparser.ConfigParser()
    cp.read(cfg_path())
    return cp


def kernel_ip(cp=None):
    if cp is None:
        cp = load_cfg()
    return cp.get("kernel", "ip", fallback=KERNEL_DEFAULT)


def on_linux():
    return os.name != "nt" and sys.platform.startswith("linux")


def wsl(cmd, check=True):
    if on_linux():
        return subprocess.run(["bash", "-lc", cmd], check=check)
    return subprocess.run(
        ["wsl", "-e", "bash", "-lc", cmd],
        check=check,
    )


def wsl_out(cmd):
    argv = ["bash", "-lc", cmd] if on_linux() else ["wsl", "-e", "bash", "-lc", cmd]
    p = subprocess.run(
        argv,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
    )
    return p.returncode, (p.stdout or "").strip()


def win_to_wsl(path):
    if path.startswith("/"):
        return path
    path = os.path.abspath(path)
    drive = path[0].lower()
    rest = path[2:].replace("\\", "/")
    return "/mnt/" + drive + rest


def find_stick():
    if os.name == "nt":
        for letter in "EFGHIJ":
            root = letter + ":\\" + STICK_DIRNAME
            if os.path.isdir(root):
                return root
        return None
    for letter in "efghij":
        root = "/mnt/" + letter + "/" + STICK_DIRNAME
        if os.path.isdir(root):
            return root
    return None


def stick_root():
    stick = find_stick()
    if stick is not None:
        return stick
    return os.path.join(HERE, "stick")


def bind_ok(root=None):
    if root is None:
        root = stick_root()
    p = os.path.join(root, "peer.bind")
    return os.path.isfile(p) and os.path.getsize(p) == 112


def detect_lan_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        if ip and not ip.startswith("127."):
            return ip
    except (OSError, socket.error):
        pass
    return "192.168.1.1"


def pid_running(name):
    rc, out = wsl_out("test -f %s/%s.pid && kill -0 $(cat %s/%s.pid) 2>/dev/null && echo ok" %
                      (WSL_TMP, name, WSL_TMP, name))
    return rc == 0 and out == "ok"


def start_daemon(name, cmd):
    wsl("mkdir -p %s/logs" % WSL_TMP)
    log = "%s/logs/%s.log" % (WSL_TMP, name)
    pidf = "%s/%s.pid" % (WSL_TMP, name)
    shell = ("nohup %s >>%s 2>&1 & echo $! >%s" % (cmd, log, pidf))
    wsl(shell)
    return pid_running(name)


def stop_daemon(name):
    wsl("test -f %s/%s.pid && kill $(cat %s/%s.pid) 2>/dev/null; rm -f %s/%s.pid" %
        (WSL_TMP, name, WSL_TMP, name, WSL_TMP, name), check=False)


def tail_log(name, lines=6):
    rc, out = wsl_out("tail -n %d %s/logs/%s.log 2>/dev/null" % (lines, WSL_TMP, name))
    if rc != 0:
        return ""
    return out


def manifest_path(stick=None):
    bases = []
    if stick is not None:
        bases.append(stick)
    bases.extend((HERE, os.path.join(HERE, "stick")))
    for base in bases:
        p = os.path.join(base, "devices.manifest")
        if os.path.isfile(p):
            return p
    return os.path.join(HERE, "devices.manifest")


def load_manifest_devices(stick=None):
    cp = configparser.ConfigParser()
    cp.read(manifest_path(stick))
    devices = []
    for section in cp.sections():
        if not section.startswith("device."):
            continue
        devices.append({
            "name": section[7:],
            "role": cp.get(section, "role", fallback=section[7:]),
            "id": cp.get(section, "id", fallback=""),
            "arch": cp.get(section, "arch", fallback="amd64"),
            "network": cp.get(section, "network", fallback="relay"),
            "host": cp.get(section, "host", fallback="local"),
            "wake": cp.get(section, "wake", fallback="no").lower() in ("yes", "1", "true"),
            "binary": cp.get(section, "binary", fallback=""),
            "args": cp.get(section, "args", fallback=""),
        })
    return devices


def _bin_dir_on_stick(stick):
    amd64 = os.path.join(stick, "bin", "amd64")
    flat = os.path.join(stick, "bin")
    if os.path.isdir(amd64) and os.path.isfile(os.path.join(amd64, "ghost_laptop")):
        return amd64
    return flat


def sync_bins_from_stick(stick=None):
    if stick is None:
        stick = stick_root()
    src_bin = _bin_dir_on_stick(stick)
    if not os.path.isdir(src_bin):
        return False
    laptop = os.path.join(src_bin, "ghost_laptop")
    relay = os.path.join(src_bin, "ghost_relay")
    if not os.path.isfile(laptop) or not os.path.isfile(relay):
        return False
    wsrc = win_to_wsl(src_bin)
    wsl("cp -f %s/ghost_* /tmp/ 2>/dev/null; chmod 700 /tmp/ghost_* 2>/dev/null" % wsrc,
        check=False)
    return True


def bind_ttl_sec(cp=None):
    if cp is None:
        cp = load_cfg()
    return cp.getint("gate", "bind_ttl_sec", fallback=900)


def write_bind_ttl_wsl(expiry_epoch=None):
    if expiry_epoch is None:
        expiry_epoch = int(time.time()) + bind_ttl_sec()
    wsl("printf '%d\\n' %d > %s/peer.bind.ttl && chmod 600 %s/peer.bind.ttl" %
        (expiry_epoch, WSL_TMP, WSL_TMP))


def sync_bind_from_stick(stick=None):
    if stick is None:
        stick = stick_root()
    src = os.path.join(stick, "peer.bind")
    if not bind_ok(stick):
        return False
    wsl("mkdir -p %s && cp -f %s %s/peer.bind && chmod 600 %s/peer.bind" %
        (WSL_TMP, win_to_wsl(src), WSL_TMP, WSL_TMP))
    rc, n = wsl_out("stat -c %%s %s/peer.bind" % WSL_TMP)
    if rc != 0 or n != "112":
        return False
    write_bind_ttl_wsl()
    return True
