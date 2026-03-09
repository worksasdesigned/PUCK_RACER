#ifndef GAME_SIMPLECOUNTDOWN_H
#define GAME_SIMPLECOUNTDOWN_H

#include "Game.h"
#include <FastLED.h> 

enum CdownState {
    CD_SETUP = 0,
    CD_PREPARE = 1,
    CD_RUNNING = 2,
    CD_FINISHED = 3
};

class Game_SimpleCountdown : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Simple Countdown"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    // Config
    int activePucks = 1;
    int startCount = 10;
    int timeLimitSeconds = 0;
    bool warnEnd = false;
    bool stopOnWin = false;

    // Runtime
    CdownState state = CD_SETUP;
    unsigned long stateStartTime = 0; 
    unsigned long gameStartTime = 0;
    unsigned long finishTime = 0;
    bool winnerAnimationDone = false;

    int currentCounts[MAX_PEERS]; 
    bool hasFinished[MAX_PEERS]; 
    unsigned long finishTimes[MAX_PEERS]; // NEU: Individuelle Endzeit
    int winnerIndex = -1; 
    unsigned long winnerTime = 0;         // NEU: Referenzzeit des Siegers
    
    unsigned long lastInput[MAX_PEERS]; 
    
    // Flash Management
    unsigned long flashStartTime[MAX_PEERS];
    bool isFlashing[MAX_PEERS];

    const CRGB PLAYER_COLORS[12] = {
        CRGB::Blue, CRGB::Green, CRGB::Red, CRGB::Yellow, 
        CRGB::Magenta, CRGB::Cyan, CRGB::Orange, CRGB::Purple, 
        CRGB::Turquoise, CRGB::Pink, CRGB::White, CRGB::Lime
    };

    void setPuckToPlayerColor(int index, int effect, int val = 0);
    void updateProgress(int index);
    void startCountdown();
    void stopGame();
    void resetGame();
    void exitGame();
};

#endif