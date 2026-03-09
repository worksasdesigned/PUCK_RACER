#ifndef GAME_MUSICAL_H
#define GAME_MUSICAL_H

#include "Game.h"
#include <FastLED.h>

// REISE NACH JERUSALEM

enum MusicalState {
    MUS_SETUP = 0,
    MUS_WAITING = 1,
    MUS_PLAYING = 2,
    MUS_OPEN = 3,
    MUS_PAUSED_INTERIM = 4, // Pause zwischen Runden
    MUS_PAUSED_MANUAL = 5,  // Trainer hat Pause gedrückt
    MUS_FINISHED = 6
};

class Game_Musical : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Reise nach Jerusalem"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    // Config
    int initialPlayers = 2;
    int baseTimeSec = 10;
    int varType = 1; // 0=2s, 1=5s, 2=10s
    int pauseTimeSec = 5;
    
    // Runtime
    MusicalState state = MUS_SETUP;
    unsigned long stateStartTime = 0;
    unsigned long targetTime = 0;
    
    int activePuckCount = 0; // Wie viele Pucks sind noch im Spiel?
    int pressedCount = 0;    // Wie viele Pucks wurden in dieser Runde schon gesichert?
    
    // 0 = Aktiv, 1 = Gedrückt (Save), 2 = Ausgeschieden, 3 = Manuell Deaktiviert
    int puckStates[MAX_PEERS]; 

    void startRound();
    void triggerOpen();
    void endRound();
    void eliminateRandomPuck();
    void recalculateActive();
    
    void setPuck(int index, int effect, CRGB color, int speed=0, int bright=255);
};

#endif