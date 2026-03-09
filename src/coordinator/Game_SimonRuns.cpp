#include "Game_SimonRuns.h"
#include "PuckNetwork.h"

// --- KONSTANTEN ---
const CRGB COLORS_NORM[] = { CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Yellow, CRGB::Magenta, CRGB::Cyan, CRGB::Orange, CRGB::White };
const CRGB COLORS_BLIND[] = { CRGB::Blue, CRGB::Yellow, CRGB::White, CRGB::Orange, CRGB::Magenta, CRGB::Cyan, CRGB::Lime, CRGB::Pink };
const CRGB TEAM_COLORS[] = { CRGB::Red, CRGB::Blue, CRGB::Yellow }; 

// --- SETUP & INIT ---

void Game_SimonRuns::setup() {
    Serial.println("GAME: Setup Simon Runs");
    delay(500); 

    for(int i=0; i<3; i++) {
        groups[i].state = SIMRUN_SETUP;
        groups[i].accumulatedTime = 0;
        groups[i].playStartTime = 0;
        groups[i].finishTime = 0;
        groups[i].inputLocked = false;
        groups[i].sequence.clear();
        groups[i].inputColors.clear();
        groups[i].inputPuckIndices.clear();
    }

    CommandPacket cp;
    cp.cmd = CMD_EFFECT; cp.effectID = EFF_OFF; cp.extra=0;
    PuckNetwork::broadcast(cp);
}

CRGB Game_SimonRuns::getPaletteColor(int idx) {
    if(colorBlind) return COLORS_BLIND[idx % 8];
    return COLORS_NORM[idx % 8];
}

void Game_SimonRuns::recalcAssignment() {
    PuckInfo* pucks = PuckNetwork::getPucks();
    int activePucksCount = 0;
    for(int i=0; i<MAX_PEERS; i++) if(pucks[i].active) activePucksCount++;
    
    int currentPuck = 0;
    int displayPuckCentral = -1;

    if (centralDisplay && numGroups > 1) {
        if (pucks[0].active) displayPuckCentral = 0;
        currentPuck = 1; 
    }

    for(int i=0; i<3; i++) {
        groups[i].id = i;
        groups[i].active = (i < numGroups);
        groups[i].inputPuckIndices.clear();
        groups[i].inputColors.clear();
        groups[i].sequence.clear();
        groups[i].state = SIMRUN_SETUP;
        groups[i].accumulatedTime = 0;
        groups[i].playStartTime = 0;
        groups[i].finishTime = 0;
        groups[i].inputLocked = false;
        
        if (groups[i].active) {
            if (centralDisplay && numGroups > 1) {
                groups[i].displayPuckIdx = displayPuckCentral;
            } else {
                while(currentPuck < MAX_PEERS && !pucks[currentPuck].active) currentPuck++;
                if(currentPuck < MAX_PEERS) groups[i].displayPuckIdx = currentPuck++;
            }
            
            int availableInputPucks = activePucksCount - (centralDisplay ? 1 : numGroups);
            int pucksPerGroup = availableInputPucks / numGroups;
            if(pucksPerGroup < 1) pucksPerGroup = 1; 
            
            for(int p=0; p<pucksPerGroup; p++) {
                while(currentPuck < MAX_PEERS && !pucks[currentPuck].active) currentPuck++;
                if(currentPuck < MAX_PEERS) {
                    groups[i].inputPuckIndices.push_back(currentPuck);
                    groups[i].inputColors.push_back(getPaletteColor(p));
                    currentPuck++;
                }
            }
        }
    }
}

void Game_SimonRuns::showPreview() {
    recalcAssignment();
    
    if (centralDisplay && numGroups > 1 && groups[0].displayPuckIdx != -1) {
        setPuck(groups[0].displayPuckIdx, EFF_SINGLE_CHASE, CRGB::White, 100);
        delay(50);
    }

    for(int i=0; i<numGroups; i++) {
        if(!groups[i].active) continue;

        CRGB teamCol = TEAM_COLORS[i % 3];
        
        if (!centralDisplay || numGroups == 1) {
            setPuck(groups[i].displayPuckIdx, EFF_SINGLE_CHASE, teamCol, 100);
            delay(50);
        }
        
        for(int pid : groups[i].inputPuckIndices) {
            setPuck(pid, EFF_BREATHE_MOD4, teamCol, 40, 50); 
            delay(50); 
        }
    }
}

void Game_SimonRuns::updateCentralPuckLight() {
    if (!centralDisplay || numGroups <= 1) return;
    int dispIdx = groups[0].displayPuckIdx;
    if (dispIdx == -1) return;

    if (checkAllGroupsReady()) {
        setPuck(dispIdx, EFF_SINGLE_CHASE, CRGB::White, 100);
    } else {
        setPuck(dispIdx, EFF_BREATHE_MOD4, CRGB::White, 40, 150);
    }
}

void Game_SimonRuns::processCommand(String cmd, int value) {
    if (cmd == "setup") setup();
    else if (cmd == "cfg_groups") numGroups = value;
    else if (cmd == "cfg_len") targetLength = value;
    else if (cmd == "cfg_central") centralDisplay = (value == 1);
    else if (cmd == "cfg_blind") colorBlind = (value == 1);
    else if (cmd == "cfg_sound") soundOn = (value == 1);
    
    else if (cmd == "preview") showPreview(); 

    else if (cmd == "start_all") {
        for(int i=0; i<numGroups; i++) {
            if(groups[i].active && groups[i].state == SIMRUN_SETUP) { 
                groups[i].currentLevel = 0; 
                startNextLevel(i);
            }
        }
    }
    else if (cmd == "stop_all" || cmd == "exit") {
        CommandPacket cp; cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; 
        cp.r=0; cp.g=255; cp.b=0; cp.extra=10; 
        PuckNetwork::broadcast(cp);
        for(int i=0; i<3; i++) groups[i].state = SIMRUN_SETUP;
    }
    
    else if (cmd.startsWith("start_")) {
        int g = cmd.substring(6).toInt();
        if(g < numGroups && groups[g].active) {
            groups[g].currentLevel = 0;
            startNextLevel(g);
        }
    }
    else if (cmd.startsWith("disq_")) {
        int g = cmd.substring(5).toInt();
        if(g < numGroups) {
            groups[g].state = SIMRUN_DISQUALIFIED;
            for(int pid : groups[g].inputPuckIndices) setPuck(pid, EFF_OFF, CRGB::Black, 0);
            if(!centralDisplay) setPuck(groups[g].displayPuckIdx, EFF_OFF, CRGB::Black, 0);
            if(centralDisplay) updateCentralPuckLight();
        }
    }
}

void Game_SimonRuns::startNextLevel(int gIdx) {
    RunGroup* g = &groups[gIdx];
    
    g->currentLevel++;
    generateSequenceStep(gIdx); 
    
    g->state = SIMRUN_PREPARE;
    g->stateStartTime = millis();
    g->inputLocked = false;
    
    for(int pid : g->inputPuckIndices) {
        setPuck(pid, EFF_STATIC, CRGB::Black, 0, 10); 
        delay(20);
    }
    
    setPuck(g->displayPuckIdx, EFF_DOUBLE_CHASE, CRGB::White, 60);
}

void Game_SimonRuns::generateSequenceStep(int gIdx) {
    int numInputs = groups[gIdx].inputPuckIndices.size();
    if(numInputs > 0) {
        if(centralDisplay && gIdx > 0 && groups[0].sequence.size() > groups[gIdx].sequence.size()) {
             groups[gIdx].sequence.push_back(groups[0].sequence.back());
        } else {
             int rnd = random(numInputs);
             groups[gIdx].sequence.push_back(rnd);
        }
    }
}

void Game_SimonRuns::loop() {
    for(int i=0; i<numGroups; i++) {
        if(groups[i].active) updateGroup(i);
    }
}

void Game_SimonRuns::updateGroup(int gIdx) {
    RunGroup* g = &groups[gIdx];
    unsigned long now = millis();
    
    if (g->state == SIMRUN_DISQUALIFIED) return; 

    if (g->state == SIMRUN_PREPARE) {
        long elapsed = now - g->stateStartTime;
        static int lastSec[3] = {-1,-1,-1};
        int sec = elapsed / 1000;
        
        if (sec != lastSec[gIdx] && sec < 3) {
            if(soundOn) {
                CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 100;
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->displayPuckIdx].mac, snd);
            }
            lastSec[gIdx] = sec;
        }
        
        if (elapsed >= 3000) {
            g->state = SIMRUN_SHOW;
            g->playbackStep = 0;
            g->lastStepTime = now;
            g->lightOn = false;
            
            setPuck(g->displayPuckIdx, EFF_OFF, CRGB::Black, 0);
            delay(800); 
        }
    }
    else if (g->state == SIMRUN_SHOW) {
        int duration = 800; 
        
        if (now - g->lastStepTime > (g->lightOn ? duration : 300)) { 
            g->lastStepTime = now;
            
            if (!g->lightOn) {
                if (g->playbackStep < g->sequence.size()) {
                    int localIdx = g->sequence[g->playbackStep];
                    CRGB col = g->inputColors[localIdx];
                    
                    setPuck(g->displayPuckIdx, EFF_STATIC, col, 0, 255);
                    if(soundOn) {
                        CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 150;
                        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->displayPuckIdx].mac, snd);
                    }
                    g->lightOn = true;
                } else {
                    g->state = SIMRUN_INPUT;
                    g->playStartTime = millis(); 
                    g->inputProgress = 0;
                    g->inputLocked = false;
                    
                    if (centralDisplay && numGroups > 1) {
                         delay(500);
                         updateCentralPuckLight();
                    } else {
                         setPuck(g->displayPuckIdx, EFF_OFF, CRGB::Black, 0); 
                    }
                    
                    for(int i=0; i<g->inputPuckIndices.size(); i++) {
                        setPuck(g->inputPuckIndices[i], EFF_STATIC, g->inputColors[i], 0, 230); 
                        delay(30); 
                    }
                }
            } else {
                setPuck(g->displayPuckIdx, EFF_OFF, CRGB::Black, 0);
                g->lightOn = false;
                g->playbackStep++;
            }
        }
    }
}

void Game_SimonRuns::handleEvent(int puckIndex, EventPacket event) {
    if (event.type != EVT_BTN_CLICK) return;

    if (centralDisplay && numGroups > 1 && puckIndex == groups[0].displayPuckIdx) {
        bool anyMistake = false;
        for(int i=0; i<numGroups; i++) {
            if(groups[i].active && groups[i].state == SIMRUN_INPUT && groups[i].inputLocked) {
                groups[i].state = SIMRUN_SHOW;
                groups[i].playbackStep = 0;
                groups[i].lastStepTime = millis();
                groups[i].lightOn = false;
                for(int pid : groups[i].inputPuckIndices) setPuck(pid, EFF_STATIC, groups[i].inputColors[0], 0, 20); 
                anyMistake = true;
            }
        }
        
        if(anyMistake) return;

        if (checkAllGroupsReady()) {
            for(int i=0; i<numGroups; i++) {
                if(groups[i].active && groups[i].state == SIMRUN_WAITING) startNextLevel(i);
            }
            return;
        }

        for(int i=0; i<numGroups; i++) {
            if(groups[i].active && groups[i].state == SIMRUN_INPUT) {
                groups[i].state = SIMRUN_SHOW;
                groups[i].playbackStep = 0;
                groups[i].lastStepTime = millis();
                groups[i].lightOn = false;
                for(int pid : groups[i].inputPuckIndices) setPuck(pid, EFF_STATIC, groups[i].inputColors[0], 0, 20); 
            }
        }
        return;
    }

    int gIdx = -1;
    bool isDisplay = false;
    int inputLocalIdx = -1;
    
    for(int i=0; i<numGroups; i++) {
        if(groups[i].active) {
            if(groups[i].displayPuckIdx == puckIndex) {
                gIdx = i; isDisplay = true; break;
            }
            for(size_t k=0; k<groups[i].inputPuckIndices.size(); k++) {
                if(groups[i].inputPuckIndices[k] == puckIndex) {
                    gIdx = i; inputLocalIdx = k; break;
                }
            }
        }
        if(gIdx != -1) break;
    }
    
    if (gIdx == -1) return; 
    RunGroup* g = &groups[gIdx];

    // Wenn der Anzeige-Puck gedrückt wird
    if (isDisplay) {
        if (g->state == SIMRUN_INPUT && g->inputLocked) {
            g->state = SIMRUN_SHOW;
            g->playbackStep = 0;
            g->lastStepTime = millis();
            g->lightOn = false;
            for(int pid : g->inputPuckIndices) setPuck(pid, EFF_STATIC, g->inputColors[0], 0, 20); 
            return;
        }
        // Startet das nächste Level, wenn die Gruppe im Warten-Status ist
        if (g->state == SIMRUN_WAITING) startNextLevel(gIdx);
    }
    // Wenn ein Spiel-Puck gedrückt wird
    else if (!isDisplay && g->state == SIMRUN_INPUT && !g->inputLocked) {
        int expectedLocalIdx = g->sequence[g->inputProgress];
        
        if (inputLocalIdx == expectedLocalIdx) {
            g->inputProgress++;
            
            setPuck(puckIndex, EFF_STATIC, g->inputColors[inputLocalIdx], 0, 255);
            if(soundOn) {
                CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 100;
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
            }
            delay(100);
            setPuck(puckIndex, EFF_STATIC, g->inputColors[inputLocalIdx], 0, 230); 

            // Wurde die Sequenz erfolgreich beendet?
            if (g->inputProgress >= g->sequence.size()) {
                g->accumulatedTime += (millis() - g->playStartTime);
                
                for(int pid : g->inputPuckIndices) {
                    setPuck(pid, EFF_SINGLE_CHASE, CRGB::Green, 60);
                    delay(30); 
                }
                
                CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 600; 
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
                
                if (g->sequence.size() >= targetLength) {
                    g->state = SIMRUN_FINISHED;
                    g->finishTime = millis();
                    for(int pid : g->inputPuckIndices) {
                         setPuck(pid, EFF_RAINBOW, CRGB::Black, 0, 255);
                         delay(30);
                    }
                    setPuck(g->displayPuckIdx, EFF_RAINBOW, CRGB::Black, 0, 255);
                    
                } else {
                    // NEU: Immer in den WAITING Status wechseln, egal ob 1 Gruppe oder mehrere!
                    g->state = SIMRUN_WAITING;
                    for(int pid : g->inputPuckIndices) {
                        setPuck(pid, EFF_STATIC, CRGB::Green, 0, 20); 
                        delay(20);
                    }
                    
                    if (centralDisplay && numGroups > 1) {
                        updateCentralPuckLight();
                    } else {
                        // Der gruppeneigene Anzeigepuck atmet weiß als Aufforderung ihn zu drücken
                        setPuck(g->displayPuckIdx, EFF_BREATHE_MOD4, CRGB::White, 40, 150);
                    }
                }
            }
            
        } else {
            g->inputLocked = true; 
            g->inputProgress = 0;  
            
            setPuck(puckIndex, EFF_FAIL, CRGB::Red, 0, 255);
            if(soundOn) {
                CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 50;
                uint8_t* mac = PuckNetwork::getPucks()[puckIndex].mac;
                PuckNetwork::sendToPuck(mac, snd); delay(80);
                PuckNetwork::sendToPuck(mac, snd); delay(80);
                PuckNetwork::sendToPuck(mac, snd);
            }
            
             for(size_t k=0; k<g->inputPuckIndices.size(); k++) {
                 if(g->inputPuckIndices[k] != puckIndex) {
                     setPuck(g->inputPuckIndices[k], EFF_STATIC, g->inputColors[k], 0, 25); 
                     delay(20);
                 }
             }
             
             if(centralDisplay) updateCentralPuckLight();
        }
    }
}

bool Game_SimonRuns::checkAllGroupsReady() {
    for(int i=0; i<numGroups; i++) {
        if (groups[i].active) {
            if (groups[i].state == SIMRUN_DISQUALIFIED) continue;
            if (groups[i].state == SIMRUN_FINISHED) continue;
            if (groups[i].state != SIMRUN_WAITING) return false;
        }
    }
    return true;
}

void Game_SimonRuns::setPuck(int globalIdx, int effect, CRGB color, int speed, int bright) {
    if(globalIdx < 0 || globalIdx >= MAX_PEERS) return;
    CommandPacket cp;
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[globalIdx].mac, cp);
}

String Game_SimonRuns::getStatusJSON() {
    String json = "{";
    json += "\"groups\":[";
    for(int i=0; i<numGroups; i++) {
        if(i>0) json += ",";
        json += "{";
        json += "\"id\":" + String(i) + ",";
        json += "\"state\":" + String(groups[i].state) + ",";
        json += "\"locked\":" + String(groups[i].inputLocked ? 1 : 0) + ","; 
        json += "\"lvl\":" + String(groups[i].currentLevel) + ",";
        json += "\"target\":" + String(targetLength) + ",";
        
        unsigned long t = groups[i].accumulatedTime;
        if(groups[i].state == SIMRUN_INPUT || groups[i].state == SIMRUN_SHOW) {
            t += (millis() - groups[i].playStartTime);
        }
        json += "\"time\":" + String(t) + ",";
        
        json += "\"prog\":" + String(groups[i].inputProgress) + ",";
        json += "\"seq\":[";
        for(size_t s=0; s<groups[i].sequence.size(); s++) {
             if(s>0) json += ",";
             CRGB c = groups[i].inputColors[groups[i].sequence[s]];
             char hex[8]; sprintf(hex, "#%02X%02X%02X", c.r, c.g, c.b);
             json += "\"" + String(hex) + "\"";
        }
        json += "]";
        
        json += "}";
    }
    json += "]}";
    return json;
}