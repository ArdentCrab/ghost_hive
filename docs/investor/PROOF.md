# Ghost Hive — Proof Pack (v2)

**Date:** 2026-08-31  
**Norm:** [`SPEC-v2.md`](../SPEC-v2.md)  
**Inventory:** [`WHATISV2.md`](../WHATISV2.md)

This document lists **reproducible evidence** for the frozen v2 stack. It is not a security certificate or third-party audit.

---

## 1. Regression suite (`make test`)

| Metric | Value |
|--------|-------|
| Result | **EXIT 0** |
| `PASS` lines | **449** |
| `FAIL` lines | **0** |
| Wall time (typical) | ~46–65 s |
| Log (local) | `/tmp/ghost_v2_make_test.log` |

**Command:**

```bash
make test
```

**CI:** `.github/workflows/ghost.yml` runs `make test` on every push.

---

## 2. Peer red-team (`make redteam`)

| Metric | Value |
|--------|-------|
| Attacks | **2001** |
| Match | **2001** |
| Mismatch | **0** |
| Result | **PASS hive_redteam** |
| Attack catalog | `tests/hive_redteam/attacks.csv` |
| Results | `tests/hive_redteam/results.csv` |

**Command:**

```bash
make redteam
```

### Latency (T₁ / T₂ / T₃)

Measured inside `tests/hive_redteam/run_redteam.cpp` on attacks that set `watch_danger_headline`:

| Stage | Meaning | Avg (µs) |
|-------|---------|----------|
| **T₁** | Pipeline + registry after ingest | ~1131 |
| **T₂** | `watch_danger_headline` | ~1132 |
| **T₃** | `watch_fill_alert()` | ~1134 |

Sample run (2026-08-31):

```
redteam attacks=2001 match=2001 mismatch=0
redteam headline_avg_us t1=1131 t2=1132 t3=1134 (n=520)
```

`n=520` = attacks that produced a danger headline (not all 2001 attack types expect one).

---

## 3. Soak / UI (`test_v2_final`)

| Check | Loops | Result |
|-------|-------|--------|
| L/R page wrap | 10 000 | PASS |
| Peer focus | 10 000 | PASS |
| Square re-blit | 10 000 | PASS |
| Peer draw raster | 10 000 | PASS |
| Twin HUD after buffer release | 1 000 | PASS |
| Down input lock | 4 000 | PASS |
| Telemetry burst | 10 000 process | acc=8 blk=9992 (Vault 8/peer) |

Wall soak: ~1.9–2.3 s.

---

## 4. Transport hardening (SPEC-v2)

| Control | Value | Verified by |
|---------|-------|-------------|
| Replay window | 64 | `test_perf`, redteam |
| Vault cap | 8 / peer | `test_telemetry`, soak |
| ACK budget | 8/s | spec tests |
| HMAC-I | Drop, never Down | `test_phase_e`, redteam |
| P16 TelemetryUpdate | LogOnly only | `test_telemetry` |
| Peer bind | 112 B, no root on peer | `test_opsec` |
| Bind TTL | 900 s | `peer_keys.cpp`, R3 `ghost_wake` |

---

## 5. R3 orchestration hardening (2026-08-31)

| Control | Implementation |
|---------|----------------|
| Whitelist | `devices.manifest` device IDs only — no default fallback |
| Rate limit | 5 bind requests / IP / minute → DENY + stderr log |
| Port window | UDP 17470 open only for `bind_ttl_sec` (900 s), then `bind_serve` exits |
| TTL on device | `ghost_wake` writes `peer.bind.ttl` from wake `epoch` |
| TTL on laptop | `sync_bind` / `sync_bind_from_stick` write WSL TTL |

**Files:** `deploy/ghost_hive/bind_serve.py`, `wake/ghost_wake.py`, `scripts/hive_gate.py`

---

## 6. What is not proven here

- Real IBSS hardware latency (stick not yet live-measured)
- LAN MITM against R3 wake/bind under active attacker
- Third-party cryptographic review
- Production key material (lab uses synthetic / ingest keys only)

---

## 7. Reproduce locally

```bash
git clone https://github.com/ArdentCrab/ghost_hive.git
cd ghost_hive
make test
make redteam
```

Expected: both exit 0.
