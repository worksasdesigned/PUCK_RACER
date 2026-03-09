#include "Game_Timer.h"
#include "PuckNetwork.h"

void Game_Timer::setup() {
    Serial.println("GAME: Setup Simple Timer");
    initGame();
}

void Game_Timer::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "cfg_players") numPlayers = constrain(value, 1, 10);
    else if (cmd == "cfg_time") globalBaseTime = value * 1000UL;
    
    // NEU: Helligkeit vom Webinterface setzen
    else if (cmd == "set_bright") {
        globalBrightness = constrain(value, 10, 100);
        // Update für alle Pucks erzwingen, damit die Helligkeit sofort sichtbar wird
        for(int i=0; i<numPlayers; i++) {
            if(players[i].puckIdx != -1) {
                players[i].lastFillLevel = -1; // Erzwingt Neuschreiben bei TS_RUNNING
                // Sende aktuellen Status-Effekt sofort neu (skaliert)
                if (players[i].state != TS_RUNNING) {
                    setPlayerState(i, players[i].state);
                }
            }
        }
    }
    
    else if (cmd == "start") {
        lastLoopTime = millis();
        gameActive = true;
        for(int i=0; i<numPlayers; i++) {
            players[i].timeLeft = globalBaseTime;
            players[i].initialTime = globalBaseTime;
            setPlayerState(i, TS_RUNNING);
        }
    }
    else if (cmd == "reset") initGame(); 
    else if (cmd == "stop") {
        gameActive = false;
        for(int i=0; i<numPlayers; i++) {
            if (players[i].state == TS_RUNNING || players[i].state == TS_PAUSED) {
                setPlayerState(i, TS_STOPPED);
            }
        }
    }
    else if (cmd == "exit") {
        CommandPacket cp; memset(&cp, 0, sizeof(cp));
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
        PuckNetwork::broadcast(cp);
    }
    else if (cmd.startsWith("p_")) {
        int us = cmd.indexOf('_', 2);
        if (us == -1) return;
        String act = cmd.substring(2, us);
        int pIdx = cmd.substring(us + 1).toInt();
        
        if (pIdx < 0 || pIdx >= numPlayers) return;
        
        if (act == "add10") modifyTime(pIdx, 10000);
        else if (act == "add30") modifyTime(pIdx, 30000);
        else if (act == "add60") modifyTime(pIdx, 60000);
        else if (act == "sub10") modifyTime(pIdx, -10000);
        else if (act == "sub30") modifyTime(pIdx, -30000);
        else if (act == "sub60") modifyTime(pIdx, -60000);
        else if (act == "pause") setPlayerState(pIdx, TS_PAUSED);
        else if (act == "res") {
            lastLoopTime = millis(); 
            setPlayerState(pIdx, TS_RUNNING);
        }
        else if (act == "stop") setPlayerState(pIdx, TS_STOPPED);
    }
}

void Game_Timer::initGame() {
    gameActive = false;
    PuckInfo* pucks = PuckNetwork::getPucks();
    int assigned = 0;

    for(int i=0; i<10; i++) {
        players[i].id = i;
        players[i].puckIdx = -1;
        
        if (i < numPlayers) {
            while(assigned < MAX_PEERS && !pucks[assigned].active) assigned++;
            if (assigned < MAX_PEERS) {
                players[i].puckIdx = assigned;
                assigned++;
            }
        }
        
        if (players[i].puckIdx != -1) {
            players[i].color = PLAYER_COLORS[i % 10];
            players[i].timeLeft = globalBaseTime;
            players[i].initialTime = globalBaseTime;
            players[i].state = TS_STOPPED;
            players[i].lastFillLevel = -1;
            
            setPuck(players[i].puckIdx, EFF_BREATHE_MOD4, players[i].color, 50, 50);
            delay(10);
        }
    }
}

void Game_Timer::modifyTime(int pIdx, long msDelta) {
    TimerPlayer* p = &players[pIdx];
    if (p->state == TS_FINISHED_EARLY || p->state == TS_TIME_UP) return;
    
    p->timeLeft += msDelta;
    if (p->timeLeft < 0) p->timeLeft = 0;
    
    if (p->timeLeft > p->initialTime) p->initialTime = p->timeLeft; 
    p->lastFillLevel = -1; 
}

void Game_Timer::setPlayerState(int pIdx, TimerState newState) {
    TimerPlayer* p = &players[pIdx];
    p->state = newState;
    p->lastFillLevel = -1; 
    
    if (newState == TS_PAUSED) {
        setPuck(p->puckIdx, EFF_BLINK, CRGB::Yellow, 500, 100);
    }
    else if (newState == TS_FINISHED_EARLY) {
        setPuck(p->puckIdx, EFF_STATIC, CRGB::Green, 0, 200);
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 150;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[p->puckIdx].mac, snd);
    }
    else if (newState == TS_TIME_UP) {
        setPuck(p->puckIdx, EFF_BREATHE_MOD4, p->color, 50, 60);
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 300;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[p->puckIdx].mac, snd);
    }
    else if (newState == TS_STOPPED) {
        setPuck(p->puckIdx, EFF_OFF, CRGB::Black, 0, 0);
    }
}

void Game_Timer::loop() {
    if (!gameActive) return;
    
    unsigned long now = millis();
    unsigned long delta = now - lastLoopTime;
    lastLoopTime = now;
    
    for (int i=0; i<numPlayers; i++) {
        TimerPlayer* p = &players[i];
        if (p->puckIdx == -1) continue;
        
        if (p->state == TS_RUNNING) {
            p->timeLeft -= delta;
            
            if (p->timeLeft <= 0) {
                p->timeLeft = 0;
                setPlayerState(i, TS_TIME_UP);
            } else {
                int fill = (p->timeLeft * 255) / p->initialTime;
                if (fill < 0) fill = 0;
                if (fill > 255) fill = 255;
                
                if (fill != p->lastFillLevel && (now - p->lastTxTime > 100)) {
                    p->lastFillLevel = fill;
                    p->lastTxTime = now;
                    setPuck(p->puckIdx, EFF_PROGRESS, p->color, fill, 200); 
                }
            }
        }
    }
}

void Game_Timer::handleEvent(int puckIndex, EventPacket event) {
    if (event.type != EVT_BTN_CLICK) return;
    
    int pIdx = getPlayerIndex(puckIndex);
    if (pIdx == -1) return;
    
    if (players[pIdx].state == TS_RUNNING) {
        setPlayerState(pIdx, TS_FINISHED_EARLY);
    }
}

int Game_Timer::getPlayerIndex(int puckIndex) {
    for (int i=0; i<numPlayers; i++) {
        if (players[i].puckIdx == puckIndex) return i;
    }
    return -1;
}

// NEU: Rechnet die Helligkeit zentral vor dem Senden um
void Game_Timer::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    if(index < 0 || index >= MAX_PEERS) return;
    
    // Skaliere die übergebene Helligkeit anhand des globalen Sliders (Prozentrechnung)
    int scaledBright = (bright * globalBrightness) / 100;
    
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; 
    cp.extra = scaledBright; // Gibt den herunterskalierten Wert an die Hardware weiter
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, cp);
}

String Game_Timer::getStatusJSON() {
    String json = "{";
    json += "\"act\":" + String(gameActive ? 1 : 0) + ",";
    
    long maxTimeLeft = 0;
    int finishedCount = 0;
    int totalCount = 0;
    
    for(int i=0; i<numPlayers; i++) {
        if (players[i].puckIdx != -1) {
            totalCount++;
            if (players[i].state == TS_FINISHED_EARLY) finishedCount++;
            if (players[i].state == TS_RUNNING && players[i].timeLeft > maxTimeLeft) maxTimeLeft = players[i].timeLeft;
        }
    }
    
    json += "\"gT\":" + String(maxTimeLeft) + ",";
    json += "\"fin\":" + String(finishedCount) + ",";
    json += "\"tot\":" + String(totalCount) + ",";
    
    json += "\"players\":[";
    bool first = true;
    for(int i=0; i<numPlayers; i++) {
        if (players[i].puckIdx != -1) {
            if(!first) json += ",";
            json += "{";
            json += "\"id\":" + String(players[i].id) + ",";
            json += "\"st\":" + String(players[i].state) + ",";
            json += "\"t\":" + String(players[i].timeLeft);
            json += "}";
            first = false;
        }
    }
    json += "]}";
    return json;
}