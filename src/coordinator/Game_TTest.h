#ifndef GAME_TTEST_H
#define GAME_TTEST_H

#include "Game.h"
#include <vector>
#include <FastLED.h>

enum TTestState {
    TT_SETUP = 0,
    TT_WAIT_HANDS = 1,
    TT_COUNTDOWN = 2,
    TT_FALSE_START = 3,
    TT_RUNNING = 4,
    TT_FINISHED = 5
};

struct TTestGroup {
    int id;
    TTestState state;
    bool active;
    
    int puckIndices[4];
    CRGB color;
    
    float score;
    int currentSeqIdx; // Wo im Ablauf befindet sich der Spieler?
    bool isHoldingStart;
    bool falseStart;
    
    unsigned long stateStartTime;
    unsigned long targetTime; // Für Auto-Modus
};

class Game_TTest : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Agility T-Test"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    int numGroups = 1;
    unsigned long timeLimit = 180000; 
    bool groupStart = true;
    bool puckPress = true;
    bool soundEnabled = true;
    int autoSpeed = 1; // 0=Slow, 1=Mid, 2=Fast
    
    TTestGroup groups[2]; // Max 2 Gruppen (braucht 8 Pucks)

    const CRGB GROUP_COLORS[2] = {CRGB::Blue, CRGB::Red};
    
    // Ablauf: 1=Mitte, 2=Links, 3=Rechts, 1=Mitte, 0=Start
    const int SEQUENCE[5] = {1, 2, 3, 1, 0}; 
    const float DISTANCES[5] = {10.0, 5.0, 10.0, 5.0, 10.0}; // in Metern
    const float SPEEDS[3] = {2.0, 3.0, 4.0}; // m/s für Slow, Mid, Fast

    void initGame();
    void setGroupState(int gIdx, TTestState newState);
    void advanceSequence(int gIdx);
    
    void setPuck(int index, int effect, CRGB color, int speed=0, int bright=100);
    int getGroupIndex(int puckIndex, int& localPuckIdx);
    void getHighestScore(float& maxScore, bool& isTie);
};

#endif