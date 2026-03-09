#include "Game_Whac.h"
#include "PuckNetwork.h"

void Game_Whac::setup() {
    Serial.println("GAME: Setup Whac-A-Mole");
    initGame();
}

void Game_Whac::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "show_layout") setGlobalState(WHAC_LAYOUT); 
    else if (cmd == "cfg_grp") numGroups = constrain(value, 1, 3);
    else if (cmd == "cfg_ppg") pucksPerGroup = constrain(value, 3, 10);
    else if (cmd == "cfg_time") durationMs = value * 1000UL;
    else if (cmd == "cfg_diff") difficulty = constrain(value, 1, 5);
    else if (cmd == "cfg_col") numColors = constrain(value, 1, 10);
    else if (cmd == "cfg_snd") soundOn = (value == 1);
    else if (cmd == "cfg_shw") showHitColors = (value == 1);
    else if (cmd == "cfg_tgt") targetColorMask = value; 
    
    else if (cmd == "start") setGlobalState(WHAC_START_SKI);
    else if (cmd == "stop") setGlobalState(WHAC_FINISHED_RAINBOW);
    else if (cmd == "reset") initGame(); 
    else if (cmd == "exit") {
        setAllPucks(EFF_STATUS, CRGB::Green, 0, 255);
        globalState = WHAC_SETUP;
    }
}

void Game_Whac::initGame() {
    PuckInfo* netPucks = PuckNetwork::getPucks();
    int assigned = 0;
    
    if (targetColorMask == 0) targetColorMask = 1;
    
    for(int i=0; i<3; i++) {
        groups[i].id = i;
        groups[i].active = false;
        groups[i].pucks.clear();
        
        if (i < numGroups) {
            for(int p=0; p<pucksPerGroup; p++) {
                while(assigned < MAX_PEERS && !netPucks[assigned].active) assigned++;
                if (assigned < MAX_PEERS) {
                    WhacPuck wp;
                    wp.globalIdx = assigned;
                    wp.colorIdx = 0;
                    wp.isTarget = false;
                    wp.isHit = false;
                    wp.revertTime = 0; 
                    groups[i].pucks.push_back(wp);
                    assigned++;
                }
            }
            if (groups[i].pucks.size() == pucksPerGroup) {
                groups[i].active = true;
                groups[i].hits = 0;
                groups[i].misses = 0;
                setGroupState(i, GRP_IDLE);
            }
        }
    }
    
    for(int i=0; i<MAX_PEERS; i++) {
        if(netPucks[i].active) {
            bool isAssigned = false;
            for(int g=0; g<numGroups; g++) {
                if(!groups[g].active) continue;
                for(size_t p=0; p<groups[g].pucks.size(); p++) {
                    if(groups[g].pucks[p].globalIdx == i) isAssigned = true;
                }
            }
            if(!isAssigned) setPuck(i, EFF_OFF, CRGB::Black);
        }
    }
    
    setGlobalState(WHAC_SETUP);
}

void Game_Whac::setGlobalState(WhacGlobalState newState) {
    globalState = newState;
    globalStateTimer = millis();
    
    if (newState == WHAC_SETUP) {
        setAllActivePucks(EFF_BREATHE_MOD4, CRGB::White, 50, 85);
    }
    else if (newState == WHAC_LAYOUT) {
        for(int i=0; i<numGroups; i++) {
            if(!groups[i].active) continue;
            for(size_t p=0; p<groups[i].pucks.size(); p++) {
                setPuck(groups[i].pucks[p].globalIdx, EFF_BREATHE_MOD4, ARENA_COLORS[i], 50, 85);
            }
        }
    }
    else if (newState == WHAC_START_SKI) {
        if (soundOn) sendSequence(groups[0].pucks[0].globalIdx, SEQ_SKI);
        setAllActivePucks(EFF_OFF, CRGB::Black, 0, 0); 
    }
    else if (newState == WHAC_START_TARGETS) {
        int targetArray[10];
        int tCount = 0;
        for(int c=0; c<numColors; c++) {
            if(targetColorMask & (1<<c)) { targetArray[tCount++] = c; }
        }
        
        for(int i=0; i<numGroups; i++) {
            if(!groups[i].active) continue;
            for(size_t p=0; p<groups[i].pucks.size(); p++) {
                int colIdx = targetArray[p % tCount]; 
                setPuck(groups[i].pucks[p].globalIdx, EFF_STATIC, GAME_COLORS[colIdx], 0, 255);
            }
        }
    }
    else if (newState == WHAC_RUNNING) {
        runStartTime = millis();
        for(int i=0; i<numGroups; i++) {
            if(groups[i].active) {
                groups[i].hits = 0;
                groups[i].misses = 0;
                setGroupState(i, GRP_WAIT_NEXT_ROUND); 
            }
        }
    }
    else if (newState == WHAC_FINISHED_RAINBOW) {
        setAllActivePucks(EFF_RAINBOW, CRGB::Black, 0, 200); 
    }
    else if (newState == WHAC_FINISHED_WHITE) {
        setGlobalState(WHAC_LAYOUT); 
    }
}

void Game_Whac::setGroupState(int gIdx, WhacGroupState newState) {
    WhacGroup* g = &groups[gIdx];
    g->state = newState;
    g->stateStartTime = millis();
    
    if (newState == GRP_IDLE) {
    }
    else if (newState == GRP_WAIT_NEXT_ROUND) {
        setGroupPucksNeutral(gIdx);
        g->nextRoundDelay = random(2000, 4001); 
    }
    else if (newState == GRP_ACTIVE) {
        spawnRound(gIdx);
    }
    else if (newState == GRP_MEM_SHOW) {
        spawnRound(gIdx); 
    }
    else if (newState == GRP_MEM_WAIT) {
        setGroupPucksNeutral(gIdx); 
    }
    else if (newState == GRP_MEM_ACTIVE) {
        if (soundOn) sendSequence(g->pucks[0].globalIdx, SEQ_SKI); 
        
        // NEU: Visueller Start-Impuls (Blitz) für den Silent Mode
        for(size_t p=0; p<g->pucks.size(); p++) {
            setPuck(g->pucks[p].globalIdx, EFF_STATIC, CRGB::White, 0, 255); // 100% Helligkeit
            g->pucks[p].revertTime = millis() + 300; // Nach 300ms wieder zurück auf 50% Weiß
        }
    }
    else if (newState == GRP_SHOW_ERRORS) {
        for(size_t p=0; p<g->pucks.size(); p++) {
            if (g->pucks[p].isTarget && !g->pucks[p].isHit) {
                g->misses++;
                setPuck(g->pucks[p].globalIdx, EFF_FLASH, CRGB::Red, 100, 255);
                if (soundOn) sendSound(g->pucks[p].globalIdx, 50);
            } else {
                setPuck(g->pucks[p].globalIdx, EFF_OFF, CRGB::Black); 
            }
        }
        if (difficulty == 5 && g->errorsThisRound >= 2) {
            for(size_t p=0; p<g->pucks.size(); p++) {
                setPuck(g->pucks[p].globalIdx, EFF_FLASH, CRGB::Red, 150, 255);
            }
        }
    }
    else if (newState == GRP_MEM_SUCCESS) {
        g->shiftCount = 0;
    }
}

void Game_Whac::spawnRound(int gIdx) {
    WhacGroup* g = &groups[gIdx];
    g->shiftCount = 0;
    g->errorsThisRound = 0;
    
    bool hasTarget = false;
    
    while (!hasTarget) {
        for(size_t p=0; p<g->pucks.size(); p++) {
            g->pucks[p].isHit = false;
            g->pucks[p].revertTime = 0; 
            
            if (numColors == 1) {
                if (random(100) < 30 || p == 0) { 
                    g->pucks[p].colorIdx = 0;
                    g->pucks[p].isTarget = true;
                    hasTarget = true;
                } else {
                    g->pucks[p].isTarget = false; 
                }
            } else {
                int rCol = random(numColors);
                g->pucks[p].colorIdx = rCol;
                g->pucks[p].isTarget = (targetColorMask & (1 << rCol)) > 0;
                if (g->pucks[p].isTarget) hasTarget = true;
            }
        }
    }
    
    int effect = EFF_STATIC;
    int speed = 0;

    if (difficulty == 2) { effect = EFF_COUNTDOWN; speed = LVL2_TIMEOUT / 35; }
    else if (difficulty == 3) { effect = EFF_COUNTDOWN; speed = LVL3_TIMEOUT / 35; }
    else if (difficulty == 4) { effect = EFF_COUNTDOWN; speed = LVL4_SHIFT / 35; } 

    if (speed < 1 && effect == EFF_COUNTDOWN) speed = 1;
    
    for(size_t p=0; p<g->pucks.size(); p++) {
        if (numColors == 1 && !g->pucks[p].isTarget) {
            setPuck(g->pucks[p].globalIdx, EFF_BREATHE_MOD4, CRGB::White, 50, 85); 
        } else {
            setPuck(g->pucks[p].globalIdx, effect, GAME_COLORS[g->pucks[p].colorIdx], speed, 255);
        }
    }
}

void Game_Whac::loop() {
    unsigned long now = millis();

    if (globalState >= WHAC_START_SKI && globalState <= WHAC_START_TARGETS) {
        long elapsed = now - globalStateTimer;
        
        if (globalState == WHAC_START_SKI && elapsed > 3100) {
            if (showHitColors && numColors > 1) setGlobalState(WHAC_START_TARGETS);
            else setGlobalState(WHAC_RUNNING); 
        }
        else if (globalState == WHAC_START_TARGETS && elapsed > 4000) {
            setGlobalState(WHAC_RUNNING);
        }
        return; 
    }
    
    if (globalState == WHAC_FINISHED_RAINBOW && now - globalStateTimer > 4000) {
        setGlobalState(WHAC_FINISHED_WHITE);
    }
    
    if (globalState == WHAC_RUNNING) {
        if (now - runStartTime >= durationMs) {
            setGlobalState(WHAC_FINISHED_RAINBOW);
            return;
        }

        for (int i=0; i<numGroups; i++) {
            if (!groups[i].active) continue;
            WhacGroup* g = &groups[i];
            
            for(size_t p=0; p<g->pucks.size(); p++) {
                if (g->pucks[p].revertTime > 0 && now >= g->pucks[p].revertTime) {
                    g->pucks[p].revertTime = 0;
                    
                    if (g->state != GRP_ACTIVE && g->state != GRP_MEM_ACTIVE) continue;
                    
                    if (difficulty == 5 || numColors == 1) {
                        setPuck(g->pucks[p].globalIdx, EFF_BREATHE_MOD4, CRGB::White, 50, 85);
                    } else {
                        long remaining = 0;
                        long elapsedRnd = now - g->stateStartTime;
                        if (difficulty == 4) remaining = ((g->shiftCount + 1) * LVL4_SHIFT) - elapsedRnd;
                        else remaining = ((difficulty == 3) ? LVL3_TIMEOUT : LVL2_TIMEOUT) - elapsedRnd;
                        
                        if (remaining < 0) remaining = 0;
                        int speed = remaining / 35;
                        if (speed < 1) speed = 1;
                        
                        int effect = (difficulty >= 2 && difficulty <= 4) ? EFF_COUNTDOWN : EFF_STATIC;
                        setPuck(g->pucks[p].globalIdx, effect, GAME_COLORS[g->pucks[p].colorIdx], speed, 255);
                    }
                }
            }

            long elapsed = now - g->stateStartTime;

            if (g->state == GRP_WAIT_NEXT_ROUND && elapsed >= g->nextRoundDelay) {
                if (difficulty == 5) setGroupState(i, GRP_MEM_SHOW);
                else setGroupState(i, GRP_ACTIVE);
            }
            else if (g->state == GRP_MEM_SHOW && elapsed >= LVL5_SHOW) {
                setGroupState(i, GRP_MEM_WAIT);
            }
            else if (g->state == GRP_MEM_WAIT && elapsed >= LVL5_WAIT) {
                setGroupState(i, GRP_MEM_ACTIVE);
            }
            else if (g->state == GRP_ACTIVE) {
                if (difficulty == 2 && elapsed >= LVL2_TIMEOUT) setGroupState(i, GRP_SHOW_ERRORS);
                else if (difficulty == 3 && elapsed >= LVL3_TIMEOUT) setGroupState(i, GRP_SHOW_ERRORS);
                else if (difficulty == 4) {
                    if (elapsed >= LVL4_TIMEOUT) setGroupState(i, GRP_SHOW_ERRORS);
                    else if (elapsed > (g->shiftCount + 1) * LVL4_SHIFT) handleLevel4Shift(i);
                }
            }
            else if (g->state == GRP_SHOW_ERRORS && elapsed >= 1500) {
                setGroupState(i, GRP_WAIT_NEXT_ROUND);
            }
            else if (g->state == GRP_MEM_SUCCESS) {
                if (elapsed >= 750 && g->shiftCount == 0) {
                    g->shiftCount = 1; 
                    for(size_t p=0; p<g->pucks.size(); p++) {
                        if (g->pucks[p].isTarget) {
                            setPuck(g->pucks[p].globalIdx, EFF_FLASH, GAME_COLORS[g->pucks[p].colorIdx], 100, 255);
                        }
                    }
                    if (soundOn) sendSound(g->pucks[0].globalIdx, 80); 
                }
                else if (elapsed >= 1500) {
                    setGroupState(i, GRP_WAIT_NEXT_ROUND);
                }
            }
        }
    }
}

void Game_Whac::handleLevel4Shift(int gIdx) {
    WhacGroup* g = &groups[gIdx];
    g->shiftCount++;
    
    int lastIdx = g->pucks.size() - 1;
    int tempColor = g->pucks[lastIdx].colorIdx;
    bool tempTarget = g->pucks[lastIdx].isTarget;
    bool tempHit = g->pucks[lastIdx].isHit;
    
    for (int p = lastIdx; p > 0; p--) {
        g->pucks[p].colorIdx = g->pucks[p-1].colorIdx;
        g->pucks[p].isTarget = g->pucks[p-1].isTarget;
        g->pucks[p].isHit    = g->pucks[p-1].isHit;
    }
    
    g->pucks[0].colorIdx = tempColor;
    g->pucks[0].isTarget = tempTarget;
    g->pucks[0].isHit    = tempHit;

    int speed = LVL4_SHIFT / 35;
    if (speed < 1) speed = 1;

    for(size_t p=0; p<g->pucks.size(); p++) {
        g->pucks[p].revertTime = 0; 
        if (g->pucks[p].isHit || (numColors == 1 && !g->pucks[p].isTarget)) {
            setPuck(g->pucks[p].globalIdx, EFF_BREATHE_MOD4, CRGB::White, 50, 85);
        } else {
            setPuck(g->pucks[p].globalIdx, EFF_COUNTDOWN, GAME_COLORS[g->pucks[p].colorIdx], speed, 255);
        }
    }
    
    checkRoundEnd(gIdx); 
}

void Game_Whac::handleEvent(int globalIdx, EventPacket event) {
    if (event.type != EVT_BTN_CLICK || globalState != WHAC_RUNNING) return;
    
    for (int i=0; i<numGroups; i++) {
        if (!groups[i].active) continue;
        WhacGroup* g = &groups[i];
        
        if (g->state != GRP_ACTIVE && g->state != GRP_MEM_ACTIVE) continue;
        
        int pIdx = -1;
        for (size_t p=0; p<g->pucks.size(); p++) {
            if (g->pucks[p].globalIdx == globalIdx) { pIdx = p; break; }
        }
        if (pIdx == -1 || g->pucks[pIdx].isHit) continue; 
        
        WhacPuck* wp = &g->pucks[pIdx];
        
        if (wp->isTarget) {
            wp->isHit = true;
            wp->revertTime = 0; // NEU: Verhindert, dass der weiße Start-Blitz die Farbe überschreibt!
            g->hits++;
            
            if (difficulty == 5) {
                setPuck(globalIdx, EFF_STATIC, GAME_COLORS[wp->colorIdx], 0, 255);
                if (soundOn) sendSound(globalIdx, 80);
            } else {
                setPuck(globalIdx, EFF_BREATHE_MOD4, CRGB::White, 50, 85);
                if (soundOn) sendSound(globalIdx, 80);
            }
            checkRoundEnd(i);
        } else {
            g->misses++;
            if (soundOn) sendSequence(globalIdx, SEQ_ERROR);
            
            if (difficulty == 5) {
                g->errorsThisRound++;
                if (g->errorsThisRound >= 2) {
                    setGroupState(i, GRP_SHOW_ERRORS); 
                } else {
                    setPuck(globalIdx, EFF_FLASH, CRGB::Red, 100, 255); 
                    wp->revertTime = millis() + 300; 
                }
            } else {
                setPuck(globalIdx, EFF_FLASH, CRGB::Red, 100, 255);
                wp->revertTime = millis() + 300; 
            }
        }
        break; 
    }
}

void Game_Whac::checkRoundEnd(int gIdx) {
    WhacGroup* g = &groups[gIdx];
    bool allDone = true;
    for(size_t p=0; p<g->pucks.size(); p++) {
        if (g->pucks[p].isTarget && !g->pucks[p].isHit) {
            allDone = false; break;
        }
    }
    if (allDone) {
        if (difficulty == 5) {
            setGroupState(gIdx, GRP_MEM_SUCCESS);
        } else {
            setGroupState(gIdx, GRP_WAIT_NEXT_ROUND); 
        }
    }
}

void Game_Whac::setAllPucks(int effect, CRGB color, int speed, int bright) {
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::broadcast(cp);
}

void Game_Whac::setAllActivePucks(int effect, CRGB color, int speed, int bright) {
    for(int i=0; i<numGroups; i++) {
        if(!groups[i].active) continue;
        for(size_t p=0; p<groups[i].pucks.size(); p++) {
            setPuck(groups[i].pucks[p].globalIdx, effect, color, speed, bright);
        }
    }
}

void Game_Whac::setGroupPucksNeutral(int gIdx) {
    for(size_t p=0; p<groups[gIdx].pucks.size(); p++) {
        setPuck(groups[gIdx].pucks[p].globalIdx, EFF_BREATHE_MOD4, CRGB::White, 50, 85);
    }
}

void Game_Whac::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, cp);
}
void Game_Whac::sendSound(int index, int duration) {
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = duration;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, snd);
}
void Game_Whac::sendSequence(int index, int seqID) {
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = seqID;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, snd);
}

String Game_Whac::getStatusJSON() {
    String json = "{";
    json += "\"st\":" + String(globalState) + ","; 
    
    long gT = 0;
    if (globalState == WHAC_RUNNING) gT = durationMs - (millis() - runStartTime);
    if (gT < 0 || globalState >= WHAC_FINISHED_RAINBOW) gT = 0;
    if (globalState < WHAC_RUNNING) gT = durationMs;
    json += "\"gT\":" + String(gT) + ",";
    
    long passed10s = (millis() - runStartTime) / 10000;
    if (passed10s < 1) passed10s = 1; 
    if (globalState < WHAC_RUNNING) passed10s = 1;
    
    json += "\"grps\":[";
    for(int i=0; i<numGroups; i++) {
        if(i>0) json += ",";
        
        int h10 = (groups[i].hits * 10) / passed10s;
        if (globalState < WHAC_RUNNING) h10 = 0;
        
        json += "{";
        json += "\"id\":" + String(groups[i].id) + ",";
        json += "\"act\":" + String(groups[i].active ? 1 : 0) + ",";
        json += "\"st\":" + String(groups[i].state) + ",";
        json += "\"sc\":" + String(groups[i].hits) + ",";
        json += "\"m\":" + String(groups[i].misses) + ",";
        json += "\"h10\":" + String(h10);
        json += "}";
    }
    json += "]}";
    return json;
}