#ifndef GAME_SIMONSAYS_H
#define GAME_SIMONSAYS_H

#include "Game.h"
#include <FastLED.h> 
#include <vector>

#define MAX_GROUPS 3
#define MAX_SEQ_LEN 50

enum SimonState {
    SIM_SETUP = 0,
    SIM_READY = 1,       // Gruppenfarbe Atmen
    SIM_COUNTDOWN = 2,   // 3..2..1
    SIM_SHOW = 3,        // Computer zeigt Muster
    SIM_INPUT = 4,       // Spieler drückt
    SIM_FEEDBACK = 5,    // Kurz warten (Richtig/Falsch Animation)
    SIM_FINISHED = 6,    // Gewonnen (Regenbogen)
    SIM_FAIL = 7         // <--- DAS HIER FEHLTE!
};

struct SimonGroup {
    bool active;
    SimonState state;
    int numPucks;
    std::vector<int> puckIndices; // Globale Puck IDs
    std::vector<CRGB> puckColors; // Farben der Pucks
    
    // Spiel Logik
    std::vector<uint8_t> sequence; // Die Farbfolge (0 bis numPucks-1)
    int currentSeqLength;          // Wie viele Schritte zeigen wir aktuell?
    int inputIndex;                // Wo ist der Spieler gerade?
    
    // Timing
    unsigned long stateStartTime;
    unsigned long lastStepTime;
    int playbackStep;              // Welchen Schritt zeigt der Computer gerade?
    bool lightOn;                  // Für das Blinken beim Zeigen
    
    // Debounce (gleicher Puck 400ms, verschiedene Pucks sofort)
    unsigned long lastInputTime;
    int lastInputPuck = -1;
    
    // Stats
    int stars;
    unsigned long playTimeStart;
    unsigned long totalPlayTime; // Gespeicherte Zeit
};

class Game_SimonSays : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Simon Says"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
// Config mit Standardwerten initialisieren!
    int activeGroups = 1;
    int pucksPerGroup[MAX_GROUPS] = {4, 4, 4}; // Standard: 4 Pucks pro Gruppe
    int targetSeqLength = 15;
    int speedSetting = 1; // 0=Slow, 1=Med, 2=Fast
    bool colorBlind = false;
    bool autoRestart = true;
    bool soundOn = true;

    // Runtime
    SimonGroup groups[MAX_GROUPS];

    // Farbpaletten
    const CRGB COLORS_NORMAL[10] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Yellow, CRGB::Cyan, CRGB::Magenta, CRGB::Orange, CRGB::Purple, CRGB::White, CRGB::Lime};
    const CRGB COLORS_BLIND[10]  = {CRGB::Blue, CRGB::Yellow, CRGB::White, CRGB::Purple, CRGB::Cyan, CRGB::Orange, CRGB::Magenta, CRGB::Pink, CRGB::Aquamarine, CRGB::Grey};

    // Helper
    void initGroup(int groupIdx);
    void startGroup(int groupIdx);
    void stopGroup(int groupIdx);
    void resetGroupLogik(int groupIdx);
    
    void updateGroup(int groupIdx); // Die State Machine pro Gruppe
    void setPuck(int puckIdx, int effect, CRGB color, int speed=0, int bright=100);
    int getGroupIndex(int puckIdx);
    int getInternalIndex(int groupIdx, int puckIdx);
    
    int getStepDuration(); // Basierend auf SpeedSetting
    void generateNewColor(int groupIdx);
};

#endif