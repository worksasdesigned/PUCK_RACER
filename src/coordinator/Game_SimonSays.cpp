#include "Game_SimonSays.h"
#include "PuckNetwork.h"

void Game_SimonSays::setup() {
    Serial.println("GAME: Setup Simon Says");
    
    PuckInfo* pucks = PuckNetwork::getPucks();
    int assignedPucks = 0;

    for(int i=0; i<MAX_GROUPS; i++) {
        groups[i].active = (i < activeGroups);
        groups[i].puckIndices.clear();
        groups[i].puckColors.clear();
        groups[i].sequence.clear();
        groups[i].stars = 0; 
        groups[i].state = SIM_SETUP;
        groups[i].totalPlayTime = 0;
        
        if (groups[i].active) {
            groups[i].numPucks = pucksPerGroup[i];
            
            for(int p=0; p<groups[i].numPucks; p++) {
                while(assignedPucks < MAX_PEERS && !pucks[assignedPucks].active) {
                    assignedPucks++;
                }
                if(assignedPucks < MAX_PEERS) {
                    groups[i].puckIndices.push_back(assignedPucks);
                    
                    CRGB c = colorBlind ? COLORS_BLIND[p % 10] : COLORS_NORMAL[p % 10];
                    groups[i].puckColors.push_back(c);
                    
                    assignedPucks++;
                }
            }
            initGroup(i);
        }
    }
    
    // Übrige Pucks auf Grün
    for(int i=assignedPucks; i<MAX_PEERS; i++) {
        if(pucks[i].active) {
            setPuck(i, EFF_STATUS, CRGB::Green, 0, 50);
            delay(20);
        }
    }
}

void Game_SimonSays::initGroup(int gIdx) {
    groups[gIdx].state = SIM_READY;
    if(groups[gIdx].puckIndices.empty()) return;

    // Feste Teamfarben anstatt der ersten Farbe der Spielpalette
    CRGB teamColors[] = { CRGB::Red, CRGB::Blue, CRGB::Yellow };
    CRGB groupColor = teamColors[gIdx % 3];
    
    for(int pid : groups[gIdx].puckIndices) {
        setPuck(pid, EFF_BREATHE_MOD4, groupColor, 50, 50);
        delay(40); 
    }
}

void Game_SimonSays::processCommand(String cmd, int value) {
    if (cmd == "setup") setup();
    else if (cmd == "cfg_groups") activeGroups = value;
    else if (cmd == "cfg_pucks_0") pucksPerGroup[0] = value;
    else if (cmd == "cfg_pucks_1") pucksPerGroup[1] = value;
    else if (cmd == "cfg_pucks_2") pucksPerGroup[2] = value;
    else if (cmd == "cfg_len") targetSeqLength = value;
    else if (cmd == "cfg_speed") speedSetting = value;
    else if (cmd == "cfg_blind") colorBlind = (value == 1);
    else if (cmd == "cfg_auto") autoRestart = (value == 1);
    else if (cmd == "cfg_sound") soundOn = (value == 1);
    
    else if (cmd == "start_all") {
        for(int i=0; i<activeGroups; i++) { startGroup(i); delay(50); }
    }
    else if (cmd == "stop_all") {
        for(int i=0; i<activeGroups; i++) { stopGroup(i); delay(50); }
    }
    else if (cmd == "exit") {
        CommandPacket light;
        light.cmd = CMD_EFFECT; light.effectID = EFF_STATUS; 
        light.r = 0; light.g = 255; light.b = 0; 
        light.extra = 255; light.duration = 0; 
        PuckNetwork::broadcast(light);
    }
    
    else if (cmd.startsWith("start_")) {
        int g = cmd.substring(6).toInt();
        startGroup(g);
    }
    else if (cmd.startsWith("stop_")) {
        int g = cmd.substring(5).toInt();
        stopGroup(g);
    }
}

void Game_SimonSays::startGroup(int g) {
    if (g >= activeGroups) return;
    
    groups[g].sequence.clear();
    groups[g].totalPlayTime = 0;
    groups[g].playTimeStart = millis();
    
    groups[g].state = SIM_COUNTDOWN;
    groups[g].stateStartTime = millis();
    
    for(int pid : groups[g].puckIndices) {
        setPuck(pid, EFF_DOUBLE_CHASE, CRGB::White, 40, 150);
        delay(50); 
    }
}

void Game_SimonSays::stopGroup(int g) {
    if (g >= activeGroups) return;
    initGroup(g); 
}

void Game_SimonSays::resetGroupLogik(int g) {
    groups[g].state = SIM_SHOW;
    groups[g].playbackStep = 0;
    groups[g].lastStepTime = millis();
    groups[g].lightOn = false;
    groups[g].inputIndex = 0;
    
    // Alle Pucks dimmen
    for(int i=0; i<groups[g].numPucks; i++) {
        setPuck(groups[g].puckIndices[i], EFF_STATIC, groups[g].puckColors[i], 0, 25); 
        delay(30); 
    }
    // Längere Pause vor Start
    delay(1200); 
}

void Game_SimonSays::loop() {
    for(int i=0; i<activeGroups; i++) {
        if (groups[i].active) updateGroup(i);
    }
}

void Game_SimonSays::updateGroup(int g) {
    SimonGroup* grp = &groups[g];
    unsigned long now = millis();
    
    // --- COUNTDOWN ---
    if (grp->state == SIM_COUNTDOWN) {
        long elapsed = now - grp->stateStartTime;
        static int lastSec = -1;
        int sec = elapsed / 1000;
        
        if (sec != lastSec && sec < 3) {
            if(soundOn) {
                CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 100;
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[grp->puckIndices[0]].mac, snd);
            }
            lastSec = sec;
        }
        
        if (elapsed >= 3000) {
            if(soundOn) {
                CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 600;
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[grp->puckIndices[0]].mac, snd);
            }
            generateNewColor(g);
            resetGroupLogik(g);
        }
    }
    
    // --- COMPUTER ZEIGT MUSTER ---
    else if (grp->state == SIM_SHOW) {
        int duration = getStepDuration();
        
        if (now - grp->lastStepTime > (grp->lightOn ? duration : 250)) {
            grp->lastStepTime = now;
            
            if (!grp->lightOn) {
                // ANZEIGEN
                if (grp->playbackStep < grp->currentSeqLength) {
                    int puckLocalIdx = grp->sequence[grp->playbackStep];
                    int puckGlobalIdx = grp->puckIndices[puckLocalIdx];
                    
                    setPuck(puckGlobalIdx, EFF_STATIC, grp->puckColors[puckLocalIdx], 0, 255);
                    if(soundOn) {
                        CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 150;
                        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckGlobalIdx].mac, snd);
                    }
                    grp->lightOn = true;
                } else {
                    grp->state = SIM_INPUT;
                    grp->inputIndex = 0;
                }
            } else {
                // DIMMEN (mit Pause)
                int puckLocalIdx = grp->sequence[grp->playbackStep];
                int puckGlobalIdx = grp->puckIndices[puckLocalIdx];
                
                setPuck(puckGlobalIdx, EFF_STATIC, grp->puckColors[puckLocalIdx], 0, 25);
                delay(20); 
                
                grp->lightOn = false;
                grp->playbackStep++;
            }
        }
    }
    
    // --- FEEDBACK PAUSE ---
    else if (grp->state == SIM_FEEDBACK) {
        if (now - grp->stateStartTime > 1500) { 
            generateNewColor(g);
            resetGroupLogik(g);
        }
    }
    
// --- FAIL STATE ---
    else if (grp->state == SIM_FAIL) {
        // Pucks resetten nach 5 Sekunden optisch, aber Status bleibt FAIL für WebUI
        if (now - grp->stateStartTime > 5000 && now - grp->stateStartTime < 5500) {
             CRGB teamColors[] = { CRGB::Red, CRGB::Blue, CRGB::Yellow };
             CRGB groupColor = teamColors[g % 3];
             for(int pid : grp->puckIndices) {
                setPuck(pid, EFF_BREATHE_MOD4, groupColor, 50, 50);
                delay(20);
             }
        }
    }
}

void Game_SimonSays::handleEvent(int puckIndex, EventPacket event) {
    int g = getGroupIndex(puckIndex);
    if (g == -1) return;
    
    SimonGroup* grp = &groups[g];
    int localIdx = getInternalIndex(g, puckIndex);
    
    Serial.printf("[%lu] EVT Puck %d Type %d\n", millis(), puckIndex, event.type);

    if (event.type == EVT_BTN_CLICK) { // PRESS
        
        // RESTART LOGIK
        if (grp->state == SIM_READY || grp->state == SIM_FINISHED || grp->state == SIM_FAIL) {
             if (grp->state == SIM_FAIL && !autoRestart) {
                 startGroup(g);
             } else if (autoRestart || grp->state != SIM_FAIL) {
                 startGroup(g);
             }
             return;
        }

        if (grp->state == SIM_INPUT) {
            
            // Feedback
            setPuck(puckIndex, EFF_STATIC, grp->puckColors[localIdx], 0, 255);
            if(soundOn) {
                CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 100;
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
            }

            if (localIdx == grp->sequence[grp->inputIndex]) {
                // RICHTIG
                grp->inputIndex++;
                
                if (grp->inputIndex >= grp->currentSeqLength) {
                    // RUNDE GESCHAFFT
                    if (grp->currentSeqLength >= targetSeqLength) {
                        // --- GEWONNEN ---
                        grp->stars++;
                        grp->state = SIM_FINISHED;
                        
                        // Rundruf Rainbow 1
                        for(int pid : grp->puckIndices) {
                            setPuck(pid, EFF_RAINBOW, CRGB::Black, 0, 200);
                            delay(40);
                        }
                        delay(100); 
                        // Rundruf Rainbow 2
                        for(int pid : grp->puckIndices) {
                            setPuck(pid, EFF_RAINBOW, CRGB::Black, 0, 200);
                            delay(40);
                        }

                    } else {
                        // --- NÄCHSTE RUNDE ---
                        grp->state = SIM_FEEDBACK;
                        grp->stateStartTime = millis();
                        if(soundOn) {
                            CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 400;
                            PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
                        }
                    }
                }
            } else {
                // FALSCH
                grp->state = SIM_FAIL;
                grp->stateStartTime = millis();
                
                // Nur der falsche Puck
                setPuck(puckIndex, EFF_FAIL, CRGB::Red, 0, 255);
                
                if(soundOn) {
                    CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 50;
                    uint8_t* mac = PuckNetwork::getPucks()[puckIndex].mac;
                    PuckNetwork::sendToPuck(mac, snd); delay(80);
                    PuckNetwork::sendToPuck(mac, snd); delay(80);
                    PuckNetwork::sendToPuck(mac, snd);
                }
            }
        }
    }
    else if (event.type == EVT_BTN_RELEASE) {
        if (grp->state == SIM_INPUT) {
            setPuck(puckIndex, EFF_STATIC, grp->puckColors[localIdx], 0, 25);
        }
    }
}

void Game_SimonSays::generateNewColor(int g) {
    int rnd = random(groups[g].numPucks);
    groups[g].sequence.push_back(rnd);
    groups[g].currentSeqLength = groups[g].sequence.size();
}

int Game_SimonSays::getStepDuration() {
    switch(speedSetting) {
        case 0: return 1200; 
        case 1: return 800;  
        case 2: return 500;  
        default: return 800;
    }
}

int Game_SimonSays::getGroupIndex(int puckIdx) {
    for(int i=0; i<activeGroups; i++) {
        for(int pid : groups[i].puckIndices) {
            if(pid == puckIdx) return i;
        }
    }
    return -1;
}

int Game_SimonSays::getInternalIndex(int g, int puckIdx) {
    for(size_t i=0; i<groups[g].puckIndices.size(); i++) {
        if(groups[g].puckIndices[i] == puckIdx) return i;
    }
    return 0;
}

void Game_SimonSays::setPuck(int puckIdx, int effect, CRGB color, int speed, int bright) {
    Serial.printf("[%lu] SEND -> Puck %d: Eff %d\n", millis(), puckIdx, effect);
    CommandPacket cp;
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIdx].mac, cp);
}

String Game_SimonSays::getStatusJSON() {
    String json = "{";
    json += "\"groups\":[";
    for(int i=0; i<activeGroups; i++) {
        if(i>0) json += ",";
        json += "{";
        json += "\"state\":" + String(groups[i].state) + ",";
        json += "\"len\":" + String(groups[i].currentSeqLength) + ",";
        json += "\"step\":" + String(groups[i].playbackStep) + ","; 
        json += "\"stars\":" + String(groups[i].stars) + ",";
        json += "\"seq\":[";
        for(size_t s=0; s<groups[i].sequence.size(); s++) {
            if(s>0) json += ",";
            CRGB c = groups[i].puckColors[groups[i].sequence[s]];
            char hex[8]; sprintf(hex, "#%02X%02X%02X", c.r, c.g, c.b);
            json += "\"" + String(hex) + "\"";
        }
        json += "]}";
    }
    json += "]}";
    return json;
}