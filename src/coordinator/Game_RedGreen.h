#ifndef GAME_REDGREEN_H
#define GAME_REDGREEN_H

#include "Game.h"
#include <vector>
#include <FastLED.h>

enum RG_GameState {
    RG_SETUP = 0,
    RG_PREPARE = 1,   // Start Countdown
    RG_RUNNING = 2,
    RG_FINISHED = 3
};

enum RG_TrafficLight {
    TL_RED = 0,
    TL_GREEN = 1,
    TL_YELLOW = 2
};

struct RG_Group {
    int id;
    std::vector<int> puckIndices;
    CRGB color;
    
    RG_TrafficLight lightState;
    unsigned long nextSwitchTime;
    int score;
    
    // Non-blocking Sound Trigger
    bool pendingSound;
    int pendingSoundID; // ID des Sounds
    unsigned long soundTriggerTime;
    unsigned long lastStateChange; // Gnadenfrist
};

class Game_RedGreen : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Red Light Green Light"; }
    
    String getStatusJSON() override; 
    void processCommand(String cmd, int value) override;

private:
    // Config
    int numGroups = 1;
    int speedSetting = 1; // 0=Slow, 1=Med, 2=Fast
    int timeLimit = 60;
    
    // Runtime
    RG_GameState state = RG_SETUP;
    unsigned long gameStartTime = 0;
    unsigned long finishTime = 0;
    unsigned long stateStartTime = 0; // Für Countdown

    RG_Group groups[10]; // Max 10 Gruppen
    
    const CRGB GROUP_COLORS[10] = {
        CRGB::Blue, CRGB::Magenta, CRGB::Cyan, CRGB::Orange, 
        CRGB::Purple, CRGB::Pink, CRGB::Turquoise, CRGB::White, 
        CRGB::Lime, CRGB::Yellow // Gelb/Rot/Grün vermeiden wir als Gruppenfarbe wegen Ampel, aber als Fallback ok
    };

    void initGame();
    void startGame();
    void stopGame();
    
    void updateGroup(int gIdx);
    void setGroupState(int gIdx, RG_TrafficLight newState);
    long getRandomDuration(RG_TrafficLight state);
    
    void setPuck(int index, int effect, CRGB color, int speed=0, int bright=100);
    int getGroupIndex(int puckIndex);
};

#endif