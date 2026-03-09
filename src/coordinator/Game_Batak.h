#ifndef GAME_BATAK_H
#define GAME_BATAK_H

#include "Game.h"
#include <FastLED.h>

enum BatakState {
    BATAK_SETUP = 0,
    BATAK_WAIT_HANDS = 1,
    BATAK_COUNTDOWN = 2,
    BATAK_FALSE_START = 3,
    BATAK_RUNNING = 4,
    BATAK_FINISHED = 5,
    BATAK_SHOW_COLORS = 6
};

struct BatakPuck {
    int globalIdx;
    bool isActive;
    bool isTarget;
    unsigned long spawnTime;
    unsigned long expireTime;
    
    // Stats
    int hits;
    int misses;
    unsigned long totalReactionTime;
};

class Game_Batak : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Batak Pro"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    // Config
    int activePucksCount = 3;
    int difficulty = 2;
    bool holdToStart = false;
    bool speedupMode = true; 
    
    int targetColorIdx = 2;
    bool soundOn = true;
    bool fakeColors = false;
    
    unsigned long durationMs = 60000;

    // Runtime
    BatakState gameState = BATAK_SETUP;
    unsigned long stateStartTime = 0;
    unsigned long runStartTime = 0;
    
    BatakPuck pucks[MAX_PEERS];
    
    // Spielmechanik Parameter
    unsigned long currentSpawnInterval;
    unsigned long minSpawnInterval;
    unsigned long hitTimeWindow;
    unsigned long nextSpawnTime;
    
    int totalHits = 0;
    int totalMisses = 0;
    unsigned long globalTotalRT = 0;

    // Farben für den Layout-Aufbau (Hardware-Mapping)
    const CRGB BLOCK_COLORS[8] = {
        CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Yellow, 
        CRGB::Magenta, CRGB::Cyan, CRGB::Orange, CRGB::DeepPink
    };
    
    // Farben für das eigentliche Spiel (0=Red, 1=Blue, 2=Green, 3=Yellow, 4=Magenta, 5=Cyan, 6=Orange, 7=White)
    const CRGB GAME_COLORS[8] = {
        CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Yellow,
        CRGB::Magenta, CRGB::Cyan, CRGB::Orange, CRGB::White
    };

    void initGame();
    void showLayout();
    void startGameSequence();
    void triggerFalseStart();
    void spawnPuck();
    void expirePuck(int idx);
    void applyDifficulty();
    
    CRGB getPuckColor(int idx);
    void setPuck(int globalIdx, int effect, CRGB color, int speed=0, int bright=100);
    void sendSound(int globalIdx, int duration);
    void sendSequence(int globalIdx, int seqID);
};

#endif