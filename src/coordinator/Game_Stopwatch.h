#ifndef GAME_STOPWATCH_H
#define GAME_STOPWATCH_H

#include "Game.h"
#include <vector>
#include <FastLED.h>

enum SW_State {
    SW_SETUP = 0,
    SW_WAIT_HANDS = 1,  
    SW_COUNTDOWN = 2,
    SW_FALSE_START = 3,
    SW_RUNNING = 4,     
    SW_FINISHED = 5     
};

enum SW_PlayerState {
    PL_IDLE = 0,
    PL_READY = 1,       
    PL_RUNNING = 2,
    PL_FINISHED = 3
};

struct SW_Player {
    SW_PlayerState state;
    unsigned long startTime;
    unsigned long finishTime;
    std::vector<unsigned long> laps; 
    bool isHolding;
    bool falseStart;
};

struct SW_Action {
    unsigned long triggerTime;
    int puckIdx;
    int type; // 0 = Light, 1 = Sound
    int val1; 
    int val2; 
    // Parameter für Lichter (NEU)
    int effect;
    CRGB color;
    int speed;
    int bright;
};

class Game_Stopwatch : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Stopwatch"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    int numPlayers = 1;
    bool centralStart = false;
    bool holdToStart = false;
    bool lapMode = false;
    
    SW_State gameState = SW_SETUP;
    unsigned long stateStartTime = 0;
    unsigned long allReadyTime = 0; 
    
    SW_Player players[MAX_PEERS];
    
    const CRGB PLAYER_COLORS[10] = {
        CRGB::Blue, CRGB::Red, CRGB::Green, CRGB::Yellow, 
        CRGB::Magenta, CRGB::Cyan, CRGB::Orange, CRGB::Purple, 
        CRGB::White, CRGB::Lime
    };

    void initGame();
    void startSequence();
    void triggerFalseStart(int pIdx);
    void stopAll();
    void resetAll();
    
    void queueAction(int pIdx, int type, int v1, int v2, int delayMs);
    void queueLight(int pIdx, int effect, CRGB color, int speed, int bright, int delayMs); // NEU
    void processActions();
    
    std::vector<SW_Action> actionQueue;
    
    void setPuck(int index, int effect, CRGB color, int speed = 0, int bright = 255);
};

#endif