#include "Game_React2P.h"
#include "PuckNetwork.h"

void Game_React2P::setup() {
    Serial.println("GAME: Setup React 2-Player");
    initGame();
}

void Game_React2P::processCommand(String cmd, int value) {
    if (cmd == "setup") {
        initGame();
        for(int i=0; i<numGroups; i++) {
            if(groups[i].active) {
                for(int pid : groups[i].puckIndices) setPuck(pid, EFF_BREATHE_MOD4, groups[i].groupColor, 50, 50);
            }
        }
    }
    else if (cmd == "cfg_groups") numGroups = constrain(value, 1, 5);
    else if (cmd == "cfg_pucks") pucksPerGroup = constrain(value, 3, 10);
    else if (cmd == "cfg_time") timeLimit = value * 1000UL;
    else if (cmd == "cfg_colors") colorsPerPlayer = constrain(value, 1, 3);
    else if (cmd == "cfg_fake") useFakeColors = (value == 1);
    
    else if (cmd == "start") {
        gameStartTime = millis();
        stoppedTime = 0;
        for(int i=0; i<numGroups; i++) {
            if(groups[i].active) {
                // FIX: Setzt die Punkte und Zeiten bei "START ALL" sauber zurück
                groups[i].p1.score = 0; groups[i].p1.totalReactionTime = 0;
                groups[i].p2.score = 0; groups[i].p2.totalReactionTime = 0;
                setGroupState(i, R2_PRE_SHOW);
            }
        }
    }
    else if (cmd == "reset") initGame(); 
    else if (cmd == "stop") {
        // FIX: Stoppt die globale Uhr
        if (gameStartTime > 0) {
            stoppedTime = millis() - gameStartTime;
            gameStartTime = 0;
        }
        for(int i=0; i<numGroups; i++) {
            if(groups[i].active) setGroupState(i, R2_FINISHED);
        }
    }
    else if (cmd == "exit") {
        CommandPacket cp; memset(&cp, 0, sizeof(cp));
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
        PuckNetwork::broadcast(cp);
    }
    // FIX: Sichere URL Parameter Übergabe
    else if (cmd == "grp_start") {
        if (value >= 0 && value < numGroups && groups[value].active) {
            groups[value].p1.score = 0; groups[value].p1.totalReactionTime = 0;
            groups[value].p2.score = 0; groups[value].p2.totalReactionTime = 0;
            setGroupState(value, R2_PRE_SHOW);
        }
    }
    else if (cmd == "grp_stop") {
        if (value >= 0 && value < numGroups && groups[value].active) {
            setGroupState(value, R2_FINISHED);
        }
    }
}

void Game_React2P::initGame() {
    gameStartTime = 0;
    stoppedTime = 0;
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
                groups[i].groupColor = GROUP_COLORS[i];
                groups[i].p1.score = 0; groups[i].p1.totalReactionTime = 0;
                groups[i].p2.score = 0; groups[i].p2.totalReactionTime = 0;
                setGroupState(i, R2_SETUP);
            }
        }
    }
}

void Game_React2P::setGroupState(int gIdx, ReactState newState) {
    ReactGroup* g = &groups[gIdx];
    g->state = newState;
    g->stateStartTime = millis();
    
    if (newState == R2_SETUP) {
        for(int pid : g->puckIndices) setPuck(pid, EFF_STATUS, g->groupColor, 0, 50);
    }
    else if (newState == R2_PRE_SHOW) {
        g->preShowStep = -1; // Trigger für den Loop
    }
    else if (newState == R2_COUNTDOWN) {
        for(int pid : g->puckIndices) setPuck(pid, EFF_OFF, CRGB::Black);
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->puckIndices[0]].mac, snd);
    }
    else if (newState == R2_WAITING) {
        for(int pid : g->puckIndices) setPuck(pid, EFF_STATUS, g->groupColor, 0, 50);
        g->nextWaitDuration = random(1500, 4000); 
    }
    else if (newState == R2_RUNNING) {
        setupNextRound(gIdx);
    }
    else if (newState == R2_SHOW_WIN) {
        for(int pid : g->puckIndices) setPuck(pid, EFF_OFF, CRGB::Black);
        
        int winPuck = (g->roundWinner == 1) ? g->p1.currentPuckLocalIdx : g->p2.currentPuckLocalIdx;
        CRGB winColor = (g->roundWinner == 1) ? g->p1.currentColor : g->p2.currentColor;
        
        setPuck(g->puckIndices[winPuck], EFF_WIN, winColor, 0, 255);
        
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 100;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->puckIndices[winPuck]].mac, snd);
    }
    else if (newState == R2_FINISHED) {
        if (g->p1.score == g->p2.score) {
            for(int pid : g->puckIndices) setPuck(pid, EFF_RAINBOW, CRGB::Black, 0, 200);
        } else {
            int winner = (g->p1.score > g->p2.score) ? 1 : 2;
            // FIX: Nimmt nur die Hauptfarbe (Index 0) für die Sieger-Animation
            CRGB c = (winner == 1) ? P1_POOL[0] : P2_POOL[0];
            for(int i=0; i<g->puckIndices.size(); i++) {
                setPuck(g->puckIndices[i], EFF_DOUBLE_CHASE, c, 30, 200);
                delay(10);
            }
        }
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_FANFARE;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->puckIndices[0]].mac, snd);
    }
}

void Game_React2P::setupNextRound(int gIdx) {
    ReactGroup* g = &groups[gIdx];
    g->roundStartTime = millis();
    
    g->p1.currentPuckLocalIdx = random(pucksPerGroup);
    do { g->p2.currentPuckLocalIdx = random(pucksPerGroup); } while (g->p2.currentPuckLocalIdx == g->p1.currentPuckLocalIdx);
    
    g->p1.currentColor = P1_POOL[random(colorsPerPlayer)];
    g->p2.currentColor = P2_POOL[random(colorsPerPlayer)];

    for(int i=0; i<pucksPerGroup; i++) {
        if (i == g->p1.currentPuckLocalIdx) {
            setPuck(g->puckIndices[i], EFF_STATIC, g->p1.currentColor, 0, 255);
        } else if (i == g->p2.currentPuckLocalIdx) {
            setPuck(g->puckIndices[i], EFF_STATIC, g->p2.currentColor, 0, 255);
        } else {
            if (useFakeColors) {
                CRGB fakeC = FAKE_POOL[random(4)];
                setPuck(g->puckIndices[i], EFF_STATIC, fakeC, 0, 200);
            } else {
                setPuck(g->puckIndices[i], EFF_OFF, CRGB::Black);
            }
        }
        delay(10);
    }
    
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 150;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->puckIndices[g->p1.currentPuckLocalIdx]].mac, snd);
}

void Game_React2P::loop() {
    unsigned long now = millis();
    
    if (gameStartTime > 0 && now - gameStartTime > timeLimit) {
        gameStartTime = 0; 
        for(int i=0; i<numGroups; i++) {
            if (groups[i].active && groups[i].state != R2_FINISHED && groups[i].state != R2_SETUP) {
                setGroupState(i, R2_FINISHED);
            }
        }
    }

    for (int i=0; i<numGroups; i++) {
        if (!groups[i].active) continue;
        ReactGroup* g = &groups[i];

        // FIX: Deutlich längere Anzeige der Farben (1 Sekunde pro Farbe gleichzeitig auf beiden Pucks)
        if (g->state == R2_PRE_SHOW) {
            long elapsed = now - g->stateStartTime;
            int step = elapsed / 1000; 
            
            if (step != g->preShowStep && step < colorsPerPlayer) {
                g->preShowStep = step;
                
                CRGB c1 = P1_POOL[step];
                CRGB c2 = P2_POOL[step];
                
                setPuck(g->puckIndices[0], EFF_STATIC, c1, 0, 255);
                delay(10);
                setPuck(g->puckIndices[1], EFF_STATIC, c2, 0, 255);
                
                CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 100;
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[g->puckIndices[0]].mac, snd);
            }
            if (elapsed >= (colorsPerPlayer * 1000) + 500) setGroupState(i, R2_COUNTDOWN);
        }
        else if (g->state == R2_COUNTDOWN) {
            if (now - g->stateStartTime >= 3000) setGroupState(i, R2_WAITING);
        }
        else if (g->state == R2_WAITING) {
            if (now - g->stateStartTime >= g->nextWaitDuration) setGroupState(i, R2_RUNNING);
        }
        else if (g->state == R2_SHOW_WIN) {
            if (now - g->stateStartTime >= 1500) setGroupState(i, R2_WAITING);
        }
    }
}

void Game_React2P::handleEvent(int puckIndex, EventPacket event) {
    if (event.type != EVT_BTN_CLICK) return;
    
    int localIdx = -1;
    int gIdx = getGroupIndex(puckIndex, localIdx);
    if (gIdx == -1) return;
    
    ReactGroup* g = &groups[gIdx];

    if (g->state == R2_RUNNING) {
        unsigned long reactTime = millis() - g->roundStartTime;
        
        if (localIdx == g->p1.currentPuckLocalIdx) {
            g->p1.score++;
            g->p1.totalReactionTime += reactTime;
            g->roundWinner = 1;
            setGroupState(gIdx, R2_SHOW_WIN);
        } 
        else if (localIdx == g->p2.currentPuckLocalIdx) {
            g->p2.score++;
            g->p2.totalReactionTime += reactTime;
            g->roundWinner = 2;
            setGroupState(gIdx, R2_SHOW_WIN);
        }
    }
}

int Game_React2P::getGroupIndex(int puckIndex, int& localPuckIdx) {
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

void Game_React2P::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    if(index < 0 || index >= MAX_PEERS) return;
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, cp);
}

String Game_React2P::colorToHex(CRGB c) {
    char hex[8];
    sprintf(hex, "#%02X%02X%02X", c.r, c.g, c.b);
    return String(hex);
}

String Game_React2P::getStatusJSON() {
    String json = "{";
    
    long t = 0;
    if (gameStartTime > 0) t = millis() - gameStartTime;
    else t = stoppedTime; // FIX: Angehaltene Uhr wird gesendet
    
    json += "\"time\":" + String(t) + ",";
    json += "\"limit\":" + String(timeLimit) + ",";
    
    json += "\"groups\":[";
    for(int i=0; i<numGroups; i++) {
        if(i>0) json += ",";
        json += "{";
        json += "\"id\":" + String(groups[i].id) + ",";
        json += "\"st\":" + String(groups[i].state) + ",";
        
        json += "\"p1s\":" + String(groups[i].p1.score) + ",";
        json += "\"p2s\":" + String(groups[i].p2.score) + ",";
        
        long p1Avg = (groups[i].p1.score > 0) ? (groups[i].p1.totalReactionTime / groups[i].p1.score) : 0;
        long p2Avg = (groups[i].p2.score > 0) ? (groups[i].p2.totalReactionTime / groups[i].p2.score) : 0;
        
        json += "\"p1a\":" + String(p1Avg) + ",";
        json += "\"p2a\":" + String(p2Avg) + ",";
        
        json += "\"p1c\":[";
        for(int c=0; c<colorsPerPlayer; c++) {
            if(c>0) json += ",";
            json += "\"" + colorToHex(P1_POOL[c]) + "\"";
        }
        json += "],";
        
        json += "\"p2c\":[";
        for(int c=0; c<colorsPerPlayer; c++) {
            if(c>0) json += ",";
            json += "\"" + colorToHex(P2_POOL[c]) + "\"";
        }
        json += "]";
        
        json += "}";
    }
    json += "]}";
    return json;
}