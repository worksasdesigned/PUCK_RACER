#ifndef GAME_ZOMBIE_H
#define GAME_ZOMBIE_H

#include "Game.h"
#include <vector>
#include <FastLED.h>

enum ZombieState {
    Z_SETUP = 0,         // Pucks leuchten nur (warten auf Spielstart durch Trainer)
    Z_WAIT_HANDS = 1,    // Warten darauf, dass der Spieler den Startpuck hält
    Z_COUNTDOWN = 2,     // Countdown läuft (Beep... Beep... GO)
    Z_FALSE_START = 3,   // Spieler hat zu früh losgelassen
    Z_RUNNING = 4,       // Spieler rennt zum Ziel
    Z_EVALUATE = 5,      // Ziel erreicht, zeige Win/Fail Effekt für 3 Sekunden
    Z_ELIMINATED = 6     // Ausgeschieden
};

struct ZombieGroup {
    int id;
    ZombieState state;
    
    int startPuckIdx;
    int targetPuckIdx;
    CRGB color;
    
    unsigned long currentTargetTime; 
    int roundsPlayed;
    unsigned long lastRunTime;       
    
    bool isHolding;
    bool isWinner;                   
    int shuttleLaps;
    bool expectStart;
    
    unsigned long stateStartTime;
    unsigned long runStartTime;
};

class Game_Zombie : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Zombie Escape"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    int numGroups = 1;
    unsigned long startTargetTime = 10000; 
    unsigned long timeReduction = 500;     
    bool autoReduce = false;
    bool suddenDeath = false;
    bool groupStart = true;
    bool shuttleMode = false;

    ZombieGroup groups[10]; 
    unsigned long globalBestTime = 0;
    int globalBestGroup = -1;
    
    unsigned long globalCountdownStart = 0;
    int countdownStep = 0;

    const CRGB GROUP_COLORS[10] = {
        CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Yellow, 
        CRGB::Magenta, CRGB::Cyan, CRGB::Orange, CRGB::White, 
        CRGB::Lime, CRGB::Pink
    };

    void initGame();
    void setGroupState(int gIdx, ZombieState newState);
    void evaluateRun(int gIdx, unsigned long runTime, bool timeout = false);
    void triggerFalseStart(int gIdx);
    
    void setPuck(int index, int effect, CRGB color, int speed=0, int bright=100);
    int getGroupIndex(int puckIndex, bool& isTarget);
    void updateShuttlePucks(int gIdx);
};

#endif