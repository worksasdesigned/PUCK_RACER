#include "Game_Target.h"
#include "PuckNetwork.h"

void Game_Target::setup() {
    Serial.println("GAME: Setup Target Touch");
    initGame();
}

void Game_Target::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "cfg_groups") numGroups = constrain(value, 1, 5);
    else if (cmd == "cfg_pucks") pucksPerGroup = constrain(value, 2, 10);
    else if (cmd == "cfg_delay") delayMs = value * 1000UL;
    else if (cmd == "cfg_mode") gameMode = constrain(value, 0, 1);
    else if (cmd == "cfg_time") timeLimit = value * 1000UL;
    else if (cmd == "cfg_rounds") targetRounds = value;
    else if (cmd == "cfg_seq") seqMode = constrain(value, 0, 1);
    else if (cmd == "cfg_grpstart") groupStart = (value == 1);
    
    else if (cmd == "start") {
        for(int i=0; i<numGroups; i++) {
            if(groups[i].active) {
                groups[i].score = 0;
                groups[i].falseStart = false;
                setGroupState(i, groupStart ? TG_WAIT_HANDS : TG_COUNTDOWN);
            }
        }
    }
    else if (cmd == "reset") initGame(); 
    else if (cmd == "stop") {
        for(int i=0; i<numGroups; i++) {
            if(groups[i].active) setGroupState(i, TG_FINISHED);
        }
        evaluateWinners();
    }
    else if (cmd == "exit") {
        CommandPacket cp; memset(&cp, 0, sizeof(cp));
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
        PuckNetwork::broadcast(cp);
    }
    else if (cmd.startsWith("grp_start_")) {
        int gIdx = cmd.substring(10).toInt();
        if (gIdx >= 0 && gIdx < numGroups && groups[gIdx].active) {
            groups[gIdx].score = 0;
            groups[gIdx].falseStart = false;
            setGroupState(gIdx, TG_COUNTDOWN); 
        }
    }
    else if (cmd.startsWith("grp_stop_")) {
        int gIdx = cmd.substring(9).toInt();
        if (gIdx >= 0 && gIdx < numGroups && groups[gIdx].active) {
            setGroupState(gIdx, TG_FINISHED);
        }
    }
}

void Game_Target::initGame() {
    PuckInfo* pucks = PuckNetwork::getPucks();
    int assigned = 0;

    for(int i=0; i<5; i++) {
        groups[i].id = i;
        groups[i].active = false;
        groups[i].puckIndices.clear();
        
        if (i < numGroups) {
            for(int p=0; p<pucksPerGroup; p++) {
                while(assigned < MAX_PEERS && !pucks[assigned].active) assigned++;
                if (assigned < MAX_PEERS) {
                    groups[i].puckIndices.push_back(assigned);
                    assigned++;
                }
            }
            if (groups[i].puckIndices.size() == pucksPerGroup) {
                groups[i].active = true;
                groups[i].color = GROUP_COLORS[i];
                groups[i].score = 0;
                groups[i].isHoldingStart = false;
                groups[i].falseStart = false;
                setGroupState(i, TG_SETUP);
            }
        }
    }
}

void Game_Target::setGroupState(int gIdx, TargetState newState) {
    TargetGroup* g = &groups[gIdx];
    g->state = newState;
    g->stateStartTime = millis();
    
    if (newState == TG_SETUP) {
        for(int p=0; p<g->puckIndices.size(); p++) {
            if (seqMode == 1) {
                // Sequenz-Modus: Dynamische Berechnung des Füllstands (EFF_PROGRESS)
                int fillLevel = ((p + 1) * 255) / pucksPerGroup;
                setPuck(g->puckIndices[p], EFF_PROGRESS, g->color, fillLevel, 200);
            } else {
                // Zufalls-Modus: Pulsieren (EFF_BREATHE_MOD4)
                setPuck(g->puckIndices[p], EFF_BREATHE_MOD4, g->color, 50, 50);
            }
            delay(15); // Sicherer ESP-NOW Abstand
        }
    }
    else if (newState == TG_WAIT_HANDS) {
        g->isHoldingStart = false;
        for(int p=1; p<g->puckIndices.size(); p++) {
            setPuck(g->puckIndices[p], EFF_BREATHE_MOD4, g->color, 50, 30);
            delay(15);
        }
        setPuck(g->puckIndices[0], EFF_SINGLE_CHASE, g->color, 40, 200);
    }
    else if (newState == TG_COUNTDOWN) {
        for(int p=1; p<g->puckIndices.size(); p++) {
            setPuck(g->puckIndices[p], EFF_BREATHE_MOD4, g->color, 50, 30);
            delay(15);
        }
        setPuck(g->puckIndices[0], EFF_DOUBLE_CHASE, g->color, 20, 255);
        delay(15);
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->puckIndices[0]].mac, snd);
    }
    else if (newState == TG_FALSE_START) {
        g->falseStart = true;
        for(int pid : g->puckIndices) {
            setPuck(pid, EFF_POLICE, CRGB::Red, 0, 255);
            delay(10);
        }
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_ERROR;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->puckIndices[0]].mac, snd);
    }
    else if (newState == TG_RUNNING) {
        g->runStartTime = millis();
        g->currentTargetIdx = -1; 
        g->lastPuckIdx = -1;
        g->nextPuckTime = millis() + 500; 
        for(int pid : g->puckIndices) {
            setPuck(pid, EFF_OFF, CRGB::Black);
            delay(10);
        }
    }
    else if (newState == TG_FINISHED) {
        g->finishTime = millis();
        for(int pid : g->puckIndices) {
            setPuck(pid, EFF_OFF, CRGB::Black);
            delay(10);
        }
    }
}

void Game_Target::evaluateWinners() {
    int bestScore = -1;
    unsigned long bestTime = 99999999;
    
    for(int i=0; i<numGroups; i++) {
        if(groups[i].active) {
            if (gameMode == 0 && groups[i].score > bestScore) bestScore = groups[i].score;
            if (gameMode == 1 && groups[i].score >= targetRounds) {
                unsigned long t = groups[i].finishTime - groups[i].runStartTime;
                if (t < bestTime) bestTime = t;
            }
        }
    }

    for(int i=0; i<numGroups; i++) {
        if(groups[i].active) {
            bool isWinner = false;
            if (gameMode == 0 && groups[i].score == bestScore && bestScore > 0) isWinner = true;
            if (gameMode == 1 && groups[i].score >= targetRounds && (groups[i].finishTime - groups[i].runStartTime) == bestTime) isWinner = true;
            
            if (isWinner) {
                for(int pid : groups[i].puckIndices) {
                    setPuck(pid, EFF_RAINBOW, CRGB::Black, 30, 255);
                    delay(10);
                }
            } else {
                for(int pid : groups[i].puckIndices) {
                    setPuck(pid, EFF_BREATHE_MOD4, groups[i].color, 50, 50);
                    delay(10);
                }
            }
        }
    }
}

void Game_Target::loop() {
    unsigned long now = millis();
    bool allFinished = true;
    
    for (int i=0; i<numGroups; i++) {
        if (!groups[i].active) continue;
        TargetGroup* g = &groups[i];

        if (g->state != TG_FINISHED) allFinished = false;

        if (g->state == TG_WAIT_HANDS) {
            // Warten auf Hold
        }
        else if (g->state == TG_COUNTDOWN) {
            if (now - g->stateStartTime >= 3000) setGroupState(i, TG_RUNNING);
        }
        else if (g->state == TG_FALSE_START) {
            if (now - g->stateStartTime >= 2000) setGroupState(i, TG_WAIT_HANDS);
        }
        else if (g->state == TG_RUNNING) {
            if (gameMode == 0 && now - g->runStartTime >= timeLimit) {
                setGroupState(i, TG_FINISHED);
                CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_FANFARE;
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->puckIndices[0]].mac, snd);
                continue;
            }
            
            if (g->currentTargetIdx == -1 && now >= g->nextPuckTime) {
                int nextIdx = 0;
                if (seqMode == 1) { 
                    nextIdx = (g->lastPuckIdx + 1) % pucksPerGroup;
                } else { 
                    do { nextIdx = random(pucksPerGroup); } while (nextIdx == g->lastPuckIdx && pucksPerGroup > 1);
                }
                
                g->currentTargetIdx = nextIdx;
                g->lastPuckIdx = nextIdx;
                
                setPuck(g->puckIndices[nextIdx], EFF_STATIC, g->color, 0, 255);
                CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 100;
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->puckIndices[nextIdx]].mac, snd);
            }
        }
    }
    
    static bool winEvalDone = false; 
    if (allFinished && numGroups > 0) {
        if (!winEvalDone) {
            evaluateWinners();
            winEvalDone = true;
        }
    } else {
        winEvalDone = false;
    }
}

void Game_Target::handleEvent(int puckIndex, EventPacket event) {
    if (event.type != EVT_BTN_CLICK && event.type != EVT_BTN_RELEASE) return;
    
    int localIdx = -1;
    int gIdx = getGroupIndex(puckIndex, localIdx);
    if (gIdx == -1) return;
    
    TargetGroup* g = &groups[gIdx];

    if (g->state == TG_WAIT_HANDS || g->state == TG_COUNTDOWN) {
        if (localIdx == 0) { 
            if (event.type == EVT_BTN_CLICK) {
                g->isHoldingStart = true;
                setPuck(puckIndex, EFF_DOUBLE_CHASE, g->color, 20, 255);
                
                bool allReady = true;
                for(int j=0; j<numGroups; j++) {
                    if (groups[j].active && groups[j].state == TG_WAIT_HANDS && !groups[j].isHoldingStart) allReady = false;
                }
                if (allReady) {
                    for(int j=0; j<numGroups; j++) {
                        if (groups[j].active && groups[j].state == TG_WAIT_HANDS) setGroupState(j, TG_COUNTDOWN);
                    }
                }
            } else if (event.type == EVT_BTN_RELEASE) {
                g->isHoldingStart = false;
                if (g->state == TG_WAIT_HANDS) {
                    setPuck(puckIndex, EFF_SINGLE_CHASE, g->color, 40, 200);
                } else if (g->state == TG_COUNTDOWN && groupStart) {
                    setGroupState(gIdx, TG_FALSE_START);
                }
            }
        }
    }
    else if (g->state == TG_RUNNING && event.type == EVT_BTN_CLICK) {
        if (localIdx == g->currentTargetIdx) {
            g->score++;
            setPuck(puckIndex, EFF_OFF, CRGB::Black);
            
            CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 50;
            PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
            
            if (gameMode == 1 && g->score >= targetRounds) {
                setGroupState(gIdx, TG_FINISHED);
                CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_FANFARE;
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
            } else {
                g->currentTargetIdx = -1; 
                g->nextPuckTime = millis() + delayMs;
            }
        }
    }
}

int Game_Target::getGroupIndex(int puckIndex, int& localPuckIdx) {
    for (int i=0; i<numGroups; i++) {
        if (groups[i].active) {
            for (int p=0; p<groups[i].puckIndices.size(); p++) {
                if (groups[i].puckIndices[p] == puckIndex) {
                    localPuckIdx = p;
                    return i;
                }
            }
        }
    }
    return -1;
}

void Game_Target::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    if(index < 0 || index >= MAX_PEERS) return;
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, cp);
}

String Game_Target::getStatusJSON() {
    String json = "{";
    json += "\"mode\":" + String(gameMode) + ",";
    json += "\"lim\":" + String(gameMode == 0 ? timeLimit : targetRounds) + ",";
    
    json += "\"groups\":[";
    for(int i=0; i<numGroups; i++) {
        if(i>0) json += ",";
        json += "{";
        json += "\"id\":" + String(groups[i].id) + ",";
        json += "\"st\":" + String(groups[i].state) + ",";
        json += "\"sc\":" + String(groups[i].score) + ",";
        json += "\"hld\":" + String(groups[i].isHoldingStart ? 1 : 0) + ",";
        json += "\"fs\":" + String(groups[i].falseStart ? 1 : 0) + ",";
        
        long t = 0;
        if (groups[i].state == TG_RUNNING) t = millis() - groups[i].runStartTime;
        else if (groups[i].state == TG_FINISHED) t = groups[i].finishTime - groups[i].runStartTime;
        json += "\"time\":" + String(t);
        json += "}";
    }
    json += "]}";
    return json;
}