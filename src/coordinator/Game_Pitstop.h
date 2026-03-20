#ifndef GAME_PITSTOP_H
#define GAME_PITSTOP_H

#include "Game.h"
#include <FastLED.h>

enum PS_GameState {
    PS_SETUP    = 0,
    PS_RUNNING  = 1,
    PS_PAUSED   = 2,
    PS_FINISHED = 3
};

struct PitstopSlot {
    int entryPuckIdx = -1; 
    int exitPuckIdx  = -1; 

    bool isActive    = true; 
    int  stopCount   = 0;    
    int  penaltyMs   = 0;    

    // --- Status Entry Puck ---
    bool entryRunning         = false; 
    unsigned long entryStart  = 0;     
    unsigned long entryDuration = 0;   
    unsigned long entryDoneAt = 0;     

    // --- Status Exit Puck ---
    bool exitRunning          = false; 
    unsigned long exitStart   = 0;     
    unsigned long exitDoneAt  = 0;     
    bool exitWaitingForButton = false; 
    
    // --- Stau-Logik (Queue) ---
    bool exitPending          = false; 
    unsigned long exitPendingStart = 0; // Merkt sich, wann die Fahrt im Hintergrund begonnen hat
};

class Game_Pitstop : public Game {
public:
    void   setup()    override;
    void   loop()     override;
    void   handleEvent(int puckIndex, EventPacket event) override;
    String getName()  override { return "Pitstop"; }

    String getStatusJSON() override;
    void   processCommand(String cmd, int value) override;

private:
    static const int MAX_PS_PLAYERS = 10;

    int           numPlayers        = 1;
    unsigned long pitstopMs         = 5000;
    bool          pitlaneMode       = false;
    bool          showCountdown     = true;
    bool          alwaysYellow      = false;
    bool          statusPuckEnabled = false;
    int           statusPuckIdx     = -1;
    unsigned long pitlaneExitMs     = 5000;
    bool          exitButtonMode    = false;

    PS_GameState  gameState       = PS_SETUP;
    unsigned long pauseStartMs    = 0;
    PitstopSlot   slots[MAX_PS_PLAYERS];
    int           totalStops      = 0;
    int           lastStatusState = 0;

    bool          blinkStateOn    = false;
    unsigned long lastBlinkToggle = 0;

    const CRGB PLAYER_COLORS[10] = {
        CRGB::Blue, CRGB(136,0,255), CRGB(0,136,255), CRGB::Yellow,
        CRGB::Magenta, CRGB::Cyan, CRGB(255,136,0), CRGB::White,
        CRGB(255,105,180), CRGB(0,255,170)
    };

    void initGame();
    void triggerEntry(int slotIdx);
    void completeEntry(int slotIdx);
    void completeExit(int slotIdx);
    void handleBlinking();
    void setSlotIdleVisual(int slotIdx, int puckIdx); 
    void restoreSlotVisuals(int slotIdx);
    void updateStatusPuck();
    void startExitCountdown(int slotIdx, unsigned long elapsed = 0); // Nimmt nun die bereits vergangene Zeit entgegen

    void setPuck(int globalIdx, int effect, CRGB color, int speed = 0, int bright = 200);
    void sendSound(int globalIdx, int duration);
    void sendSequence(int globalIdx, int seqID);
};

#endif