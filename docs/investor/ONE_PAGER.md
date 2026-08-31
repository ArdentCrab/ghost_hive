# Ghost Hive — One Pager

**Personal Adaptive Security Mesh · SPEC v2 frozen · PSP = Kernel**

---

## Problem

Home networks have no trustworthy local authority. Options today are cloud SaaS (privacy leak, vendor lock-in) or nothing. Smart devices are untrusted; the router is not a policy engine; phones cannot run a real security kernel.

Families need **local, air-gapped mesh security** with a single decision-maker and provable peer boundaries — without another app store product.

---

## Solution

**Ghost Hive** puts policy authority on a **PSP (PlayStation Portable)** as the only kernel:

- **PSP** = kernel, Watch HUD, pipeline, Vault, Ghost Down
- **Every other device** = untrusted host peer (Worker, Phone, NAS, Router, Sensor, Mine)
- **Wire:** UDP 17471, 112-byte `peer.bind`, HMAC authenticated frames
- **Stick** = physical arming bus (code + bind + manifest + wake) — no `root.key` on stick

**Network modes:**

- **IBSS direct** (PSP + Worker + Relay only) — rare, hardware-dependent
- **Relay** (recommended) — LAN devices → laptop relay → PSP over IBSS

**Ghost Down:** signed hive-wide halt. HMAC integrity failures drop traffic; they never auto-trigger Down.

---

## Moat

| Layer | Why it sticks |
|-------|----------------|
| **Frozen SPEC-v2** | No feature creep; agents and builders follow written law |
| **Hardware trust anchor** | PSP is not replacable by “another app” — physical kernel role |
| **Proven pipeline** | 449 automated tests, 2001 peer red-team attacks, 0 mismatches |
| **Host OPSEC** | Peers never hold root; bind TTL 900 s; path locked to `/tmp/ghost_hive/` |
| **R3 Hive bus** | Stick wakes real devices (Termux, Linux) without touching PSP firmware |

---

## Status (2026-08-31)

| Area | State |
|------|--------|
| SPEC-v2 Watch (4 pages) | **Frozen, tested** |
| TelemetryUpdate + P16 | **Shipped** |
| Host peers (7 roles) | **Built, WSL sim + R2 one-click** |
| Red-team suite | **2001 attacks PASS** |
| R3 wake/bind/relay path | **Software ready** |
| CI | **`make test` + `make redteam` on every push** |
| IBSS hardware live | **Pending USB stick arrival** |
| First real phone (Termux) | **Next hardware milestone** |

---

## Proof (see `PROOF.md`)

```
make test     → 449 PASS / 0 FAIL
make redteam  → 2001 match / 0 mismatch
T₁/T₂/T₃      → ~1.1 ms avg (sim host; IBSS TBD)
```

Repo: [github.com/ArdentCrab/ghost_hive](https://github.com/ArdentCrab/ghost_hive)

---

## Ask / Next

1. **Hardware live day** — PSP ingest + IBSS stick + one Termux phone via relay  
2. **Optional** — R3 red-team cases (bind abuse, rate limit, TTL expiry)  
3. **Not in scope** — cloud dashboard, AES module, PSP kernel changes without v3 order

**Contact:** Noah · Ghost Hive v2
