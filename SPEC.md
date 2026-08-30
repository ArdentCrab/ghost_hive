# 🕸️ GHOST HIVE — Ultra‑Frozen Hardware Specification v1.7.3

**Status:** Frozen **MVP-Archiv** (2026-08-29). Nicht mehr die Live-Produktspec.  
**Live:** [`SPEC-v2.md`](SPEC-v2.md). UI-v1-Archiv: [`SPEC-v1.md`](SPEC-v1.md).  
**Kern:** PSP‑1004 (FAT)  
**Typ:** Personal Adaptive Security Mesh  
**Sprache:** Normativ (MUST / SHOULD / MAY)

---

## 1. Systemüberblick

Ghost Hive ist ein lokales, mobiles Sicherheitssystem aus mehreren Geräten. Die PSP‑1004 ist der unsichtbare Kern. Alle anderen Geräte sind Scanner, Analysatoren, Backups oder Alarmgeber.

Das System arbeitet ohne Cloud, ohne Abo, ohne fremde Server. Alle Daten bleiben lokal und verschlüsselt.

---

## 2. Systemgrenzen & Nicht‑Ziele

### 2.1 Systemgrenze

Das System endet an der lokalen Netzwerkgrenze.

### 2.2 Verbote

- PSP MUST NOT aktiv scannen im Game‑Mode
- PSP MUST NOT neue Geräte automatisch aufnehmen
- Worker MUST NOT blockieren
- Sensor MUST NOT schreiben
- NAS MUST NOT alarmieren
- Router MUST NOT konfigurieren
- Kein Gerät außer PSP MUST Entscheidungen treffen
- Keine Kommunikation zu fremden Servern
- Minen MUST NOT einen Rückkanal besitzen
- Minen MUST NOT Anfragen beantworten
- PSP MUST NOT als WPA2/WPA3-Client agieren

### 2.3 Nicht‑Ziele

- Kein Ersatz für Suricata/Snort
- Kein Enterprise‑SIEM
- Kein Virenscanner
- Kein Cloud‑Produkt
- Kein Botnetz
- Kein Angriffswerkzeug
- Kein Massenprodukt
- Kein WLAN-Monitor-Mode
- Kein NTP-Server
- Kein High-Speed-Storage

---

## 3. Geräteklassen

| Klasse | Gerät | Funktion |
|--------|-------|----------|
| Kernel | PSP‑1004 | Entscheidung, Koordination, IR, Vault |
| Worker | Laptop Noah | Analyse, Rechenpower |
| Sensor | Laptop Familie, Phone Familie | Scan, Rohdaten |
| Safe | NAS | Backup, Forensik |
| Netz | Router | Netz-Events |
| Mine | Ghost‑Minen | lokale Sensor‑Geister |

---

## 4. Ghost Minen

### 4.1 Definition

Dumme, lokale Sensoren. Sie senden nur in eine Richtung: zur PSP.

### 4.2 Eigenschaften

- lokal
- dumm
- einseitig
- kein Rückkanal
- keine Entscheidungslogik
- keine Policies
- keine Schreibrechte
- keine Empfangslogik

### 4.3 Fähigkeiten

- scannen
- sammeln
- Muster erkennen
- Events senden
- still sein

### 4.4 Platzierung

| Ort | Mine |
|-----|------|
| OS-Nähe | Syscalls, Prozesse, Dienste |
| Browser-Nähe | Tabs, Verbindungen, Crashes |
| Router-Nähe | DHCP, ARP, Clients |
| NAS-Nähe | Dateizugriffe |
| IoT-Nähe | Requests, States |

---

## 5. Rollenmodell

| Gerät | Rolle |
|-------|-------|
| PSP | kernel |
| Laptop Noah | worker |
| Phone Noah | mobile alert + sensor |
| Laptop Familie | sensor only |
| Phone Familie | sensor only |
| NAS | safe |
| Router | net sensor |
| Minen | sibling |

---

## 6. Berechtigungsmodell

| Recht | Bedeutung |
|-------|-----------|
| W | schreiben |
| R | lesen |
| X | ausführen |
| D | entscheiden |
| C | konfigurieren |
| B | Backup |
| A | Alarmieren |

---

## 7. Berechtigungsmatrix

| Gerät | W | R | X | D | C | B | A |
|-------|---|---|---|---|---|---|---|
| PSP | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Laptop Noah | ⚠️ | ✅ | ✅ | ❌ | ❌ | ❌ | ✅ |
| Phone Noah | ❌ | ⚠️ | ✅ | ❌ | ❌ | ❌ | ✅ |
| Laptop Familie | ❌ | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ |
| Phone Familie | ❌ | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ |
| NAS | ✅ | ⚠️ | ✅ | ❌ | ❌ | ✅ | ❌ |
| Router | ❌ | ⚠️ | ✅ | ❌ | ❌ | ❌ | ❌ |
| Mine | ❌ | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ |

---

## 8. Gerätezustände

online, degraded, offline, unknown, suspected, blocked, pending, ghost_down, danger_mode, silent

---

## 9. Zustandsübergänge

### Erlaubt

- online → degraded
- degraded → offline
- offline → pending
- pending → online
- unknown → suspected
- suspected → blocked
- suspected → online
- terminal_mode → ghost_down
- ghost_down → danger_mode
- danger_mode → online
- silent → active
- active → silent

### Verboten

- game_mode → danger_mode
- ghost_down → game_mode
- worker → kernel
- sensor → worker
- safe → kernel
- Mine → worker
- Mine → kernel

---

## 10. PSP‑State‑Machine

Zustände:

- game_mode
- terminal_mode
- ghost_down
- danger_mode
- low_power_mode

Übergänge:

- game_mode → terminal_mode
- terminal_mode → game_mode
- terminal_mode → ghost_down
- ghost_down → danger_mode
- danger_mode → terminal_mode
- danger_mode → ghost_down
- ghost_down → low_power_mode
- low_power_mode → ghost_peek
- ghost_peek → low_power_mode

### low_power_mode

- Display-Off
- CPU-Minimal
- WLAN-Off
- Timer-Wakeup

---

## 11. Module

### PSP

- ghost-core
- ghost-terminal
- ghost-scanner
- ghost-vault
- ghost-heartbeat
- ghost-policy
- ghost-stealth
- ghost-ir
- ghost-down
- ghost-peek

### Laptop

- hive-worker
- hive-analyzer
- hive-log-client
- hive-alert
- hive-kill

### Phone

- hive-sensor
- hive-alert

### NAS

- hive-safe
- hive-index
- hive-access

### Minen

- ghost-mine-os
- ghost-mine-browser
- ghost-mine-router
- ghost-mine-nas
- ghost-mine-iot

---

## 12. Modulpflichten

### ghost-core

- Registry verwalten
- Rollen berechnen
- Policies prüfen
- Events routen
- Prioritäten berechnen
- Fallbacks auslösen
- Heartbeats verwalten
- Replay-Schutz anwenden

### ghost-scanner

- aktive WLAN-Scans im Terminal-Mode
- Bluetooth-Scans
- IR-Scans
- Probe Requests erzeugen (minimal sichtbar)
- Scanner-Buffer sofort freigeben

### ghost-vault

- RAM zuerst
- Storage verzögert
- Integrität prüfen

### ghost-policy

- Bedingungen auswerten
- Aktionen auslösen

### ghost-heartbeat

- senden
- Ausfälle erkennen

### ghost-down

- RAM-Snapshot
- optionaler NAS-Flush
- Kill (Peer-OS-Halt, nie PSP)
- Game-Mode
- PSP bleibt passiver Beobachter

### ghost-peek

- low_power_mode nutzen
- Minen-Signale auswerten
- kein Kaltstart

### Minen

- scannen
- counter inkrementieren
- TOTP erneuern
- senden
- nicht empfangen

---

## 13. Capabilities

scan_wifi, scan_bt, scan_ir, analyze, store_logs, notify, backup, classify, router_events, ir_tx, ir_rx, passive_monitor, active_monitor, heartbeat, heartbeat_receive, snapshot, kill, peek, mine_scan, mine_event, replay_guard

---

## 14. Datenstrukturen

### Device

id, role, capabilities[], trust_level, last_seen, status, tags[]

### Event

type, source_device_id, timestamp, payload, severity

### Log

id, event_id, hash, stored_at, retention_class

### Policy

id, name, scope, condition, action

### Mine Payload

mine_id, counter, totp, event, timestamp, hash

---

## 15. Replay-Guard

- TOTP-Fenster: 60–120 Sekunden
- Counter monoton
- alte Counter verwerfen
- letzte 32 Einträge pro Mine speichern
- Replay → Mine blockieren
- blockierte Mine: manuelle Neuinstallation MAY

---

## 16. Policy‑DSL

```
condition = expression (AND expression)*
expression = field operator value
action = log_only | alert | backup | block | kill | ghost_down
```

---

## 17. Vordefinierte Policies

| ID | Bedingung | Aktion |
|----|-----------|--------|
| P01 | unknown_device | log_only |
| P02 | critical_device_lost | alert + backup |
| P03 | worker_degraded | PSP übernimmt Analyse |
| P04 | sensor_spam | block + snapshot |
| P05 | nas_full | local_vault |
| P06 | router_new_client | log + classify |
| P07 | ir_signal_unknown | log + classify |
| P08 | heartbeat_miss_worker | alert + degraded |
| P09 | ghost_down_enter | snapshot + local_backup |
| P10 | danger_mode_enter | passive_scans_only |
| P11 | mine_event_critical | alert + snapshot |
| P12 | mine_silent_too_long | check_mine_status |
| P13 | mine_replay_detected | block + alert |
| P14 | time_drift_detected | worker_time_anchor |
| P15 | hmac_invalid | 3 HMAC-I / 60s → 1 alert (kein ghost_down) |

---

## 18. Decision Pipeline

```
receive → validate → replay_guard → enrich → classify → policy → priority → fallback → escalation → route → store → ack
```

- ACK nur bidirektional
- ACK-Timeout 1–2 Sekunden
- invalid HMAC (HMAC-I) → drop. MUST NOT Ghost Down. Evidence once per 60s window. P15: 3 HMAC-I in that window → 1 alert.

---

## 19. Prioritäten

| Rolle | Trust | Priorität |
|-------|-------|-----------|
| worker | 2 | 1 |
| phone | 1 | 2 |
| psp | 3 | 3 |
| nas | 1 | 4 |
| mine | 0 | 5 |

---

## 20. Fallback

- worker degradiert → PSP analysiert
- phone offline → PSP alarmiert lokal
- NAS voll → PSP Vault
- Sensor Heartbeat fehlt → degraded
- Mine schweigt → suspected
- Replay → Mine blockiert

---

## 21. Context Engine

home, mobile, public, offline

---

## 22. Kommunikationsfluss

- Alle zu PSP
- PSP koordiniert
- Minen senden nur
- Minen empfangen nichts

---

## 23. Zyklen

| Aktion | Intervall |
|--------|-----------|
| Heartbeat | 30s |
| WLAN/BT-Scan | 30s |
| Sensor-Scan | 60–300s |
| Backup | 5min |
| Ghost Peek | 5min |
| Minen-Signal | periodisch |

---

## 24. Memory-Plan

Gesamt RAM ≤ 24 MB

| Modul | Budget |
|-------|--------|
| ghost-core | 256 KB |
| vault RAM | 2 MB |
| vault Storage | 4–64 MB |
| policy | 32 KB |
| terminal | 128 KB |
| scanner | 256 KB |
| ir | 128 KB |
| ghost-down | 64 KB |
| ghost-peek | 64 KB |
| snapshot | 1 MB |
| event-queue | 256 KB |
| replay-guard | 128 KB |

---

## 25. Topologie

1 Kernel, 1 Worker, 1 Safe, 1–8 Sensoren, 1 Router, 1–N Minen

---

## 26. Lifecycles

Device:

```
unknown → pending → online → degraded → offline → suspected → blocked → deleted
```

Mine:

```
install → silent → active → triggered → silent → removed
```

---

## 27. Event-Lifecycle

```
create → validate → replay_guard → policy_check → route → store → ack
```

---

## 28. Backup-Lifecycle

```
trigger → snapshot → encrypt → RAM store → flush → index → verify → ack
```

---

## 29. Alarm-Lifecycle

```
detect → classify → severity → route → notify → log → escalate
```

---

## 30. Threat Model

Gegner:

- Scriptkiddie
- kompromittiertes Familiengerät
- gezielter Angreifer
- physischer Zugriff
- Malware
- Replay-Angriff

Angriffswege:

- Worker, Phone, Router, PSP, NAS, AP, MITM, Minen-Exploit, Replay

---

## 31. Fehler & Recovery

| Fehler | Reaktion |
|--------|----------|
| I/O | safe_mode |
| Netz | Retry |
| Zeitdrift | Worker-Zeit, trust_level ≥ 2 |
| Korrupte Logs | isolieren |
| Registry | Vault Recovery |
| Minen-Ausfall | Status prüfen |
| Replay | Mine blockieren |

---

## 32. Bootstrap

1. PSP Terminal-Mode
2. Schlüssel
3. Registry
4. Warten

Worker: Pairing-Code, pending → online

Sensor: wie Worker, nur sensor

Safe: safe + Backup

Mine: Counter + TOTP, silent, erster Event

---

## 33. Krypto

Keys:

- Root-Key
- Device-Key
- Log-Key
- Session-Key
- Minen-Key
- TOTP-Seed
- Master-Key (PSP-intern, nie Datei auf USB/ms0-Drop)
- Wrap-Key (KDF, nie exportiert)

Verwaltung:

- lokal
- PSP hält Root
- Rotation manuell
- Reset physisch

### 33.1 Root-Wrap

Root-Plaintext existiert MUST NOT außerhalb der PSP-RAM.

`ms0:/ghost_hive/k/root.key` ist MUST Ciphertext (AES-256-CTR + HMAC-SHA1).

USB-/XMB-Kopie von `root.key` MUST wertlos sein.

Wrap:

- Master-Key = aus PSP-Hardware-Identität (nie exportiert, nie auf dem USB-Drop)
- optionale Passphrase nur lokal an der PSP / Kernel-Persistenz außerhalb des USB-Drops
- wrap_key = KDF(master_key, passphrase)
- root.key = ENC(wrap_key, root_plain) || MAC

Laptop / Worker / Stick MUST NOT Master-Key, Wrap-Key oder Root-Plaintext halten.

---

## 34. CLI

| Kommando | Funktion |
|----------|----------|
| hive status | Status |
| hive devices | Geräte |
| hive policies | Policies |
| hive scan | Scan |
| hive backup | Backup |
| hive alert | Testalarm |
| ghost down | Notfall |
| ghost peek | passiver Scan |
| danger mode | Rückkehr |
| mine check | Minen-Signale |
| mine block | Mine sperren |
| time check | Zeitstatus |

---

## 35. Observability

Views: Geräte, Events, Heartbeat, Vault, Policy, Minen, Replay

Levels: normal, verbose, trace

---

## 36. Tests

Pflicht:

- nur PSP
- Worker fällt aus
- NAS voll
- neues Gerät
- Heartbeat-Verlust
- Registry kaputt
- Zeitdrift
- Ghost Down
- Danger Mode
- Final Backup
- Mine kritisch
- Mine still
- NAS down bei Ghost Down
- Replay
- Kaltstart vermeiden

---

## 37. Versionierung

Alle Komponenten 1.x

Kompatibel:

- PSP 1.7.3 mit Worker 1.0
- PSP 1.7.3 mit Sensor 1.0
- PSP 1.7.3 mit Safe 1.0
- PSP 1.7.3 mit Mine 1.0

---

## 38. Honeypot-Verteilung

Nicht auf PSP.

Laptop, Phones, NAS, Router

---

## 39. Aktive Verteidigung

1. Alarm
2. Beweise
3. Isolieren
4. Sperren
5. Tarnung
6. Lockvogel
7. Segmentierung
8. Lokales Backup
9. WLAN aus
10. Selbstschutz

---

## 40. Ghost Down

Ghost Down ist globaler, sofortiger Geräte-Shutdown. Die PSP bleibt als einziger leiser Beobachter.

Befehl an Peers: bestehendes Event `GhostDownStart` (Kernel, signiert, KillFrame). Kein neues Event.

`GhostDownStart` MUST nur ein signiertes Kernel-Event sein. Invalid HMAC / unsigned Frame MUST NOT Ghost Down auslösen (Blind-Schalter / DoS). HMAC-I: evidence + drop (P15).

1. kritischer Zustand — Freeze in derselben Loop-Runde
2. RAM-Snapshot
3. Final Snapshot
4. NAS-Flush Timeout 5–10s (kein Wipe, kein Datenverlust)
5. Kill — `GhostDownStart` an alle bidirektionalen Peers
6. Game-Mode (Tarn-Look, Funk aus, State bleibt ghost_down)
7. Stop — keine Heartbeats, Scans, Events, NetSensor, Minen-Trips
8. Storage-Flush 30–120s später
9. Storage-Flush retry once

Peer-Halt nach Kill (MUST):

- Worker (Laptop): OS-Shutdown / Hard-Freeze. Host-Sim: Prozess tot, Marker, kein `shutdown` außer `GHOST_OS_HALT`.
- Phone: Funk aus, Gerät aus. Host-Sim: Prozess tot.
- Sensor (Familie): wie Phone, Prozess tot.
- Router: Dienste aus, Ports zu, ggf. Reboot Minimalzustand. Host-Sim: Prozess tot, Ports-Marker.
- NAS: Ghost-Prozesse aus, Shares weg. Host-Sim: Prozess tot, Shares-Marker.
- Minen: kein RX. Sterben mit dem Host-OS. Kein neuer Trip.

PSP nach Down (MUST):

- Kernel läuft
- Root bleibt
- Logs intern (Vault)
- keine aktive Funk/Netz-Interaktion
- Ghost Peek passiv, physische Intervention für Rückkehr (Danger Mode)

---

## 41. Ghost Peek

- low_power_mode
- passiv
- Minen-Signale
- Replay-Guard
- Gefahrenprüfung
- kein Kaltstart

---

## 42. Danger Mode

Phasen:

1. PSP passiv
2. Minen auswerten
3. Phone minimal
4. Laptop lokal
5. NAS read-only
6. Router informiert
7. Hive hoch

Regeln:

- keine Auto-Backups
- keine neuen Geräte
- PSP entscheidet
- Scans passiv
- Minen stumm
- Replay-Guard aktiv

---

## 43. Kill-Switch

- signiert (`GhostDownStart` / KillFrame)
- Snapshot zuerst
- nie gegen PSP
- Peers MUST OS/Dienste haltieren
- PSP MUST NOT haltieren
- GhostDownStart nur Kernel, nur gültige Signatur. Invalid HMAC → evidence + drop, nie Kill/Down.

---

## 44. Normative Regeln

- PSP entscheidet
- Sensoren schreiben nicht
- Worker blockiert nicht
- NAS alarmiert nicht
- Router konfiguriert nicht
- PSP scannt nicht im Game-Mode
- PSP nimmt nicht automatisch auf
- PSP ist nie Honigtopf
- Snapshot vor Kill
- Ghost Peek nach Ghost Down
- Minen senden nur
- Minen zählen monoton
- TOTP-Fenster 60–120s
- ACK-Timeout 1–2s
- NAS-Flush 5–10s
- Storage-Flush 30–120s
- Storage-Flush retry once
- RAM ≤ 24 MB
- Scanner-Buffer freigeben
- Worker-Zeit nur bei trust ≥ 2
- Replay-Guard 32 Einträge pro Mine

---

## 45. Was Ghost Hive IST

Ein lokales, mobiles, getarntes Sicherheitssystem.

- persönlich
- erweiterbar
- kontrollierbar
- dokumentierbar
- unabhängig
- leise
- hardware-bewusst
- replay-geschützt
- immer dabei

---

## 46. TETACT

TETACT ist KEIN PSP-Modul. Es ist das Zusammenspiel von:

- hive-analyzer (Laptop Noah)
- hive-sensor (Phone Noah / Familie)
- Minen (OS / Browser / Apps)
- Router-NetSensor (Upgrade-Kante, keine Config)

Aufgabe: echte Wurzeln in die Host-Systeme legen und Dynamik als Events an die PSP geben.

Wurzeln (kein WLAN-Monitor-Mode, §2.3):

- OS-Sicht: WLAN-Liste des OS, BT-Nachbarn, neue Geräte
- App-Sicht: Browser, Prozesse
- Netz-Sicht: Router-Clients, Ports, Flows soweit der Host sie sieht

Events (nur §14-Typen):

- ScanResult
- DeviceSeen
- MineEvent
- AnomalyDetected

Awareness:

- Umfeld manipuliert (OS, Browser, Router, Netz) → ScanResult / DeviceSeen / MineEvent / AnomalyDetected
- Selbst manipuliert (Prozess gekillt, Uhr verdreht, Config geändert, Debugger) → AnomalyDetected und Mine sendTrip (Tamper = `event=AnomalyDetected` im MinePayload, kein neues Feld)
- Heartbeat-Ausfall → PSP sieht Sensor weg (P08/P12). HeartbeatMiss allein ist kein Ghost Down.

PSP bleibt der einzige, der Danger, Freeze, Kill und GhostDown entscheidet.

TETACT MUST NOT schreiben, blockieren, konfigurieren oder entscheiden.

---

**Ende v1.7.3 Ultra‑Frozen Hardware Specification.** 🕸️💀
