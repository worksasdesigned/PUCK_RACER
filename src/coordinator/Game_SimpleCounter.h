#ifndef GAME_SIMPLECOUNTER_H
#define GAME_SIMPLECOUNTER_H

#include "Game.h"
#include <FastLED.h> 

enum GameState {
    STATE_SETUP = 0,
    STATE_PREPARE = 1,
    STATE_RUNNING = 2,
    STATE_FINISHED = 3
};

class Game_SimpleCounter : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Simple Counter"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    // Config
    int activePucks = 1; // NEU: Anzahl der konfigurierten Spieler
    int timeLimitSeconds = 0; 
    bool warnEnd = false;

    // Runtime
    GameState state = STATE_SETUP;
    unsigned long stateStartTime = 0; 
    unsigned long gameStartTime = 0;
    unsigned long finishTime = 0; // Wann wurde das Spiel beendet?
    bool winnerAnimationDone = false; // Läuft die Gewinner-Show noch?

    int scores[MAX_PEERS];
    unsigned long lastInput[MAX_PEERS]; 
    
    // Flash Management (Damit der Puck nach dem Blitz wieder zurück wechselt)
    unsigned long flashStartTime[MAX_PEERS];
    bool isFlashing[MAX_PEERS];

    const CRGB PLAYER_COLORS[12] = {
        CRGB::Blue, CRGB::Green, CRGB::Red, CRGB::Yellow, 
        CRGB::Magenta, CRGB::Cyan, CRGB::Orange, CRGB::Purple, 
        CRGB::Turquoise, CRGB::Pink, CRGB::White, CRGB::Lime
    };

    void setPuckToPlayerColor(int index, int effect, int speed = 100);
    void startCountdown();
    void stopGame();
    void resetGame();
    void exitGame();
};

#endif