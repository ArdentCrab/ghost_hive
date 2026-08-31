# Ghost Hive — Live-Day Checkliste (IBSS + PSP)

**Version:** v2.0.1-R3  
**Norm:** [`SPEC-v2.md`](../SPEC-v2.md)  
**Ziel Tag 1:** PSP Watch live + Worker/Relay per IBSS. Optional: ein Phone via Relay.

Dies ist **Bring-up**, kein garantiertes Plug-and-Play. Erfolg = messbare Checks pro Phase.

---

## Hardware & Software (vor dem Tag)

| Item | Rolle |
|------|--------|
| PSP-1004 + Speicherkarte | Kernel, Watch, IBSS Master |
| IBSS-WLAN-Stick (USB) am PSP | Ad-hoc `GHSTHIVE` |
| Laptop (Noah) | Worker, Relay, Gate, WSL |
| USB-Stick `E:\ghost_hive\` | Hive-Bus (Code, bind, manifest) |
| USB-Kabel PSP ↔ Laptop | Whitelist + `fetch_bind` |
| Optional: Android + Termux | Erstes echtes Remote-Gerät (Relay) |

**Software auf Laptop:**

- WSL2 mit `g++`, `make`, `python3`
- Repo: `git clone https://github.com/ArdentCrab/ghost_hive.git`
- PSPDEV für EBOOT: `make eboot` (einmalig)
- Optional Termux: `pkg install python`

**Ports (Laptop-Firewall):**

| Port | Dienst |
|------|--------|
| 17469 | `wake_lan` / `ghost_wake` |
| 17470 | `bind_serve` (nur Gate-Fenster, 900 s) |
| 17471 | Hive UDP (Relay ↔ Peers) |

---

## Phase 0 — Repo aktuell (WSL)

```bash
cd ~/ghost_hive   # oder dein Clone-Pfad
git pull origin main
git log -1 --oneline   # erwarte: v2.0.1-R3 oder neuer
```

Optional Vertrauenscheck:

```bash
make test      # ~1 min, EXIT 0
make redteam   # 2001 match, EXIT 0
```

---

## Phase 1 — Stick bauen (WSL)

```bash
cd ~/ghost_hive
make peers
make eboot          # braucht PSPDEV; erzeugt src/psp/EBOOT.PBP
make live           # packt deploy/ghost_hive/
```

Optional arm64 für Termux:

```bash
sudo apt-get install -y g++-aarch64-linux-gnu   # falls nicht da
make peers-arm64
make live
```

**Nach Windows kopieren** (gesamter Ordner):

```
deploy/ghost_hive/  →  E:\ghost_hive\
```

Prüfen:

```
E:\ghost_hive\
  bin\amd64\ghost_laptop
  bin\amd64\ghost_relay
  devices.manifest
  bind_serve.py
  wake_lan.py
  wake\ghost_wake.py
  live\EBOOT.PBP
  root_config.json
```

`peer.bind` kommt **noch nicht** aus Git — Phase 2.

---

## Phase 2 — PSP Erst-Ingest (einmalig)

### 2a Whitelist auf PSP legen

PSP: **Einstellungen → System → USB-Verbindung → USB** (nicht nur Laden).

Windows, im Stick-Ordner oder Repo-Deploy:

```cmd
cd E:\ghost_hive
python ghost_v2_final.py whitelist
```

Erwartung: `Whitelist -> D:\ghost_hive\root_config.json` (Laufwerk kann abweichen).

PSP **sicher auswerfen**, Kabel trennen.

### 2b GHOST Ingest auf PSP

1. IBSS-Stick am PSP (falls noch nicht)
2. FastRecovery / GHOST-Menü starten (dein üblicher Ingest-Pfad)
3. Ingest läuft → `root_config.json` auf PSP wird verbraucht/gelöscht
4. PSP erzeugt Hive-Keys; `peer.bind` landet auf Speicherkarte

### 2c peer.bind holen

PSP wieder per **USB** verbinden (XMB USB-Modus).

```cmd
cd E:\ghost_hive
python fetch_bind.py
```

Erwartung:

```
ok C:\Users\...\Documents\ghost_hive_peers\k\peer.bind 112
```

Stick neu packen (WSL), damit `peer.bind` im Stick-Layout ist:

```bash
cd ~/ghost_hive
make live
# peer.bind wird aus Documents/ghost_hive_peers/k/ kopiert wenn vorhanden
```

Nochmal `E:\ghost_hive\` überschreiben/kopieren. Prüfen:

```cmd
dir E:\ghost_hive\peer.bind
```

→ **112 Bytes**.

---

## Phase 3 — PSP live (IBSS)

1. `EBOOT.PBP` auf PSP-Speicherkarte:
   - `ms0:/ghost_hive/live/EBOOT.PBP` oder dein üblicher Pfad
2. PSP bootet Ghost EBOOT
3. Watch sollte erscheinen: Kopfzeile `ghost:hive`, Titel `[GHv2]`

**Checks auf Watch (Net-Seite):**

| Feld | Erwartung |
|------|-----------|
| IBSS | `GHSTHIVE (ON)` wenn Funk bereit |
| IP | `10.17.47.1` |
| UDP | `17471` |
| STATE | `Watch` (nicht Down) |

Wenn `GHSTHIVE (OFF)`: IBSS-Stick, Funk-Schalter, Abstand/Störung prüfen. **Nicht** mit Phase 4 weitermachen, bis ON.

---

## Phase 4 — Laptop Hive (Gate + Worker + Relay)

Stick `E:\ghost_hive\` eingesteckt lassen.

**Windows One-Click:**

```cmd
cd E:\ghost_hive
hive_live.cmd
```

**Oder WSL/Linux im Deploy-Ordner:**

```bash
cd /mnt/e/ghost_hive
./hive_live.sh
```

**Oder manuell:**

```cmd
python hive_gate.py once
python auto_peers.py start
python ui\hive_manager.py
```

### Erwartete Ausgabe (Gate)

```
Stick: E:\ghost_hive
Hive an: Worker + Relay (Kernel 10.17.47.1).
R3: bind_serve gestartet (Fenster 900s).
R3: wake_lan gesendet.
```

### Erwartete Ausgabe (auto_peers)

```
auto_peers: kernel=10.17.47.1 lan=<deine-LAN-IP> ...
  relay: started
  worker: started
  phone: remote (ghost_wake on device)
  ...
  mines: started          # WSL local sim
auto_peers: 3/3 local up  # relay + worker + mines
```

### Status prüfen (WSL)

```bash
python3 /mnt/e/ghost_hive/auto_peers.py status
```

Logs bei Problemen:

```
/tmp/ghost_hive/worker.log
/tmp/ghost_hive/relay.log
/tmp/ghost_hive/logs/bind_serve.log
```

---

## Phase 5 — Verifikation (Erfolg Tag 1)

### Laptop

- [ ] `relay` UP, `worker` UP
- [ ] `/tmp/ghost_hive/peer.bind` = 112 B
- [ ] `/tmp/ghost_hive/peer.bind.ttl` existiert
- [ ] Manager zeigt Peers

### PSP Watch

- [ ] Hive-Seite: Worker `W` sichtbar / online (nach HB, kann ~30 s dauern)
- [ ] Net: `GHSTHIVE (ON)`, Kernel-IP korrekt
- [ ] Peer-Seite: Fokus L/R funktioniert

### Negativ-Checks (soll so bleiben)

- [ ] Kein `root.key` auf USB-Stick
- [ ] HMAC-Fehler auf Watch → Drop, **nicht** Ghost Down
- [ ] Stick raus → Gate stoppt Hive (nach `hive_gate.py watch` / Neustart)

**Tag 1 = Erfolg**, wenn PSP + Worker + Relay stabil sind. Alle 7 Rollen auf echter Hardware sind **nicht** Pflicht.

---

## Phase 6 — Erstes echtes Gerät (Phone, Relay)

Voraussetzungen:

- Phone und Laptop im **selben WLAN**
- `bin/arm64/ghost_phone` auf Stick (oder auf Phone kopiert)
- `ghost_wake` **vor** oder **beim** Gate-Insert gestartet

### Termux (Phone)

```bash
# Dateien von Stick/Laptop kopieren, z.B. nach ~/ghost_hive/
cd ~/ghost_hive
export GHOST_BIND_DIR=$PREFIX/tmp/ghost_hive
python3 wake/ghost_wake.py P ./bin/arm64/ghost_phone
```

Dann Laptop-Stick einstecken (oder Gate erneut):

```cmd
python hive_gate.py once
```

Phone sollte:

1. `GHST_WAKE` hören
2. `peer.bind` + `.ttl` schreiben
3. `ghost_phone <LAN-IP> P` starten

Watch Peer-Seite: Phone `P` nach Heartbeat.

---

## Troubleshooting

| Symptom | Wahrscheinliche Ursache | Fix |
|---------|-------------------------|-----|
| Gate: `peer.bind fehlt` | Kein Ingest / fetch_bind | Phase 2 wiederholen |
| Gate: `bin/ghost_laptop fehlt` | `make live` nicht gelaufen | Phase 1 |
| `bind_serve skipped` | `devices.manifest` fehlt | `make live`, Stick neu kopieren |
| Worker startet, Watch leer | IBSS nicht verbunden | Phase 3, Funk/SSID |
| `bind denied` auf Phone | ID nicht in manifest / Rate-Limit | `devices.manifest` id=P; warten 1 min |
| Relay UP, Remote down | ghost_wake nicht gestartet | Phase 6 vor Gate |
| WSL: Permission denied | ELF nicht executable | `chmod 700 /tmp/ghost_*` |
| Firewall | Ports blockiert | 17469–17471 freigeben |

### Hive sauber stoppen

```cmd
python hive_gate.py stop
python auto_peers.py stop
```

### Bind-Fenster abgelaufen (900 s)

`bind_serve` beendet sich automatisch. Stick kurz raus/rein oder:

```cmd
python hive_gate.py once
```

---

## Was bewusst nicht an Tag 1

| Item | Grund |
|------|--------|
| Router (mips) | Kein `peers-mips` im Repo — extra Cross-Build |
| Alle Geräte gleichzeitig | Relay + Wake pro Gerät einzeln debuggen |
| Latenz &lt; 10 ms | IBSS-Hardware noch nicht gemessen |
| „Perfekt ohne Tweaks“ | Erstes Hardware-Live ist immer Bring-up |

---

## Kurzablauf (Copy-Paste)

```bash
# WSL — Build
git pull && make peers && make eboot && make live
# → E:\ghost_hive\ kopieren

# Windows — PSP einmalig
python ghost_v2_final.py whitelist
# PSP: Ingest
python fetch_bind.py && make live   # WSL nochmal, Stick aktualisieren

# Tag — PSP EBOOT booten, dann:
cd E:\ghost_hive && hive_live.cmd
```

---

## Referenzen

| Dokument | Inhalt |
|----------|--------|
| [`deploy/ghost_hive/ANLEITUNG.txt`](../deploy/ghost_hive/ANLEITUNG.txt) | Komponenten-Kurzreferenz |
| [`docs/investor/PROOF.md`](investor/PROOF.md) | Test- und Redteam-Zahlen |
| [`WHATISV2.md`](../WHATISV2.md) | v2 Inventar |
