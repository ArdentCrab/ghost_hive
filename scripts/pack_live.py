# Live pack: EBOOT -> PSP GAME/GHOST. IBSS-Stick-Layout -> Documents/ghost_ibss_stick.
# Kein root.key. Mini ist getrennt (make mini), nicht auf die PSP.

from __future__ import print_function

import os
import shutil
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EBOOT = os.path.join(REPO, "src", "psp", "EBOOT.PBP")
WIN_DOCS = "/mnt/c/Users/noahp/Documents"
STICK_PSP = os.path.join(WIN_DOCS, "ghost_stick", "PSP", "GAME", "GHOST")
IBSS_OUT = os.path.join(WIN_DOCS, "ghost_ibss_stick")
PEERS = os.path.join(WIN_DOCS, "ghost_hive_peers")


def psp_usb():
    for letter in "defghij":
        root = "/mnt/" + letter
        game = os.path.join(root, "PSP", "GAME")
        if os.path.isdir(game):
            return os.path.join(root, "PSP", "GAME", "GHOST")
    if os.name == "nt":
        for letter in "DEFGHIJ":
            game = letter + ":\\PSP\\GAME"
            if os.path.isdir(game):
                return os.path.join(game, "GHOST")
    return None


def copy_eboot(dst_dir):
    if not os.path.isfile(EBOOT):
        print("Kein EBOOT.PBP — in WSL: make eboot")
        return False
    os.makedirs(dst_dir, exist_ok=True)
    shutil.copy2(EBOOT, os.path.join(dst_dir, "EBOOT.PBP"))
    print("EBOOT ->", dst_dir)
    return True


def pack_ibss_layout():
    os.makedirs(os.path.join(IBSS_OUT, "bin"), exist_ok=True)
    bind = os.path.join(PEERS, "k", "peer.bind")
    if os.path.isfile(bind) and os.path.getsize(bind) == 112:
        shutil.copy2(bind, os.path.join(IBSS_OUT, "peer.bind"))
    gate = os.path.join(REPO, "scripts", "hive_gate.py")
    if os.path.isfile(gate):
        shutil.copy2(gate, os.path.join(IBSS_OUT, "hive_gate.py"))
    src_bin = os.path.join(PEERS, "bin")
    if os.path.isdir(src_bin):
        for name in os.listdir(src_bin):
            shutil.copy2(os.path.join(src_bin, name), os.path.join(IBSS_OUT, "bin", name))
    print("IBSS-Pack (wenn Stick da: nach E:\\ghost_hive kopieren) ->", IBSS_OUT)


def main():
    copy_eboot(STICK_PSP)
    usb = psp_usb()
    if usb:
        copy_eboot(usb)
    else:
        print("PSP USB nicht gemountet — EBOOT liegt in Documents/ghost_stick")
    pack_ibss_layout()
    return 0


if __name__ == "__main__":
    sys.exit(main())
