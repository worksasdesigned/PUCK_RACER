#ifndef GAME_DISPLAYMODE_H
#define GAME_DISPLAYMODE_H

#include "Game.h"
#include "Common.h" 
#include <FastLED.h> // <--- HIER HAT ES GEFEHLT!
#include <vector>    

enum DispMode {
    DISP_MANUAL = 0,  // DJ Mischpult Modus
    DISP_RANDOM = 1,  // Auto: Jeder Puck macht was er will
    DISP_SYNC = 2     // Auto: Alle Pucks zeigen synchron das Gleiche
};

struct PuckState {
    uint8_t effId;
    CRGB color;
    int speed;
    uint8_t bright;
    bool muted;
};

class Game_DisplayMode : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override; 
    String getName() override { return "Display Mode"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    int durationMin = 15;
    unsigned long gameStartTime;
    
    DispMode mode = DISP_MANUAL;
    
    PuckState puckStates[MAX_PEERS];
    
    // Auto-Mode Variablen
    unsigned long nextEventTime[MAX_PEERS];
    unsigned long globalNextEvent;
    int autoStep = 0;

    void initGame();
    void setPuck(int targetIdx, uint8_t eff, uint8_t r, uint8_t g, uint8_t b, int spd, uint8_t bri);
    void parsePlayCommand(String data);
    
    void updateRandom();
    void updateSync();
};

#endif