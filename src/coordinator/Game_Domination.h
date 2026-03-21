#ifndef GAME_DOMINATION_H
#define GAME_DOMINATION_H

#include "Game.h"
#include <vector>
#include <FastLED.h>

enum DomState {
    DOM_SETUP = 0,
    DOM_WAIT_HANDS = 1,
    DOM_COUNTDOWN = 2,
    DOM_FALSE_START = 3,
    DOM_RUNNING = 4,
    DOM_FINISHED = 5
};

struct DomPuck {
    int globalIdx;
    int owner; // 0 = Neutral, 1 = Team 1, 2 = Team 2
    int lastOwnerBeforeLock;
    
    unsigned long lastClickTime;
    unsigned long lockedUntil;
    unsigned long errorUntil;
    bool isLockedVisual; 
};

struct DomGroup {
    int id;
    bool active;
    DomState state;
    
    std::vector<DomPuck> pucks;
    
    CRGB colorT1;
    CRGB colorT2;
    
    int scoreT1;
    int scoreT2;

    int hpT1;
    int hpT2;
    int dmgT1;
    int dmgT2;
    unsigned long lastHpTick;

    bool holdT1;
    bool holdT2;
    bool falseStart;

    unsigned long stateStartTime;
    unsigned long runStartTime;

    bool finishedAnimDone;
};

class Game_Domination : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Domination"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    int numGroups = 1;
    unsigned long durationMs = 180000;
    unsigned long blockTimeMs = 2000;
    bool groupStart = true;
    bool deathmatch = false;
    int hpPerTeam = 600;

    DomGroup groups[2]; 
    unsigned long globalCountdownStart = 0;

    // Farben angepasst: T1={Red, Green}, T2={Yellow, Blue}
    const CRGB T1_COLORS[2] = {CRGB::Red, CRGB::Green};
    const CRGB T2_COLORS[2] = {CRGB::Yellow, CRGB::Blue};
    // Kennfarben für die Arenen im Setup (Lila, Türkis)
    const CRGB ARENA_COLORS[2] = {CRGB::Purple, CRGB::Cyan};

    void initGame();
    void setGroupState(int gIdx, DomState newState);
    void updateScores();
    void triggerFalseStart(int gIdx);
    void neutralizePuck(int globalIdx);
    
    void setPuck(int index, int effect, CRGB color, int speed=0, int bright=100);
    void sendSound(int index, int duration);
    void sendSequence(int index, int seqID);
};

#endif