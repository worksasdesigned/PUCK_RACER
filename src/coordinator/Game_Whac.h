#ifndef GAME_WHAC_H
#define GAME_WHAC_H

#include "Game.h"
#include <vector>
#include <FastLED.h>

enum WhacGlobalState {
    WHAC_SETUP = 0,
    WHAC_LAYOUT = 1,          
    WHAC_START_SKI = 2,
    WHAC_START_TARGETS = 3,
    WHAC_RUNNING = 4,
    WHAC_FINISHED_RAINBOW = 5,
    WHAC_FINISHED_WHITE = 6
};

enum WhacGroupState {
    GRP_IDLE = 0,
    GRP_WAIT_NEXT_ROUND = 1,
    GRP_ACTIVE = 2,           
    GRP_MEM_SHOW = 3,         
    GRP_MEM_WAIT = 4,         
    GRP_MEM_ACTIVE = 5,       
    GRP_SHOW_ERRORS = 6,      
    GRP_MEM_SUCCESS = 7       
};

struct WhacPuck {
    int globalIdx;
    int colorIdx;
    bool isTarget;
    bool isHit;
    unsigned long revertTime; // NEU: Verhindert blockierende Delays
};

struct WhacGroup {
    int id;
    bool active;
    std::vector<WhacPuck> pucks;
    
    WhacGroupState state;
    unsigned long stateStartTime;
    unsigned long nextRoundDelay; 
    
    int hits;          
    int misses;          
    int errorsThisRound; 
    int shiftCount;    
};

class Game_Whac : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Whac-A-Mole"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    int numGroups = 1;        
    int pucksPerGroup = 3;    
    unsigned long durationMs = 60000; 
    int difficulty = 1;       
    int numColors = 2;        
    bool soundOn = true;
    bool showHitColors = true;
    
    uint16_t targetColorMask = 0; 

    WhacGlobalState globalState = WHAC_SETUP;
    unsigned long globalStateTimer = 0;
    unsigned long runStartTime = 0;
    
    WhacGroup groups[3];
    
    const unsigned long LVL2_TIMEOUT = 5000;
    const unsigned long LVL3_TIMEOUT = 2000;
    const unsigned long LVL4_TIMEOUT = 6000;
    const unsigned long LVL4_SHIFT = 1500;
    const unsigned long LVL5_SHOW = 1000;
    const unsigned long LVL5_WAIT = 1500;
    
    const CRGB GAME_COLORS[10] = {
        CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Yellow, CRGB::Magenta, 
        CRGB::Cyan, CRGB::Orange, CRGB::DeepPink, CRGB::Purple, CRGB::SpringGreen
    };
    
    const CRGB ARENA_COLORS[3] = {CRGB::Purple, CRGB::Cyan, CRGB::Orange};

    void initGame();
    void setGlobalState(WhacGlobalState newState);
    void setGroupState(int gIdx, WhacGroupState newState);
    
    void spawnRound(int gIdx);
    void checkRoundEnd(int gIdx);
    void handleLevel4Shift(int gIdx);
    
    void setAllPucks(int effect, CRGB color, int speed, int bright);
    void setAllActivePucks(int effect, CRGB color, int speed, int bright);
    void setGroupPucksNeutral(int gIdx);
    void setPuck(int globalIdx, int effect, CRGB color, int speed=0, int bright=100);
    void sendSound(int globalIdx, int duration);
    void sendSequence(int globalIdx, int seqID);
};

#endif