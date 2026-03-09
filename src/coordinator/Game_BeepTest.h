#ifndef GAME_BEEPTEST_H
#define GAME_BEEPTEST_H

#include "Game.h"
#include <vector>
#include <FastLED.h>

enum BeepGameState {
    BEEP_SETUP = 0,
    BEEP_WAIT_HANDS = 1,
    BEEP_COUNTDOWN = 2,
    BEEP_FALSE_START = 3,
    BEEP_RUNNING = 4,
    BEEP_FINISHED = 5
};

struct BeepPlayer {
    int id;
    int startPuckIdx;
    int targetPuckIdx;
    CRGB color;
    
    bool isHolding;
    bool falseStart;     
    bool isEliminated;
    
    int currentPuckTarget; 
    int playerLap;         // Welche Runde läuft der Spieler gerade?
    int yellowCards;
    
    unsigned long errorTime; 
    
    int highestLevel;
    float vo2max;
    
    int visualState; 
};

class Game_BeepTest : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Luc Leger Test"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    int numPlayers = 1;
    BeepGameState state = BEEP_SETUP;
    
    BeepPlayer players[10];
    
    unsigned long globalCountdownStart = 0;
    
    unsigned long testStartTime = 0;
    unsigned long finishTime = 0;
    
    int globalLap = 1;
    int currentLevel = 1;
    float currentSpeed = 8.5;
    
    unsigned long stateStartTime = 0;

    // Der feste Fahrplan für alle Runden in absoluten Millisekunden!
    unsigned long schedule[250]; 
    int totalScheduleLaps = 0;

    const int lapsPerLevel[21] = {7, 8, 8, 8, 9, 9, 10, 10, 10, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 15};
    const CRGB PLAYER_COLORS[10] = {
        CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Yellow, 
        CRGB::Magenta, CRGB::Cyan, CRGB::Orange, CRGB::White, 
        CRGB::Lime, CRGB::Pink
    };

    void initGame();
    void resetPlayerStats();
    void setGameState(BeepGameState newState);
    void eliminatePlayer(int pIdx, bool sendSound);
    void assignYellowCard(int pIdx);
    void updatePlayerVisuals(int pIdx, unsigned long now);
    void triggerFalseStart(int pIdx);
    void updatePlayerStats(int pIdx);
    
    void setPuck(int index, int effect, CRGB color, int speed=0, int bright=100);
    int getPlayerIndex(int puckIndex, int& puckRole);
};

#endif