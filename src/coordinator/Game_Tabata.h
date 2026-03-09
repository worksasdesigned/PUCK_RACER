#ifndef GAME_TABATA_H
#define GAME_TABATA_H

#include "Game.h"
#include <FastLED.h>

enum TabataState {
    TABATA_SETUP = 0,
    TABATA_COUNTDOWN = 1,
    TABATA_WORK = 2,
    TABATA_REST = 3,
    TABATA_FINISHED = 4,
    TABATA_PAUSED = 5
};

class Game_Tabata : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "HIIT Controller"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    // Config
    int activePucksCount = 1;
    unsigned long workTimeMs = 20000;
    unsigned long restTimeMs = 10000;
    int soundMode = 0; // 0 = Countdown, 1 = Single Beep, 2 = Silent
    
    // Limits
    int limitMode = 0; // 0 = Rounds, 1 = Time
    int limitValue = 1; 
    int targetRounds = 0;
    unsigned long targetTimeMs = 0;

    // Runtime
    TabataState gameState = TABATA_SETUP;
    unsigned long globalStartTime = 0;
    unsigned long phaseStartTime = 0;
    
    // Pause Logic
    TabataState prePauseState = TABATA_WORK;
    unsigned long pauseBeginTime = 0;
    
    int rounds = 0;
    int warningStep = 0;
    int lastFill = -1;
    unsigned long lastTxTime = 0;

    void initGame();
    void startGameSequence();
    void triggerFinish();
    void setAllPucks(int effect, CRGB color, int speed, int bright);
    void sendBeep(int duration);
};

#endif