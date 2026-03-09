#include "Game_RedGreen.h"
#include "PuckNetwork.h"

// Globale Variablen für Non-Blocking Logik innerhalb dieser Datei
static unsigned long flashResetTime[MAX_PEERS] = {0}; // Wann soll der Flash aufhören?
static unsigned long lastButtonTime[MAX_PEERS] = {0}; // Für Debounce

void Game_RedGreen::setup() {
    Serial.println("GAME: Setup Red Light Green Light");
    state = RG_SETUP;
    // initGame/Licht kommt erst via cmd=setup (names.html)
}

void Game_RedGreen::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "cfg_groups") { numGroups = constrain(value, 1, 10); initGame(); }
    else if (cmd == "cfg_speed") speedSetting = value;
    else if (cmd == "cfg_time") timeLimit = value;
    
    else if (cmd == "start") startGame();
    else if (cmd == "stop") stopGame();
    else if (cmd == "exit") {
        state = RG_SETUP;
        CommandPacket cp; 
        memset(&cp, 0, sizeof(cp));
        cp.cmd=CMD_EFFECT; cp.effectID=EFF_STATUS; cp.r=0; cp.g=255; cp.b=0; cp.extra=255; cp.duration=0;
        PuckNetwork::broadcast(cp);
    }
    
    else if (cmd.startsWith("man_")) {
        char type = cmd.charAt(4); 
        String target = cmd.substring(6);
        RG_TrafficLight newState = (type == 'g') ? TL_GREEN : TL_RED;
        
        if (target == "all") {
            for(int i=0; i<numGroups; i++) setGroupState(i, newState);
        } else {
            int gIdx = target.toInt();
            if (gIdx >= 0 && gIdx < numGroups) setGroupState(gIdx, newState);
        }
    }
}

void Game_RedGreen::initGame() {
    PuckInfo* pucks = PuckNetwork::getPucks();
    int assignedCount = 0;
    
    for(int i=0; i<10; i++) {
        groups[i].id = i;
        groups[i].puckIndices.clear();
        groups[i].score = 0;
        groups[i].color = GROUP_COLORS[i];
        groups[i].lightState = TL_RED; 
        groups[i].pendingSound = false;
        groups[i].lastStateChange = 0;
    }

    for(int i=0; i<MAX_PEERS; i++) {
        lastButtonTime[i] = 0; // Debounce Reset
        flashResetTime[i] = 0; // Timer Reset
        
        if(pucks[i].active) {
            int gIdx = assignedCount % numGroups;
            groups[gIdx].puckIndices.push_back(i);
            assignedCount++;
        }
    }
    
    for(int i=0; i<numGroups; i++) {
        for(int pid : groups[i].puckIndices) {
            setPuck(pid, EFF_BREATHE_MOD4, groups[i].color, 50, 80);
            delay(25); 
        }
    }
}

void Game_RedGreen::startGame() {
    state = RG_PREPARE;
    stateStartTime = millis();
    
    // FIX: Score Reset
    for(int i=0; i<numGroups; i++) groups[i].score = 0;

    for(int i=0; i<numGroups; i++) {
        for(int pid : groups[i].puckIndices) {
            setPuck(pid, EFF_DOUBLE_CHASE, groups[i].color, 40, 150);
            delay(15);
        }
    }
    
    CommandPacket snd; 
    memset(&snd, 0, sizeof(snd));
    snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
    PuckNetwork::broadcast(snd);
}

void Game_RedGreen::stopGame() {
    state = RG_FINISHED;
    finishTime = millis();
    
    CommandPacket snd; 
    memset(&snd, 0, sizeof(snd));
    snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_FANFARE;
    PuckNetwork::broadcast(snd);
    
    for(int i=0; i<numGroups; i++) {
        for(int pid : groups[i].puckIndices) {
            setPuck(pid, EFF_OFF, CRGB::Black);
            delay(10);
        }
    }
}

void Game_RedGreen::loop() {
    unsigned long now = millis();

    // 0. Flash Reset Check (Non-Blocking Logic für Puck Feedback)
    for(int i=0; i<MAX_PEERS; i++) {
        if (flashResetTime[i] > 0 && now >= flashResetTime[i]) {
            flashResetTime[i] = 0; // Erledigt
            
            // Puck wieder auf den Status seiner Gruppe setzen
            int gIdx = getGroupIndex(i);
            if (gIdx != -1) {
                RG_Group* g = &groups[gIdx];
                int effect = (g->lightState == TL_GREEN) ? EFF_DOUBLE_CHASE : ((g->lightState == TL_YELLOW) ? EFF_BLINK : EFF_DOUBLE_CHASE);
                int spd = (g->lightState == TL_GREEN) ? 60 : ((g->lightState == TL_YELLOW) ? 200 : 100);
                CRGB col = (g->lightState == TL_GREEN) ? CRGB::Green : ((g->lightState == TL_YELLOW) ? CRGB::Yellow : CRGB::Red);
                setPuck(i, effect, col, spd, 150);
            }
        }
    }

    if (state == RG_PREPARE) {
        if (now - stateStartTime > 3100) { 
            state = RG_RUNNING;
            gameStartTime = now;
            for(int i=0; i<numGroups; i++) setGroupState(i, TL_RED);
        }
        return;
    }

    if (state == RG_RUNNING) {
        if (now - gameStartTime > (unsigned long)timeLimit * 1000) {
            stopGame();
            return;
        }
        for(int i=0; i<numGroups; i++) {
            updateGroup(i);
        }
    }
}

void Game_RedGreen::updateGroup(int gIdx) {
    RG_Group* g = &groups[gIdx];
    unsigned long now = millis();

    if (g->pendingSound && now >= g->soundTriggerTime) {
        CommandPacket snd; 
        memset(&snd, 0, sizeof(snd));
        snd.cmd = CMD_SOUND;
        if (g->pendingSoundID == 99) snd.duration = 400;
        else if (g->pendingSoundID == 1) snd.duration = 100;
        else snd.duration = 50;
        
        for(int pid : g->puckIndices) {
             PuckNetwork::sendToPuck(PuckNetwork::getPucks()[pid].mac, snd);
             delay(10); 
        }
        g->pendingSound = false;
    }

    if (now >= g->nextSwitchTime) {
        if (g->lightState == TL_RED) setGroupState(gIdx, TL_GREEN);
        else if (g->lightState == TL_GREEN) setGroupState(gIdx, TL_YELLOW);
        else if (g->lightState == TL_YELLOW) setGroupState(gIdx, TL_RED);
    }
}

void Game_RedGreen::setGroupState(int gIdx, RG_TrafficLight newState) {
    RG_Group* g = &groups[gIdx];
    unsigned long now = millis();
    
    if (g->lightState == TL_GREEN && newState == TL_RED) newState = TL_YELLOW; 
    
    g->lightState = newState;
    g->nextSwitchTime = now + getRandomDuration(newState);
    g->lastStateChange = now;
    
    int effect = EFF_STATIC;
    CRGB color = CRGB::Black;
    int sndID = 0;
    int spd = 0;
    
    if (newState == TL_RED) {
        effect = EFF_DOUBLE_CHASE; color = CRGB::Red; sndID = 99; spd = 100; 
    } 
    else if (newState == TL_GREEN) {
        effect = EFF_DOUBLE_CHASE; color = CRGB::Green; sndID = 1; spd = 60; 
    } 
    else if (newState == TL_YELLOW) {
        effect = EFF_BLINK; color = CRGB::Yellow; sndID = 2; spd = 200; 
    }

    for(int pid : g->puckIndices) {
        setPuck(pid, effect, color, spd, 150); 
        delay(25); 
    }
    
    if (newState == TL_RED) {
        delay(60); 
        for(int pid : g->puckIndices) {
            setPuck(pid, effect, color, spd, 150); 
            delay(25);
        }
    }
    
    g->pendingSound = true;
    g->pendingSoundID = sndID;
    g->soundTriggerTime = now + 250; 
}

long Game_RedGreen::getRandomDuration(RG_TrafficLight state) {
    if (state == TL_RED) return random(4000, 10000); 
    else if (state == TL_YELLOW) return random(1500, 2500); 
    else { 
        switch(speedSetting) {
            case 0: return random(4000, 7000);
            case 1: return random(3000, 5000);
            case 2: return random(2000, 4000);
            default: return 3000;
        }
    }
}

void Game_RedGreen::handleEvent(int puckIndex, EventPacket event) {
    if (state != RG_RUNNING) return;
    if (event.type != EVT_BTN_CLICK) return;
    
    // FIX: Software Debounce (333ms)
    unsigned long now = millis();
    if (now - lastButtonTime[puckIndex] < 333) return;
    lastButtonTime[puckIndex] = now;
    
    int gIdx = getGroupIndex(puckIndex);
    if (gIdx == -1) return;
    
    RG_Group* g = &groups[gIdx];
    
    if (g->lightState == TL_GREEN || g->lightState == TL_YELLOW) {
        g->score++;
        
        // 1. Flash Feedback
        setPuck(puckIndex, EFF_FLASH, CRGB::White, 100, 255);
        
        // 2. Sound Feedback
        CommandPacket snd; 
        memset(&snd, 0, sizeof(snd));
        snd.cmd = CMD_SOUND; snd.duration = 50;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
        
        // 3. FIX: Reset vormerken (statt blocking delay)
        flashResetTime[puckIndex] = now + 120;
    } 
    else {
        // --- FEHLER (ROT) ---
        // FIX: Grace Period (150ms Toleranz bei Wechsel auf Rot)
        if (now - g->lastStateChange < 150) {
            // Wir werten es als "gerade noch geschafft" -> Punkt!
            g->score++; 
            return; 
        }
        
        setPuck(puckIndex, EFF_FAIL, CRGB::Red, 0, 255);
        
        CommandPacket snd; 
        memset(&snd, 0, sizeof(snd));
        snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_ERROR;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
        
        // Bei Fehler auch Reset vormerken (damit er nicht ewig Fail anzeigt)
        flashResetTime[puckIndex] = now + 1000; 
    }
}

int Game_RedGreen::getGroupIndex(int puckIndex) {
    for(int i=0; i<numGroups; i++) {
        for(int pid : groups[i].puckIndices) {
            if(pid == puckIndex) return i;
        }
    }
    return -1;
}

void Game_RedGreen::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    if(index < 0 || index >= MAX_PEERS) return;
    PuckInfo* p = PuckNetwork::getPucks();
    if(!p[index].active) return;
    
    CommandPacket cp;
    memset(&cp, 0, sizeof(cp)); 
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(p[index].mac, cp);
}

String Game_RedGreen::getStatusJSON() {
    String json = "{";
    json += "\"state\":" + String(state) + ",";
    long elapsed = 0;
    if (state == RG_RUNNING) elapsed = millis() - gameStartTime;
    else if (state == RG_FINISHED) elapsed = finishTime - gameStartTime;
    if (elapsed < 0) elapsed = 0;
    json += "\"time\":" + String(elapsed) + ",";
    json += "\"limit\":" + String(timeLimit) + ",";
    json += "\"groups\":[";
    for(int i=0; i<numGroups; i++) {
        if(i>0) json += ",";
        json += "{";
        json += "\"id\":" + String(i) + ",";
        json += "\"score\":" + String(groups[i].score) + ",";
        json += "\"light\":" + String(groups[i].lightState);
        json += "}";
    }
    json += "]}";
    return json;
}