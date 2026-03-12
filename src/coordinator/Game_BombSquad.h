#ifndef GAME_BOMBSQUAD_H
#define GAME_BOMBSQUAD_H

#include "Game.h"
#include <FastLED.h> 

enum BombState {
    BOMB_SETUP = 0,
    BOMB_INFO = 1,      // Pucks blinken Anzahl Sekunden
    BOMB_COUNTDOWN = 2, // Ski-Start Sound läuft
    BOMB_RUNNING = 3,   // Zeit läuft (Blindflug)
    BOMB_FINISHED = 4
};

enum BombResult {
    RES_NONE = 0,
    RES_DEFUSED = 1,
    RES_EXPLODED = 2
};

struct PlayerStats {
    int score; 
    BombResult lastResult;
    long lastDelta; 
    long totalDelta;
    int roundsPlayed;
    int targetTime; // NEU: Speichert die individuelle Zielzeit pro Puck
};

class Game_BombSquad : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Bomb Squad"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    // Config
    int activePucks = 1;
    int maxSeconds = 10;
    int difficulty = 1; 
    int maxRounds = 5;
    bool groupMode = false;

    // Runtime
    int maxTargetTimeThisRound = 5; // Die höchste Zielzeit aller Pucks in der aktuellen Runde
    int globalTargetTime = 0;       // 0 = Individuell, >0 = Sync (gleiche Zeit für alle)
    unsigned long finishTime = 0;
    
    BombState state = BOMB_SETUP;
    unsigned long stateStartTime = 0; 
    unsigned long gameStartTime = 0; 
    
    PlayerStats players[MAX_PEERS];
    bool hasActed[MAX_PEERS];
    int currentRound = 0;

    const CRGB PLAYER_COLORS[12] = {
        CRGB::Blue, CRGB::Green, CRGB::Red, CRGB::Yellow, 
        CRGB::Magenta, CRGB::Cyan, CRGB::Orange, CRGB::Purple, 
        CRGB::Turquoise, CRGB::Pink, CRGB::White, CRGB::Lime
    };

    void initGame();
    void startRound();
    void evaluateRound(); 
    int getTolerance(); 
    void setPuck(int index, int effect, CRGB color, int speed=0, int bright=100, int extra=0);
    void sendSequence(int seqID);
};

#endif