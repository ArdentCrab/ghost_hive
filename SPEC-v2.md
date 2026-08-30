# Ghost Hive — SPEC v2 (Live-Norm)

**Status:** Live-Gesetz (Noah 2026-08-29).  
**Archiv UI/Kernel-Overlay v1:** [`SPEC-v1.md`](SPEC-v1.md)  
**Archiv MVP:** [`SPEC.md`](SPEC.md) v1.7.3  
**Gerät:** PSP-1004, RAM ≤ 24 MB, lokal, kein Cloud.

v2 ändert **nur** das hier Benannte. Alles übrige (Module, States, Down-Semantik, HMAC-I, Arming locked, Watch-Tasten außer Peer-Fokus, P01–P15-Wirkungen) bleibt v1 / 1.7.3.

---

## 1. Delta zu v1 (knapp)

| # | v1 | v2 |
|---|----|----|
| D1 | Device ohne Peer-Messwerte | sechs Telemetrie-Felder + Absent-Sentinel |
| D2 | Event-Typen = 1.7.3-Liste | **ein** neuer Typ: `TelemetryUpdate` |
| D3 | P01–P15, Policy-ok = 15 Regeln | **P16** `telemetry_update`, LogOnly; Policy-ok = 16 |
| D4 | Watch Devices: eine Zeile/Device, kein Peer-RAM; Patch-JSON HUD | Vier Seiten Hive/Kernel/Net/Peer; Peer = Fokus + Status/Trust/Last/Age/Drop + Cap/Tag/HB + 6 Messzeilen; **kein** Patch-JSON |
| D5 | D-Pad tot auf allen Watch-Seiten | D-Pad **Up/Down nur auf Peer** (`page==3`) = Fokus ±1; sonst tot |
| D6 | Titel `[GHv1]` | Titel `[GHv2]` |
| D7 | Telemetrie erfinden verboten; Anzeige `--` | Anzeige nur Registry nach validem Event; sonst `--` |
| D8 | Kernel MUST NOT neue Events | Kernel **darf** D1–D3; sonst unverändert |
| D9 | Down: Black-Screen / Game-Look-Frame, UI tot | Watch bleibt, Eintritt Kernel, `STATE:Down` rot; L/R Wrap; O tot; Select+Start tot; Square Re-Blit; Home hide |

**Unverändert:** Module, PSP-States intern, Down-**Pipeline**, HMAC-I, kein Auto-Enroll, Mines send-only, Worker blockiert nicht, Sensor schreibt nicht, NAS alarmiert nicht, Router konfiguriert nicht, kein `ghost_config.json`, kein neues Modul, Watch kein sechster State, EBOOT Arming locked, IBSS/IP/UDP 17471, TETACT kein PSP-Modul. **Kein Game-Look-Framebuffer.** Down: Funk aus (`radioOff`); `isGameMode` MAY intern true (1.7.3 Funk-Flag); Watch zeigt `GameLook: off`. Live-UI: nur Watch + O-Stealth.

---

## 2. Device-Struct (Telemetrie)

Bestehende Felder unverändert (`id[32]`, `role`, `capability_mask`, `trust_level`, `last_seen`, `status`, `tag_mask`).

**Zusatz, packed, little-endian in RAM:**

| Feld | Typ | Gültig | Absent |
|------|-----|--------|--------|
| `ram_mb` | `uint16` | 0…65534 (MiB, ganzzahlig abgerundet) | `0xFFFF` |
| `cpu_percent` | `uint8` | 0…100 | `0xFF` |
| `gpu_percent` | `uint8` | 0…100 | `0xFF` |
| `traffic_kbps` | `uint16` | 0…65534 (kbit/s, ganzzahlig) | `0xFFFF` |
| `battery_percent` | `uint8` | 0…100 | `0xFF` |
| `wifi_mbit` | `uint16` | 0…65534 (Mbit/s, ganzzahlig) | `0xFFFF` |

**Init:** alle sechs = Absent.  
**0 ist Messwert**, nicht „fehlt“.  
**101…254** bei Prozent-Feldern: Payload ungültig (Kap. 3).  
**65535** nur Absent, nie als Messwert speichern.

Kernel-PSP ist **kein** Registry-Peer: diese Felder existieren nicht für `kernel`. Watch System-Seite bleibt PSP-SDK, nicht Device-Telemetrie.

---

## 3. Event `TelemetryUpdate`

### 3.1 Typ

`EventType::TelemetryUpdate` = nächster Enum-Wert nach `MineEvent` (**18** / `0x12`).  
MUST NOT den UDP-Opcode `0x42` als `Event.type` verwenden (kollidiert nicht mit der bestehenden Enum-Reihenfolge). `0x42` ist nur **Payload-Magic**.

`Event.severity` nach Enrich: immer `Info`. Andere Werte vom Peer: clamp auf `Info`.

`Event.payload` bleibt 128 Byte. Kein JSON für dieses Event.

### 3.2 Payload-Layout (128 Byte)

| Offset | Größe | Inhalt |
|--------|-------|--------|
| 0 | 1 | Magic `0x42` |
| 1–2 | 2 | `ram_mb` LE |
| 3 | 1 | `cpu_percent` |
| 4 | 1 | `gpu_percent` |
| 5–6 | 2 | `traffic_kbps` LE |
| 7 | 1 | `battery_percent` |
| 8–9 | 2 | `wifi_mbit` LE |
| 10–87 | 78 | `0x00` (signierter Body nach den Feldern; inkl. AUTH-Nullen 84–87) |
| 88–127 | 40 | HMAC-Hex wie 1.7.3 (`VAULT_MAC_OFF`); nicht Teil der Messwerte |

HMAC/Frame-Auth wie bestehende Events (`payload` inkl. Magic in der signierten Region wie 1.7.3). Ungültige MAC: drop, P15-Pfad, **nie** Down, **kein** Registry-Write.

### 3.3 Wer darf senden

| Rolle | Pflichtfelder (nicht Absent) | Übrige |
|-------|------------------------------|--------|
| Worker | alle sechs | — |
| Phone | `ram_mb`, `cpu_percent`, `battery_percent`, `wifi_mbit` | GPU, Traffic = Absent |
| Sensor | `cpu_percent`, `battery_percent` | Rest Absent |
| Router | `traffic_kbps`, `wifi_mbit` | Rest Absent |

Mine, Safe/NAS, Kernel: MUST NOT `TelemetryUpdate` senden. Empfang → drop, kein Store.

Pflichtfeld Absent oder out-of-range: ganzes Event ungültig → drop, kein Teil-Update.

### 3.4 Frequenz

Soll: **1–5 s** zwischen gültigen Sends desselben `source_device_id`.  
Dichter als 1 s: drop, **interner Zähler** `dense_drops` pro Device, **kein** Alert, **kein** Down.  
Rate-Check: Kernel-`now` **und** `Event.timestamp` (beide ≥ 1 s zum letzten gültigen Accept).  
Locker als 5 s: letzter gültiger Stand bleibt in der Registry; Watch zeigt den; **kein** `DeviceLost` allein deshalb.

### 3.5 Replay

Eigenes Fenster **32** Einträge pro `source_device_id` (nicht Mine-Counter). Schlüssel: `(source_id, timestamp, payload[0..9])`.  
Treffer: drop, Mine-Block **nicht** anwenden (kein Mine).  
Gültiges Event: `last_seen = Event.timestamp` (nach bestehendem Zeitanker: Worker-Zeit nur trust ≥ 2, sonst Kernel-Zeit).

---

## 4. Pipeline

Reihenfolge unverändert:

receive → validate → replay_guard → **enrich** → classify → policy → priority → fallback → escalation → route → store → ack

### 4.1 validate

Zusätzlich für `TelemetryUpdate`: Magic, Padding 0, Werte-Range, Rolle darf senden, Device **bereits** in Registry (kein Enroll). Sonst drop.  
`Pending`: weiterer Frame drop (kein `pairDevice`, kein Freeze). Pair nur explizit. HMAC-Hex in `payload[88..127]` nicht als C-String kürzen.

### 4.2 enrich — `handleTelemetryUpdate` / `updateDeviceTelemetry`

Nur nach validate+replay OK:

1. `validateTelemetryPayload` (Layout + Range + Rollen-Pflicht).  
2. `updateDeviceTelemetry(id)` schreibt die sechs Felder (Absent bleibt Absent, gültige Werte überschreiben).  
3. Kein `trust_level`-, `role`-, `capability_mask`-, `tag_mask`-Change.  
4. `status` unverändert (kein Online-Zwang).

### 4.3 classify

Klasse normal, `severity = Info`. MUST NOT `AnomalyDetected` ableiten.

### 4.4 policy

Nur P16 für diesen Typ (Kap. 5). Extra-Flags **0**.

### 4.5 Was Telemetrie nie darf

- `GhostDownStart` / Down / Kill / Backup / Alert / Block auslösen  
- HMAC-I ersetzen oder P15 ersetzen  
- Auto-Enroll, Role-Upgrade  
- Pipeline blockieren, Watch blockieren  
- HeartbeatMiss ersetzen  

`store`: wie andere akzeptierte Events (Vault), ohne Flush-Zwang extra.

---

## 5. Policy P16

| Feld | Wert |
|------|------|
| id | `P16` |
| name | `telemetry_update` |
| scope | 0 (device) |
| condition | `type=TelemetryUpdate` |
| action | `LogOnly` |
| extra | 0 |

P01–P15 unverändert. Telemetrie-Schwellwerte (CPU 90 % usw.) **existieren nicht** in v2.  
Watch Policy-Zeile: `ok` bei **16** Regeln, sonst `error`.

---

## 6. Watch-UI v2 (Hive-Overview)

Live-Default ist Watch. **Kein Prompt, keine Commands.** Debug-CLI nur hinter `GHOST_DEBUG_CLI`.

Raster unverändert: 48×24, Glyph 9×11, ASCII `0x20`–`0x7E`, Fehlwert `--`. Zeile 0 Titel pad 48, Zeile 1 `----`×48, Zeile 2–23 Body. Kein `ghost_config.json`, kein Patch-JSON, keine erfundenen Peer-Messwerte.

Seiten Index 0…3 Wrap: **Hive** · **Kernel** · **Net** · **Peer**.

Titel exakt (Rest Spaces):

```
[GHv2] PAGE:Hive STATE:Watch
```

`PAGE:` `Hive` \| `Kernel` \| `Net` \| `Peer`  
`STATE:` `Watch` (idle) \| `Down` (`down().isActive()`). O-Stealth zeichnet **keinen** Watch-Titel (leerer Frame). Select+Start tot. Kein Game-Frame.

Policy-ok = **16**. D-Pad Up/Down **nur** `page==3` (Peer).

### 6.1 Quellen (nur Kernel-RAM)

| Label | Quelle |
|-------|--------|
| Peers / Pend / Online / Blocked / Silent / Roles / Roster | Registry |
| Status-Kürzel | `ok` Online, `pend` Pending, `deg` Degraded, `off` Offline, `blk` Blocked, `sil` Silent, `down` GhostDown, `dng` DangerMode, `sus` Suspected, `?` Unknown |
| Age | `now_sec − last_seen`; `last_seen==0` → `--` |
| HMAC-I | Transport-Fenstercount. **Nie** Down. Alert-Zeile `hmac-i` wenn P15-Schwelle |
| Ack | Anzahl offener ACK-Pending |
| Drop | Summe `telemDenseDrops`; Peer-Seite nur Fokus-id |
| HBMiss | Summe Heartbeat-Miss; Peer-Seite nur Fokus-id |
| Last | jüngstes Queue-Event: Kurzname + Source-id (max 8) |
| Time | Kernel-Unix `YYYY-MM-DD HH:MM` UTC |
| Clock/Battery/WLAN | PSP-SDK, sonst `--` |
| IBSS | `GHSTHIVE (ON)` nur `hive_net_ready()`, sonst `(OFF)` |
| UDP / IP | `17471` / `10.17.47.1` |
| Scan | HUD-Snapshot `hudWifiCount` (max 2 APs nach `releaseBuffer`) |
| Twin / XAP / Ap1 / Ap2 | ≥2× SSID `GHSTHIVE` → Twin yes; Fremd-APs; RSSI/Kanal/Enc |
| Arming | `locked` / `observe` / `armed` |
| VaultRAM | `stored/64` |
| Safe / Keys / Auth / Bind | Vault `safeMode` / `keysAttached` / `authBound` / peer.bind 112 B |
| Down / Phase / Snap / Flush / Kill | GhostDown |
| Kernel / Scan / Stealth / Peek / Danger | running, Scanner-terminal, Game\|Invisible, `peekAllowed`, Registry DangerMode |
| Drift / WorkerSync | Worker trust≥2: `last_seen−now` als `+Ns`/`-Ns`; Literal `trust>=2` sonst `--` |
| Alerts / Mines / Replay | Queue `AlertSent`, Rolle Mine, Replay `trackedCount` + blocked-any |
| RAM…WiFi | Registry-Telemetrie; Absent `0xFFFF`/`0xFF` → `--`. Einheiten `MB` `%` `KB/s` `Mbit/s` |

Hive-Headline (worst): `empty` → `danger` → `down` → `block` → `warn` (sil/sus/deg/off) → `pend` → `ok`.

`Alert:` erste Treffer: Down aktiv `down` · sonst `danger <id>` · `block <id>` · HMAC alerted `hmac-i` · Vault safe `vault-safe` · sonst `--`.

Last-Kurz: `Scan` `Seen` `Lost` `Prof` `Anom` `PViol` `Bak` `Alert` `HBeat` `HMiss` `Role` `Cfg` `GDStart` `GDEnd` `Peek` `Dng+` `Dng-` `Mine` `Telem` sonst `?`.

### 6.2 Hive (page 0) — Overview

Zehn Kopfzeilen, dann bis zu **11** Device-Zeilen (worst-first), bei `count>11` letzte Zeile `+N`. `count==0`: keine Device-Zeilen. Rest Spaces.

Device-Zeile: `id`(≤8) + Rolle + Status + `age=` (48 Zeichen max).

**Leer (Boot, keine Peers):**

```
Hive: empty
Peers: 0/32 on=0 pend=0 blk=0 sil=0
Roles: W0 P0 S0 R0 N0 M0
Alert: --
HMAC-I: 0
Ack: 0
Drop: 0
HBMiss: 0
Last: --
Time: 2026-08-29 17:01
```

**Drei Pending (W Worker, P Phone, F Sensor), Queue leer:**

```
Hive: pend
Peers: 3/32 on=0 pend=3 blk=0 sil=0
Roles: W1 P1 S1 R0 N0 M0
Alert: --
HMAC-I: 0
Ack: 0
Drop: 0
HBMiss: 0
Last: --
Time: 2026-08-29 17:01
W        Worker pend age=--
P        Phone pend age=--
F        Sensor pend age=--
```

**Worker online + gültige Telemetrie, Last=Telem:**

```
Hive: ok
Peers: 3/32 on=1 pend=2 blk=0 sil=0
…
Alert: --
Last: Telem W
W        Worker ok age=2s
```

**Danger auf Phone, HMAC-I 3 alerted:**

```
Hive: danger
Alert: danger P
HMAC-I: 3
P        Phone dng age=1s
```

### 6.3 Kernel (page 1) — PSP + Vault + Down

Idle (Arming locked, Vault leer):

```
PSP: 1004
Clock: --
Battery: --
WLAN: --
IBSS: GHSTHIVE (OFF)
Kernel: active
Arming: locked
Vault: 0/64
Safe: no
Keys: no
Auth: no
Bind: no
Down: locked
Phase: idle
Snap: 0
Flush: pending
Kill: no
Policy: ok
Peek: off
HMAC-I: 0
```

Host: Clock/Battery/WLAN `--`. PSP-SDK: MHz, `87% 32C Charging`, WLAN ON/OFF. Down-View: `locked`/`observe`/`idle`/`active`. Phase idle wenn Down inaktiv.

### 6.4 Net (page 2) — Funk

```
IBSS: GHSTHIVE (OFF)
UDP: 17471
Pin: GHSTHIVE
ReplayW: 64
AckBd: 0/8
IP: 10.17.47.1
Scan: 0
Twin: no
XAP: 0
Ap1: --
Ap2: --
Ack: 0
Mines: 0
Replay: 0
Alerts: 0
HBMiss: 0
Drift: --
WorkerSync: --
Last: --
Time: 2026-08-29 17:01
```

`Scan:` HUD-Zahl (2 APs merken, Raw-Buffer frei). `Twin: yes` nur wenn ≥2 SSIDs `GHSTHIVE`. `Ap1`/`Ap2`: `ssid r= rssi ch= enc`. `Replay:` tracked; `blk=yes` nur wenn mindestens eine Mine blocked, sonst weggelassen. Kein BT/IR-Theater (`0`).

### 6.5 Peer (page 3) — Fokus

`count==0`:

```
idx: 0/0
```

Sonst ein Block, Rest Spaces:

```
DevN: id=W role=Worker
Status: pending
Trust: 2
Last: --
Age: --
Drop: 0
Cap: 0000
Tag: 00
HB: 0
RAM: --
CPU: --
GPU: --
Traffic: --
Battery: --
WiFi: --
idx: 1/3
```

`DevN`: N=`focus+1`. `id=` volle id bis Zeile 48. Status: Online → `ok`, sonst `pending`/`degraded`/`offline`/`unknown`/`suspect`/`blocked`/`down`/`danger`/`silent`. Last: `HH:MM` UTC oder `--`. `Cap:`/`Tag:` hex aus `capability_mask`/`tag_mask`. `HB:` `getMissCount(id)`. Messzeilen wie Kap. 2; Absent `--`.

Nach gültigem Worker-Telem (512/12/4/32/87/54): `RAM: 512MB` … `WiFi: 54Mbit/s`. `Drop:` `telemDenseDrops(id)`.

**Kein** `PatchTime` / `PatchLoc`. `device_patch.json` MUST NOT.

### 6.6 Tasten

| Taste | Watch idle | O-Stealth (Black) | Down aktiv |
|-------|------------|-------------------|------------|
| L / R | Seite −/+ Wrap, Fokus merken. PSP: erster Edge sofort, dann 180 ms Pause (kein Skip) | tot | Seite −/+ Wrap. `STATE:Down` bleibt |
| Up / Down | tot außer Peer: Fokus ±1; count=0 tot | tot | tot |
| Square | Rebuild + Blit | tot | Re-Blit, Watch wieder ein wenn hidden |
| Select+Start | Tap tot. **Halten 3 s nur PAGE:Kernel + STATE:Watch** → `down().execute` (signierter Kill-Pfad folgt der Engine, Arming locked = kein KillFrame). Sonst tot | tot | tot |
| O | Black (Stealth) | Watch zurück | tot |
| X | tot | tot | tot |
| Home | `ExitHive` → XMB | tot | Watch hidden, Down weiter, kein XMB |

Watch steuert **keine** Pipeline, **kein** Pair. Down-Start **nur** Select+Start **halten 3 s** auf `PAGE:Kernel` + `STATE:Watch` (`down().execute`). Kein Auto-Down aus Telemetrie, HBMiss, Replay, HMAC-I, Mine, Vault-safe, Peer-Frames.

Titelzeile Alarm (kein neuer State): `watch_danger_headline` = Blocked ∪ DangerMode ∪ Replay-blk ∪ Twin ∪ hbMissSum≠0 ∪ telem-drops ≠0 ∪ Online-Telem-Peer mit allen sechs Feldern `--`. Pulse nur Watch, nicht Down. Sound/LED nur Viewer, steigende Flanke (`prevAlarm`/`nowAlarm`), nicht PSP-Engine, nicht Kill-Trigger.

### 6.7 Live / Home / Stealth

Jeder Draw liest Kernel-RAM. Kein Delay. Keine erfundenen Werte.

| Kontext | Home | O |
|---------|------|---|
| Idle Watch | `ExitHive` (`running_` false), zurück XMB. Kein Down, kein Kill, kein Freeze | Black. Kernel, Telemetrie, Pipeline weiter. Funk bleibt. Nur O zurück |
| GhostDown aktiv | `watch_hidden`, `running_` true | tot |

Debug-CLI (`GHOST_DEBUG_CLI`): Host-only, nicht Live-EBOOT. Live: Watch, kein Prompt.

---

## 7. Kompatibilität v1 ↔ v2

| Peer | Sendet `TelemetryUpdate` | Registry | Watch | Pipeline |
|------|--------------------------|----------|-------|----------|
| v1 Worker/Phone/Sensor/Router | nein | Felder bleiben Absent | `--` | kein Fehler, kein Alert, kein Down |
| v2 Peer, gültig | ja, 1–5 s (`TELEM_INTERVAL_SEC`, Host `/proc`) | Update | Zahlen | P16 LogOnly |
| v2 Peer, bad magic/range/role | drop | unverändert | letzter Stand oder `--` | kein P16 |
| Mine / NAS | Event drop | — | — | — |

v1-EBOOT mit v2-Peer: Event-Typ 18 unbekannt → bestehendes validate drop (kein Crash).  
v2-EBOOT mit v1-Peer: Kap. 7 Tabelle Zeile 1.

Wire anderer Events unverändert. Peer-Bind 112 B unverändert.

---

## 8. PSP-Namen (kein neues Modul)

In `ghost-core` (oder Pipeline-Datei, nicht neues Modul):  
`validateTelemetryPayload` · `updateDeviceTelemetry` · `handleTelemetryUpdate` (Enrich-Einstieg).

`ghost-output` Watch: Kap. 6 (`buildWatchPage`).  
`ghost-policy` `initDefaults`: P16, `ruleCount=16`.  
`ghost-terminal`: Fokus-Index, `[GHv2]`, D-Pad nur Peer.  
`ghost-transport`: HMAC-I-Count und ACK-Pending nur Getter für Watch.

---

## 9. Geltung

1. Diese Datei  
2. `SPEC-v1.md` wo v2 schweigt (Glyph-Raster, Game-Look-Chrome)  
3. `SPEC.md` v1.7.3 Bit-Reste  
4. Noah  

Watch-Seiten, Tasten, Down-HUD, Single-EBOOT, Härtung: Kap. 6, 10, 11, 12.

---

## 10. GhostDown v2 — Live-Anzeige (kein Black-Screen)

Kein neuer PSP-State. `down().isActive()` steuert nur Watch. Pipeline unverändert.

### 10.1 Muss

- PSP bleibt an, Kernel-Loop läuft, `down().tick` weiter.
- Watch **sichtbar** (kein `TermMode::GhostDown`-Leerframe, kein Game-Look-Chrome).
- `watch_page = 1` (Kernel) **nur beim Eintritt**, kein Delay. Danach L/R Wrap (Hive/Kernel/Net/Peer). Nicht jede Tick zurück auf Kernel.
- Titel pad 48: `[GHv2] PAGE:<Seite> STATE:Down` (Seite = aktuelle Watch-Seite; Eintritt `Kernel`).
- Zeile 0 **rot** (`pspDebugScreenPutChar` Farbe `0x000000FF`); Zeilen 1–23 grün `0x0000FF00`.
- Eingaben: Square (Re-Blit), Home, **L/R**. Up/Down, X, **O**, D-Pad, Select+Start **tot**.
- Kein Game-Framebuffer.
- Peer-Fokus **gesperrt** (Up/Down tot). Registry-Anzeige auf Peer ist der eingefrorene Stand.
- Square: Rebuild aus Kernel-RAM, kein Eingriff in Down.
- Home: **Watch aus** (`watch_hidden`), `running_` bleibt true, Down läuft weiter. **Kein XMB**, kein Kill. Square danach: Watch wieder ein (Re-Blit). Down Ende: Watch wieder ein, letzte Seite, Titel grün `STATE:Watch`. Idle-Home: Kap. 6.7 `ExitHive` (XMB).

### 10.2 Kernel-Body solange Down aktiv und `page==1`

Ersetzt die idle Kernel-Seite. Reihenfolge fest:

```
Down: active
Timer: N
Phase: freeze
Snap: N
Flush: pending
Kill: no
Arming: locked
GameLook: off
Vault: 0/64
HMAC-I: 0
Hive: ok
Peers: 0/32
Alert: down
Replay: 0
Danger: off
Policy: ok
```

`Timer:` ms seit Down-Start (Watch t0).  
`Phase:` `freeze`/`snapshot`/`flush`/`kill` (DownStep-Mapping wie idle).  
`GameLook:` immer `off` (Anzeige).  
`Hive`/`Peers`/`Alert` wie Kap. 6.2 (Headline/Zensus; Alert während Down mindestens `down`).  
`Replay:` trackedCount. `Danger:` `on` wenn Registry DangerMode, sonst `off`.  
`Policy:` `ok` bei 15 oder 16 Regeln, sonst `error`.

Hive / Net / Peer während Down: idle-Layout Kap. 6, **eingefrorener** Registry/Vault-Stand. Scanner/Funk aus; keine neuen Messwerte.

### 10.3 Darf nicht

Black-Screen als Down-Darstellung. UI-Freeze. PSP-Prozess killen. Home während Down = XMB. Select+Start. O während Down. Erfundene Phasen. Zweites EBOOT / Game-Frame / XMB-Overlay.

---

## 11. Single-EBOOT Anti-Hack (Noah)

Ghost Hive v2 ist **ein** PSP-EBOOT. Die PSP führt immer nur diesen Prozess aus. Ghost ersetzt jedes Spiel, jedes Plugin und jede parallele App. Aktiv, solange die PSP an ist **und** Ghost läuft. Kein Multitasking-OS.

**UI:** nur Watch (Kap. 6). Vier Seiten Hive / Kernel / Net / Peer. L/R Wrap. Peer Up/Down. Titel `[GHv2] PAGE:<Name> STATE:Watch` oder `STATE:Down` (rot). Kein Live-Terminal, kein Prompt, kein v1-Frame.

**O-Stealth:** einziger Tarn-Modus. Frame schwarz. Ghost, Telemetrie, Pipeline weiter. Funk bleibt (kein `enterGameMode`). Nur O zurück. Kein Shutdown, kein Frame-Switch, kein XMB, kein Game-Mode.

**Down:** Watch-Zustand. Kap. 10. **Locked:** Funk aus, Scanner aus, ACK aus, RAM-Snapshot, **kein** Kill, **kein** `applyGlobalDown`. Armed: voller Kill-Pfad.

**Kein Game-Mode:** Select+Start tot. Kein Tarn-Game, kein zweiter Framebuffer, kein Plugin neben einem ISO.

**Home idle:** Exit EBOOT, zurück XMB. Kein Down, kein Kill, kein Freeze. **Home Down:** Watch hide, Ghost weiter.

Intern darf Down weiter `stealth.enterGameMode()` / `radioOff` setzen (1.7.3 Funk-Flag). Das ist **kein** UI-Game-Mode.

---

## 12. Härtung (Noah) — machbar vs. PSP-Grenze

HMAC-SHA1 bleibt Wire-Auth. **Kein AES** im Tree (kein AES-CTR/GCM ohne neues Crypto-Modul). IBSS bleibt Ad-hoc `GHSTHIVE` (kein WPA2-Client).

| Maßnahme | v2 |
|----------|----|
| SSID-Pin | Nur Create `GHSTHIVE`. Watch `Pin: GHSTHIVE`. Kein fremdes SSID-Join |
| BSSID-Pin / Merge | Nicht ohne Peer-MAC im Frame. Adhoc-Merge bleibt IBSS-Risiko |
| peer.bind AES-GCM / nur PSP | **Nein.** Peers müssen 112 B ableiten. Host `/tmp/…/peer.bind`. PSP `ms0:/ghost_hive/k/peer.bind`. Wrap ohne Peer-Key bricht Pairing |
| Locked-Down Funk/Scan/ACK | **Ja.** `execute` auch locked: Conceal + Snapshot, `kernelDown` (kein TX/ACK), kein Kill |
| Telem Rate-Limit | schon ≥1 s / Peer |
| Replay-Window | **64** / Mine und Telemetrie-Replay 64 |
| ACK-Budget | **8 / Sekunde**, Watch `AckBd` |
| Vault-Slots / Peer | **8** von 64 |
| Worker-MAC-Pin, Key-Rotation, Vault-Sync | kein neues Event, kein Device-Feld |
| OS-Halt nur Root-Mismatch | Kill bleibt Root+Snapshot+Armed |
| Versteckter Ordner / flash0 bind | Homebrew schreibt Stick. `flash0` Sony |
| EBOOT Sony-Signatur | **Nein.** CFW lädt unsigned PBP |

Watch Net: `Pin`, `ReplayW: 64`, `AckBd: n/8`, `Twin`, `XAP`, `Ap1`/`Ap2`. Kernel: `Radio: on\|off`.

**Host-only (kein EBOOT, kein neues PSP-Modul):** `ghost_keyd` Unix-Socket `/tmp/ghost_hive/keyd.sock` hält Root lokal und gibt nur 112-Byte `peer.bind` + TTL (900 s). Worker liest weiter Klartext-112. `ghost_mon` liest `vault.bin` → `/tmp/ghost_hive/forensic.log`. Kein Cloud-Pfad für Bind (`://` tot).

