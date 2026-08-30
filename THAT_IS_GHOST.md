# That is Ghost

**Was dieses Dokument ist:** einmal durch das ganze Repo — Spec v1.7.3, jede Pflicht, die Datei die sie trägt, was die Maschine kann, UI, Zeiten, wie man sie angreift (Lab + Threat Model, keine Exploit-Rezepte), Tests, Code.  
**Was es nicht ist:** ein zweites Spec. `SPEC.md` bleibt die einzige normative Quelle. Widerspruch → Spec.  
**Stand:** 2026-08-28. Code: C/C++11, RAM ≤ 24 MB, lokal, kein Cloud.

---

## 0. In einem Satz

Ghost Hive ist ein **lokales Mesh**: PSP-1004 als einziger Entscheider, ein Worker, ein Safe, Sensoren, ein Router, send-only Minen. Tarnung im Game-Mode, Terminal auf der PSP, Kill nur als **signiertes Kernel-KillFrame**. HMAC-I ist Drop, nie Down.

---

## 1. Identität und Grenzen

| | |
|--|--|
| Spec | `SPEC.md` v1.7.3 Ultra-Frozen |
| Agent-Gesetz | `AGENTS.md` |
| Constraints | `.cursor/rules/ghost-hive-constitution.mdc`, `ghost-hive-constraints.mdc` |
| Controls-MVP | `CONTROLS.md` (Noah 2026-08-28) |
| Attack Lab | `README.md`, `sim/`, `engines/`, `invariants/`, `controller/` |
| Kernel | PSP-FAT EBOOT (`src/psp/Makefile` → `EBOOT.PBP`) oder Host `make host` |
| UDP | Port **17471**, Wire 346 Byte (`transport_frame.h`) |

**§2.1** Systemgrenze = LAN. **§2.2** Verbote sitzen im Code als Rollenflags, Scanner-Gate, `admitPeer` ohne Auto-Enroll, Minen `canReceive()==false`. **§2.3** kein SIEM, kein Virenscanner, kein Monitor-Mode, kein Botnetz, kein Angriffswerkzeug.

---

## 2. Spec-Atlas: Abschnitt → Dateien → was die Maschine tut

### §1 Überblick

PSP unsichtbar, Rest Scanner/Analyse/Backup/Alarm. Implementierung: `src/psp/main.cpp` Loop (WLAN, `transport.rx`, Pipeline, Terminal-Tick).

### §3–§5 Geräte und Rollen

| Rolle | Gerät | Binary / Entry | Dateien |
|-------|--------|----------------|---------|
| kernel | PSP-1004 | `make eboot` / `src/psp/main.cpp` | gesamtes `src/psp/` |
| worker | Laptop Noah | `/tmp/ghost_laptop` `src/laptop/main.cpp` | worker, analyzer, log_client, alert, kill, tetact, peer_* |
| phone | Phone Noah | `/tmp/ghost_phone` `src/phone/main.cpp` | sensor + alert + tetact + Minen-Geschwister |
| sensor | Familie | `/tmp/ghost_family` `src/sensor/main.cpp` | sensor, tetact, **kein Write** |
| safe | NAS | `/tmp/ghost_nas` `src/nas/main.cpp` | safe, index, access, honeypot |
| net sensor | Router | `/tmp/ghost_router` `src/router/main.cpp` | net_sensor, tetact, honeypot |
| mine | Geschwister | `/tmp/ghost_mines` `src/mine/main.cpp` | os/browser/router/iot + `mine.cpp` |

Rollen-Konstanten: `ghost_core.h` `ROLE_*`. Kernel-ID: `"kernel"`.

### §6–§7 Rechte

Abbildung in `ghost_core.h` `Permission`. Enforcement in Role-APIs:

- Sensor `canWrite()==false` — `src/phone/sensor.cpp` / Family-Main
- NAS `canAlert()==false` — `src/nas/safe.*`
- Router `canConfigure()==false` — `src/router/net_sensor.*`
- Analyzer `canDecide()==false` — `src/laptop/analyzer.*`
- Worker blockiert nicht — `src/laptop/worker.*`

### §8–§10 Zustände

Geräte: `DeviceState` in `ghost_core.h` + `registry.cpp`.  
PSP-Maschine: `GhostStealth` (`game_mode` / Terminal), `GhostDown` (`ghost_down`), `GhostPeek` / `low_power_mode`, Danger über Transport/`DangerModeEnter`.

**Verboten und so gebaut:** kein `game_mode → danger_mode`, kein `ghost_down → game_mode` als State-Hop. Down-Look ist Game-**Look**, State bleibt `ghost_down` (`ghost_down.h`). Lab INV-02 prüft den Hop.

Terminal-Modi (UI, nicht extra Spec-States): `TermMode` in `ghost_terminal.h` — Terminal, Output, Black, Game, GhostDown.

### §11–§12 Module

| Spec-Modul | Dateien | Pflicht (kurz) |
|------------|---------|----------------|
| ghost-core | `ghost_core.h`, `registry.*`, `event_queue.*`, `decision_pipeline.*`, `context_engine.*`, `priority_engine.*`, `fallback_engine.*` | Registry, Route, Prio, Fallback |
| ghost-terminal | `ghost_terminal.*`, `ghost_output.*`, `psp_input.*` | §34 CLI, CRT, Lock |
| ghost-scanner | `ghost_scanner.*` | WLAN-Scan nur Terminal; nie WPA-Client |
| ghost-vault | `ghost_vault.*` | RAM zuerst, persist `vault.bin` |
| ghost-heartbeat | `ghost_heartbeat.*` | 30s, Miss |
| ghost-policy | `ghost_policy.*` | P01–P15 + DSL |
| ghost-stealth | `ghost_stealth.*` | Game = Funk aus |
| ghost-ir | `ghost_ir.*` | RX `sceSircsReceive`; TX `sendSignal` → false |
| ghost-down | `ghost_down.*`, `ghost_arm.h` | Snapshot, Flush, Kill, Tarn |
| ghost-peek | `ghost_peek.*` | nach Down, kein Kaltstart |
| hive-worker … hive-kill | `src/laptop/*` | Analyse, Alert, Kill-Fill (Kernel sendet Kill) |
| hive-sensor / alert | `src/phone/*` | Scan + Alarm |
| hive-safe / index / access | `src/nas/*` | Backup, Index, Share-Watch |
| ghost-mine-* | `src/mine/*` | send-only |

**Transport (kein Extra-Modul, Draht für §18/§22):**  
`transport/ghost_transport.*`, `medium_wlan.*`, `medium_ir.*` (In-Process-Stub), `transport_frame.*`, `transport_bidi.*`, `worker_transport.*`, `sensor_transport.*`, `safe_transport.*`, `mine_transport.*`.

### §13 Capabilities

Nur die Spec-Liste, Bitmaske `Device.capability_mask` — `ghost_core.h` `Capability`.

### §14 Daten

`ghost_core.h` / `ghost_data.h`: `Device`, `Event`, `Log`, `Policy`, `MinePayload`. **Keine Extra-Felder.** Event-Typen: ScanResult … MineEvent (siehe Enum). Payload 128 Byte.

### §15 Replay-Guard

`replay_guard.*` — 32 Slots/Mine, monotoner Counter, TOTP 60–120s (`ghost_crypto.h`), Replay → `blockMine`. Pipeline ruft das vor Enrich.

### §16–§17 Policy

`ghost_policy.cpp` `initDefaults()` — genau P01–P15. Aktionen: log_only, alert, backup, block, kill, ghost_down (+ Extra-Bits Snapshot/Vault/Classify).

### §18 Pipeline

`decision_pipeline.cpp` `process()`:

```
validate → replay_guard → enrich → classify → policy → priority
→ fallback → escalation → route → store → ack
```

HMAC-I liegt **vor** der Pipeline im Transport: `rejectHmacI` → Evidence + P15, **kein** `enterHiveDown`.

### §19–§21 Prio, Fallback, Context

`priority_engine.cpp`, `fallback_engine.cpp`, `context_engine.cpp` (home/mobile/public/offline).

### §22 Fluss

Alle Frames zu Kernel `listenKernel(17471)`. Minen: `MineTransport::recv` immer false. Peers: `connectKernel(id, psp-ip, 17471)`.

### §23 Zyklen (Reaktionszeiten, Spec)

| Aktion | Intervall | Code |
|--------|-----------|------|
| Heartbeat | 30s | `ghost_heartbeat` |
| WLAN/BT-Scan | 30s | scanner / TETACT-Poll |
| Sensor-Scan | 60–300s | `sensor.intervalSec()`, TETACT 60s |
| Backup | 5 min | vault flush / NAS |
| Ghost Peek | 5 min | `PEEK_WINDOW_SEC = 300` |
| Minen | periodisch | `mine.intervalSec()` |
| ACK | 1–2s, 1 Retry | `ACK_TIMEOUT_SEC = 2`, `ACK_RETRY_SEC = 1` |
| NAS-Flush bei Down | 5–10s | `NAS_FLUSH_TIMEOUT_SEC = 5` |
| Storage-Flush | 30–120s, 1 Retry | `ghost_down.cpp` |
| Terminal-Tick | 10 ms | `CONTROLS.md` |
| Auto-Reset UI | 10 min | `AUTO_RESET_MS = 600000` |
| HMAC-I Evidence | 1 / 60s | Transport |
| P15 Alert | 3 HMAC-I / 60s | Policy |
| Freeze | **dieselbe Loop-Runde** | `enterHiveDown` in `rx` |

### §24 RAM

`PSP_HEAP_SIZE_KB(20480)` in `main.cpp`. Budgets in File-Köpfen. Output-Puffer 1024. Scanner-Buffer freigeben: `ghost_scanner.cpp`.

### §25 Topologie

1 Kernel, 1 Worker, 1 Safe, 1–8 Sensoren, 1 Router, 1–N Minen. Registry ≤ 32. Makefile-Kommentare: `ghost_mines` IDs.

### §26–§29 Lifecycles

Registry-States; Mine install→silent→active→triggered; Event-Lifecycle = Pipeline; Backup `GhostVault::persist` / NAS ingest; Alarm `hive-alert` + P02/P08/P11.

### §30 Threat Model → Angriffsflächen (ohne Rezepte)

Gegner: Scriptkiddie, kompromittiertes Familiengerät, Gezielt, physisch, Malware, Replay.  
Wege: Worker, Phone, Router, PSP, NAS, AP, MITM, Minen, Replay.

**Was der Code dagegen tut (Soll):**

| Weg | Abwehr | Datei |
|-----|--------|-------|
| Unsigned / bad HMAC Kill | Drop, nie Down; Peers haltieren nicht | `ghost_transport.cpp` `rejectHmacI`; `peer_halt_authorized` |
| Replay Mine | Block + P13 | `replay_guard.cpp` |
| Auto-Enroll | Pending, kein Admit | `admitPeer` |
| Root-Diebstahl Stick | Wrap, XMB-Kopie wertlos | `ghost_wrap.cpp`, `ms0:/ghost_hive/k/root.key` |
| Peer-Kill von Worker | Kernel ignoriert Peer-GDS, kein Freeze | `handleEvent` GhostDownStart → PolicyViolation `peer_kill` |
| Game-Mode Scan | Scanner aus | `setTerminalMode(false)` |
| WPA-Client | nicht implementiert, verboten | scanner / `hive_wlan_on` = Attach only |
| Lab-Angriff | nur `127.0.0.1:17471` | `README.md` |

**Attack Lab (Simulator, nicht die echte PSP):** Engines A–E schicken Wire-Mutationen, Replay, unsigned Kill, MAC-Gitter. Invarianten INV-01…08. Das Lab **ist** der erlaubte Angriff auf `ghost-sim`. Es ist kein Freibrief für LAN-Exploits.

### §31 Recovery

safe_mode Vault, Retry Netz, Worker-Zeit nur trust≥2, Replay block, Registry/Vault.

### §32 Bootstrap

`physicalProvision` in `main.cpp`: Root intern, Wrap, `root_config.json` Ingest (keine Secrets in der JSON). Worker pairing pending→online.

### §33 / §33.1 Krypto

`ghost_keys.*`, `ghost_crypto.*` (HMAC-SHA1, TOTP), `ghost_wrap.*`. Peer-Bind 112 Byte **ohne Root**: `exportPeerBind` / `peer_bind_keys`.  
**Lücke Live-PSP:** `writePeerBindHost` unter `__PSP__` ist No-op — Peers kriegen von der EBOOT kein `peer.bind`.

### §34–§35 CLI und Views

Zwölf Kommandos, `ghost_terminal.cpp` `kCommands[]`, Rendering `ghost_output.cpp`. `ghost down` = **Status**, startet Down nicht (`CONTROLS.md`). `mine block` CLI ist **Anzeige**, Block passiert intern bei Replay.

### §36 Pflichttests → `tests/`

| Spec-Pflicht | Test |
|--------------|------|
| nur PSP / Controls | `test_controls.cpp`, `test_p36.cpp` |
| Worker aus / Route | `test_p30_worker.cpp`, `test_p30_route.cpp` |
| NAS / Vault / Stick | `test_p30_vault.cpp`, `test_p30_stick.cpp` |
| neues Gerät / MITM | `test_p30_mitm.cpp` |
| Heartbeat / Zeit | `test_p30_time.cpp` |
| Registry | `test_registry.cpp` |
| Ghost Down / Kill | `test_p30_kill.cpp`, `test_phase_d.cpp`, `test_phase_e.cpp` |
| Replay | `test_replay_guard.cpp`, `test_p30_replay.cpp` |
| Policy | `test_policy.cpp` |
| Root wrap / config | `test_root_wrap.cpp`, `test_root_config.cpp` |
| TETACT | `test_tetact.cpp` |
| Peer-Halt + MAC | `test_peer_halt.cpp` |
| Phase C | `test_phase_c.cpp` |
| Spec-Smoke | `test_spec.cpp` |

`make test` setzt `GHOST_DOWN_ARMED=1` (sonst Host-Down fail-closed).

### §37 Version

Makefile/EBOOT 1.7.x; Spec 1.7.3. Worker/Sensor/Safe/Mine 1.0-kompatibel laut Spec.

### §38 Honeypot

Nicht auf PSP. `src/nas/honeypot.*`, `src/mine/router_mine.*`, Laptop-Minen in `main.cpp`.

### §39 Verteidigung

Alarm, Evidence (Vault), Isolieren (Danger/silent), Sperren (Replay block), Tarnung (Game/Stealth), Lockvogel (Honeypot), Segmentierung (Rollen), Backup, WLAN aus, Selbstschutz (HMAC-I drop).

### §40 / §43 Ghost Down und Kill

Sequenz in `ghost_down.cpp` + `enterHiveDown` + `sendKill`. Arming: `ghost_arm.h`.  
PSP: immer **aus**. Host: nur `GHOST_DOWN_ARMED=1`. Lab-Sim: Controller setzt 1.  
Peer-Halt: Prozess tot + Marker `/tmp/ghost_hive/halt.<id>`; `GHOST_OS_HALT` nicht im Default-Build (kein `shutdown`).  
**Fail-closed:** Halt nur Kernel-Quelle + MAC (`peer_halt_authorized`).

### §41 Peek / §42 Danger

`ghost_peek.cpp`, `DangerModeEnter` im Transport, CLI `ghost peek` / `danger mode` (Views).

### §44 MUST-Liste

Siehe Constitution + Constraints. Jede technische Entscheidung zitiert Spec zuerst.

### §45 IST

Persönlich, lokal, getarnt, replay-geschützt, hardware-bewusst — nicht Enterprise.

### §46 TETACT

**Kein PSP-Modul.** `src/laptop/tetact.*` auf Worker, Phone, Family-Sensor, Router. Events nur §14-Typen. `MUST NOT` schreiben/blocken/konfigurieren/entscheiden. Tamper → `AnomalyDetected` + Mine `sendTrip`. HeartbeatMiss allein ≠ Down.

---

## 3. Was die Maschine kann (Operator)

**PSP, Terminal-Mode, Funk an**

- Registry, Heartbeats, Policies, Vault, Scan (WLAN im Terminal)
- Events annehmen, HMAC prüfen, Replay, Pipeline, ACK
- CLI der 12 Befehle, History 6, Output 6 Seiten
- Game-Mode: Funk aus, kein aktiver Scan
- Black: leerer Screen
- Down: leer, keine Tasten; Kernel bleibt; Peek passiv danach

**PSP kann nicht (absichtlich)**

- Down per Taste oder CLI starten
- WPA2/WPA3 joinen
- Auto-Enroll
- IR senden (TX false)
- Honigtopf sein

**Worker**

- Heartbeat, TETACT (OS/Netz-Sicht), Analyse, Alert anzeigen, Logs pullen
- GhostDownStart **füllen** darf er — Kernel **friert davon nicht**
- Halt nur bei autorisiertem Kernel-Kill

**Phone Noah**

- Sensor + Alert + TETACT + optionale Minen

**Familie (Sensor)**

- Nur senden (Heartbeat, TETACT), Halt bei autorisiertem Kill

**NAS**

- Backup ingest, Index, Share-Watch → Lockvogel-Mine; kein Alarm
- Bei autorisiertem Kill: Write-Lock, Shares-Marker, Prozess tot

**Router**

- Net-Sensor / TETACT, keine Config; Halt + Ports-Marker

**Minen**

- Counter++, TOTP, TX; kein RX; sterben mit Host

---

## 4. UI und Controls (vollständig)

Normativ für Tasten: `CONTROLS.md`. Orchestrator: Taste → `TermEvent` (kein Spec-Event) → State → `GhostOutput` → Frame.

**Prompt:** live `>ghost_$<cli>`; Snapshot ohne `<>`; Lock `*`. Kopf nur `ghost:hive`. Grün, kein Pager, kein LOCK/DOWN im Chrome.

**Host-Tastatur** (`GHOST_PSP_KEYS=1`): i/k, j/l, x, o, s, g=Select+Start, `[` `]`, h=Home.

**Down:** keine Tasten. L+R startet Down **nicht** (`test_controls.cpp`).

---

## 5. Draht und Timing (wie schnell)

```
Peer UDP 17471
  → MediumWlan decode (346 B)
  → GhostTransport::handleFrame
       HMAC fail → rejectHmacI (drop, evidence ≤1/60s)
       else rx queue
  → DecisionPipeline::process  (eine Loop-Runde)
  → ACK 1–2s bidirektional
```

Kritischer Freeze: **in derselben `rx`-Runde** wie der Trigger (`enterHiveDown`). Nicht „irgendwann im 1-Hz-SNAP“.

TOTP-Fenster 60–120s. Ungültiges Fenster → Mine nicht accept.

---

## 6. Wie man es angreift (erlaubt / verboten)

### Erlaubt: Attack Lab

Nur `ghost-sim`, nur `127.0.0.1:17471`, synthetische Keys `i+1`. Nie PSP/Laptop/NAS.

| Engine | Idee |
|--------|------|
| A fuzzer / corpus | Wire-Mutationen, `kill_unsigned.bin`, `mine.bin` |
| B guided | Payload/Auth/MAC-Bits |
| C replay | Counter/TOTP-Kanten |
| D desync | unsigned/signed Kill, Noise, alte Heartbeats |
| E counter_mac | Counter-Sprünge + MAC-Gitter |

Invarianten (`invariants/`):

| INV | Aussage |
|-----|---------|
| 01 | Root/wrap ok |
| 02 | erlaubter State-Hop |
| 03 | HMAC-I nicht accepted |
| 04 | Replay nicht accepted |
| 05 | TOTP-out nicht accepted |
| 06 | kein ASan; Crash nur mit Restart |
| 07 | **uk_froze** — unsigned Kill hat **diesen Frame** gefroren (nicht 1-Hz-Smear) |
| 08 | Freeze hat Snapshot |

**Lab-Ergebnisse (Stand 2026-08-28):**

- Unit `test_invariants`: PASS (n=29).
- Isolierte Matrix A/B/C (`tests/test_inv07_frame.py`): unsigned allein `frozen=0 uk_froze=0`; Mine-Replay friert `uk_froze=0`; unsigned auf frozen Hive = Smear `uk_froze=0`. PASS.
- 90s b/c/d/e isoliert Port 17472: INV-07 = 0, 5 Freeze-Restarts.
- 20-Minuten-Volumenlauf (Faktor 200, Port 17472): separat vom 4h-Job.
- 4h-Lauf Port **17471**, Binary 11:44, **altes** INV-07-Prädikat: tausende INV-07 = Messartefakt (Smear), nicht Kernel-Beweis. S4.D (unsigned allein) blieb `frozen=0`. Live-Binary nicht überschrieben.

Altes Prädikat „letzter Frame unsigned ∧ Freeze 0→1 im Fenster“ war das falsche Lineal (u. a. 38 idle-before in derselben Sim-Sekunde ohne Reihenfolge).

### Verboten / was ein Angreifer *versuchen* würde (nur Modell)

- Unsigned Kill als Blind-Schalter → **Kernel drop**; **Peer früher:** Typ-Halt (Loch, jetzt MAC+kernel-Quelle).
- Replay Mine → Block.
- MITM HMAC → drop P15.
- PSP als Client ins Fremdnetz → Spec-Verbot, kein Associate.
- `root.key` von Stick klauen → Ciphertext ohne Master-Key wertlos.
- Worker sendet GhostDownStart → Kernel freeze **nicht**.

Keine Payloads, keine LAN-PoCs in diesem Dokument.

---

## 7. Code-Ausschnitte (die Maschine in fünf Schnitten)

**Pipeline §18** — `src/psp/decision_pipeline.cpp`:

```cpp
PipelineResult DecisionPipeline::process(const Event& event, uint32_t now) {
    if (transport_ != nullptr && transport_->hiveFrozen()) {
        return PipelineResult::Rejected;
    }
    // validate → replayGuard → enrich → classify → checkPolicies
    // → computePriority → computeFallback → computeEscalation
    // → route → store → ack
}
```

**HMAC-I nie Down** — Transport; Kill nur Kernel-Signatur.

**Arming** — `src/psp/ghost_arm.h`:

```cpp
#elif defined(__PSP__)
    return false;
#else
    const char* e = getenv("GHOST_DOWN_ARMED");
    return e != nullptr && e[0] == '1' && e[1] == '\0';
#endif
```

**Peer-Halt fail-closed** — `src/laptop/peer_halt.cpp`:

```cpp
bool peer_halt_authorized(const GhostKeys& keys, const Event& event) {
    if (!peer_halt_is_kill(event)) return false;
    // source_device_id == "kernel"
    return peer_verify_event(keys, event);
}
```

**P01–P15** — `ghost_policy.cpp` `initDefaults()`, u. a. P15 `hmac_invalid` Alert, nicht ghost_down.

**UI-Hop** — Select+Start `TermEvent::GameToggle`; Down-Mode schluckt alle Keys.

**Heap** — `PSP_HEAP_SIZE_KB(20480)` §24.

---

## 8. Persistenz (Stick / Host)

PSP:

```
ms0:/PSP/GAME/<ordner>/EBOOT.PBP
ms0:/ghost_hive/k/root.key      # Ciphertext
ms0:/ghost_hive/vault.bin
ms0:/ghost_hive/root_config.json  # optional, einmal Ingest
```

Host-Peers: `/tmp/ghost_hive/peer.bind` (112 B, kein Root), Halt-Marker `halt.<id>`.

Minimaler Stick: nur EBOOT; Rest erzeugt die EBOOT selbst. Kein Lab, kein SPEC, keine ISOs auf die FAT für Funktionstests.

---

## 9. Bauen und starten

```
make test          # Host-Unit, Down für Tests armed
make eboot         # PSPSDK, src/psp
make peers         # laptop phone family router nas mines
make host          # /tmp/ghost_hive_kernel — Down nur mit GHOST_DOWN_ARMED=1
make lab           # NUR wenn kein Live-Sim auf 17471
```

Peers: `/tmp/ghost_<role> <psp-ipv4> [id] …`  
Lab: `python3 controller/controller.py --hours … --accelerated --time-factor 200`

**Nicht** `make lab` während PID auf `127.0.0.1:17471` (überschreibt dieselbe Binary/ denselben Port).

---

## 10. Was noch nicht „fertig außer Down“ ist

Diese Liste ist Teil von That-is-Ghost, kein Wunschkonzert:

1. Peer-Bind-Export von der **EBOOT** (heute Host-only).
2. FAT-IPv4 ohne WPA-Client — UDP 17471 braucht eine Adresse, an die Peers senden.
3. `mine block` CLI → wirklich `ReplayGuard::blockMine`.
4. IR-Medium: Stub-Queue, kein IR-Draht zu Peers.
5. Lab: INV-07 mit `uk_froze` unter 4h-Volumen noch nicht als offizielles Lab-Binary bestätigt (4h-Referenz = altes Lineal).
6. Live-Mesh-Test erst nach 1–2 und nach INV-07-Volumen mit neuem Prädikat = 0.

Down auf der PSP bleibt locked bis v1. Host-OS-Halt nur mit explizitem `GHOST_OS_HALT`.

---

## 11. Dateibaum (alles was zählt)

```
SPEC.md AGENTS.md CONTROLS.md README.md THAT_IS_GHOST.md Makefile
src/psp/          Kernel, Terminal, Vault, Pipeline, Transport, Wrap
src/laptop/       Worker-Stack + TETACT + Halt/Keys
src/phone/        Alert-Sensor
src/sensor/       Family sensor-only
src/nas/          Safe
src/router/       Net-Sensor
src/mine/         Send-only Geschwister
tests/            §36 + P30 + Phases + Controls + Halt
sim/ engines/ invariants/ controller/   Attack Lab only
```

---

## 12. Schluss

That is Ghost: **eine PSP entscheidet**, der Rest liefert Augen und Beine, Kill ist ein signierter Befehl nach Snapshot, Tarnung ist Game-Look mit Funk aus, Angriff auf den Kernel im Lab ist UDP-localhost, Angriff auf die Familie im LAN soll an MAC und Fail-closed Arming sterben.

Norm bleibt `SPEC.md`. Diese Datei ist die Landkarte des Repos, nicht die Verfassung.
