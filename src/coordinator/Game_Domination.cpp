#include "Game_Domination.h"
#include "PuckNetwork.h"

void Game_Domination::setup() {
    Serial.println("GAME: Setup Domination");
    // initGame() wird erst via cmd=setup (names.html) aufgerufen
}

void Game_Domination::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "cfg_groups") numGroups = constrain(value, 1, 2);
    else if (cmd == "cfg_time") durationMs = value * 1000UL;
    else if (cmd == "cfg_block") blockTimeMs = value * 1000UL;
    else if (cmd == "cfg_sync") groupStart = (value == 1);
    else if (cmd == "cfg_mode") { deathmatch = (value == 1); Serial.printf("DOM: mode=%s\n", deathmatch ? "DEATHMATCH" : "STANDARD"); }
    else if (cmd == "cfg_hp") { hpPerTeam = constrain(value, 100, 2000); Serial.printf("DOM: hp=%d\n", hpPerTeam); }
    
    else if (cmd == "start") {
        globalCountdownStart = 0;
        for(int i=0; i<numGroups; i++) {
            if(groups[i].active) setGroupState(i, groupStart ? DOM_WAIT_HANDS : DOM_COUNTDOWN);
        }
    }
    else if (cmd == "stop") {
        for(int i=0; i<numGroups; i++) {
            if(groups[i].active) setGroupState(i, DOM_FINISHED);
        }
    }
    else if (cmd == "reset") initGame(); 
    else if (cmd == "exit") {
        CommandPacket cp; memset(&cp, 0, sizeof(cp));
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
        PuckNetwork::broadcast(cp);
    }
    else if (cmd == "neut") {
        neutralizePuck(value);
    }
}

void Game_Domination::initGame() {
    PuckInfo* netPucks = PuckNetwork::getPucks();
    int assigned = 0;
    int pucksPerGroup = 0;
    
    int totalActive = 0;
    for(int i=0; i<MAX_PEERS; i++) if(netPucks[i].active) totalActive++;
    
    if (numGroups > 0) pucksPerGroup = totalActive / numGroups;
    
    globalCountdownStart = 0;

    for(int i=0; i<2; i++) {
        groups[i].id = i;
        groups[i].active = false;
        groups[i].pucks.clear();
        
        if (i < numGroups && pucksPerGroup >= 2) { 
            groups[i].active = true;
            groups[i].colorT1 = T1_COLORS[i];
            groups[i].colorT2 = T2_COLORS[i];
            groups[i].finishedAnimDone = false;
            
            for(int p=0; p<pucksPerGroup; p++) {
                while(assigned < MAX_PEERS && !netPucks[assigned].active) assigned++;
                if (assigned < MAX_PEERS) {
                    DomPuck dp;
                    dp.globalIdx = assigned;
                    dp.owner = 0;
                    dp.lastOwnerBeforeLock = 0;
                    dp.lastClickTime = 0;
                    dp.lockedUntil = 0;
                    dp.errorUntil = 0;
                    dp.isLockedVisual = false;
                    groups[i].pucks.push_back(dp);
                    assigned++;
                }
            }
            setGroupState(i, DOM_SETUP);
        }
    }
}

void Game_Domination::setGroupState(int gIdx, DomState newState) {
    DomGroup* g = &groups[gIdx];
    g->state = newState;
    g->stateStartTime = millis();
    
    if (newState == DOM_SETUP) {
        g->holdT1 = false;
        g->holdT2 = false;
        g->scoreT1 = 0;
        g->scoreT2 = 0;
        g->hpT1 = hpPerTeam;
        g->hpT2 = hpPerTeam;
        g->dmgT1 = 0;
        g->dmgT2 = 0;
        g->lastHpTick = 0;
        g->finishedAnimDone = false;
        
        // Im Setup zeigen die Pucks die Arena-Farbe (Lila/Türkis) zur Identifikation
        for(size_t p=0; p<g->pucks.size(); p++) {
            g->pucks[p].owner = 0;
            g->pucks[p].lastOwnerBeforeLock = 0;
            g->pucks[p].lockedUntil = 0;
            g->pucks[p].isLockedVisual = false;
            setPuck(g->pucks[p].globalIdx, EFF_BREATHE_MOD4, ARENA_COLORS[gIdx], 50, 85);
            delay(10);
        }
    }
    else if (newState == DOM_WAIT_HANDS) {
        g->holdT1 = false;
        g->holdT2 = false;
        
        for(size_t p=2; p<g->pucks.size(); p++) {
            setPuck(g->pucks[p].globalIdx, EFF_OFF, CRGB::Black);
            delay(10);
        }
        setPuck(g->pucks[0].globalIdx, EFF_SINGLE_CHASE, g->colorT1, 40, 200); delay(10);
        setPuck(g->pucks[1].globalIdx, EFF_SINGLE_CHASE, g->colorT2, 40, 200);
    }
    else if (newState == DOM_COUNTDOWN) {
        setPuck(g->pucks[0].globalIdx, EFF_DOUBLE_CHASE, g->colorT1, 20, 255); delay(10);
        setPuck(g->pucks[1].globalIdx, EFF_DOUBLE_CHASE, g->colorT2, 20, 255);
        if (!groupStart || gIdx == 0) {
            CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
            if (groupStart) PuckNetwork::broadcast(snd);
            else PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->pucks[0].globalIdx].mac, snd);
        }
    }
    else if (newState == DOM_FALSE_START) {
        g->falseStart = true;
        setPuck(g->pucks[0].globalIdx, EFF_POLICE, CRGB::Red, 0, 255); delay(10);
        setPuck(g->pucks[1].globalIdx, EFF_POLICE, CRGB::Red, 0, 255);
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_ERROR;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->pucks[0].globalIdx].mac, snd);
    }
    else if (newState == DOM_RUNNING) {
        g->runStartTime = millis();
        g->lastHpTick = millis();
        g->hpT1 = hpPerTeam;
        g->hpT2 = hpPerTeam;
        g->dmgT1 = 0;
        g->dmgT2 = 0;
        // Im Spiel sind neutrale Pucks Weiß
        for(size_t p=0; p<g->pucks.size(); p++) {
            g->pucks[p].owner = 0;
            g->pucks[p].lastOwnerBeforeLock = 0;
            g->pucks[p].lockedUntil = 0;
            g->pucks[p].isLockedVisual = false;
            setPuck(g->pucks[p].globalIdx, EFF_BREATHE_MOD4, CRGB::White, 50, 85);
            delay(10);
        }
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 800;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->pucks[0].globalIdx].mac, snd);
    }
    else if (newState == DOM_FINISHED) {
        g->finishedAnimDone = false;
        updateScores();

        // Determine winner
        bool isTie;
        CRGB winColor;
        if (deathmatch) {
            isTie = (g->hpT1 == g->hpT2);
            winColor = (g->hpT1 > g->hpT2) ? g->colorT1 : g->colorT2;
        } else {
            isTie = (g->scoreT1 == g->scoreT2);
            winColor = (g->scoreT1 > g->scoreT2) ? g->colorT1 : g->colorT2;
        }

        for(size_t p=0; p<g->pucks.size(); p++) {
            if (isTie) {
                setPuck(g->pucks[p].globalIdx, EFF_RAINBOW, CRGB::Black, 0, 200);
            } else {
                setPuck(g->pucks[p].globalIdx, EFF_DOUBLE_CHASE, winColor, 30, 255);
            }
            delay(10);
        }
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_FANFARE;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->pucks[0].globalIdx].mac, snd);
    }
}

void Game_Domination::updateScores() {
    for (int i=0; i<numGroups; i++) {
        if (!groups[i].active) continue;
        int s1 = 0; int s2 = 0;
        for (size_t p=0; p<groups[i].pucks.size(); p++) {
            if (groups[i].pucks[p].owner == 1) s1++;
            else if (groups[i].pucks[p].owner == 2) s2++;
        }
        groups[i].scoreT1 = s1;
        groups[i].scoreT2 = s2;
    }
}

void Game_Domination::loop() {
    unsigned long now = millis();
    
    if (groupStart) {
        bool anyoneWaiting = false;
        bool allHolding = true;

        for (int i=0; i<numGroups; i++) {
            if (groups[i].active && (groups[i].state == DOM_WAIT_HANDS || groups[i].state == DOM_COUNTDOWN)) {
                anyoneWaiting = true;
                if (!groups[i].holdT1 || !groups[i].holdT2) allHolding = false;
            } else if (groups[i].active) {
                allHolding = false; 
            }
        }

        if (anyoneWaiting && allHolding && globalCountdownStart == 0) {
            globalCountdownStart = now;
            CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
            PuckNetwork::broadcast(snd);
            for (int i=0; i<numGroups; i++) {
                if (groups[i].state == DOM_WAIT_HANDS) setGroupState(i, DOM_COUNTDOWN);
            }
        }
        else if (globalCountdownStart > 0 && !allHolding && anyoneWaiting) {
            globalCountdownStart = 0; 
            for (int i=0; i<numGroups; i++) {
                if (groups[i].state == DOM_COUNTDOWN && (!groups[i].holdT1 || !groups[i].holdT2)) {
                    triggerFalseStart(i);
                } else if (groups[i].state == DOM_COUNTDOWN) {
                    setGroupState(i, DOM_WAIT_HANDS); 
                }
            }
        }

        if (globalCountdownStart > 0 && now - globalCountdownStart >= 3000) {
            globalCountdownStart = 0;
            for (int i=0; i<numGroups; i++) {
                if (groups[i].state == DOM_COUNTDOWN) setGroupState(i, DOM_RUNNING);
            }
        }
    }

    for (int i=0; i<numGroups; i++) {
        if (!groups[i].active) continue;
        DomGroup* g = &groups[i];
        unsigned long currentNow = millis();

        if (!groupStart && g->state == DOM_COUNTDOWN) {
            if (currentNow - g->stateStartTime >= 3000) setGroupState(i, DOM_RUNNING);
        }

        if (g->state == DOM_FALSE_START) {
            if (currentNow - g->stateStartTime > 2000) setGroupState(i, DOM_WAIT_HANDS);
        }

        if (g->state == DOM_RUNNING) {
            // Standard mode: time limit
            if (!deathmatch && currentNow - g->runStartTime >= durationMs) {
                setGroupState(i, DOM_FINISHED);
                continue;
            }

            // Deathmatch: HP tick every second
            if (deathmatch && currentNow - g->lastHpTick >= 1000) {
                g->lastHpTick = currentNow;
                updateScores();

                int totalPucks = g->pucks.size();
                int ownedT1 = g->scoreT1;
                int ownedT2 = g->scoreT2;

                int lossT1 = 0;
                int lossT2 = 0;

                if (ownedT1 == totalPucks && totalPucks > 0) {
                    // Team 1 owns ALL pucks -> Team 2 loses double
                    lossT2 = 2 * totalPucks;
                } else if (ownedT2 == totalPucks && totalPucks > 0) {
                    // Team 2 owns ALL pucks -> Team 1 loses double
                    lossT1 = 2 * totalPucks;
                } else {
                    // Normal: lose 1 HP per enemy puck
                    lossT1 = ownedT2;  // T1 loses HP for pucks T2 owns
                    lossT2 = ownedT1;  // T2 loses HP for pucks T1 owns
                }

                g->dmgT1 = lossT1;
                g->dmgT2 = lossT2;
                g->hpT1 -= lossT1;
                g->hpT2 -= lossT2;
                if (g->hpT1 < 0) g->hpT1 = 0;
                if (g->hpT2 < 0) g->hpT2 = 0;

                if (g->hpT1 <= 0 || g->hpT2 <= 0) {
                    setGroupState(i, DOM_FINISHED);
                    continue;
                }
            }

            for (size_t p=0; p<g->pucks.size(); p++) {
                DomPuck* puck = &g->pucks[p];
                
                if (puck->errorUntil > 0 && currentNow > puck->errorUntil) {
                    puck->errorUntil = 0;
                    CRGB col = (puck->owner == 1) ? g->colorT1 : ((puck->owner == 2) ? g->colorT2 : CRGB::White);
                    
                    if (puck->isLockedVisual) {
                        long remaining = puck->lockedUntil - currentNow;
                        if (remaining > 0) {
                            int speed = remaining / 35;
                            if (speed < 1) speed = 1;
                            setPuck(puck->globalIdx, EFF_COUNTDOWN, col, speed, 255);
                        }
                    } else {
                        setPuck(puck->globalIdx, EFF_BREATHE_MOD4, col, 50, 150);
                    }
                }
                
                if (puck->isLockedVisual && currentNow > puck->lockedUntil && puck->errorUntil == 0) {
                    puck->isLockedVisual = false;
                    CRGB col = (puck->owner == 1) ? g->colorT1 : g->colorT2;
                    setPuck(puck->globalIdx, EFF_BREATHE_MOD4, col, 50, 150);
                }
            }
        }
        
        if (g->state == DOM_FINISHED) {
            if (!g->finishedAnimDone && currentNow - g->stateStartTime > 2000) {
                g->finishedAnimDone = true;
                for (size_t p=0; p<g->pucks.size(); p++) {
                    setPuck(g->pucks[p].globalIdx, EFF_BREATHE_MOD4, CRGB::White, 50, 85);
                    delay(10);
                }
            }
        }
    }
}

void Game_Domination::handleEvent(int globalIdx, EventPacket event) {
    if (event.type != EVT_BTN_CLICK && event.type != EVT_BTN_RELEASE) return;
    
    unsigned long now = millis();
    int gIdx = -1;
    int pLocalIdx = -1;
    
    for (int i=0; i<numGroups; i++) {
        if (!groups[i].active) continue;
        for (size_t p=0; p<groups[i].pucks.size(); p++) {
            if (groups[i].pucks[p].globalIdx == globalIdx) {
                gIdx = i; pLocalIdx = p; break;
            }
        }
        if (gIdx != -1) break;
    }
    if (gIdx == -1) return;
    
    DomGroup* g = &groups[gIdx];
    DomPuck* puck = &g->pucks[pLocalIdx];

    if (g->state == DOM_WAIT_HANDS || g->state == DOM_COUNTDOWN) {
        if (pLocalIdx == 0 || pLocalIdx == 1) { 
            bool isT1 = (pLocalIdx == 0);
            
            if (event.type == EVT_BTN_CLICK) {
                if (isT1) g->holdT1 = true; else g->holdT2 = true;
                setPuck(globalIdx, EFF_DOUBLE_CHASE, isT1 ? g->colorT1 : g->colorT2, 20, 255);
                
                if (!groupStart && g->state == DOM_WAIT_HANDS && g->holdT1 && g->holdT2) {
                    setGroupState(gIdx, DOM_COUNTDOWN);
                }
            } else if (event.type == EVT_BTN_RELEASE) {
                if (isT1) g->holdT1 = false; else g->holdT2 = false;
                
                if (g->state == DOM_WAIT_HANDS) {
                    setPuck(globalIdx, EFF_SINGLE_CHASE, isT1 ? g->colorT1 : g->colorT2, 40, 200);
                } else if (g->state == DOM_COUNTDOWN) {
                    triggerFalseStart(gIdx);
                }
            }
        }
        return;
    }

    if (g->state == DOM_RUNNING && event.type == EVT_BTN_CLICK) {
        
        bool isDoubleClick = (now - puck->lastClickTime < 450);

        if (puck->owner == 0) {
            puck->lastOwnerBeforeLock = 0;
            puck->owner = 1;
            puck->lockedUntil = now + blockTimeMs;
            puck->isLockedVisual = true;
            
            int speed = blockTimeMs / 35;
            if (speed < 1) speed = 1;
            setPuck(globalIdx, EFF_COUNTDOWN, g->colorT1, speed, 255);
            sendSound(globalIdx, 80);
            updateScores();
        }
        else if (isDoubleClick && puck->lastOwnerBeforeLock == 0) {
            puck->owner = 2;
            puck->lockedUntil = now + blockTimeMs;
            puck->isLockedVisual = true;
            
            int speed = blockTimeMs / 35;
            if (speed < 1) speed = 1;
            setPuck(globalIdx, EFF_COUNTDOWN, g->colorT2, speed, 255);
            sendSound(globalIdx, 150);
            updateScores();
        }
        else if (now > puck->lockedUntil) {
            puck->lastOwnerBeforeLock = puck->owner;
            puck->owner = (puck->owner == 1) ? 2 : 1; 
            puck->lockedUntil = now + blockTimeMs;
            puck->isLockedVisual = true;
            
            CRGB newCol = (puck->owner == 1) ? g->colorT1 : g->colorT2;
            int sndDur = (puck->owner == 1) ? 80 : 150;
            int speed = blockTimeMs / 35;
            if (speed < 1) speed = 1;
            
            setPuck(globalIdx, EFF_COUNTDOWN, newCol, speed, 255);
            sendSound(globalIdx, sndDur);
            updateScores();
        }
        else {
            setPuck(globalIdx, EFF_FLASH, CRGB::White, 50, 255);
            sendSequence(globalIdx, SEQ_ERROR);
            puck->errorUntil = now + 400; 
        }
        
        puck->lastClickTime = now;
    }
}

void Game_Domination::triggerFalseStart(int gIdx) {
    if (groupStart) {
        globalCountdownStart = 0;
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_ERROR;
        PuckNetwork::broadcast(snd);
        for(int i=0; i<numGroups; i++) {
            if(groups[i].state == DOM_COUNTDOWN) setGroupState(i, DOM_FALSE_START);
        }
    } else {
        setGroupState(gIdx, DOM_FALSE_START);
    }
}

void Game_Domination::neutralizePuck(int globalIdx) {
    for (int i=0; i<numGroups; i++) {
        for (size_t p=0; p<groups[i].pucks.size(); p++) {
            if (groups[i].pucks[p].globalIdx == globalIdx) {
                if (groups[i].state != DOM_RUNNING && groups[i].state != DOM_SETUP) return;
                
                groups[i].pucks[p].owner = 0;
                groups[i].pucks[p].lastOwnerBeforeLock = 0;
                groups[i].pucks[p].lockedUntil = 0;
                groups[i].pucks[p].errorUntil = 0;
                groups[i].pucks[p].isLockedVisual = false;
                
                // Im Setup: Arena-Farbe. Im Spiel: Weiß.
                CRGB baseColor = (groups[i].state == DOM_SETUP) ? ARENA_COLORS[i] : CRGB::White;
                setPuck(globalIdx, EFF_BREATHE_MOD4, baseColor, 50, 85);
                updateScores();
                return;
            }
        }
    }
}

void Game_Domination::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, cp);
}
void Game_Domination::sendSound(int index, int duration) {
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = duration;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, snd);
}
void Game_Domination::sendSequence(int index, int seqID) {
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = seqID;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, snd);
}

String Game_Domination::getStatusJSON() {
    updateScores();
    String json = "{";
    json += "\"st\":" + String(groups[0].state) + ",";
    json += "\"dm\":" + String(deathmatch ? 1 : 0) + ",";

    long t = 0;
    if (deathmatch) {
        // Count up in deathmatch
        if (groups[0].state == DOM_RUNNING) t = millis() - groups[0].runStartTime;
        else if (groups[0].state == DOM_FINISHED) t = groups[0].stateStartTime - groups[0].runStartTime;
        else t = 0;
    } else {
        if (groups[0].state == DOM_RUNNING) t = durationMs - (millis() - groups[0].runStartTime);
        if (t < 0 || groups[0].state == DOM_FINISHED) t = 0;
        if (groups[0].state <= DOM_COUNTDOWN) t = durationMs;
    }

    json += "\"t\":" + String(t) + ",";
    
    json += "\"grps\":[";
    for(int i=0; i<numGroups; i++) {
        if(i>0) json += ",";
        json += "{";
        json += "\"id\":" + String(groups[i].id) + ",";
        json += "\"t1s\":" + String(groups[i].scoreT1) + ",";
        json += "\"t2s\":" + String(groups[i].scoreT2) + ",";
        json += "\"h1\":" + String(groups[i].holdT1 ? 1 : 0) + ",";
        json += "\"h2\":" + String(groups[i].holdT2 ? 1 : 0) + ",";
        json += "\"fs\":" + String(groups[i].falseStart ? 1 : 0) + ",";
        json += "\"hp1\":" + String(groups[i].hpT1) + ",";
        json += "\"hp2\":" + String(groups[i].hpT2) + ",";
        json += "\"d1\":" + String(groups[i].dmgT1) + ",";
        json += "\"d2\":" + String(groups[i].dmgT2) + ",";
        
        json += "\"pucks\":[";
        for(size_t p=0; p<groups[i].pucks.size(); p++) {
            if(p>0) json += ",";
            json += "{\"gIdx\":" + String(groups[i].pucks[p].globalIdx) + ",";
            json += "\"o\":" + String(groups[i].pucks[p].owner) + ",";
            json += "\"l\":" + String(groups[i].pucks[p].isLockedVisual ? 1 : 0) + "}";
        }
        json += "]}";
    }
    json += "]}";
    return json;
}