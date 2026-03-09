#include "Game_Ball.h"
#include "PuckNetwork.h"

void Game_Ball::setup() {
    Serial.println("GAME: Setup Ball Prellen");
    globalState = BALL_SETUP;
    
    // Arrays sicherheitshalber leeren, damit noch keine Gruppe aktiv ist
    for(int i=0; i<6; i++) groups[i].active = false;
    
    // Alle Pucks auf "Status Grün" setzen (verbunden und bereit), 
    // aber noch keine Gruppen zuweisen oder leuchten lassen.
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; 
    cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
    PuckNetwork::broadcast(cp);
}

void Game_Ball::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "show_layout") showLayout();
    else if (cmd == "cfg_ppg") pucksPerGroup = constrain(value, 3, 5);
    else if (cmd == "cfg_grp") numGroups = constrain(value, 1, 6);
    else if (cmd == "cfg_time") durationMs = value * 1000UL;
    else if (cmd == "cfg_auto") autoRestart = (value == 1);
    else if (cmd == "cfg_snd") soundOn = (value == 1);
    else if (cmd == "cfg_rnd") isRandom = (value == 1);
    
    else if (cmd == "start") {
        globalState = BALL_RUNNING;
        globalRunStartTime = millis();
        globalElapsedTime = 0;
        for(int i=0; i<numGroups; i++) {
            if(groups[i].active) setGroupState(i, BALL_COUNTDOWN);
        }
    }
    else if (cmd == "stop") {
        if (globalState == BALL_RUNNING) {
            globalElapsedTime = millis() - globalRunStartTime;
            if (globalElapsedTime > durationMs) globalElapsedTime = durationMs;
        }
        globalState = BALL_FINISHED;
        for(int i=0; i<numGroups; i++) {
            if(groups[i].active) setGroupState(i, BALL_FINISHED);
        }
    }
    else if (cmd == "g_start") { 
        if (value >= 0 && value < numGroups && groups[value].active) {
            if (globalState != BALL_RUNNING) {
                globalState = BALL_RUNNING;
                globalRunStartTime = millis();
                globalElapsedTime = 0;
            }
            setGroupState(value, BALL_COUNTDOWN);
        }
    }
    else if (cmd == "g_stop") { 
        if (value >= 0 && value < numGroups && groups[value].active) {
            setGroupState(value, BALL_FINISHED);
        }
    }
    else if (cmd == "reset") initGame(); 
    else if (cmd == "exit") {
        // Beim Verlassen des Spiels zur Startseite wieder auf "Status Grün" wechseln
        CommandPacket cp; memset(&cp, 0, sizeof(cp));
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
        PuckNetwork::broadcast(cp);
        globalState = BALL_SETUP;
        for(int i=0; i<6; i++) groups[i].active = false;
    }
}

void Game_Ball::initGame() {
    PuckInfo* netPucks = PuckNetwork::getPucks();
    int assigned = 0;
    
    globalState = BALL_SETUP;
    globalElapsedTime = 0;
    
    // Zuerst alle Lichter ausschalten, bevor neu zugewiesen wird. 
    // Das stellt sicher, dass Pucks, die für dieses Spiel nicht gebraucht werden, dunkel bleiben.
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = EFF_OFF;
    PuckNetwork::broadcast(cp);
    
    for(int i=0; i<6; i++) {
        groups[i].id = i;
        groups[i].active = false;
        groups[i].pucks.clear();
        groups[i].color = GRP_COLORS[i];
        
        if (i < numGroups) {
            for(int p=0; p<pucksPerGroup; p++) {
                while(assigned < MAX_PEERS && !netPucks[assigned].active) assigned++;
                if (assigned < MAX_PEERS) {
                    BallPuck bp;
                    bp.globalIdx = assigned;
                    groups[i].pucks.push_back(bp);
                    assigned++;
                }
            }
            if (groups[i].pucks.size() == pucksPerGroup) {
                groups[i].active = true;
                setGroupState(i, BALL_SETUP); // Das schaltet das Atmen in der jeweiligen Teamfarbe ein
            }
        }
    }
}

void Game_Ball::showLayout() {
    for(int i=0; i<numGroups; i++) {
        if (!groups[i].active) continue;
        for(size_t p=0; p<groups[i].pucks.size(); p++) {
            int fill = ((p + 1) * 255) / pucksPerGroup;
            setPuck(groups[i].pucks[p].globalIdx, EFF_PROGRESS, groups[i].color, fill, 150);
        }
    }
}

void Game_Ball::setGroupState(int gIdx, BallState newState) {
    BallGroup* g = &groups[gIdx];
    g->state = newState;
    g->stateStartTime = millis();
    
    if (newState == BALL_SETUP) {
        g->score = 0;
        g->elapsedTime = 0;
        for(size_t p=0; p<g->pucks.size(); p++) {
            setPuck(g->pucks[p].globalIdx, EFF_BREATHE_MOD4, g->color, 50, 85);
        }
    }
    else if (newState == BALL_COUNTDOWN) {
        g->score = 0;
        g->elapsedTime = 0;
        for(size_t p=0; p<g->pucks.size(); p++) {
            setPuck(g->pucks[p].globalIdx, EFF_BLINK, g->color, 200, 200);
        }
        if (soundOn) sendSequence(g->pucks[0].globalIdx, SEQ_SKI);
    }
    else if (newState == BALL_RUNNING) {
        g->runStartTime = millis();
        for(size_t p=0; p<g->pucks.size(); p++) {
            setPuck(g->pucks[p].globalIdx, EFF_OFF, CRGB::Black);
        }
        g->currentPuckLocalIdx = -1;
        advanceGroupPuck(gIdx);
    }
    else if (newState == BALL_FINISHED) {
        for(size_t p=0; p<g->pucks.size(); p++) {
            setPuck(g->pucks[p].globalIdx, EFF_WIN, CRGB::Green, 0, 200);
        }
        if (soundOn) sendSequence(g->pucks[0].globalIdx, SEQ_FANFARE);
    }
    else if (newState == BALL_WAIT_RESTART) {
        for(size_t p=0; p<g->pucks.size(); p++) {
            setPuck(g->pucks[p].globalIdx, EFF_SINGLE_CHASE, g->color, 40, 255);
        }
    }
}

void Game_Ball::advanceGroupPuck(int gIdx) {
    BallGroup* g = &groups[gIdx];
    
    if (g->currentPuckLocalIdx >= 0) {
        setPuck(g->pucks[g->currentPuckLocalIdx].globalIdx, EFF_OFF, CRGB::Black);
    }
    
    if (isRandom) {
        int nextIdx = random(g->pucks.size());
        if (nextIdx == g->currentPuckLocalIdx) nextIdx = (nextIdx + 1) % g->pucks.size();
        g->currentPuckLocalIdx = nextIdx;
    } else {
        g->currentPuckLocalIdx = (g->currentPuckLocalIdx + 1) % g->pucks.size();
    }
    
    setPuck(g->pucks[g->currentPuckLocalIdx].globalIdx, EFF_SINGLE_CHASE, g->color, 30, 255);
}

void Game_Ball::loop() {
    unsigned long now = millis();

    for (int i=0; i<numGroups; i++) {
        if (!groups[i].active) continue;
        BallGroup* g = &groups[i];

        if (g->state == BALL_COUNTDOWN) {
            if (now - g->stateStartTime > 3100) {
                setGroupState(i, BALL_RUNNING);
            }
        }
        else if (g->state == BALL_RUNNING) {
            g->elapsedTime = now - g->runStartTime;
            if (g->elapsedTime >= durationMs) {
                g->elapsedTime = durationMs; 
                setGroupState(i, BALL_FINISHED);
            }
        }
        else if (g->state == BALL_FINISHED) {
            if (now - g->stateStartTime > 2000) { 
                if (autoRestart) {
                    setGroupState(i, BALL_WAIT_RESTART);
                } else {
                    g->state = BALL_SETUP; 
                    for(size_t p=0; p<g->pucks.size(); p++) {
                        setPuck(g->pucks[p].globalIdx, EFF_BREATHE_MOD4, g->color, 50, 85);
                    }
                }
            }
        }
    }
}

void Game_Ball::handleEvent(int globalIdx, EventPacket event) {
    if (event.type != EVT_BTN_CLICK) return;
    
    for (int i=0; i<numGroups; i++) {
        if (!groups[i].active) continue;
        BallGroup* g = &groups[i];
        
        int localIdx = -1;
        for (size_t p=0; p<g->pucks.size(); p++) {
            if (g->pucks[p].globalIdx == globalIdx) { localIdx = p; break; }
        }
        if (localIdx == -1) continue; 
        
        if (g->state == BALL_RUNNING && localIdx == g->currentPuckLocalIdx) {
            g->score++;
            if (soundOn) sendSound(globalIdx, 80);
            advanceGroupPuck(i);
        }
        else if (g->state == BALL_WAIT_RESTART) {
            if (globalState != BALL_RUNNING) {
                globalState = BALL_RUNNING;
                globalRunStartTime = millis();
            }
            setGroupState(i, BALL_COUNTDOWN);
        }
    }
}

void Game_Ball::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, cp);
}
void Game_Ball::sendSound(int index, int duration) {
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = duration;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, snd);
}
void Game_Ball::sendSequence(int index, int seqID) {
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = seqID;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, snd);
}

String Game_Ball::getStatusJSON() {
    String json = "{";
    json += "\"st\":" + String(globalState) + ","; 
    
    long gT = globalElapsedTime;
    if (globalState == BALL_RUNNING) {
        gT = millis() - globalRunStartTime;
        if (gT > durationMs) gT = durationMs; // Stoppuhr stoppt bei Ziel-Zeit!
    }
    json += "\"gT\":" + String(gT) + ",";
    json += "\"lim\":" + String(durationMs) + ",";
    
    json += "\"grps\":[";
    for(int i=0; i<numGroups; i++) {
        if(i>0) json += ",";
        json += "{";
        json += "\"id\":" + String(groups[i].id) + ",";
        json += "\"act\":" + String(groups[i].active ? 1 : 0) + ",";
        json += "\"st\":" + String(groups[i].state) + ",";
        json += "\"sc\":" + String(groups[i].score) + ",";
        json += "\"t\":" + String(groups[i].elapsedTime);
        json += "}";
    }
    json += "]}";
    return json;
}