# WHATISV2 — Ghost Hive v2 Bestandsaufnahme

**Status:** v2 frozen (Host-Test 2026-08-29).  
**Norm:** [`SPEC-v2.md`](SPEC-v2.md) (live). Archiv: [`SPEC-v1.md`](SPEC-v1.md), [`SPEC.md`](SPEC.md) v1.7.3.  
**Agent-Gesetz:** [`AGENTS.md`](AGENTS.md).  
**Dies ist kein zweites Spec.** Widerspruch → SPEC-v2. Dieses Dokument ist Inventar + Testergebnis + UI/Control-Entscheidungen.

**Gerät:** PSP-1004 FAT, RAM ≤ 24 MB, lokal, kein Cloud.  
**Wire:** UDP **17471**, Frame 346 Byte. IBSS SSID **`GHSTHIVE`**, Kernel-IP **10.17.47.1**.

---

## 0. In einem Satz

v2 ist **ein** PSP-EBOOT mit Watch-HUD (vier Seiten), einem Telemetrie-Event, P16 LogOnly, härterem Transport (Replay 64, ACK 8/s, Vault 8/Peer), Locked-Down ohne Kill, und **Host-OPSEC** außerhalb des EBOOT (`keyd`, `ghost_mon`, Worker-Pfad). Die PSP entscheidet weiter allein. HMAC-I ist Drop, nie Down.

---

## 1. Geltung und Dateien

| Rolle | Datei |
|-------|--------|
| Live-Norm | `SPEC-v2.md` |
| UI-Raster/Glyph wo v2 schweigt | `SPEC-v1.md` |
| Kernel-Bits 1.7.3 | `SPEC.md` |
| v1-Gesamtlesung (Archiv) | `THAT_IS_GHOST.md` |
| v1-CLI-Tasten (Host-Debug) | `CONTROLS.md` |
| Attack-Lab (Sim, nicht PSP) | `README.md`, `sim/`, `engines/`, `invariants/` |

**Entscheidungsreihenfolge:** SPEC-v2 → SPEC-v1 (Chrome) → SPEC 1.7.3 → Noah.

---

## 2. Topologie (unverändert)

1 Kernel (PSP) · 1 Worker (Laptop Noah) · 1 Safe (NAS) · 1–8 Sensoren · 1 Router · 1–N Mines. Registry ≤ 32.

| Rolle | Binary (Host) | Darf |
|-------|----------------|------|
| kernel | `make eboot` / `make host` | Entscheiden, Vault, Down, Scan (nur Terminal/Watch, nie Game) |
| worker | `/tmp/ghost_laptop` | Analysieren, Alert zeigen, nicht blocken |
| phone | `/tmp/ghost_phone` | Sensor + Alert |
| sensor | `/tmp/ghost_family` | Lesen, nicht schreiben |
| safe | `/tmp/ghost_nas` | Backup, nicht alarmieren |
| router | `/tmp/ghost_router` | Net-Sensor, nicht konfigurieren |
| mine | `/tmp/ghost_mines` | Send-only, kein RX, kein Policy |

**Verboten:** Role-Upgrade, Auto-Enroll, WPA2/WPA3-Client, Honeypot-Modul auf PSP, TETACT als PSP-Modul, zweiter Kernel/Worker/NAS/Router.

---

## 3. Was v2 ist (Delta)

### 3.1 Spec-Delta (D1–D9)

| # | Inhalt |
|---|--------|
| D1 | Device: sechs Telemetrie-Felder + Absent `0xFFFF` / `0xFF` |
| D2 | Ein neuer Event-Typ: `TelemetryUpdate` (Enum 18). UDP-Opcode `0x42` ist nur Payload-Magic |
| D3 | P16 `telemetry_update`, LogOnly; Policy-ok = **16** |
| D4 | Watch vier Seiten Hive / Kernel / Net / Peer; kein Patch-JSON |
| D5 | D-Pad Up/Down **nur Peer**; sonst tot |
| D6 | Titel `[GHv2]` |
| D7 | Keine erfundenen Peer-Messwerte; `--` |
| D8 | Kernel darf D1–D3, sonst 1.7.3 |
| D9 | Down bleibt Watch, Eintritt Kernel, `STATE:Down`; kein Black-Screen als Down-UI |

### 3.2 UI-Architektur (Noah, v2)

- **Ein Prozess, ein Framebuffer, kein Plugin-OS, kein Multitasking.**
- Live-EBOOT: Watch, **kein** `ghost_$` Prompt. Debug-CLI nur `GHOST_DEBUG_CLI` (Host).
- **Kein Game-Look / kein Game-Mode als UI.** Select+Start tot.
- **O** = Stealth-Schwarz. Kernel, Pipeline, Telemetrie, Funk weiter. Nur O zurück.
- **Home idle** = ExitHive (XMB). **Home Down** = Watch hide, Kernel weiter.
- EBOOT-Arming **locked** (PSP). Host-Tests können `GHOST_DOWN_ARMED=1` setzen.

### 3.3 Transport / Vault (v2 Härtung, im EBOOT)

| Hebel | Wert | Watch |
|-------|------|--------|
| Replay-Fenster Mine | 64 | `ReplayW` |
| Telemetrie-Replay | 64 Slots | intern |
| ACK-Budget | 8 / s | `AckBd n/8` |
| Vault RAM | 64 | `Vault: n/64` |
| Vault / Peer | 8 | Store danach `Rejected` |
| SSID-Pin | nur `GHSTHIVE` | `Pin` |
| Telem Rate | ≥ 1 s / Peer (Send-Intervall Host oft 2 s) | Drop-Zähler |
| Locked-Down | Funk/Scan/ACK aus, RAM-Snapshot, **kein Kill** | `Down: locked` / `STATE:Down` |

### 3.4 Host-only (nicht Spec-treu, bewusst, kein PSP-Modul)

| Tool | Pfad | Tut |
|------|------|-----|
| Worker-OPSEC | `src/laptop/peer_keys.cpp` | `umask 077`, Linux dumpable=0, kein `://`, kein `..`, Bind nur `/tmp/ghost_hive/peer.bind` oder `/tmp/ghost_lab/peer.bind`, Datei `0600` |
| Keyd | `/tmp/ghost_keyd` | Root lokal `keyd.root`; Unix-Socket `keyd.sock`; gibt **112 Byte derived** + TTL 900 s; `peer.bind` bleibt Worker-lesbar |
| Forensik | `/tmp/ghost_mon` | liest `vault.bin`, `forensic.log`, Alert danger/down/kill auf stdout |

Peers laden Keys mit `peer_load_keys` (Keyd, sonst Datei). **Kein Root auf dem Worker.**

---

## 4. Watch — Seiten und Labels

Raster: **48×24**, Glyph 9×11, ASCII `0x20`–`0x7E`. Zeile 0 Titel pad 48, Zeile 1 `----`×48.

Titel: `[GHv2] PAGE:<Hive|Kernel|Net|Peer> STATE:Watch|Down`

### 4.1 Hive (page 0)

Headline worst-first: `empty` → `danger` → `down` → `block` → `warn` → `pend` → `ok`.  
Zensus Peers/on/pend/blk/sil, Roles W/P/S/R/N/M, Alert, HMAC-I, Ack, Drop, HBMiss, Last, Time.  
Bis 11 Device-Zeilen, `+N` Overflow.

### 4.2 Kernel (page 1)

PSP-SDK / Clock / Battery / WLAN (Host `--`), IBSS, Kernel, Arming, Vault, Safe, Keys, Auth, Bind, Down, Phase, Snap, Flush, Kill, Policy, Peek, HMAC-I.  
Down-Eintritt: diese Seite zuerst, dann L/R Wrap. `GameLook: off` (Anzeige). Intern darf Radio-Off/Game-Flag 1.7.3 bleiben.

### 4.3 Net (page 2)

IBSS, UDP 17471, Pin `GHSTHIVE`, ReplayW 64, AckBd, IP 10.17.47.1, Scan (HUD-Zahl), **Twin** (≥2× SSID GHSTHIVE), **XAP** (Fremd-APs), **Ap1/Ap2** `ssid r= rssi ch= enc`, Ack, Mines, Replay, Alerts, HBMiss, Drift, WorkerSync, Last, Time.

Scan-HUD merkt **2 APs** nach `releaseBuffer()` (Rohbuffer frei, §12.2). PSP mit IBSS-up: Infrastruktur-Scan würde Adhoc reißen → oft nur Pin-SSID, Twin dann nein.

### 4.4 Peer (page 3)

Leer: `idx: 0/0`.  
Sonst Fokus: DevN id/role, Status, Trust, Last, Age, Drop, **Cap** hex16, **Tag** hex8, **HB** Miss der Fokus-id, RAM/CPU/GPU/Traffic/Battery/WiFi, idx `n/N`.  
Up/Down wrap Fokus. Absent `--`. Kein `PatchTime`/`PatchLoc`.

---

## 5. Controls (v2 Live vs. v1 Debug)

### 5.1 Live-Watch (EBOOT default)

| Taste | Watch idle | O-Black | Down aktiv |
|-------|------------|---------|------------|
| L / R | Seite ± Wrap, Fokus merken | tot | Seite ± Wrap, `STATE:Down` bleibt |
| Up / Down | tot außer Peer Fokus ±1 | tot | tot |
| Square | Rebuild + Blit | tot | Re-Blit; Watch wieder sichtbar wenn hidden |
| Select+Start | tot | tot | tot |
| O | Black | Watch zurück | tot |
| X | tot | tot | tot |
| Home | ExitHive → XMB | tot | hide Watch, Kernel weiter |

Watch startet **kein** Down, **kein** Pair, **keine** Pipeline.

### 5.2 Host-Debug-CLI (`GHOST_DEBUG_CLI`, `test_controls`)

v1-Prompt `ghost_$`, Zahnrad, Lock, Output — **nicht** Live-PSP. Siehe `CONTROLS.md`. `test_controls` wird mit `-DGHOST_DEBUG_CLI=1` gebaut.

Host-Tasten (`GHOST_PSP_KEYS=1`): i/k, j/l, x, o, s=Square, g=Select+Start, `[` `]`=L/R, h=Home.

---

## 6. Datenpfad (unverändert + Telemetrie)

`receive → validate → replay_guard → enrich → classify → policy → priority → fallback → escalation → route → store → ack`

Telemetrie: Magic `0x42`, Felder LE, HMAC-Hex [88..127]. Ungültig → drop, kein Registry-Write, **kein Down**. Mine/NAS dürfen kein `TelemetryUpdate` senden (drop). P16 LogOnly: nie Alert/Kill/Down aus Telemetrie.

HMAC-I / MITM bad MAC: drop; Evidence ≤1/60s; P15 ein Alert nach 3 Fails; **nie ghost_down**.

Ghost Down global: signiertes `GhostDownStart`. HeartbeatMiss allein nicht Down.

---

## 7. PSP-Hardware / Anti-Hack-Grenze (v2 bewusst nicht)

| Thema | v2 |
|-------|-----|
| AES / GCM | **nein** (kein neues Crypto-Modul) |
| Sony-Sign / offizielles EBOOT | **nein** (CFW unsigned PBP) |
| flash0 Tresor | **nein** |
| `peer.bind` AES nur-PSP | **nein** (bricht Pairing; 112 B derived lesbar) |
| Zweites Thread / zweites EBOOT / XMB-Launcher | **nein** |
| Neue Module, States, Capabilities, Policies >P16, Events außer TelemetryUpdate | **nein** |
| BSSID-Pin / IBSS-Merge | nicht ohne Peer-MAC im Frame; Adhoc-Risiko bleibt |
| WLAN-Scan bei IBSS-up | kein Infrastructure-BSS-Scan (würde IBSS reißen) |

Wire-Auth bleibt **HMAC-SHA1**.

---

## 8. Module (Whitelist, kein Zuwachs)

`ghost-core`, `ghost-terminal`, `ghost-scanner`, `ghost-vault`, `ghost-heartbeat`, `ghost-policy`, `ghost-stealth`, `ghost-ir`, `ghost-down`, `ghost-peek`.

PSP-States intern: `game_mode` (Funk-Flag), `terminal_mode`, `ghost_down`, `danger_mode`, `low_power_mode`. Watch ist Display in `terminal_mode`, kein sechster State. `ghost_peek` Transition.

TETACT = Peer-Verhalten (`src/laptop/tetact.cpp`), kein PSP-Modul. Kein WLAN-Monitor-Mode.

---

## 9. Testergebnisse — Kampagne 2026-08-29 (WSL2 Host)

Umgebung: Linux 6.6 WSL2, `g++` C++11, **kein** PSPSDK-Lauf in dieser Session (`make eboot` nicht ausgeführt).  
`make test` setzt `GHOST_DOWN_ARMED=1` (Host-Kill-Pfad in Unit-Tests; Live-PSP bleibt locked).

### 9.1 `make test` — volles Repo

| | |
|--|--|
| Ergebnis | **EXIT 0** |
| Wall | **46.22 s** (`time -p` real) |
| user / sys | 23.29 s / 15.53 s |
| `PASS `-Zeilen | **449** |
| `FAIL `-Zeilen | **0** |

Log: `/tmp/ghost_v2_make_test.log`.

### 9.2 Suite-Köpfe (Auswahl)

| Test | Ergebnis | Hinweis |
|------|----------|---------|
| registry, event_queue, policy, context_priority, replay_guard | PASS | |
| spec | PASS | |
| p36 | PASS | Scanner Game-Mode tot |
| p30 mitm / replay / worker / stick / vault / time / kill / route | PASS | |
| root_config, root_wrap, tetact, peer_halt | PASS | |
| phase_c | PASS n=838 | |
| phase_d | PASS **n=10436** | längster Phasenlauf in `make test` |
| phase_e | PASS n=1744 findings=65 | Findings = Lab-Zählung, kein make-FAIL |
| test_controls | PASS | Debug-CLI, nicht Live-Watch |
| test_watch | PASS | v2 Seiten, Twin/HUD, Cap/Tag/HB, Down |
| test_opsec | PASS | https/s3/`..` tot; 112-Bind ok; kein Root auf Peer |
| telemetry | PASS | Rate, Replay, Mine-drop, Absent |
| security | PASS | HMAC/UDP/Kernel-Role |
| perf | PASS | Heap <24 MB; Watch-Frame; Down endlich |
| ui | PASS | Raster 48×24; 1000× Fokus/Square/Draw/Race/Down/Home |
| test_v2_final | PASS | siehe 9.3 |

Perf-Frame: `frametime_ns_max 103775` (~0.10 ms) gegen Budget 16 ms.  
Replay-Perf an v2 angepasst: Fenster 64, **Vault 8/Peer** → 9. Store `Rejected` (nicht `Blocked`).

### 9.3 `test_v2_final` — Soak (10× UI-1000 + Extra)

| Check | Loops | Ergebnis |
|-------|-------|----------|
| L/R Seiten-Wrap | 10 000 | PASS |
| Peer-Fokus Down | 10 000 | PASS (4 Devices inkl. Mine-Enroll) |
| Square Re-Blit | 10 000 | PASS |
| Peer-Draw Raster | 10 000 | PASS |
| Twin-HUD nach `releaseBuffer` | 1 000 | PASS |
| Down L/R + O/Select+Start/Up/Down tot | 4 000 | PASS |
| Telemetrie-Burst | 10 000 process | **acc=8 blk=9992** (Vault-Cap 8/Peer; Rest Rejected/Blocked) |
| Wall soak | | **1928 ms** |

Konstanten-Asserts: P16=16, ReplayW=64, Telem-Replay=64, ACK=8, Vault 8/64, SSID `GHSTHIVE`, Opsec-Pfad.

X tot, Select+Start tot, O-Stealth, Home idle Exit, Home Down Kernel weiter: PASS.

### 9.4 `make peers`

EXIT 0, **7.74 s**. Binaries:

`/tmp/ghost_laptop`, `ghost_phone`, `ghost_family`, `ghost_router`, `ghost_nas`, `ghost_mines`, **`ghost_keyd`**, **`ghost_mon`**.

### 9.5 Attack-Lab

| Ziel | Ergebnis |
|------|----------|
| `make lab-invariants` | PASS **n=29**, 0.61 s |
| lab `gen_corpus` / `fuzz_smoke` | PASS n=81 (im `lab-test-phase`-Build) |
| `make lab-test-phase` | **STOP Safety-Pre-Check** (kein Kernel-FAIL) |

Safety-STOP (ehrlich):

- **S0.3** — `README.md` erwähnt `10.17.47.1` als *verbotene* Live-IBSS-IP (Dokumentation, kein Lab-Socket).
- **S0.7** — Introspection-Socket-Grep.

Das Lab hat **nicht** 24 h gedreht (`lab-24h` / accelerated bewusst nicht). Sim-only, `127.0.0.1`.

### 9.6 Nicht in dieser Kampagne

- PSP-Hardware / `make eboot` (PSPSDK oft fehlend).
- Echte IBSS mit mehreren APs (Evil-Twin auf Gerät).
- Keyd als Dauer-Daemon + Worker gegen Live-Kernel.
- 24-h-Controller.

---

## 10. Bekannte Grenzen (v2 bleibt so)

1. **`peer.bind` 112 Byte Klartext** auf Stick/`/tmp` — Pairing. Keyd verkürzt Lebensdauer, verschlüsselt nicht.
2. **IBSS Adhoc** — Merge/Evil-Twin radio-seitig; Watch Twin nur wenn Scan ≥2× `GHSTHIVE` liefert.
3. **Vault 8/Peer** — Telemetrie danach `Rejected`; HUD zeigt letzten Stand.
4. **HMAC-SHA1** — Wire, nicht modern AEAD.
5. **Locked EBOOT** — Conceal ohne Kill; Armed nur Test-Env / spätere Noah-Freigabe.
6. **Scanner Host** — 0 BSS ohne PSP-HW; HUD-Tests injizieren Snapshots.
7. **Debug-CLI** vs Live-Watch — zwei Welten; CONTROLS.md beschreibt CLI.

---

## 11. v3 — nur Planung (nicht gebaut)

Noah: v3 nur **Hive als Anti-Hack + Scanner besser**. Kein Scope-Creep in v2.

Kandidaten (müssen später gegen Spec-Whitelist: kein neues Modul/Event/Policy ohne Noah):

- Scanner: Scan-Detail trotz IBSS (ohne Associate, ohne Monitor-Mode-Verbot brechen); Evil-Twin über BSSID wenn Frame es hergibt; Buffer-Disziplin halten.
- Anti-Hack: Replay/HMAC-I Sichtbarkeit; ACK-Starve; Worker-OPSEC härter; Keyd-Rotation die 112 B nicht bricht; Forensik-Signale ohne neues Event.
- Nicht v3-Default: AES-Modul, Sony-Sign, flash0, zweites EBOOT, Game-Look, Cloud.

---

## 12. Freeze-Checkliste

- [x] Watch Hive/Kernel/Net/Peer, `[GHv2]`, Raster 48×24  
- [x] TelemetryUpdate + P16 LogOnly, Absent `--`  
- [x] Controls: L/R Wrap, Peer Up/Down, O Stealth, Home idle/down, Select+Start tot  
- [x] Replay 64, ACK 8/s, Vault 8/64, Pin GHSTHIVE  
- [x] Locked-Down: Snapshot, Funk/ACK aus, kein Kill  
- [x] `peer.bind` 112 lesbar  
- [x] Host keyd + mon + Worker-OPSEC  
- [x] `make test` 449 PASS / 0 FAIL + Soak 10k  
- [x] `hive_redteam` 2001 Angriffe, `make redteam` PASS (T₁/T₂/T₃)  
- [x] `deploy/ghost_hive/` Stick-Layout (`ghost:hive` Gate, kein `root.key`)  
- [x] Kein neues PSP-Modul / kein AES / kein zweiter Thread  

**v2 ready** im Sinne dieses Host-Freeze. Hardware-EBOOT bleibt ein Noah-Schritt (`make eboot` auf PSPDEV).
