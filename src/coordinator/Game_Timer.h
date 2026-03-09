#ifndef GAME_TIMER_H
#define GAME_TIMER_H

#include "Game.h"
#include <vector>
#include <FastLED.h>

enum TimerState {
    TS_RUNNING = 0,
    TS_PAUSED = 1,
    TS_FINISHED_EARLY = 2, 
    TS_TIME_UP = 3,        
    TS_STOPPED = 4         
};

struct TimerPlayer {
    int id;
    int puckIdx;
    CRGB color;
    
    long timeLeft;
    long initialTime; 
    
    TimerState state;
    
    int lastFillLevel;
    unsigned long lastTxTime;
};

class Game_Timer : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Simple Timer"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    int numPlayers = 1;
    long globalBaseTime = 60000; 
    bool gameActive = false;
    unsigned long lastLoopTime = 0;
    
    int globalBrightness = 100; // NEU: Helligkeitsskalierung in Prozent (10-100)
    
    TimerPlayer players[10]; 
    
    const CRGB PLAYER_COLORS[10] = {
        CRGB::Blue, CRGB::Purple, CRGB::Orange, CRGB::Cyan, 
        CRGB::Magenta, CRGB::Yellow, CRGB::Lime, CRGB::DeepPink,
        CRGB::Aqua, CRGB::White
    };

    void initGame();
    void setPlayerState(int pIdx, TimerState newState);
    void modifyTime(int pIdx, long msDelta);
    
    void setPuck(int index, int effect, CRGB color, int speed=0, int bright=100);
    int getPlayerIndex(int puckIndex);
};

#endif