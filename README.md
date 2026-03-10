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

## Puck Racer System – Modes Overview

The Puck Racer System offers a wide range of modes, from simple measurement tools to high-intensity, gamified training experiences.

## Focus Legend

⚡ Reaction: Hand-eye coordination and fast stimulus processing  
🏃‍♂️ Agility: Rapid direction changes, sprints, and footwork  
🫀 Endurance: Cardiovascular load and pacing  
🧠 Cognition: Memory, timing, and stress resistance  
🤪 Fun: Teambuilding and motivation, especially for kids and youth  
🛠️ Utility: Practical tools for coaches and teachers  

---

## 🏃‍♂️ Agility & Reaction

## 🎯 Hunt! (Color Hunt)

Chase your assigned color around a circle or across the field. A dynamic drill that forces quick direction changes, peripheral awareness, and sprinting under time pressure. When one puck lights up and gets pressed, the next one appears. Fail to reach it in time and you get penalized.

**Players:** 1–10  
**Pucks:** 3–20 (distributed in the area)  
**Focus:** ⚡ Reaction, 🏃‍♂️ Agility, 🧠 Cognition  
**Features:** Dynamic queues, adjustable difficulty (timeout), visual 1-second warning  

---

## ⚡ React 2-Player

The ultimate head-to-head duel. Two players face each other across a row of pucks. Each player has their own color. When it lights up, react faster than your opponent.

**Groups:** 1–5 (duel pairs)  
**Pucks:** 3–10 per group  
**Focus:** ⚡ Reaction, 🧠 Cognition  
**Features:** Configurable number of colors per player, fake colors for confusion  

---

## 🎯 Target Touch

Improves core stability and reaction time. Ideal as a smart assistant for sit-ups, planks, wall jumps, and similar exercises. Players must touch pucks in a predefined or random sequence.

**Groups:** 1–5  
**Pucks:** 2–10 per group  
**Focus:** ⚡ Reaction, 🛠️ Utility  
**Features:** Adjustable rest times, sequence or random mode, time or round limits  

---

## 🏃‍♂️💨 Agility T-Test

Digital implementation of the global standard agility test. Pucks are arranged in a T-shape to train sprints and multidirectional movement.

**Groups:** 1–2  
**Pucks:** Exactly 4 per group  
**Focus:** 🏃‍♂️ Agility  
**Features:** Automatic pacing or mandatory touch mode  

---

## 🫀 Endurance & Pacing

## ⏱️🟢 The Pacemaker

A virtual pacer that maintains an exact target speed. Pucks are placed at fixed intervals. A moving light indicates the precise pace. The runner must reach each puck before the light turns off.

**Players:** Entire running groups  
**Pucks:** 4–20  
**Focus:** 🫀 Endurance, 🧠 Cognition  
**Features:** Circle or shuttle mode, live distance calculation, precise pace control  

---

## 🏃‍♂️🔁⏱️ Luc Léger (Beep Test)

The classic progressive shuttle run test for estimating VO₂max. Signal frequency increases every minute.

**Players:** 1–10  
**Pucks:** 2 per player  
**Focus:** 🫀 Endurance  
**Features:** Official tables included, live VO₂max calculation, false start detection  

---

## 🏃‍♂️🔁 Shuttle Run

Simple shuttle running between two points.

**Players:** 1–5  
**Pucks:** 2 per player  
**Focus:** 🏃‍♂️ Agility, 🫀 Endurance  
**Features:** Automatic winner detection, false start detection  

---

## 🧟 Zombie Escape

Repeated sprint challenge with decreasing time limits. Fail to reach the target in time and you're eliminated.

**Players / Groups:** 1–10  
**Pucks:** 2 per group  
**Focus:** 🫀 Endurance, 🧠 Cognition, 🤪 Fun  
**Features:** Automatic time reduction, sudden death mode, synchronized start  

---

## 🧠 Cognition & Fun

## 🔴🟢🔵🟡 Simon Says

The classic memory challenge. Repeat the shown color sequence without mistakes.

**Groups:** 1–3  
**Pucks:** 2–10 per group  
**Focus:** 🧠 Cognition  
**Features:** Adjustable speed, color-blind friendly palette  

---

## 🔴🟢🔵🏃‍♂️ Simon Runs

Simon Says — but with movement. Players must run to reproduce the shown sequence.

**Groups:** 1–3  
**Pucks:** 1 display puck + unlimited input pucks  
**Focus:** 🧠 Cognition, 🏃‍♂️ Agility  
**Features:** Central display mode, disqualification management  

---

## 💣⏰ Bomb Squad

Trains internal timing under heavy stress. Press exactly when the target time expires.

**Players:** 1–10  
**Pucks:** 1 per player  
**Focus:** 🧠 Cognition, 🤪 Fun  
**Features:** Adjustable tolerance windows, explosion feedback  

---

## 🚦🛑 Red Light, Green Light

Fun warm-up game. Move on green, freeze on red.

**Groups:** 1–10  
**Pucks:** 1–20  
**Focus:** 🤪 Fun, ⚡ Reaction  
**Features:** Independent phases, grace period, coach override  

---

## 🎩✨ The Sorting Hat

Fair and random team assignment tool.

**Players:** 2–100  
**Pucks:** 1–3  
**Focus:** 🛠️ Utility, 🤪 Fun  
**Features:** Balanced assignment, live rebalance suggestions  

---

## 🛠️ Measurement Tools & Utilities

## ⏱⏱️ Stopwatch

Each puck acts as an independent or synchronized timer.

**Players:** 1–10  
**Features:** Split times, centralized or individual start  

---

## ⏱️ Simple Timer

Fixed time limit mode for training stations.

**Players:** 1–10  
**Features:** Visual countdown, live penalty adjustment  

---

## ⏱️🧮 Simple Countdown / 🔢 Simple Counter

Counts touches up or down.

**Players:** 1–10  
**Features:** Debounce filter, automatic winner detection  

---
---

## ⏱️🟢 The Pacemaker

A virtual "rabbit" that dictates the exact running speed. A light travels from puck to puck at a configured pace.

**Players:** 1+ (Entire running groups)  
**Pucks:** 4–20  
**Focus:** 🫀 Endurance, 🧠 Cognition (Pacing)  
**Features:** Precise pace setting (min/km), circle or shuttle mode, live distance calculation, dynamic countdown warnings  

---

## 🧠🏃 Memory Sprint

Trains spatial working memory under maximal physical stress. Players must find matching color pairs spread across a large area.

**Players:** 1+  
**Pucks:** 4–20 (even numbers for pairs)  
**Focus:** 🧠 Cognition, 🏃‍♂️ Agility  
**Features:** Configurable memorize times, blind search mode, auto-restart function, live error tracking  

---

## ⚔️🔴🔵 Domination (Turf War)

High-intensity team interval game. Two teams fight to claim and steal pucks scattered around the arena before time runs out.

**Players:** 2 Teams per Arena  
**Pucks:** 2+ per Arena  
**Focus:** 🏃‍♂️ Agility, 🫀 Endurance  
**Features:** 1-click vs 2-click capture mechanics, visual protection timers, dual-arena support (run 2 games simultaneously), live score tracking  

---

## ⚡🎯👀 Batak Pro

Tests reaction time and peripheral vision. Pucks light up randomly and accelerate continuously. Hit them before they expire!

**Players:** 1  
**Pucks:** 3–20 (Wall-mounted or spread on the floor)  
**Focus:** ⚡ Reaction, 🧠 Cognition  
**Features:** Adaptive acceleration (speedup mode), fake colors (no-go stimulus), per-puck reaction time analysis, layout visualization  

---

## ⏱️🥵 TABATA Controller

Visual and acoustic workout assistant for Tabata and circuit training. Perfectly synchronizes all circuit stations.

**Players:** Unlimited (Circuit stations)  
**Pucks:** 1 per station  
**Focus:** 🛠️ Utility, 🫀 Endurance  
**Features:** Synchronized progress bars, visual rest/work phases, round & time limits, dynamic pause/resume function, configurable sound modes  

---  
🐱⚡ Cat Reflex
Fast-paced group reaction game. A central puck signals the start, and everyone sprints to their designated puck as quickly as possible.

Groups: 1–11 Players

Pucks: 1 per player + 1 central display puck

Focus: ⚡ Reaction & Explosive Start

Features: Highlander mode (only the fastest gets a point) and Color Chaos mode to train focus and prevent false starts.
---  
❌⭕ Tactical TicTacToe (TTTT)
Precision meets tactics! Play classic TicTacToe, but instead of drawing on paper, you claim the fields by throwing a ball at the pucks mounted on a wall.

Groups: 1–3 Players

Pucks: 4, 6, 7, or 10 (Grid size adapts to puck count)

Focus: 🎯 Precision Throwing & 🧠 Tactics

Features: Hard-Mode (you must neutralize an opponent's puck before claiming it) and auto-restarting grids.

---  
🪑🎵 Musical Chairs
The ultimate party and gym class highlight. Move around the room while the music plays, and secure a glowing puck as soon as the sound abruptly stops!

Groups: 2–13 Players

Pucks: Player count minus one (dynamically decreases each round)

Focus: 🏃‍♂️ Acceleration & ⚡ Reaction under pressure

Features: Custom MP3 upload and adjustable music randomness/variance.

---  


## ▶️✨ Display Mode

Coach control console for lighting and event usage.

**Features:** Manual control, sync mode, random mode  

---

## 👥 Player Manager  

Manage teams, classes, friendlists for easier name assignment

**Features:** Upload and backup simple CSV lists. Manually create teams for later use.  

## 📋 Training Manager  

Create predefined trainings with a selection of games, sequence and already preset game settings. **No need anymore for friggeling with setup page during the training!** 

**Features:** Define a set of 3 games, especially for reaction training. Preset the parameters (distance, times, number of players, ...). Start the training and jump from one game to the next one.  

---


## Technical Updates:
- "fly in" - If a pucks loses his connection or needs to be restartet, his last effect and game assignment is recovered.
- Impressum added
- harmonization of most setup pages

## 🤝 Contributing
Found a bug? Have a game idea? Feel free to open an issue or submit a pull request. Let's make school sports digital (and affordable)!  
**Licence:** Use it, change it, DO NOT SELL it.  see License.txt

*Project by WorksAsDesigned*
