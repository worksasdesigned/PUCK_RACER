#include "Game_Zombie.h"
#include "PuckNetwork.h"

void Game_Zombie::setup() {
    Serial.println("GAME: Setup Zombie Escape");
    // Minimal reset without puck assignments or effects
    // (puck assignment happens via processCommand("setup") from names.html)
    globalBestTime = 0;
    globalBestGroup = -1;
    globalCountdownStart = 0;
    for(int i = 0; i < 10; i++) {
        groups[i].state = Z_SETUP;
        groups[i].startPuckIdx = -1;
        groups[i].targetPuckIdx = -1;
    }
    // All pucks show status — no game-specific lighting on setup page
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
    PuckNetwork::broadcast(cp);
}

void Game_Zombie::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "cfg_groups") numGroups = constrain(value, 1, 10);
    else if (cmd == "cfg_time") {
        if (value < 3) value = 3; 
        startTargetTime = value * 1000UL;
    }
    else if (cmd == "cfg_reduce") timeReduction = value * 100UL;
    else if (cmd == "cfg_auto") autoReduce = (value == 1);
    else if (cmd == "cfg_sudden") suddenDeath = (value == 1);
    else if (cmd == "cfg_sync") groupStart = (value == 1);
    else if (cmd == "cfg_shuttle") shuttleMode = (value == 1);
    
    else if (cmd == "start") {
        globalCountdownStart = 0;
        // Feedback-Beep auf allen Pucks
        CommandPacket beep; memset(&beep, 0, sizeof(beep));
        beep.cmd = CMD_SOUND; beep.duration = 150;
        PuckNetwork::broadcast(beep);
        
        for(int i=0; i<numGroups; i++) {
            if (groups[i].startPuckIdx != -1 && groups[i].targetPuckIdx != -1) {
                groups[i].currentTargetTime = startTargetTime;
                groups[i].roundsPlayed = 0;
                groups[i].shuttleLaps = 0;
                groups[i].expectStart = false;
                setGroupState(i, Z_WAIT_HANDS);
            }
        }
    }
    else if (cmd == "reset") initGame(); 
    else if (cmd == "stop") {
        for(int i=0; i<numGroups; i++) setGroupState(i, Z_SETUP);
    }
    else if (cmd == "exit") {
        CommandPacket cp; memset(&cp, 0, sizeof(cp));
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
        PuckNetwork::broadcast(cp);
    }
    else if (cmd == "man_reduce") {
        for(int i=0; i<numGroups; i++) {
            if (groups[i].state != Z_ELIMINATED && groups[i].currentTargetTime > timeReduction) {
                groups[i].currentTargetTime -= timeReduction;
            }
        }
    }
    // FIX: Sicherere Parameter-Übergabe für die Buttons
    else if (cmd == "man_elim") {
        if (value >= 0 && value < numGroups) setGroupState(value, Z_ELIMINATED);
    }
    else if (cmd == "man_revive") {
        if (value >= 0 && value < numGroups) setGroupState(value, Z_WAIT_HANDS);
    }
}

void Game_Zombie::initGame() {
    globalBestTime = 0;
    globalBestGroup = -1;
    globalCountdownStart = 0;
    
    PuckInfo* pucks = PuckNetwork::getPucks();
    int assigned = 0;

    for(int i=0; i<10; i++) {
        groups[i].id = i;
        groups[i].state = Z_SETUP;
        groups[i].color = GROUP_COLORS[i % 10];
        groups[i].currentTargetTime = startTargetTime;
        groups[i].roundsPlayed = 0;
        groups[i].lastRunTime = 0;
        groups[i].isHolding = false;
        groups[i].isWinner = false;
        groups[i].shuttleLaps = 0;
        groups[i].expectStart = false;
        
        if (i < numGroups) {
            while(assigned < MAX_PEERS && !pucks[assigned].active) assigned++;
            groups[i].startPuckIdx = (assigned < MAX_PEERS) ? assigned++ : -1;
            
            while(assigned < MAX_PEERS && !pucks[assigned].active) assigned++;
            groups[i].targetPuckIdx = (assigned < MAX_PEERS) ? assigned++ : -1;
            
            if (groups[i].startPuckIdx != -1 && groups[i].targetPuckIdx != -1) {
                setGroupState(i, Z_SETUP);
            } else {
                setGroupState(i, Z_ELIMINATED); 
            }
        }
    }
}

void Game_Zombie::setGroupState(int gIdx, ZombieState newState) {
    ZombieGroup* g = &groups[gIdx];
    g->state = newState;
    g->stateStartTime = millis();
    
    if (newState == Z_SETUP) {
        g->isHolding = false;
        // FIX: Start und Ziel sind hier optisch unterscheidbar fürs Aufbauen
        setPuck(g->startPuckIdx, EFF_SINGLE_CHASE, g->color, 40, 150);
        setPuck(g->targetPuckIdx, EFF_BREATHE_MOD4, g->color, 50, 50);
    }
    else if (newState == Z_WAIT_HANDS) {
        g->isHolding = false;
        setPuck(g->startPuckIdx, EFF_SINGLE_CHASE, g->color, 40, 150);
        setPuck(g->targetPuckIdx, EFF_BREATHE_MOD4, g->color, 50, 50);
    }
    else if (newState == Z_COUNTDOWN) {
        // Optik bleibt auf Holding
    }
    else if (newState == Z_RUNNING) {
        g->runStartTime = millis();
        g->expectStart = false;
        if (shuttleMode) g->shuttleLaps = 0;
        setPuck(g->startPuckIdx, EFF_STATUS, g->color, 0, 20);
        setPuck(g->targetPuckIdx, EFF_SINGLE_CHASE, g->color, 30, 200);
    }
    else if (newState == Z_EVALUATE) {
        if (g->isWinner) {
            setPuck(g->targetPuckIdx, EFF_WIN, CRGB::Green, 0, 200);
            int aliveCount = 0;
            for(int i=0; i<numGroups; i++) if(groups[i].state != Z_ELIMINATED && groups[i].state != Z_SETUP) aliveCount++;
            if (aliveCount == 1) setPuck(g->targetPuckIdx, EFF_RAINBOW, CRGB::Black, 0, 200);
        } else {
            // FIX: Aggressives Blinken, das definitiv 3 Sekunden lang gut sichtbar ist
            setPuck(g->targetPuckIdx, EFF_BLINK, CRGB::Red, 150, 255);
        }
        setPuck(g->startPuckIdx, EFF_STATUS, g->color, 0, 20);
    }
    else if (newState == Z_ELIMINATED) {
        // FIX: Gedimmtes Rot zeigt an, dass die Gruppe ausgeschieden (aber noch an) ist
        setPuck(g->startPuckIdx, EFF_STATIC, CRGB::Red, 0, 20);
        setPuck(g->targetPuckIdx, EFF_STATIC, CRGB::Red, 0, 20);
    }
}

void Game_Zombie::loop() {
    unsigned long now = millis();
    
    // --- 1. SYNCHRONER GRUPPENSTART LOGIK ---
    if (groupStart) {
        bool anyoneWaiting = false;
        bool allHolding = true;

        for (int i=0; i<numGroups; i++) {
            if (groups[i].state != Z_ELIMINATED && groups[i].state != Z_SETUP) {
                if (groups[i].state == Z_WAIT_HANDS || groups[i].state == Z_COUNTDOWN) {
                    anyoneWaiting = true;
                    if (!groups[i].isHolding) allHolding = false;
                } else {
                    allHolding = false; 
                }
            }
        }

        if (anyoneWaiting && allHolding && globalCountdownStart == 0) {
            globalCountdownStart = now;
            CommandPacket snd; memset(&snd, 0, sizeof(snd)); 
            snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
            PuckNetwork::broadcast(snd);
            
            for (int i=0; i<numGroups; i++) {
                if (groups[i].state == Z_WAIT_HANDS) setGroupState(i, Z_COUNTDOWN);
            }
        }
        else if (globalCountdownStart > 0 && !allHolding && anyoneWaiting) {
            globalCountdownStart = 0; 
            for (int i=0; i<numGroups; i++) {
                if (groups[i].state == Z_COUNTDOWN && !groups[i].isHolding) {
                    triggerFalseStart(i);
                } else if (groups[i].state == Z_COUNTDOWN) {
                    setGroupState(i, Z_WAIT_HANDS); 
                }
            }
        }

        if (globalCountdownStart > 0) {
            if (now - globalCountdownStart >= 3000) {
                globalCountdownStart = 0;
                for (int i=0; i<numGroups; i++) {
                    if (groups[i].state == Z_COUNTDOWN) setGroupState(i, Z_RUNNING);
                }
            }
        }
    }

    // --- 2. GRUPPEN LOGIK ---
    for (int i=0; i<numGroups; i++) {
        ZombieGroup* g = &groups[i];
        
        if (!groupStart && g->state == Z_COUNTDOWN) {
            if (now - g->stateStartTime >= 3000) {
                setGroupState(i, Z_RUNNING);
            }
        }

        if (g->state == Z_FALSE_START) {
            if (now - g->stateStartTime > 2000) setGroupState(i, Z_WAIT_HANDS);
        }

        if (g->state == Z_RUNNING) {
            unsigned long currentNow = millis(); 
            if (currentNow >= g->runStartTime) { 
                unsigned long runTime = currentNow - g->runStartTime;
                if (runTime > g->currentTargetTime + 3000) {
                    evaluateRun(i, runTime, true); 
                }
            }
        }

        if (g->state == Z_EVALUATE) {
            if (now - g->stateStartTime > 3000) {
                if (!g->isWinner && suddenDeath) {
                    setGroupState(i, Z_ELIMINATED);
                } else {
                    setGroupState(i, Z_WAIT_HANDS);
                }
            }
        }
    }
}

void Game_Zombie::evaluateRun(int gIdx, unsigned long runTime, bool timeout) {
    ZombieGroup* g = &groups[gIdx];
    g->lastRunTime = runTime;
    g->roundsPlayed++;

    if (timeout || runTime > g->currentTargetTime) {
        g->isWinner = false;
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_EXPLOSION;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->targetPuckIdx].mac, snd);
    } else {
        g->isWinner = true;
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 200;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->targetPuckIdx].mac, snd);
        
        if (globalBestTime == 0 || runTime < globalBestTime) {
            globalBestTime = runTime;
            globalBestGroup = gIdx;
        }

        if (autoReduce && g->currentTargetTime > timeReduction) {
            g->currentTargetTime -= timeReduction;
        }
    }

    setGroupState(gIdx, Z_EVALUATE);
}

void Game_Zombie::triggerFalseStart(int gIdx) {
    ZombieGroup* g = &groups[gIdx];
    g->isHolding = false;
    g->state = Z_FALSE_START;
    g->stateStartTime = millis();
    
    // FIX: Polizei-Blinken auf Start UND Ziel
    setPuck(g->startPuckIdx, EFF_POLICE, CRGB::Red, 0, 255);
    setPuck(g->targetPuckIdx, EFF_POLICE, CRGB::Red, 0, 255);
    
    // FIX: Abbruch des Countdown-Sounds
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); 
    snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_ERROR;
    
    if (groupStart) {
        PuckNetwork::broadcast(snd); // Bricht auf allen Pucks den Sound ab!
    } else {
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->startPuckIdx].mac, snd);
    }
}

void Game_Zombie::handleEvent(int puckIndex, EventPacket event) {
    bool isTarget = false;
    int gIdx = getGroupIndex(puckIndex, isTarget);
    if (gIdx == -1) return;
    
    ZombieGroup* g = &groups[gIdx];

    if (!isTarget) {
        if (g->state == Z_WAIT_HANDS || g->state == Z_COUNTDOWN) {
            if (event.type == EVT_BTN_CLICK) { 
                g->isHolding = true;
                setPuck(puckIndex, EFF_DOUBLE_CHASE, g->color, 20, 150); 
                
                if (!groupStart && g->state == Z_WAIT_HANDS) {
                    setGroupState(gIdx, Z_COUNTDOWN);
                    CommandPacket snd; memset(&snd, 0, sizeof(snd)); 
                    snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
                    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
                }
            }
            else if (event.type == EVT_BTN_RELEASE) { 
                g->isHolding = false;
                if (g->state == Z_WAIT_HANDS) {
                    setPuck(puckIndex, EFF_SINGLE_CHASE, g->color, 40, 150);
                } 
                else if (g->state == Z_COUNTDOWN) {
                    triggerFalseStart(gIdx);
                }
            }
        }
        // Shuttle: Start-Puck Klick während RUNNING = Bahn geschafft (Rückweg)
        else if (shuttleMode && g->state == Z_RUNNING && g->expectStart && event.type == EVT_BTN_CLICK) {
            g->shuttleLaps++;
            g->runStartTime = millis();
            g->expectStart = false;
            if (g->currentTargetTime > timeReduction) g->currentTargetTime -= timeReduction;
            updateShuttlePucks(gIdx);
            CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 100;
            PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
        }
    }
    else {
        if (g->state == Z_RUNNING && event.type == EVT_BTN_CLICK) {
            if (shuttleMode) {
                if (!g->expectStart) {
                    g->shuttleLaps++;
                    g->runStartTime = millis();
                    g->expectStart = true;
                    if (g->currentTargetTime > timeReduction) g->currentTargetTime -= timeReduction;
                    updateShuttlePucks(gIdx);
                    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 100;
                    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
                }
            } else {
                unsigned long runTime = millis() - g->runStartTime;
                evaluateRun(gIdx, runTime, false);
            }
        }
    }
}

int Game_Zombie::getGroupIndex(int puckIndex, bool& isTarget) {
    for (int i=0; i<numGroups; i++) {
        if (groups[i].startPuckIdx == puckIndex) {
            isTarget = false;
            return i;
        }
        if (groups[i].targetPuckIdx == puckIndex) {
            isTarget = true;
            return i;
        }
    }
    return -1;
}

void Game_Zombie::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    if(index < 0 || index >= MAX_PEERS) return;
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, cp);
}

void Game_Zombie::updateShuttlePucks(int gIdx) {
    ZombieGroup* g = &groups[gIdx];
    if (g->expectStart) {
        // Laufe zurück zum Start
        setPuck(g->startPuckIdx, EFF_SINGLE_CHASE, g->color, 30, 200);
        setPuck(g->targetPuckIdx, EFF_STATUS, g->color, 0, 20);
    } else {
        // Laufe zum Ziel
        setPuck(g->startPuckIdx, EFF_STATUS, g->color, 0, 20);
        setPuck(g->targetPuckIdx, EFF_SINGLE_CHASE, g->color, 30, 200);
    }
}

String Game_Zombie::getStatusJSON() {
    String json = "{";
    json += "\"sync\":" + String(groupStart ? 1 : 0) + ",";
    json += "\"auto\":" + String(autoReduce ? 1 : 0) + ",";
    json += "\"shuttle\":" + String(shuttleMode ? 1 : 0) + ",";
    json += "\"bestT\":" + String(globalBestTime) + ",";
    json += "\"bestG\":" + String(globalBestGroup) + ",";
    
    json += "\"groups\":[";
    for(int i=0; i<numGroups; i++) {
        if(i>0) json += ",";
        json += "{";
        json += "\"id\":" + String(groups[i].id) + ",";
        json += "\"st\":" + String(groups[i].state) + ",";
        json += "\"targ\":" + String(groups[i].currentTargetTime) + ",";
        json += "\"rnd\":" + String(groups[i].roundsPlayed) + ",";
        json += "\"hld\":" + String(groups[i].isHolding ? 1 : 0) + ",";
        json += "\"shl\":" + String(groups[i].shuttleLaps) + ",";
        json += "\"exp\":" + String(groups[i].expectStart ? 1 : 0) + ",";
        
        long t = 0;
        if (groups[i].state == Z_RUNNING) t = millis() - groups[i].runStartTime;
        else t = groups[i].lastRunTime; 
        
        json += "\"time\":" + String(t);
        json += "}";
    }
    json += "]}";
    return json;
}