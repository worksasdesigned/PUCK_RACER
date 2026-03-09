#ifndef GAME_SORTINGHAT_H
#define GAME_SORTINGHAT_H

#include "Game.h"
#include <vector>
#include <FastLED.h> 

enum SortState {
    SORT_SETUP = 0,
    SORT_STARTING = 1, // NEU: Non-blocking Startsequenz
    SORT_RUNNING = 2,  
    SORT_FINISHED = 3 
};

class Game_SortingHat : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Sorting Hat"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    // Config
    int numGroups = 2;
    int targetPlayers = 20;
    int activePucks = 1;

    // Runtime
    SortState state = SORT_SETUP;
    std::vector<int> ticketBag; 
    int groupCounts[8];         
    int assignedPlayers = 0;    
    
    // Pucks
    std::vector<int> puckIndices; 
    unsigned long lastTriggerTime[MAX_PEERS]; 

    // --- NON-BLOCKING START VARIABLES ---
    unsigned long startSeqTimer = 0;
    int startSeqStep = 0;

    // --- NON-BLOCKING FEEDBACK VARIABLES ---
    int delayedSoundPuckIdx = -1;
    unsigned long delayedSoundTimer = 0;

    const CRGB GROUP_COLORS[8] = {
        CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Yellow, 
        CRGB::Magenta, CRGB::Cyan, CRGB::Orange, CRGB::White
    };

    void initGame();
    void fillTicketBag();
    void updatePuckList();
    void setPuck(int index, int effect, CRGB color, int speed=0, int bright=100);
    
    // Hilfsfunktion startet die State-Machine
    void triggerStartSequence();
};

#endif