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

    // Verbleibende Nag-Delay Sekunden (0 = kein Delay aktiv)
    static int getNagDelayRemaining();

private:
    static Game* currentGame;
    static void _doStartGame(int gameID);

    // Nag-Delay bei abgelaufener Testphase
    static int _pendingGameID;
    static unsigned long _nagStartMs;
    static const unsigned long NAG_DELAY_MS = 30000;
};
#endif