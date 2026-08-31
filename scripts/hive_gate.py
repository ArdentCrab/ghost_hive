# Host-only: Hive (Worker+Relay+LAN-Peers) laeuft nur mit USB-Stick.
# PSP Watch bleibt EBOOT-Default. Ghost Down bleibt PSP (Select+Start).
# Binaries sind Linux-ELF: Start immer via WSL, nie Windows-Popen.
# Kein root.key auf dem Stick. Bind nur 112 Byte -> /tmp/ghost_hive/peer.bind 0600.

from __future__ import print_function

import configparser
import os
import shutil
import subprocess
import sys
import time

KERNEL = "10.17.47.1"
WSL_TMP = "/tmp/ghost_hive"
STICK_DIRNAME = "ghost_hive"
BINARIES = (
    "ghost_laptop",
    "ghost_relay",
    "ghost_phone",
    "ghost_nas",
    "ghost_router",
    "ghost_family",
    "ghost_mines",
)
HIVE_PROCS = BINARIES


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


def bind_ok(stick):
    p = os.path.join(stick, "peer.bind")
    return os.path.isfile(p) and os.path.getsize(p) == 112


def gate_ttl_sec(stick):
    cfg = os.path.join(stick, "ghost.cfg")
    if os.path.isfile(cfg):
        cp = configparser.ConfigParser()
        cp.read(cfg)
        return cp.getint("gate", "bind_ttl_sec", fallback=900)
    return 900


def sync_bind_ttl(stick):
    expiry = int(time.time()) + gate_ttl_sec(stick)
    wsl("printf '%d\\n' %d > %s/peer.bind.ttl && chmod 600 %s/peer.bind.ttl" %
        (expiry, WSL_TMP, WSL_TMP))


def sync_bind(stick):
    src = win_to_wsl(os.path.join(stick, "peer.bind"))
    wsl("mkdir -p %s && cp -f %s %s/peer.bind && chmod 600 %s/peer.bind" %
        (WSL_TMP, src, WSL_TMP, WSL_TMP))
    rc, n = wsl_out("stat -c %%s %s/peer.bind" % WSL_TMP)
    if rc != 0 or n != "112":
        print("peer.bind Sync fehlgeschlagen:", n)
        return False
    sync_bind_ttl(stick)
    return True


def wipe_bind():
    wsl("rm -f %s/peer.bind %s/peer.bind.ttl %s/*.pid" % (WSL_TMP, WSL_TMP, WSL_TMP), check=False)


def manifest_ok(stick):
    if os.path.isfile(os.path.join(stick, "devices.manifest")):
        return True
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.isfile(os.path.join(here, "devices.manifest"))


def bin_dir_on_stick(stick):
    amd64 = os.path.join(stick, "bin", "amd64")
    flat = os.path.join(stick, "bin")
    if os.path.isdir(amd64) and os.path.isfile(os.path.join(amd64, "ghost_laptop")):
        return amd64
    return flat


def sync_bins(stick):
    src_bin = bin_dir_on_stick(stick)
    if not os.path.isdir(src_bin):
        return False
    laptop = os.path.join(src_bin, "ghost_laptop")
    relay = os.path.join(src_bin, "ghost_relay")
    if not os.path.isfile(laptop) or not os.path.isfile(relay):
        return False
    wsrc = win_to_wsl(src_bin)
    wsl("cp -f %s/ghost_laptop %s/ghost_relay /tmp/ && chmod 700 /tmp/ghost_laptop /tmp/ghost_relay" %
        (wsrc, wsrc))
    for name in BINARIES:
        wp = os.path.join(src_bin, name)
        if os.path.isfile(wp) and name not in ("ghost_laptop", "ghost_relay"):
            wsl("cp -f %s/%s /tmp/%s && chmod 700 /tmp/%s" %
                (wsrc, name, name, name), check=False)
    return True


def hive_running():
    rc, _ = wsl_out("pgrep -f '/tmp/ghost_laptop |/tmp/ghost_relay ' >/dev/null")
    return rc == 0


def start_hive():
    wsl("mkdir -p %s" % WSL_TMP)
    wsl("nohup /tmp/ghost_laptop %s W >>%s/worker.log 2>&1 & echo $! >%s/worker.pid" %
        (KERNEL, WSL_TMP, WSL_TMP))
    wsl("nohup /tmp/ghost_relay %s >>%s/relay.log 2>&1 & echo $! >%s/relay.pid" %
        (KERNEL, WSL_TMP, WSL_TMP))
    print("Hive an: Worker + Relay (Kernel %s)." % KERNEL)


def r3_script(stick, name):
    here = os.path.dirname(os.path.abspath(__file__))
    for base in (stick, here):
        p = os.path.join(base, name)
        if os.path.isfile(p):
            return win_to_wsl(p)
    return None


def stop_r3():
    wsl("test -f %s/bind_serve.pid && kill $(cat %s/bind_serve.pid) 2>/dev/null; "
        "rm -f %s/bind_serve.pid" % (WSL_TMP, WSL_TMP, WSL_TMP), check=False)


def start_r3(stick):
    bs = r3_script(stick, "bind_serve.py")
    wl = r3_script(stick, "wake_lan.py")
    if bs is None and wl is None:
        return
    if bs and not manifest_ok(stick):
        print("R3: bind_serve skipped (devices.manifest fehlt)")
        bs = None
    wsl("mkdir -p %s/logs" % WSL_TMP)
    rc, out = wsl_out("test -f %s/bind_serve.pid && kill -0 $(cat %s/bind_serve.pid) 2>/dev/null && echo ok" %
                      (WSL_TMP, WSL_TMP))
    if bs and rc != 0:
        wsl("nohup python3 %s >>%s/logs/bind_serve.log 2>&1 & echo $! >%s/bind_serve.pid" %
            (bs, WSL_TMP, WSL_TMP))
        print("R3: bind_serve gestartet (Fenster %ds)." % gate_ttl_sec(stick))
    if wl:
        wsl("python3 %s" % wl)
        print("R3: wake_lan gesendet.")


def stop_hive():
    names = " ".join("/tmp/" + n for n in HIVE_PROCS)
    wsl("pkill -f '/tmp/ghost_(laptop|relay|phone|nas|router|family|mines)' || true",
        check=False)
    stop_r3()
    wipe_bind()
    print("Hive aus: Prozesse tot, Bind aus RAM-Pfad.")


def pack_stick(stick):
    dst_bin = os.path.join(stick, "bin")
    os.makedirs(dst_bin, exist_ok=True)
    rc, _ = wsl_out("test -x /tmp/ghost_laptop && test -x /tmp/ghost_relay")
    if rc != 0:
        print("Zuerst in WSL: make peers")
        return 1
    wsl("cp -f /tmp/ghost_laptop /tmp/ghost_relay /tmp/ghost_phone /tmp/ghost_nas /tmp/ghost_router /tmp/ghost_family /tmp/ghost_mines %s/" %
        win_to_wsl(dst_bin), check=False)
    bind_src = os.path.join(
        os.environ.get("USERPROFILE", r"C:\Users\noahp"),
        "Documents", "ghost_hive_peers", "k", "peer.bind")
    if os.path.isfile(bind_src) and os.path.getsize(bind_src) == 112:
        shutil.copy2(bind_src, os.path.join(stick, "peer.bind"))
    else:
        wsl("test -s %s/peer.bind && cp -f %s/peer.bind %s/peer.bind" %
            (WSL_TMP, WSL_TMP, win_to_wsl(stick)), check=False)
    here = os.path.abspath(__file__)
    try:
        shutil.copy2(here, os.path.join(stick, "hive_gate.py"))
    except OSError:
        pass
    print("Pack ->", stick)
    return 0


def once():
    stick = find_stick()
    if stick is None:
        print("Stick nicht da (kein ?:\\ghost_hive). Laptop ohne Hive.")
        if hive_running():
            stop_hive()
        return 0
    print("Stick:", stick)
    if not bind_ok(stick):
        print("peer.bind fehlt oder nicht 112 Byte. Hive bleibt aus.")
        if hive_running():
            stop_hive()
        return 1
    if not sync_bins(stick):
        print("bin/ghost_laptop oder bin/ghost_relay fehlt. hive_gate.py pack")
        return 1
    if not sync_bind(stick):
        return 1
    if not hive_running():
        start_hive()
    else:
        print("Hive laeuft bereits.")
    start_r3(stick)
    return 0


def watch():
    print("Gate wartet auf Stick (Strg+C = Watcher aus, Hive bleibt wie zuletzt).")
    last = None
    while True:
        stick = find_stick()
        present = stick is not None and bind_ok(stick)
        if present != last:
            last = present
            once()
        time.sleep(2)


def main():
    cmd = "watch"
    if len(sys.argv) > 1:
        cmd = sys.argv[1]
    print("=== Ghost Hive Stick-Gate ===")
    if cmd == "pack":
        stick = find_stick()
        if stick is None:
            if on_linux():
                letter = "e"
                if len(sys.argv) > 2:
                    letter = sys.argv[2][0].lower()
                stick = "/mnt/" + letter + "/" + STICK_DIRNAME
            else:
                letter = "E"
                if len(sys.argv) > 2:
                    letter = sys.argv[2][0]
                stick = letter + ":\\" + STICK_DIRNAME
            os.makedirs(stick, exist_ok=True)
        return pack_stick(stick)
    if cmd == "stop":
        stop_hive()
        return 0
    if cmd == "once":
        return once()
    if cmd == "watch":
        return watch()
    print("usage: hive_gate.py [watch|once|pack|stop]")
    return 2


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("Watcher Ende.")
        sys.exit(0)
