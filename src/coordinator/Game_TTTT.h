#ifndef GAME_TTTT_H
#define GAME_TTTT_H

#include "Game.h"
#include <FastLED.h>

enum TTTT_State {
    TTTT_SETUP = 0,
    TTTT_RUNNING = 1,
    TTTT_FINISHED = 2
};

class Game_TTTT : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Tactical TicTacToe"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    // Config
    int numPlayers = 2; // 1 bis 3
    int numPucks = 10;  // 4, 6, 7 oder 10
    bool hardMode = false;
    bool autoRestart = false;

    // Runtime
    TTTT_State state = TTTT_SETUP;
    unsigned long runStartTime = 0;
    unsigned long finishedTime = 0;
    
    int currentPlayer = 0;
    int grid[9]; // Speichert den Besitzer der Grid-Pucks (Index 0-8 entspricht Pucks 1-9). -1 = Neutral
    int winIndices[3]; // Speichert die 3 Pucks, die zum Sieg geführt haben
    
    // Timer für die 500ms Blockzeit (Entprellung/Debounce)
    unsigned long lastInput[MAX_PEERS];
    
    // Farben für Spieler 1, 2, 3
    const CRGB PLAYER_COLORS[3] = { CRGB::Blue, CRGB::Red, CRGB::Green };

    void initGame();
    void startGame();
    void nextPlayer();
    void checkWin();
    void applyGridColor(int gridIdx);
    void updateSelectorPuck();
    void showLayout();
    
    void setPuck(int index, int effect, CRGB color, int speed=0, int bright=255);
};

#endif