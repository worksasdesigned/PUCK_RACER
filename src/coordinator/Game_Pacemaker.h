#ifndef GAME_PACEMAKER_H
#define GAME_PACEMAKER_H

#include "Game.h"
#include <FastLED.h>

enum PM_State {
    PM_SETUP = 0,
    PM_WAIT_HANDS = 1,
    PM_COUNTDOWN = 2,
    PM_FALSE_START = 3,
    PM_RUNNING = 4,
    PM_FINISHED = 5
};

class Game_Pacemaker : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "The Pacemaker"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    // Config
    int inputMode = 0; // 0 = Pace (min/km), 1 = Lap Time
    int paceSecondsPerKm = 300; 
    int distanceBetweenPucks = 10; 
    unsigned long lapTimeMs = 60000;
    unsigned long durationMs = 60000;
    bool shuttleMode = false;
    bool holdToStart = true;
    int numGroups = 1; // 1 oder 2 Gruppen
    
    // Runtime
    PM_State gameState = PM_SETUP;
    unsigned long stateStartTime = 0;
    unsigned long runStartTime = 0;
    unsigned long stoppedElapsedMs = 0;
    
    int activePucksCount = 0;
    int puckGlobalIds[MAX_PEERS]; // Mappt den logischen Pfad auf die echten Pucks
    
    // Arrays für bis zu 2 Läufer/Gruppen
    int currentPuckIdx[2] = {0, 0};
    bool isHoldingStart[2] = {false, false};
    int falseStartPlayer = -1; // NEU: Für die Anzeige auf der Webseite
    
    unsigned long msPerPuck = 3000;
    
    // Virtueller Hase Runtime
    int currentSegment = -1;
    bool halfPassedFlag = false;
    
    // 4er Blöcke Farben (Rot, Blau, Grün, Gelb, Magenta, Cyan, Orange, Pink) für Setup
    const CRGB BLOCK_COLORS[8] = {
        CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Yellow,
        CRGB::Magenta, CRGB::Cyan, CRGB::Orange, CRGB::DeepPink
    };

    void initGame();
    void showTrack();
    void startGameSequence();
    void triggerFalseStart(int runner);
    int getSequencePuck(int group, int step);
    
    CRGB getPuckColor(int idx);    // Für buntes Setup
    CRGB getGroupColor(int group); // Für den Lauf (Grün / Magenta)
    
    void setPuck(int globalIdx, int effect, CRGB color, int speed=0, int bright=100);
    void sendSound(int globalIdx, int duration);
    void sendSequence(int globalIdx, int seqID);
};

#endif