#ifndef GAME_HUNT_H
#define GAME_HUNT_H

#include "Game.h"
#include <vector>
#include <FastLED.h>

enum HuntState {
    HUNT_SETUP = 0,
    HUNT_COUNTDOWN = 1,
    HUNT_RUNNING = 2,
    HUNT_FINISHED = 3,
    HUNT_WINNER = 4,
    HUNT_IDLE = 5
};

enum HuntPuckState {
    P_NEUTRAL = 0,
    P_ACTIVE = 1,
    P_PENALTY = 2,
    P_ERROR = 3
};

struct HuntPlayer {
    int id;
    CRGB color;
    int score;
    int currentPuckIdx; // -1 wenn in der Warteschlange
    int lastPuckIdx;    // Verhindert, dass 2x hintereinander der gleiche Puck aufleuchtet
    unsigned long penaltyEndTime; // Wann darf der Spieler wieder einen Puck bekommen?
};

struct HuntPuck {
    int globalIdx;
    HuntPuckState state;
    int assignedPlayerId; // Welchem Spieler gehört der Puck gerade?
    unsigned long stateEndTime; // Für Timeout, Penalty oder Error
    bool warningSent; // NEU: Merker, ob die 1-Sekunden-Warnung gesendet wurde
};

class Game_Hunt : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Hunt!"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    // Config
    int numPlayers = 1;
    int numPucks = 3;
    unsigned long gameDuration = 60000;
    int radius = 5;
    int difficulty = 3;
    bool soundOn = true;

    // Runtime
    HuntState gameState = HUNT_SETUP;
    unsigned long stateStartTime = 0;
    unsigned long lastWarningBeep = 0;
    
    HuntPlayer players[MAX_PEERS];
    HuntPuck activePucks[MAX_PEERS];
    int actualPuckCount = 0;

    const CRGB PLAYER_COLORS[10] = {
        CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Yellow, 
        CRGB::Magenta, CRGB::Cyan, CRGB::Orange, CRGB::DeepPink, 
        CRGB::Lime, CRGB::Purple
    };

    void initGame();
    void resetRound();
    void showColors();
    void startGameSequence();
    void evaluateWinners();
    void assignPucksFromQueue();
    unsigned long calculateTimeout();
    
    void setPuck(int index, int effect, CRGB color, int speed=0, int bright=100);
    void sendSound(int index, int duration);
    void sendSequence(int index, int seqID);
};

#endif