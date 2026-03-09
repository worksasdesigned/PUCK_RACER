#ifndef GAME_BALL_H
#define GAME_BALL_H

#include "Game.h"
#include <vector>
#include <FastLED.h>

enum BallState {
    BALL_SETUP = 0,
    BALL_COUNTDOWN = 1,
    BALL_RUNNING = 2,
    BALL_FINISHED = 3,
    BALL_WAIT_RESTART = 4
};

struct BallPuck {
    int globalIdx;
};

struct BallGroup {
    int id;
    bool active;
    BallState state;
    
    std::vector<BallPuck> pucks;
    CRGB color;
    
    int score;
    int currentPuckLocalIdx;
    
    unsigned long stateStartTime;
    unsigned long runStartTime;
    unsigned long elapsedTime;
};

class Game_Ball : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Ball Prellen"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    int pucksPerGroup = 3; // 3 bis 5
    int numGroups = 1;
    unsigned long durationMs = 60000;
    bool autoRestart = false;
    bool soundOn = true;
    bool isRandom = false; 

    BallGroup groups[6]; 
    BallState globalState = BALL_SETUP;
    unsigned long globalRunStartTime = 0;
    unsigned long globalElapsedTime = 0; // NEU: Friert die globale Zeit ein

    const CRGB GRP_COLORS[6] = {
        CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Yellow, CRGB::Magenta, CRGB::Cyan
    };

    void initGame();
    void showLayout();
    void setGroupState(int gIdx, BallState newState);
    void advanceGroupPuck(int gIdx);
    
    void setPuck(int globalIdx, int effect, CRGB color, int speed=0, int bright=100);
    void sendSound(int globalIdx, int duration);
    void sendSequence(int globalIdx, int seqID);
};

#endif