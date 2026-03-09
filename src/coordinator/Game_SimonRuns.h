#ifndef GAME_SIMONRUNS_H
#define GAME_SIMONRUNS_H

#include "Game.h"
#include "Common.h"
#include <vector>
#include <FastLED.h> 

enum RunState {
    SIMRUN_SETUP,       
    SIMRUN_PREPARE,     
    SIMRUN_SHOW,        
    SIMRUN_INPUT,       
    SIMRUN_WAITING,     
    SIMRUN_FINISHED,    
    SIMRUN_DISQUALIFIED 
};

struct RunGroup {
    int id;
    bool active;
    RunState state;
    
    // Pucks
    int displayPuckIdx;             
    std::vector<int> inputPuckIndices; 
    std::vector<CRGB> inputColors;     
    
    // Spielstand
    std::vector<int> sequence;      
    int inputProgress;              
    bool inputLocked;               
    int currentLevel;               
    
    // Zeitmessung
    unsigned long stateStartTime;   
    unsigned long playStartTime;    
    unsigned long accumulatedTime;  
    unsigned long finishTime;       
    
    // Visuelles Feedback
    int playbackStep;               
    unsigned long lastStepTime;     
    bool lightOn;                   
};

class Game_SimonRuns : public Game {
public:
    void setup() override;
    void loop() override;
    void handleEvent(int puckIndex, EventPacket event) override;
    String getName() override { return "Simon Runs"; }
    
    String getStatusJSON() override;
    void processCommand(String cmd, int value) override;

private:
    int numGroups = 1;
    bool centralDisplay = false;
    int targetLength = 10;
    bool colorBlind = false;
    bool soundOn = true;
    
    RunGroup groups[3]; 
    
    void initGroup(int gIdx);
    void startNextLevel(int gIdx);
    void generateSequenceStep(int gIdx);
    void setPuck(int globalIdx, int effect, CRGB color, int speed, int bright = 255);
    CRGB getPaletteColor(int idx);
    void updateGroup(int gIdx);
    bool checkAllGroupsReady(); 
    
    void recalcAssignment();
    void showPreview();
    
    // --- NEU ---
    void updateCentralPuckLight();
};

#endif