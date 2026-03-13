# Puck Racer 🏃‍♂️💨
**The DIY agility System**

## 📖 The Story: Why this exists
Let's be real for a second: I built this because I was simply too stingy to drop **300-400€** on a commercial reaction light system that consists of only 4 pucks, needs a paid subscription and provides 200 extremely similar games... which can be summerized in 5 different game modes.  
**Puck Racer has now 26 games**. Highly adjustable. Multi-Player, Multi-Group support.  
All options combined easily provide 200+ "games" ;-)  

I also noticed that the PE equipment at my kids' elementary school hasn't really changed in 40 years. It was time for an upgrade.

**Puck Racer** is the answer. It’s perfect for professional training support in sports clubs, spicing up PE lessons in schools, or just being the absolute highlight of the next kids' birthday party.  
I would descibe it as a **blaze**ing**POD** - DIY solution - just on steroids :-)  

**The Trade-off:**
Yes, building 4-10 Pucks and a Coordinator is a bit of a grind. It takes time and patience.

**The Reward:**
You get a fully customizable system where the hardware cost per Puck can be pushed **under 20€**. You do the math. 😉  
Nice side effect: Teach your kids how to solder. Its easy to medium difficulty. My 9 year old twins built 5 pucks by them selfe with very little help.  

---

## 🛠 Hardware
*detailed howto and hardware list will be provided once its worth it*  
ESP32-C3 for pucks   (use the one with external antenna or built a small antenna!)
ESP32-S3 for Coordinator  
WS2812B 35LED ring  
something for 18650 Battery placement + stepup to 5V  
Arcade Button (bigger is better) 60mm  
3D printer for pucks  
Soldering Stuff  
**P A T I E N C E** - printing 10 pucks, soldering 10 times the same stuff and initial flashing needs a good amount of patience.

---

## 💻 Software & Installation

This system relies on a **Coordinator** (ESP32-S3) that acts as the brain and the WiFi Access Point, and multiple **Pucks** (ESP32-C3) that communicate via ESP-NOW.

### 📦 Prerequisites

To compile and flash this project, you need the **Arduino IDE**. Of cause VS Code will also work. 

1.  **ESP32 Board Manager:**
    * Go to *Tools > Board > Boards Manager*.
    * Install **esp32 by Espressif Systems**.
    * **Recommended Version:** `3.0.0` or higher (we need proper S3/C3 support).

2.  **Required Libraries:**
    Install these via the Arduino Library Manager:
    * `FastLED` (for the WS2812B LEDs)
    * `ESPAsyncWebServer from ESPhome project` (for the Coordinator Dashboard)
    * `AsyncTCP from ESPhome project` (dependency for the WebServer)

### 📂 Uploading the Web Interface (IMPORTANT!)

The Coordinator runs a modern Web App stored in the ESP32's internal file system (LittleFS). You **must** upload the files from the `data` folder, otherwise, you'll just see a blank screen.

1.  Download the **Arduino LittleFS Upload Tool**:
    * 👉 [GitHub Link: arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload)
2.  Follow the installation instructions on the repo (copy to `tools` folder).
3.  In Arduino IDE, select the Coordinator board.
4.  Click **Tools > ESP32 LittleFS Data Upload**.

### 📡 Connectivity & OTA Updates

**The Network:**
The Coordinator opens a WiFi Access Point (Default SSID: `PuckRace_Trainer`). Connect your phone or laptop to this network and navigate to `http://192.168.42.1`. No app required, it runs in the browser.  It has no pwassword initially. Your choice to set it up if you deal with kids and their smartphones.

**Wireless Updates (OTA):**
You only need to hook the Pucks up to USB *once* for the initial flash. After that, you're free:
1.  Compile the new Puck firmware (Sketch -> Export Compiled Binary).
2.  Go to the Coordinator Web Interface Settings.
3.  Upload the `.bin` file.
4.  Hit **"Update All Pucks"**. The Coordinator will distribute the update wirelessly to all active Pucks. Magic. ✨

After the real first beta is released il will provide the bin files in case of updates. 

---

## 🎮 The Games

**26 games** across 4 categories. Highly adjustable, multi-player and multi-group support.

| Focus | Icon |
|---|---|
| Hand-eye coordination, fast stimulus processing | ⚡ Reaction |
| Rapid direction changes, sprints, footwork | 🏃‍♂️ Agility |
| Cardiovascular load and pacing | 🫀 Endurance |
| Memory, timing, stress resistance | 🧠 Cognition |
| Teambuilding and motivation | 🤪 Fun |
| Practical tools for coaches and teachers | 🛠️ Utility |

---

### ⚡🏃‍♂️ Agility & Reaction

| Game | Players | Pucks | Focus | Highlights |
|------|---------|-------|-------|------------|
| 🎯 **Hunt! (Color Hunt)** | 1–10 | 3–20 | ⚡ 🏃‍♂️ 🧠 | Dynamic queues, adjustable timeout, 1-sec warning |
| ⚡ **React 2-Player** | 2–10 (pairs) | 3–10/group | ⚡ 🧠 | Configurable colors, fake stimuli for confusion |
| 🎯 **Target Touch** | 1–5 groups | 2–10/group | ⚡ 🛠️ | Sequence or random mode, rest time control |
| 🏃‍♂️ **Agility T-Test** | 1–2 groups | 4/group | 🏃‍♂️ | Standard T-test layout, mandatory touch mode |
| 🐱 **Cat Reflex** | 1–11 | 1/player + 1 center | ⚡ | Highlander mode, Color Chaos (false-start training) |
| ⚡🎯 **Batak Pro** | 1 | 3–20 | ⚡ 🧠 | Adaptive speed, fake colors, per-puck reaction analysis |

---

### 🫀 Endurance & Pacing

| Game | Players | Pucks | Focus | Highlights |
|------|---------|-------|-------|------------|
| ⏱️ **The Pacemaker** | Groups | 4–20 | 🫀 🧠 | Exact pace (min/km), circle or shuttle, live distance |
| 🏃‍♂️ **Luc Léger (Beep Test)** | 1–10 | 2/player | 🫀 | Official VO₂max tables, false start detection |
| 🏃‍♂️ **Shuttle Run** | 1–5 | 2/player | 🏃‍♂️ 🫀 | Winner detection, false start detection |
| 🧟 **Zombie Escape** | 1–10 groups | 2/group | 🫀 🧠 🤪 | Auto time reduction, sudden death mode |

---

### 🧠🤪 Cognition & Fun

| Game | Players | Pucks | Focus | Highlights |
|------|---------|-------|-------|------------|
| 🔴 **Simon Says** | 1–3 groups | 2–10/group | 🧠 | Adjustable speed, color-blind palette |
| 🏃‍♂️ **Simon Runs** | 1–3 groups | 1 display + inputs | 🧠 🏃‍♂️ | Central display mode, disqualification management |
| 💣 **Bomb Squad** | 1–10 | 1/player | 🧠 🤪 | Adjustable tolerance, group sync mode, explosion feedback |
| 🚦 **Red Light, Green Light** | 1–10 groups | 1–20 | 🤪 ⚡ | Grace period, coach override |
| 🎩 **The Sorting Hat** | 2–100 | 1–3 | 🛠️ 🤪 | Balanced assignment, live rebalance |
| 🧠 **Memory Sprint** | 1+ | 4–20 (pairs) | 🧠 🏃‍♂️ | Blind search mode, auto-restart, live error tracking |
| ⚔️ **Domination (Turf War)** | 2 teams/arena | 2+/arena | 🏃‍♂️ 🫀 | 1-click vs 2-click capture, dual-arena support |
| ❌⭕ **Tactical TicTacToe** | 1–3 groups | 4 / 6 / 7 / 10 | 🧠 | Hard-Mode, grid auto-adapts to puck count |
| 🪑 **Musical Chairs** | 2–13 | Player count −1 | 🤪 🏃‍♂️ | Custom MP3 upload, dynamic round elimination |

---

### 🛠️ Measurement Tools & Utilities

| Game | Players | Focus | Highlights |
|------|---------|-------|------------|
| ⏱️ **Stopwatch** | 1–10 | 🛠️ | Split/lap times, central or individual start, Hold-to-Start |
| ⏳ **Simple Timer** | 1–10 | 🛠️ | Visual countdown fill, live +/− time adjustment per player |
| 🔢 **Simple Counter** | 1–10 | 🛠️ | Tap-to-count, time limit, debounce filter |
| ⬇️ **Simple Countdown** | 1–10 | 🛠️ | Count down from N, stop-on-winner option |
| 🥵 **TABATA Controller** | Unlimited | 🛠️ 🫀 | Synced progress bars, work/rest phases, configurable sound |
| ▶️ **Display Mode** | — | 🛠️ | Manual/sync/random light control for events |

---

### 👥 Player & Training Manager

| Tool | Description |
|------|-------------|
| 👥 **Player Manager** | Manage teams and class lists. Upload/backup CSV. Create named groups for quick name assignment. |
| 📋 **Training Manager** | Define training sequences with up to 3 games and pre-set all parameters. Jump between games without touching the setup screens. |

---


## Technical Updates:

**V2.88.2**  
    - iOS meta Tags added  
    - App Icon added  
    - Check that only 1 device is connected via WiFi  
**V2.89.7**   
    - Simple Statistics added for players maintained in Playermanager
    - CatReflexes (avarage reaction time)
    - BombSquad (avarage reaction time)
    - Shuttle run (time) assuming trainer always uses same distance, else its just a tracking of usage
    - Countdown (time and clicks) tracking of usage - Fix of copy&paste error in game_simplecounter_setup.html      
**V2.89.13**
    - statistics for all relevant games finished
        - Whac-A-Mole
        - Pacemaker
        - React2Player
        - T-Test
        - Beep Test
        - Bataka (incl. System record)
    - Player manager with extended Export and Import incl. highscores and statistics   
    - Minor bugfixes in some games (Player-List added, minor layout changes)
        - fix for Whac-A-Mole color picker  

--- 


## 🤝 Contributing
Found a bug? Have a game idea? Feel free to open an issue or submit a pull request. Let's make school sports digital (and affordable)!  
**Licence:** Use it, change it, DO NOT SELL it.  see License.txt

*Project by WorksAsDesigned*
