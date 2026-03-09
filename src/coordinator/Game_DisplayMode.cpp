#include "Game_DisplayMode.h"
#include "PuckNetwork.h"

void Game_DisplayMode::setup() {
    Serial.println("GAME: Setup Display Mode V2");
    initGame();
}

void Game_DisplayMode::initGame() {
    gameStartTime = millis();
    mode = DISP_MANUAL;
    globalNextEvent = millis() + 5000;
    
    PuckInfo* pucks = PuckNetwork::getPucks();
    
    for(int i=0; i<MAX_PEERS; i++) {
        puckStates[i].effId = EFF_STATUS;
        puckStates[i].color = CRGB::Blue;
        puckStates[i].speed = 0;
        puckStates[i].bright = 50;
        puckStates[i].muted = false;
        
        nextEventTime[i] = millis() + random(1000, 5000);
        
        if (pucks[i].active) {
            setPuck(i, EFF_STATUS, 0, 0, 255, 0, 50);
        }
    }
}

void Game_DisplayMode::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "cfg_time") durationMin = value;
    else if (cmd == "stop" || cmd == "exit") {
        mode = DISP_MANUAL;
        setPuck(-1, EFF_OFF, 0, 0, 0, 0, 0); // -1 = Alle Pucks
    }
    else if (cmd == "mode_man") mode = DISP_MANUAL;
    else if (cmd == "mode_rand") mode = DISP_RANDOM;
    else if (cmd == "mode_sync") mode = DISP_SYNC;
    
    else if (cmd.startsWith("mute_")) {
        int pIdx = cmd.substring(5).toInt();
        if(pIdx >= 0 && pIdx < MAX_PEERS) puckStates[pIdx].muted = !puckStates[pIdx].muted;
    }
    else if (cmd.startsWith("snd_")) {
        int pIdx = cmd.substring(4).toInt();
        CommandPacket cp; memset(&cp, 0, sizeof(cp));
        cp.cmd = CMD_SOUND; cp.duration = 150;
        
        if (pIdx == -1) {
            PuckNetwork::broadcast(cp);
        } else if (pIdx >= 0 && pIdx < MAX_PEERS && !puckStates[pIdx].muted) {
            PuckNetwork::sendToPuck(PuckNetwork::getPucks()[pIdx].mac, cp);
        }
    }
    // Der mächtige DJ-Befehl: "play,target,id,r,g,b,spd,bri"
    else if (cmd.startsWith("play,")) {
        mode = DISP_MANUAL;
        parsePlayCommand(cmd.substring(5));
    }
}

void Game_DisplayMode::parsePlayCommand(String data) {
    int vals[7] = {0};
    int start = 0;
    
    for(int i=0; i<7; i++) {
        int idx = data.indexOf(',', start);
        if(idx == -1) idx = data.length();
        vals[i] = data.substring(start, idx).toInt();
        start = idx + 1;
    }
    
    // vals: 0=target, 1=effId, 2=R, 3=G, 4=B, 5=speed, 6=brightness
    setPuck(vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6]);
}

void Game_DisplayMode::setPuck(int targetIdx, uint8_t eff, uint8_t r, uint8_t g, uint8_t b, int spd, uint8_t bri) {
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT;
    cp.effectID = eff;
    cp.r = r; cp.g = g; cp.b = b;
    cp.duration = spd;
    cp.extra = bri;

    if (targetIdx == -1) {
        // An alle senden (3x für Zuverlässigkeit)
        PuckNetwork::broadcast(cp); delay(15);
        PuckNetwork::broadcast(cp); delay(15);
        PuckNetwork::broadcast(cp);
        
        for(int i=0; i<MAX_PEERS; i++) {
            puckStates[i].effId = eff;
            puckStates[i].color = CRGB(r, g, b);
            puckStates[i].speed = spd;
            puckStates[i].bright = bri;
        }
    } else if (targetIdx >= 0 && targetIdx < MAX_PEERS) {
        // Gezielt an einen Puck senden
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[targetIdx].mac, cp);
        delay(10);
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[targetIdx].mac, cp);
        
        puckStates[targetIdx].effId = eff;
        puckStates[targetIdx].color = CRGB(r, g, b);
        puckStates[targetIdx].speed = spd;
        puckStates[targetIdx].bright = bri;
    }
}

void Game_DisplayMode::loop() {
    unsigned long now = millis();
    
    if (now - gameStartTime > (unsigned long)durationMin * 60000) {
        if(mode != DISP_MANUAL) {
            processCommand("stop", 0);
        }
        return;
    }

    if (mode == DISP_RANDOM) updateRandom();
    else if (mode == DISP_SYNC) updateSync();
}

void Game_DisplayMode::updateRandom() {
    unsigned long now = millis();
    PuckInfo* pucks = PuckNetwork::getPucks();
    
    // Liste schöner Demo-Effekte
    const uint8_t demoEffs[] = {EFF_RAINBOW, EFF_BREATHE_MOD4, EFF_SINGLE_CHASE, EFF_DOUBLE_CHASE, EFF_SPARKLE, EFF_SPLIT_ROT};
    
    for(int i=0; i<MAX_PEERS; i++) {
        if(!pucks[i].active) continue;
        
        if (now >= nextEventTime[i]) {
            uint8_t eff = demoEffs[random(6)];
            CRGB c = CHSV(random8(), 255, 255);
            setPuck(i, eff, c.r, c.g, c.b, random(20, 80), 80);
            nextEventTime[i] = now + random(3000, 8000);
        }
    }
}

void Game_DisplayMode::updateSync() {
    unsigned long now = millis();
    if (now >= globalNextEvent) {
        const uint8_t demoEffs[] = {EFF_RAINBOW, EFF_POLICE, EFF_FLASH, EFF_DOUBLE_CHASE, EFF_LOADING};
        uint8_t eff = demoEffs[autoStep % 5];
        CRGB c = CHSV(random8(), 255, 255);
        
        setPuck(-1, eff, c.r, c.g, c.b, 30, 100);
        
        autoStep++;
        globalNextEvent = now + 4000;
    }
}

void Game_DisplayMode::handleEvent(int puckIndex, EventPacket event) {
    if(event.type == EVT_BTN_CLICK && !puckStates[puckIndex].muted) {
        // Kleines Feedback beim Drücken
        CommandPacket snd; memset(&snd, 0, sizeof(snd));
        snd.cmd = CMD_SOUND; snd.duration = 80;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
    }
}

String Game_DisplayMode::getStatusJSON() {
    String json = "{";
    json += "\"mode\":" + String(mode) + ",";
    json += "\"pucks\":[";
    
    PuckInfo* pucks = PuckNetwork::getPucks();
    int count = 0;
    for(int i=0; i<MAX_PEERS; i++) {
        if(pucks[i].active) {
            if(count > 0) json += ",";
            json += "{";
            json += "\"id\":" + String(i) + ",";
            json += "\"muted\":" + String(puckStates[i].muted ? 1 : 0) + ",";
            json += "\"eff\":" + String(puckStates[i].effId) + ",";
            
            // Hex Color
            char hex[8];
            sprintf(hex, "#%02X%02X%02X", puckStates[i].color.r, puckStates[i].color.g, puckStates[i].color.b);
            json += "\"col\":\"" + String(hex) + "\"";
            
            json += "}";
            count++;
        }
    }
    json += "]}";
    return json;
}