#include "Game_TTest.h"
#include "PuckNetwork.h"

void Game_TTest::setup() {
    Serial.println("GAME: Setup Agility T-Test");
    initGame();
}

void Game_TTest::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "cfg_groups") numGroups = constrain(value, 1, 2);
    else if (cmd == "cfg_time") timeLimit = value * 1000UL;
    else if (cmd == "cfg_grpstart") groupStart = (value == 1);
    else if (cmd == "cfg_press") puckPress = (value == 1);
    else if (cmd == "cfg_sound") soundEnabled = (value == 1);
    else if (cmd == "cfg_speed") autoSpeed = constrain(value, 0, 2);
    
    else if (cmd == "start") {
        for(int i=0; i<numGroups; i++) {
            if(groups[i].active) {
                groups[i].score = 0;
                groups[i].falseStart = false;
                setGroupState(i, groupStart ? TT_WAIT_HANDS : TT_COUNTDOWN);
            }
        }
    }
    else if (cmd == "reset") initGame(); 
    else if (cmd == "stop") {
        for(int i=0; i<numGroups; i++) {
            if(groups[i].active) setGroupState(i, TT_FINISHED);
        }
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
            setGroupState(gIdx, TT_COUNTDOWN); 
        }
    }
    else if (cmd.startsWith("grp_stop_")) {
        int gIdx = cmd.substring(9).toInt();
        if (gIdx >= 0 && gIdx < numGroups && groups[gIdx].active) {
            setGroupState(gIdx, TT_FINISHED);
        }
    }
}

void Game_TTest::initGame() {
    PuckInfo* pucks = PuckNetwork::getPucks();
    int assigned = 0;

    for(int i=0; i<2; i++) {
        groups[i].id = i;
        groups[i].active = false;
        
        if (i < numGroups) {
            bool hasAll = true;
            for(int p=0; p<4; p++) {
                while(assigned < MAX_PEERS && !pucks[assigned].active) assigned++;
                if (assigned < MAX_PEERS) {
                    groups[i].puckIndices[p] = assigned;
                    assigned++;
                } else {
                    hasAll = false;
                }
            }
            if (hasAll) {
                groups[i].active = true;
                groups[i].color = GROUP_COLORS[i];
                groups[i].score = 0;
                groups[i].currentSeqIdx = 0;
                groups[i].isHoldingStart = false;
                groups[i].falseStart = false;
                setGroupState(i, TT_SETUP);
            }
        }
    }
}

void Game_TTest::setGroupState(int gIdx, TTestState newState) {
    TTestGroup* g = &groups[gIdx];
    g->state = newState;
    g->stateStartTime = millis();
    
    if (newState == TT_SETUP) {
        // FIX: Der Puck erwartet den Füllstand (0-255) im Speed-Parameter, nicht in der Helligkeit!
        // Parameter: setPuck(Index, Effekt, Farbe, Speed(Füllstand), Helligkeit)
        setPuck(g->puckIndices[0], EFF_PROGRESS, g->color, 64, 200);  delay(15);
        setPuck(g->puckIndices[1], EFF_PROGRESS, g->color, 128, 200); delay(15);
        setPuck(g->puckIndices[2], EFF_PROGRESS, g->color, 192, 200); delay(15);
        setPuck(g->puckIndices[3], EFF_PROGRESS, g->color, 255, 200); delay(15);
    }
    else if (newState == TT_WAIT_HANDS) {
        g->isHoldingStart = false;
        for(int p=0; p<4; p++) {
            setPuck(g->puckIndices[p], EFF_BREATHE_MOD4, g->color, 50, 64);
            delay(15);
        }
        setPuck(g->puckIndices[0], EFF_SINGLE_CHASE, g->color, 40, 200);
    }
    else if (newState == TT_COUNTDOWN) {
        for(int p=1; p<4; p++) {
            setPuck(g->puckIndices[p], EFF_BREATHE_MOD4, g->color, 50, 64);
            delay(15);
        }
        setPuck(g->puckIndices[0], EFF_DOUBLE_CHASE, g->color, 20, 255);
        delay(15);
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->puckIndices[0]].mac, snd);
    }
    else if (newState == TT_FALSE_START) {
        g->falseStart = true;
        for(int p=0; p<4; p++) {
            setPuck(g->puckIndices[p], EFF_POLICE, CRGB::Red, 0, 255);
            delay(10);
        }
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_ERROR;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->puckIndices[0]].mac, snd);
    }
    else if (newState == TT_RUNNING) {
        g->currentSeqIdx = 0;
        advanceSequence(gIdx); 
    }
    else if (newState == TT_FINISHED) {
        float maxScore; bool isTie;
        getHighestScore(maxScore, isTie);
        
        if (maxScore > 0 && g->score == maxScore) {
            int eff = isTie ? EFF_WIN : EFF_RAINBOW;
            CRGB c = isTie ? CRGB::Green : CRGB::Black;
            for(int p=0; p<4; p++) {
                setPuck(g->puckIndices[p], eff, c, 30, 255);
                delay(10);
            }
            CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_FANFARE;
            PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->puckIndices[0]].mac, snd);
        } else {
            for(int p=0; p<4; p++) {
                setPuck(g->puckIndices[p], EFF_BREATHE_MOD4, g->color, 50, 64);
                delay(10);
            }
        }
    }
}

void Game_TTest::advanceSequence(int gIdx) {
    TTestGroup* g = &groups[gIdx];
    int targetPuckIdx = SEQUENCE[g->currentSeqIdx];
    
    for(int p=0; p<4; p++) {
        if (p == targetPuckIdx) {
            setPuck(g->puckIndices[p], EFF_DOUBLE_CHASE, g->color, 30, 255);
        } else {
            setPuck(g->puckIndices[p], EFF_BREATHE_MOD4, g->color, 50, 64);
        }
        delay(10);
    }
    
    if (!puckPress) {
        float dist = DISTANCES[g->currentSeqIdx];
        float speed = SPEEDS[autoSpeed];
        unsigned long dur = (unsigned long)((dist / speed) * 1000.0);
        g->targetTime = millis() + dur;
        
        if (soundEnabled) {
            CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 100;
            PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->puckIndices[targetPuckIdx]].mac, snd);
        }
    }
}

void Game_TTest::getHighestScore(float& maxScore, bool& isTie) {
    maxScore = -1.0; isTie = false;
    for(int i=0; i<numGroups; i++) {
        if(groups[i].active && groups[i].score > maxScore) maxScore = groups[i].score;
    }
    int count = 0;
    for(int i=0; i<numGroups; i++) {
        if(groups[i].active && groups[i].score == maxScore) count++;
    }
    if (count > 1) isTie = true;
}

void Game_TTest::loop() {
    unsigned long now = millis();
    
    for (int i=0; i<numGroups; i++) {
        if (!groups[i].active) continue;
        TTestGroup* g = &groups[i];

        if (g->state == TT_WAIT_HANDS) {
            // Warten
        }
        else if (g->state == TT_COUNTDOWN) {
            if (now - g->stateStartTime >= 3000) setGroupState(i, TT_RUNNING);
        }
        else if (g->state == TT_FALSE_START) {
            if (now - g->stateStartTime >= 2000) setGroupState(i, TT_WAIT_HANDS);
        }
        else if (g->state == TT_RUNNING) {
            if (now - g->stateStartTime >= timeLimit) {
                setGroupState(i, TT_FINISHED);
                continue;
            }
            
            if (!puckPress && now >= g->targetTime) {
                g->score += 0.20f; 
                g->currentSeqIdx++;
                if (g->currentSeqIdx >= 5) g->currentSeqIdx = 0;
                advanceSequence(i);
            }
        }
    }
}

void Game_TTest::handleEvent(int puckIndex, EventPacket event) {
    if (event.type != EVT_BTN_CLICK && event.type != EVT_BTN_RELEASE) return;
    
    int localIdx = -1;
    int gIdx = getGroupIndex(puckIndex, localIdx);
    if (gIdx == -1) return;
    
    TTestGroup* g = &groups[gIdx];

    if (g->state == TT_WAIT_HANDS || g->state == TT_COUNTDOWN) {
        if (localIdx == 0) { 
            if (event.type == EVT_BTN_CLICK) {
                g->isHoldingStart = true;
                setPuck(puckIndex, EFF_DOUBLE_CHASE, g->color, 20, 255);
                
                bool allReady = true;
                for(int j=0; j<numGroups; j++) {
                    if (groups[j].active && groups[j].state == TT_WAIT_HANDS && !groups[j].isHoldingStart) allReady = false;
                }
                if (allReady) {
                    for(int j=0; j<numGroups; j++) {
                        if (groups[j].active && groups[j].state == TT_WAIT_HANDS) setGroupState(j, TT_COUNTDOWN);
                    }
                }
            } else if (event.type == EVT_BTN_RELEASE) {
                g->isHoldingStart = false;
                if (g->state == TT_WAIT_HANDS) {
                    setPuck(puckIndex, EFF_SINGLE_CHASE, g->color, 40, 200);
                } else if (g->state == TT_COUNTDOWN && groupStart) {
                    setGroupState(gIdx, TT_FALSE_START);
                }
            }
        }
    }
    else if (g->state == TT_RUNNING && event.type == EVT_BTN_CLICK && puckPress) {
        if (localIdx == SEQUENCE[g->currentSeqIdx]) {
            if (soundEnabled) {
                CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 80;
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
            }
            
            g->score += 0.20f; 
            g->currentSeqIdx++;
            if (g->currentSeqIdx >= 5) g->currentSeqIdx = 0;
            
            advanceSequence(gIdx);
        }
    }
}

int Game_TTest::getGroupIndex(int puckIndex, int& localPuckIdx) {
    for (int i=0; i<numGroups; i++) {
        if (groups[i].active) {
            for (int p=0; p<4; p++) {
                if (groups[i].puckIndices[p] == puckIndex) {
                    localPuckIdx = p;
                    return i;
                }
            }
        }
    }
    return -1;
}

void Game_TTest::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    if(index < 0 || index >= MAX_PEERS) return;
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, cp);
}

String Game_TTest::getStatusJSON() {
    String json = "{";
    json += "\"limit\":" + String(timeLimit) + ",";
    
    json += "\"groups\":[";
    for(int i=0; i<numGroups; i++) {
        if(i>0) json += ",";
        json += "{";
        json += "\"id\":" + String(groups[i].id) + ",";
        json += "\"st\":" + String(groups[i].state) + ",";
        json += "\"sc\":" + String(groups[i].score, 2) + ",";
        json += "\"hld\":" + String(groups[i].isHoldingStart ? 1 : 0) + ",";
        json += "\"fs\":" + String(groups[i].falseStart ? 1 : 0) + ",";
        
        long t = 0;
        if (groups[i].state == TT_RUNNING) t = millis() - groups[i].stateStartTime;
        else if (groups[i].state == TT_FINISHED) t = timeLimit;
        json += "\"time\":" + String(t);
        json += "}";
    }
    json += "]}";
    return json;
}