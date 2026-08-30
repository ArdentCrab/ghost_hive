# Ghost Hive — SPEC v1 Watch-Patch (Noah, 2026-08-29)

**Status:** Archiv (Watch-v1 Chrome). **Live Watch-Seiten/Tasten:** [`SPEC-v2.md`](SPEC-v2.md) Kap. 6 und 10. Diese Datei gilt nur noch für Glyph-Raster und Game-Look-Frame, soweit v2 schweigt.  
**Kernel:** `SPEC.md` v1.7.3 Archiv — unverändert (Memory, Structs, Policies, Pipeline).  
**Gerät:** PSP-1004, RAM ≤ 24 MB. Lokal, kein Cloud.

Debug-CLI MAY hinter Akkord (`GHOST_DEBUG_CLI`), nicht Live.

---

## 0. Verbote

Kernel MUST NOT geändert werden (keine neue Down-Semantik, keine neuen Event-Typen, Policies, Capabilities, Device-Felder).  
Keine erfundenen Peer-Telemetrien (RAM/CPU/GPU/Traffic/Battery der Peers nicht anzeigen).  
Kein `ghost_config.json`. Watch ist reine Darstellung, keine Steuerlogik.  
Kein Prompt, keine Commands, kein Zahnrad, kein Lock.  
Kein neues Modul, kein neuer PSP-State.

---

## 1. Gesamtbild

Watch läuft in `terminal_mode`. Kernel-Loop non-stop bis Power-Off oder Home-Exit.  
Game-Look: Tarnung, Funk aus, Kernel aktiv, Watch nicht sichtbar.  
Seiten: Hive, Kernel, Net, Peer (Index 0..3) — Layout in SPEC-v2 Kap. 6.  
L/R Wrap. Square = Refresh (kein Clear, kein Block). Select+Start = Watch ↔ Game-Look. Home = Exit. X/O/D-Pad tot.

---

## 2. Raster

480×272. Glyph 9×11 px → **48 Spalten × 24 Zeilen** (24×11=264 < 272). ASCII `0x20`–`0x7E`. Fehlwert `--` (kein UTF-8).

| Zeile | Inhalt |
|-------|--------|
| 0 | Titel |
| 1 | Trennlinie 48× `-` |
| 2–23 | `Label: Wert` |

Titel (aktueller Name, aktueller State), max 48 Zeichen, Rest Spaces:

```
[GHv1] PAGE:System STATE:Watch
```

`PAGE:` `System` \| `Ghost` \| `Network` \| `Devices`  
`STATE:` `Watch` \| `Game` \| `Down`  
`Down` wenn `ghost_down` / `down().isActive()`, sonst `Watch` in Watch-Mode. Game-Look zeichnet **kein** Watch-Titel (Tarn-Frame).

Trennlinie: 48 Bindestriche.

---

## 3. Seiten

### 3.1 System (page 0)

Reihenfolge fest:

```
PSP: 1004
RAM: <=24MB
Clock: 222MHz
Battery: 87% 32C Charging
WLAN: ON
IBSS: GHSTHIVE (ON)
IP: 10.17.47.1
Stick: --
FB: 480x272
UMD: --
Theme: --
Peers: 0/32
Pend: 0
Online: 0
Blocked: 0
Roles: W0 P0 S0 R0 N0 M0
UDP: 17471
Arming: locked
Hive: --
Events: 0
Silent: 0
TelemDrop: 0
```

Clock/Battery/WLAN/Stick: SDK wenn da, sonst `--`. IBSS SSID immer `GHSTHIVE`, `(ON)` nur `hive_net_ready()`, sonst `(OFF)`. IP immer Kernel `10.17.47.1`. UMD/Theme v1 `--`. Keine erfundenen Berechnungen.

Nach Theme: Registry-Zensus (kein erfundenes Peer-RAM). `Peers` = count/`MAX_DEVICES`. `Roles` Worker/Phone/Sensor/Router/Safe/Mine. `Hive` = `id:status` gekürzt (`ok`/`pend`/`deg`/`off`/`blk`/`sil`/`down`/`dng`/`sus`). `Events` = Queue-Größe. `TelemDrop` = Summe `telemDenseDrops` je Device. `Arming` wie Ghost-Seite.

### 3.2 Ghost (page 1)

Reihenfolge fest (IR-RX nur Network; Replay/Heartbeat je eine Zeile):

```
Kernel: active
Scanner: active
Stealth: inactive
Vault: loaded
VaultRAM: 0/64
Keys: not loaded
peer.bind: not loaded
Down: locked
DownTimer: --
DownPhase: idle
Snap: 0
Flush: pending
Kill: not sent
GameLook: off
Arming: locked
Replay: inactive 0 blk=no
Heartbeat: inactive miss=0
Policy: ok
Peek: inactive
Danger: inactive
Time: 2026-08-29 01:23
WorkerSync: --
```

Down: `locked`/`observe`/`idle`/`active` (wie Kernel-Arming + `isActive`).  
DownTimer: ms seit Down-Start (Watch merkt t0), sonst `--`. Kein Kernel-Feld.  
DownPhase: `idle`/`freeze`/`snapshot`/`flush`/`kill` aus bestehendem `DownStep` (Ram/Final→snapshot, NAS/Storage-Wait→flush, Conceal/Stop/Done→freeze, Kill→kill).  
Flush: `done` wenn `storageFlushDone()`, sonst `pending`.  
Kill: `sent` / `not sent`.  
Scanner: active wenn Terminal-Scan erlaubt und nicht blocked und nicht Game-Look.  
Stealth: active wenn Game-Look oder Invisible.  
Vault: `loaded` wenn Vault hängt und nicht `safeMode()`, sonst `not loaded`.  
VaultRAM: `stored/VAULT_RAM_SLOTS` (64).  
Keys: `loaded` wenn `keysAttached()`.  
peer.bind: `loaded` wenn Datei 112 Byte.  
Replay-Zeile: `active|inactive` + Count + `blk=yes|no`.  
Heartbeat-Zeile: `active|inactive` + `miss=N`.  
Policy: `ok` wenn 15 Regeln, sonst `error`.  
Peek: `active` wenn `peekAllowed()`.  
Danger: `active` wenn ein Device `DangerMode`.  
WorkerSync: Literal `trust>=2` nur bei Worker mit trust≥2, sonst `--`.  
Time: Kernel-Unix als `YYYY-MM-DD HH:MM` (UTC).

### 3.3 Network (page 2)

```
IBSS: GHSTHIVE (OFF)
Scan: 0 networks
BT: 0
IR: 0
Alerts: 0
Mines: 0
Replay: 0
Time: 2026-08-29 01:23
Drift: --
HBMiss: 0
WorkerSync: --
Peers: 0/32
Pend: 0
Online: 0
Blocked: 0
Roles: W0 P0 S0 R0 N0 M0
UDP: 17471
Hive: --
Silent: 0
TelemDrop: 0
```

Scan: `N networks` optional `(ssid,ssid)` auf 48 Zeichen gekürzt. BT/IR immer `0`.  
Alerts: Anzahl `AlertSent` in der EventQueue. Mines: Registry-Rolle Mine. Replay: `trackedCount()`.  
Nach WorkerSync: derselbe Registry-Zensus wie System (ohne Arming/Events).  
Drift: Worker `last_seen` minus Kernel-Sekunden, nur trust≥2, Format `+2s` / `-3s`, sonst `--`.

### 3.4 Devices (page 3)

Max 8 Device-Zeilen, Rest `...`:

```
Dev1: id=abc role=Worker trust=3 last=01:22 status=ok
```

role: Worker, Phone, Kernel, Safe, Sensor, Router, Mine.  
status: Online → `ok`, sonst bestehender `stateName`. last: `HH:MM` UTC oder `--`.  
Kein Peer-RAM/CPU/GPU/Traffic/Battery.

Dann:

```
PatchTime: --
PatchLoc: --
```

JSON HUD-only `ms0:/ghost_hive/config/device_patch.json` (Host `/tmp/ghost_hive/config/device_patch.json`), Keys `last_patch_time`, `patch_location`. Kernel nutzt die Datei nicht. Fehlt/Fehler → `--`.

---

## 4. Tasten

Live: [`SPEC-v2.md`](SPEC-v2.md) Kap. 6.6 und 11. Kein Game-Look. O = Stealth (Black). Select+Start tot. Home idle = XMB.

Refresh und Draw lesen denselben Kernel-RAM. Pipeline MUST NOT warten.

---

## 5. Quellen

System: SDK + IBSS/IP-Konstanten + FB.  
Ghost: `buildGhostDown` / `GhostDown` + Scanner/Stealth/Vault/Keys/Datei/Replay/Heartbeat/Policy/Peek/Registry/Time.  
Network: Scanner, Queue, Registry, Replay, Time.  
Devices: Registry, Patch-JSON.

---

## 6. Batches

A Down-HUD-Daten (Getter, kein `execute()`-Umbau) — Ist.  
B `buildWatchPage` + Patch-Read.  
C Watch-Default, Keys, Down→Ghost-Seite.  
D `GHOST_DEBUG_CLI` für `test_controls`; `test_watch.cpp`.  
E `make eboot`. Lab/INV nicht anfassen.

---

## 7. Geltung

1. Diese Datei (UI)  
2. `SPEC.md` v1.7.3 (Kernel)  
3. Noah  
