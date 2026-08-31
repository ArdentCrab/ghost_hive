# Ghost Attack Lab

![Ghost Hive CI](https://github.com/ArdentCrab/ghost_hive/actions/workflows/ghost.yml/badge.svg)

Tests + Redteam: on every push (`make test`, `make redteam`).

Offline robustness lab for the **Ghost Simulator** only. Not a security certificate, not a real-network test, not a crypto attack, not a substitute for spec-blind review.

## Constraints

- Target: `ghost-sim` (host sim-build of `ghost-core`). Never PSP, laptop, phone, NAS, or router hardware.
- Packets: **only** `127.0.0.1:17471`. SPEC-v1: never the live IBSS SSID `GHSTHIVE` / `10.17.47.1`.
- Keys: synthetic lab material (`i+1` root). No production keys, TOTP seeds, or MAC ranges.
- Introspection: Unix socket `/tmp/ghost_lab/sim.sock` (sim process only).
- Isolation: `docker-compose` uses `network_mode: none` (loopback inside the container, no host/bridge ports).

## Layout

| Path | Role |
|------|------|
| `sim/ghost_sim.cpp` | ASan/UBSan UDP target + introspect |
| `invariants/` | INV-01..08, unit test, `inv_check` CLI |
| `engines/a_fuzzer/` | libFuzzer harness + Phase-E seed corpus |
| `engines/{b,c,d,e}/` | UDP engines |
| `controller/controller.py` | spawn, restart, SQLite findings |
| `docker-compose.yml` | isolated VM/container |

## Build

```
make lab
```

Invariants alone (must pass before engines B–E):

```
make lab-invariants
```

libFuzzer (needs `clang++`):

```
make lab-fuzz
```

## Run

Smoke (about 12s, local):

```
make lab-test-phase
```

24h unattended (same machine, loopback only):

```
python3 controller/controller.py --hours 24
```

Phase A accelerated GhostMode (simulator only, 200× clock, 24 real hours ≈ 4800 sim-hours):

```
make lab-24h-accelerated
```

Exports under `/tmp/ghost_lab/export/`: `findings.jsonl`, `REPRO.txt`, `coverage.log`, `throughput.log`, `sim_time.log`, `proof_snaps.log`. Status: `/tmp/ghost_lab/PHASE_A_STATUS.txt`. `ghost_mode` is a lab flag, not a SPEC §9 state.

Docker (no host network, no published ports):

```
docker compose up --build
```

## Introspection (`GHS1` line)

The UDP sim reports `terminal_mode` while listening (radio on). `game_mode` is the Down conceal look; the PSP **state** in Down stays `ghost_down` (§9 / §40.6).

Unix commands: `SNAP`, `NOW <u32>` (sim clock, lab only).

## Invariants

| ID | Predicate |
|----|-----------|
| INV-01 | `root_ok` stays true |
| INV-02 | PSP state hop is on the §10 list (incl. forbidden `game_mode→danger_mode`, `ghost_down→game_mode`) |
| INV-03 | bad/missing HMAC ⇒ not accepted; policy `drop`, `log_only`, or `hmac_i` (P15) |
| INV-04 | replay (`counter`/`timestamp` not newer) ⇒ not accepted |
| INV-05 | TOTP outside 60–120s of last send ⇒ not accepted |
| INV-06 | no ASan/UBSan; crash only with controller restart |
| INV-07 | unsigned peer-kill must not freeze the hive on **that** frame (`uk_froze`) |
| INV-08 | entering `ghost_down` requires a snapshot step (§40) |

HMAC-I is evidence + drop (P15). INV-07 MUST stay at 0: unsigned `GhostDownStart` MUST NOT freeze the hive.

## Findings

- Append-only JSONL: `/tmp/ghost_lab/findings.jsonl`
- SQLite: `/tmp/ghost_lab/findings.sqlite` (`engine`, `inv`, `kind`, `payload_hex`, `before`, `after`)

Reproduce a payload: hex in the row is the leading bytes of the 346-byte wire blob sent to `127.0.0.1:17471`.

## What this is not

External spec-blind review, cryptographic analysis, LAN stress, or a pass/fail security rating. Coverage-guided fuzzing on `transport_decode` is Engine A; plateau is `make lab-fuzz` / longer `-max_total_time`.
