#ifndef GAMEMANAGER_H
#define GAMEMANAGER_H

#include <Arduino.h>
#include "Common.h"
#include "Game.h"

class GameManager {
public:
    static void begin();
    static void update();
    
    // Wird vom NetworkManager aufgerufen
    static void handlePuckEvent(const uint8_t* mac, EventPacket event);

    // Spiel wechseln
    static void startGame(int gameID);
    
    // Zugriff auf das aktive Spiel (für WebHandler)
    static Game* getCurrentGame();

private:
    static Game* currentGame;
};
#endif