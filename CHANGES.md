# Aenderungen - Session 29.03.2026

## 1. Custom Game System (Game ID 29)

Ein generisches Spielsystem, bei dem Trainer eigene Spiele visuell erstellen und abspielen koennen.

### Architektur
- **WYSIWYG Editor** (`data/game_custom_editor.html`) — Visueller Editor auf dem Coordinator
  - State-Liste (bis zu 30 States)
  - Pro State: Puck-Aktionen (Effekt, Farbe, Dauer, Sound) und Transitions
  - Puck-Preview zeigt Farben live an
  - Export/Import als JSON-Datei
  - Direkt auf den Coordinator speichern ("Save to Coordinator")
- **JSON Interpreter** (`src/coordinator/Game_Custom.h/cpp`) — State-Machine auf dem ESP32
  - Parst JSON aus LittleFS (`/custom_game.json`)
  - Fuehrt States sequenziell aus, reagiert auf Button-Presses und Timeouts
  - Kein PSRAM noetig (~5KB RAM fuer 30 States)
- **Setup Page** (`data/game_custom_setup.html`) — JSON laden, Pucks/Zeitlimit konfigurieren, Start
- **Run Page** (`data/game_custom_run.html`) — Live-Dashboard mit State-Anzeige, Timer, Stop/Reset

### Verfuegbare Effekte im Editor
- OFF, STATIC, BREATHE_MOD2, BREATHE_MOD4, SINGLE_CHASE, DOUBLE_CHASE, COUNTDOWN, STATUS
- Sound: BEEP (mit Dauer), SKI Melody
- Transitions: PRESS (bestimmter Puck oder Any) und TIMEOUT (in 0.1s Schritten)

### JSON Format
```json
{
  "name": "Mein Spiel",
  "states": [
    {
      "name": "Start",
      "actions": [
        { "puck": 0, "effect": "STATIC", "r": 255, "g": 0, "b": 0, "dur": 0, "sound": "BEEP", "soundDur": 100 }
      ],
      "transitions": [
        { "on": "PRESS", "puck": 0, "goto": 1 },
        { "on": "TIMEOUT", "time": 50, "goto": -1 }
      ]
    }
  ]
}
```
- `puck`: 0-basierter Index, -1 = alle Pucks
- `time`: Decisekunden (50 = 5.0 Sekunden)
- `goto`: State-Index, -1 = Spiel beenden (Fanfare + Rainbow)

### Geaenderte Dateien
- `src/coordinator/GameManager.cpp` — Game ID 29 registriert
- `src/coordinator/StatsManager.cpp` — Bounds von 28 auf 29 erhoeht
- `src/coordinator/WebHandler.cpp` — Neue Endpoints: `POST /api/custom/save`, `GET /api/custom/load`; Stats-Endpoint auf 29 erweitert
- `data/index.html` — Neues Tile in der Spielliste

---

## 2. ActivationManager — Bugfixes

### CRC32 Fix (`src/coordinator/ActivationManager.cpp`)
- **Problem:** `crc32()` war nicht deklariert (Kompilierfehler).
- **Fix:** `#include <esp_rom_crc.h>` hinzugefuegt. Aufruf geaendert zu `~esp_rom_crc32_le(~0U, data, len)` — das entspricht Standard CRC32 und matcht exakt Python `zlib.crc32()` im `keygen.py`.

### ArduinoJson Include (`src/coordinator/WebHandler.cpp`)
- **Problem:** Die ActivationManager-Endpoints (`/api/license`) verwendeten `JsonDocument`, aber `<ArduinoJson.h>` war nicht inkludiert.
- **Fix:** `#include <ArduinoJson.h>` hinzugefuegt.

### Lizenzpruefung im Frontend (`data/script.js`)
- **Problem:** `checkLicenseAndNavigate()` verglich `license.playtime_limit_hours`, aber die API liefert `playtime_limit_minutes`. Die Pruefung schlug daher nie an.
- **Fix:** Vergleich korrigiert zu `(license.playtime_hours * 60) >= license.playtime_limit_minutes`. Zusaetzlich: Pruefung greift jetzt auch fuer EXTENDED-Lizenzen (nicht nur FREE).

### Konzept-Bewertung
Das Gemini-Konzept ist solide:
- Challenge-Response ueber MAC-Adresse + Salt + CRC32 — funktioniert offline
- Drei Stufen: FREE (20h), EXTENDED (+60h = 80h total), FULL (unbegrenzt)
- Key wird in `/license.json` auf LittleFS gespeichert und bei jedem Boot revalidiert
- Debug-Modus (`/register.html?debug=1`) erlaubt Zeitlimits in Minuten zu setzen
- `checkLicenseAndNavigate()` in script.js — derzeit nur in React 2P aktiv (Testfall)

### Offene Punkte (unveraendert, aus copyprotection.txt)
1. Alle Spiele muessen `nav()` durch `checkLicenseAndNavigate()` ersetzen
2. Backend-Sperre (30s Delay bei abgelaufener Lizenz) ist noch nicht implementiert
3. Uebersetzungen fuer register.html fehlen in lang.js
