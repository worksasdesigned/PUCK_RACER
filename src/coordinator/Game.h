#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include "Common.h"

class Game {
public:
    virtual ~Game() {}
    
    // Wird aufgerufen, wenn das Spiel startet
    virtual void setup() = 0;
    
    // Die Hauptschleife des Spiels
    virtual void loop() = 0;
    
    // Wenn ein Puck gedrückt wird
    virtual void handleEvent(int puckIndex, EventPacket event) = 0;
    
    // Name des Spiels
    virtual String getName() = 0;

    // --- NEU FÜR WEB API ---
    // Gibt den aktuellen Spielstand als JSON zurück
    virtual String getStatusJSON() { return "{}"; }
    
    // Verarbeitet Befehle von der Webseite (Start, Stop, Config)
    virtual void processCommand(String cmd, int value) {}
};

#endif