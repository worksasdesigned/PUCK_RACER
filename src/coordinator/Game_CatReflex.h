#ifndef GAME_CATREFLEX_H
#define GAME_CATREFLEX_H

#include "Game.h"
#include <FastLED.h>

// Zustandsmaschine fuer den Spielablauf (State Machine)
enum CatState {
    CR_SETUP = 0,           // Warte auf Spielstart
    CR_START_COUNTDOWN = 1, // Initiale 3 Sekunden (SEQ_SKI Sound)
    CR_WAIT_GREEN = 2,      // Zufaellige Wartezeit (ggf. mit Farbchaos)
    CR_GREEN_ACTIVE = 3,    // Anzeigepuck ist gruen, Spieler muessen druecken
    CR_ROUND_RESULT = 4,    // Kurze Pause nach der Runde zur Auswertung (3 Sekunden)
    CR_FINISHED = 5         // Spielzeit abgelaufen
};

class Game_CatReflex : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Cat Reflex"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    // --- KONFIGURATION (vom Webinterface) ---
    int playerCount = 1;         // Anzahl der aktiven Spielerpucks
    bool highlanderMode = true;  // Nur der Schnellste bekommt Punkte
    int gameDurationSec = 60;    // Gesamtspielzeit in Sekunden
    int difficultyMs = 1000;     // Toleranzfenster in ms (nur wenn Highlander = false)
    bool colorChaos = false;     // Farbchaos waehrend der Wartezeit

    // --- LAUFZEIT-VARIABLEN ---
    CatState state = CR_SETUP;
    unsigned long stateStartTime = 0; 
    unsigned long gameStartTime = 0;
    unsigned long finishTime = 0;
    
    // Runden-Logik
    unsigned long greenDelayMs = 0;      // Zufaellige Wartezeit bis Gruen (1000 - 3000ms)
    unsigned long lastChaosTime = 0;     // Timer fuer den Farbwechsel im Chaos-Modus
    unsigned long greenSignalTime = 0;   // Exakter Zeitpunkt, wann Gruen aufleuchtete
    
    // Spieler-Statistiken & Tracking
    int points[MAX_PEERS];               // Aktueller Punktestand pro Spieler
    unsigned long reactTimes[MAX_PEERS]; // Reaktionszeit in der aktuellen Runde
    bool roundEarly[MAX_PEERS];          // Hat der Spieler in dieser Runde zu frueh gedrueckt?
    bool roundPressed[MAX_PEERS];        // Hat der Spieler in dieser Runde ueberhaupt gedrueckt?
    bool isRoundWinner[MAX_PEERS];       // Flag fuer das Frontend (wird gruen hervorgehoben)

    // Farbauswahl fuer die Spielerpucks
    const CRGB PLAYER_COLORS[12] = {
        CRGB::Blue, CRGB::Green, CRGB::Red, CRGB::Yellow, 
        CRGB::Magenta, CRGB::Cyan, CRGB::Orange, CRGB::Purple,
        CRGB::White, CRGB::Pink, CRGB::Teal, CRGB::Gold
    };

    // Helferfunktionen
    void setPuck(int puckIndex, int effect, CRGB color, int speed=0, int bright=100);
    void startSequence(bool forceRestart = false);
    void evaluateRound();
    void resetGame();
    void stopGame();
    void exitGame();
    CRGB getRandomChaosColor();
};

#endif