#ifndef GAME_SHUTTLERUN_H
#define GAME_SHUTTLERUN_H

#include "Game.h"
#include <FastLED.h> 

enum ShuttleState {
    SR_SETUP = 0,
    SR_WAIT_HANDS = 1,  
    SR_COUNTDOWN = 2,    
    SR_FALSE_START = 3, 
    SR_RUNNING = 4,      
    SR_FINISHED = 5
};

class Game_ShuttleRun : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Shuttle Run"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    // Config
    int playerCount = 1;
    int maxRounds = 5;
    
    // Runtime
    ShuttleState state = SR_SETUP;
    unsigned long stateStartTime = 0; 
    unsigned long gameStartTime = 0;
    unsigned long finishTime = 0; // NEU: Damit die Stoppuhr anhält
    
    // Player Stats
    int currentRound[MAX_PEERS/2]; 
    unsigned long finishTimes[MAX_PEERS/2]; 
    bool hasFinished[MAX_PEERS/2];
    bool isHolding[MAX_PEERS/2]; 
    unsigned long winnerTime = 0;
    int falseStartPlayer = -1;
    
    // Puck Management
    int activeTargetIdx[MAX_PEERS/2]; 
    
    // Flash Management
    unsigned long flashStartTime[MAX_PEERS];
    bool isFlashing[MAX_PEERS];

    const CRGB PLAYER_COLORS[12] = {
        CRGB::Blue, CRGB::Green, CRGB::Red, CRGB::Yellow, 
        CRGB::Magenta
    };

    void setPuck(int puckIndex, int effect, CRGB color, int speed=0, int bright=100);
    
    void startSequence(bool forceRestart = false);
    
    void triggerFalseStart(int playerIndex);
    void stopGame();
    void resetGame();
    void exitGame();
    
    int getPuckIndex(int player, int target); 
};

#endif