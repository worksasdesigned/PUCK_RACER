#ifndef GAME_TARGET_H
#define GAME_TARGET_H

#include "Game.h"
#include <vector>
#include <FastLED.h>

enum TargetState {
    TG_SETUP = 0,
    TG_WAIT_HANDS = 1,
    TG_COUNTDOWN = 2,
    TG_FALSE_START = 3,
    TG_RUNNING = 4,
    TG_FINISHED = 5
};

struct TargetGroup {
    int id;
    TargetState state;
    bool active;
    
    std::vector<int> puckIndices;
    CRGB color;
    
    int score;
    int currentTargetIdx; // Welcher Puck leuchtet gerade? (-1 wenn Pause)
    int lastPuckIdx;      // Verhindert, dass 2x derselbe leuchtet
    
    bool isHoldingStart;
    bool falseStart;
    
    unsigned long stateStartTime;
    unsigned long runStartTime;
    unsigned long nextPuckTime; // Wann endet die Pause?
    unsigned long finishTime;   // Wann war die Gruppe fertig?
};

class Game_Target : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Target Touch"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    int numGroups = 1;
    int pucksPerGroup = 3;
    unsigned long delayMs = 2000;
    
    int gameMode = 0; // 0 = Time, 1 = Rounds
    unsigned long timeLimit = 120000; 
    int targetRounds = 30;
    
    int seqMode = 0; // 0 = Random, 1 = Sequence
    bool groupStart = false;
    
    TargetGroup groups[5]; // Max 5 Gruppen (bei 2 Pucks = 10 gesamt)
    const CRGB GROUP_COLORS[5] = {CRGB::Magenta, CRGB::Cyan, CRGB::Yellow, CRGB::Lime, CRGB::DeepPink};

    void initGame();
    void setGroupState(int gIdx, TargetState newState);
    void evaluateWinners();
    
    void setPuck(int index, int effect, CRGB color, int speed=0, int bright=100);
    int getGroupIndex(int puckIndex, int& localPuckIdx);
};

#endif