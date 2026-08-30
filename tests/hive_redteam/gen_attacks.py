#!/usr/bin/env python3
# SPEC-v2 red-team matrix: Hive peers only, no PSP direct, no radio HW.
from __future__ import print_function

import csv
import os

OUT = os.path.join(os.path.dirname(__file__), "attacks.csv")

MINES = ["X", "ML", "MP", "MR", "MN", "MB", "MO", "MI"]
ROLES = {
    "W": 1, "P": 2, "R": 3, "N": 4, "F": 5,
}
PHASE_D = [
    "laptop.inject", "laptop.kexploit_sim", "laptop.filewrite", "laptop.netflood",
    "laptop.browser", "phone.app", "phone.radio", "phone.ostamper", "phone.replay",
    "router.portscan", "router.dns", "router.arp", "router.flood",
    "nas.smb", "nas.file", "nas.replay", "nas.inject",
    "mine.browser", "mine.os", "mine.replay", "mine.debugger",
    "pre.peerkill", "pre.fakescan", "pre.unsigned_hb", "pre.phonekill",
    "pre.router_poison", "pre.replay", "pre.flood", "pre.arp",
    "post.osexploit_sim", "post.netflood", "post.filetamper", "post.replay",
    "post.inject",
]


def row(attack_id, family, role, dev_id, vector, param,
         exp_h, exp_a, exp_d, exp_b):
    return {
        "id": attack_id,
        "family": family,
        "role": role,
        "device_id": dev_id,
        "vector": vector,
        "param": param,
        "expect_headline": exp_h,
        "expect_alert": exp_a,
        "expect_drop": exp_d,
        "expect_block": exp_b,
    }


def main():
    attacks = []
    n = 0

    # C — Mines (~1400)
    for mid in MINES:
        for c in range(1, 101):
            attacks.append(row(n, "C", "M", mid, "mine_trip", c, 0, 0, 0, 0))
            n += 1
    for mid in MINES:
        for c in range(1, 51):
            attacks.append(row(n, "C", "M", mid, "mine_replay", c, 1, 1, 0, 1))
            n += 1

    # D — Role-host simulation (544)
    for fam in PHASE_D:
        for seed in range(16):
            role = "W"
            dev = "W"
            if fam.startswith("phone."):
                role, dev = "P", "P"
            elif fam.startswith("router."):
                role, dev = "R", "R"
            elif fam.startswith("nas."):
                role, dev = "N", "N"
            elif fam.startswith("mine."):
                role, dev = "M", "X"
            elif fam.startswith("pre.") or fam.startswith("post."):
                role, dev = "W", "W"
            exp_h, exp_a, exp_d, exp_b = 0, 0, 0, 0
            if fam == "post.replay":
                exp_d = 1
            elif "replay" in fam:
                exp_h, exp_a, exp_b = 1, 1, 1
            elif fam in ("laptop.kexploit_sim", "phone.radio", "nas.smb",
                         "nas.inject", "pre.unsigned_hb"):
                exp_d = 1
            attacks.append(row(
                n, "D", role, dev, "role_host", "%s:%d" % (fam, seed),
                exp_h, exp_a, exp_d, exp_b))
            n += 1

    # A — Transport / wire (120)
    for role, dev in ROLES.items():
        for i in range(8):
            attacks.append(row(n, "A", role, dev, "unsigned_hb", i, 0, 0, 1, 0))
            n += 1
        for i in range(8):
            attacks.append(row(n, "A", role, dev, "unsigned_anom", i, 0, 0, 1, 0))
            n += 1
        for bit in range(5):
            attacks.append(row(n, "A", role, dev, "hmac_flip", bit, 0, 0, 1, 0))
            n += 1
        for i in range(6):
            attacks.append(row(n, "A", role, dev, "hb_replay", i, 0, 0, 0, 0))
            n += 1
        for i in range(2):
            attacks.append(row(n, "A", role, dev, "wrong_src", i, 0, 0, 1, 0))
            n += 1

    # B — Telemetry v2 (120)
    for dev in ("W", "P", "F", "R"):
        for i in range(10):
            attacks.append(row(n, "B", "W" if dev == "W" else dev, dev,
                               "telem_bad_magic", i, 0, 0, 1, 0))
            n += 1
        for pct in (101, 150, 200, 254):
            attacks.append(row(n, "B", "W" if dev == "W" else dev, dev,
                               "telem_bad_pct", pct, 0, 0, 1, 0))
            n += 1
        attacks.append(row(n, "B", "W" if dev == "W" else dev, dev,
                           "telem_dense", 0, 1, 0, 1, 0))
        n += 1
        attacks.append(row(n, "B", "P", "P", "telem_absent", 0, 1, 0, 0, 0))
        n += 1

    # E — Scanner HUD (32)
    for i in range(32):
        attacks.append(row(n, "E", "0", "", "twin_scan", i, 1, 0, 0, 0))
        n += 1

    # HB miss (16)
    for sec in (30, 31, 32, 45, 60, 90, 120, 180):
        for dev in ("W", "P"):
            attacks.append(row(n, "E", "P" if dev == "P" else "W", dev,
                               "hb_miss", sec, 1, 0, 0, 0))
            n += 1

    fields = ["id", "family", "role", "device_id", "vector", "param",
              "expect_headline", "expect_alert", "expect_drop", "expect_block"]
    with open(OUT, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for a in attacks:
            w.writerow(a)

    print("Wrote %d attacks -> %s" % (len(attacks), OUT))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
