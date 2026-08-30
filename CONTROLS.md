# Ghost Hive — PSP Controls

**Status:** MVP final (Noah 2026-08-28)  
**Basis:** Spec v1.7.3 §9, §10, §34, §40, §43  
**Gerät:** PSP-1004 (FAT), lokal

Ghost Down nur automatisch (Pipeline / §40). HMAC-I ist drop, kein Down. Kein Stealth-Taster.  
Prompt: `>ghost_$<cli>` live, Snapshot: ` ghost_$ cli`. Lock `*` statt `>`.  
X zweimal: Lock, dann Ausführen. O: Unlock oder Black. Kein Split-Screen.

| Modus | Taste | Wirkung |
|-------|--------|---------|
| Terminal | Hoch | Cursor hoch (Snapshots, dann Live; kein Wrap) |
| Terminal | Runter | Cursor runter (kein Wrap) |
| Terminal | Links | Live: CLI-Zahnrad vorherige Option (wrap). Snapshot: tot |
| Terminal | Rechts | Live: CLI-Zahnrad nächste Option (wrap). Snapshot: tot |
| Terminal | X (1.) | Zeile locken |
| Terminal | X (2.) | Live: Befehl ausführen. Snapshot: Output nochmal |
| Terminal | O (gelockt) | Lock lösen |
| Terminal | O (frei) | Black Mode |
| Terminal | Quadrat | View leeren (History bleibt) |
| Terminal | Dreieck | reserviert |
| Terminal | L / R | reserviert |
| Terminal | Select+Start | Game Mode (Funk aus, §2.2) |
| Output | L oder Links | vorheriges Fenster (wrap) |
| Output | R oder Rechts | nächstes Fenster (wrap) |
| Output | O | zurück Terminal (zuverlässig, auch PSP) |
| Output | Home | zurück Terminal (Host; auf PSP oft System-Exit) |
| Output | Select+Start | Game Mode; zurück landet wieder im Output |
| Black | nur O | zurück Terminal (Screen leer) |
| Game | Select+Start | zurück (Output falls von dort, sonst Terminal) |
| Ghost Down | — | keine Tasten |

| CLI (§34) | Aktion |
|-----------|--------|
| hive status | Status |
| hive devices | Geräte |
| hive policies | Policies |
| hive scan | WLAN-Scan |
| hive backup | Backup |
| hive alert | Testalarm |
| ghost down | nur Status, startet Down nicht |
| ghost peek | Peek |
| danger mode | Danger-View |
| mine check | Mine-Status |
| mine block | Mine sperren |
| time check | Zeit |

History: Index 0 neueste, max. 6, Anzeige älteste oben. Nach Output: neue Live-Zeile, Zahnrad auf `hive status`. Output max. 6. Tick 10 ms.  
Kopfzeile: nur `ghost:hive`. Lock `*` an der Zeile. L/R ohne Zähler. Output ohne Titel, Tabellen. Black leer. Schrift grün. Down = leerer Screen.  
Lock folgt Cursor. Auto-Reset 10 min: Terminal + Lock + Cursor + Zahnrad + Output + History, Black/Game aus. Quadrat leert View, nicht History.

Orchestrator: Taste → TermEvent (lokal, kein SPEC-Event) → State → GhostOutput (§34/§35) → Frame. Down nur automatisch (§40).

Host (`GHOST_PSP_KEYS=1`): i/k Hoch/Runter, j/l oder Pfeile Links/Rechts, x, o, s=Quadrat, g=Select+Start, `[` `]`=L/R, h=Home.
