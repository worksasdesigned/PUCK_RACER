#ifndef GAME_MEMORY_H
#define GAME_MEMORY_H

#include "Game.h"
#include <vector>
#include <FastLED.h>

enum MemState {
    MEM_SETUP = 0,
    MEM_MEMORIZE = 1,     // Zeigt Farben zum Merken
    MEM_SEARCH_INIT = 2,  // Zeigt Rainbow (Search Mode)
    MEM_RUNNING = 3,      // Warten auf Eingaben
    MEM_EVALUATE = 4,     // Zeigt Match/Mismatch für 1.5s
    MEM_REVEAL = 5,       // Trainer hat "Aufdecken" gedrückt
    MEM_FINISHED = 6,
    MEM_IDLE = 7
};

class Game_Memory : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Memory Sprint"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    // Config
    int numPucks = 4;
    int memorizeTime = 5000; // 2000, 5000, 10000
    bool autoRestart = false;
    bool searchMode = false;

    // Runtime
    MemState gameState = MEM_SETUP;
    unsigned long stateStartTime = 0;
    
    int activePucksCount = 0;
    int puckGlobalIds[MAX_PEERS]; 
    CRGB puckColors[MAX_PEERS];
    bool puckSolved[MAX_PEERS];
    
    int firstPuckIdx = -1;
    int secondPuckIdx = -1;
    bool isMatch = false;
    
    int pairsFound = 0;
    int errors = 0;
    
    // 10 verfügbare Farben für bis zu 20 Pucks (10 Paare)
    const CRGB PALETTE[10] = {
        CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Yellow, 
        CRGB::Magenta, CRGB::Cyan, CRGB::Orange, CRGB::DeepPink, 
        CRGB::Lime, CRGB::Purple
    };

    void initGame();
    void startGame();
    void shufflePucks();
    void resetUnsolvedToNeutral();
    
    void setPuck(int globalIdx, int effect, CRGB color, int speed=0, int bright=100);
    void sendSound(int globalIdx, int duration);
    void sendSequence(int globalIdx, int seqID);
};

#endif