#ifndef GAME_REACT2P_H
#define GAME_REACT2P_H

#include "Game.h"
#include <vector>
#include <FastLED.h>

enum ReactState {
    R2_SETUP = 0,       
    R2_PRE_SHOW = 1,    
    R2_COUNTDOWN = 2,   
    R2_WAITING = 3,     
    R2_RUNNING = 4,     
    R2_SHOW_WIN = 5,    
    R2_FINISHED = 6     
};

struct ReactPlayer {
    int score;
    unsigned long totalReactionTime;
    int currentPuckLocalIdx; 
    CRGB currentColor;       
};

struct ReactGroup {
    int id;
    ReactState state;
    bool active;
    
    std::vector<int> puckIndices;
    CRGB groupColor;
    
    ReactPlayer p1;
    ReactPlayer p2;
    
    int roundWinner; 
    
    unsigned long stateStartTime;
    unsigned long roundStartTime;
    unsigned long nextWaitDuration;
    int preShowStep;
    unsigned long groupStartTime;
    unsigned long groupStoppedTime;
};

class Game_React2P : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "React 2-Player"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    int numGroups = 1;
    int pucksPerGroup = 3;
    unsigned long timeLimit = 90000; 
    int colorsPerPlayer = 1;
    bool useFakeColors = false;

    unsigned long gameStartTime = 0;
    unsigned long stoppedTime = 0; // NEU: Für eingefrorene Stoppuhr
    
    ReactGroup groups[5]; 

    const CRGB P1_POOL[3] = {CRGB::Red, CRGB::Magenta, CRGB::DeepPink};
    const CRGB P2_POOL[3] = {CRGB::Blue, CRGB::Cyan, CRGB::Lime};
    const CRGB FAKE_POOL[4] = {CRGB::Yellow, CRGB::Orange, CRGB::White, CRGB::Gold};
    
    const CRGB GROUP_COLORS[5] = {CRGB::Purple, CRGB::Turquoise, CRGB::Coral, CRGB::Aquamarine, CRGB::MediumPurple};

    void initGame();
    void setGroupState(int gIdx, ReactState newState);
    void setupNextRound(int gIdx);
    
    void setPuck(int index, int effect, CRGB color, int speed=0, int bright=100);
    int getGroupIndex(int puckIndex, int& localPuckIdx);
    
    String colorToHex(CRGB c);
};

#endif