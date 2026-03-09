#include "Game_Memory.h"
#include "PuckNetwork.h"

void Game_Memory::setup() {
    Serial.println("GAME: Setup Memory Sprint");
    // initGame/Licht kommt erst via cmd=setup (names.html)
}

void Game_Memory::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "cfg_pucks") numPucks = constrain(value, 4, MAX_PEERS);
    else if (cmd == "cfg_speed") {
        if (value == 0) memorizeTime = 2000; // Schnell
        else if (value == 1) memorizeTime = 5000; // Mittel
        else if (value == 2) memorizeTime = 10000; // Langsam
    }
    else if (cmd == "cfg_auto") autoRestart = (value == 1);
    else if (cmd == "cfg_search") searchMode = (value == 1);
    
    else if (cmd == "start") startGame();
    else if (cmd == "stop") {
        gameState = MEM_IDLE;
        for(int i=0; i<activePucksCount; i++) setPuck(puckGlobalIds[i], EFF_OFF, CRGB::Black, 0, 0);
    }
    else if (cmd == "reset") initGame(); 
    else if (cmd == "reveal") {
        if (gameState == MEM_RUNNING) {
            // FIX: Race Condition verhindern. Erst Zeit nehmen, dann State ändern!
            stateStartTime = millis(); 
            gameState = MEM_REVEAL;
            
            firstPuckIdx = -1; // Auswahl verwerfen
            secondPuckIdx = -1;
            
            for(int i=0; i<activePucksCount; i++) {
                if (!puckSolved[i]) setPuck(puckGlobalIds[i], EFF_STATIC, puckColors[i], 0, 255);
            }
        }
    }
    else if (cmd == "exit") {
        CommandPacket cp; memset(&cp, 0, sizeof(cp));
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
        PuckNetwork::broadcast(cp);
        gameState = MEM_SETUP;
    }
}

void Game_Memory::initGame() {
    PuckInfo* netPucks = PuckNetwork::getPucks();
    activePucksCount = 0;
    
    for(int i=0; i<MAX_PEERS; i++) {
        if(netPucks[i].active && activePucksCount < numPucks) {
            puckGlobalIds[activePucksCount] = i;
            activePucksCount++;
        }
    }
    
    // Zwingend gerade Anzahl
    if (activePucksCount % 2 != 0) activePucksCount--; 
    
    // Alle aktiven Pucks zeigen ein neutrales Atmen (Setup Indikator)
    for(int i=0; i<activePucksCount; i++) {
        setPuck(puckGlobalIds[i], EFF_BREATHE_MOD4, CRGB::Turquoise, 50, 85);
        delay(10);
    }
    
    gameState = MEM_SETUP;
}

void Game_Memory::shufflePucks() {
    int pairs = activePucksCount / 2;
    std::vector<CRGB> deck;
    
    for(int i=0; i<pairs; i++) {
        deck.push_back(PALETTE[i % 10]);
        deck.push_back(PALETTE[i % 10]);
    }
    
    // Fisher-Yates Shuffle
    for (int i = deck.size() - 1; i > 0; i--) {
        int j = random(i + 1);
        CRGB temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
    
    for(int i=0; i<activePucksCount; i++) {
        puckColors[i] = deck[i];
        puckSolved[i] = false;
    }
}

void Game_Memory::startGame() {
    shufflePucks();
    pairsFound = 0;
    errors = 0;
    firstPuckIdx = -1;
    secondPuckIdx = -1;
    
    stateStartTime = millis();
    
    if (searchMode) {
        gameState = MEM_SEARCH_INIT;
        for(int i=0; i<activePucksCount; i++) {
            setPuck(puckGlobalIds[i], EFF_RAINBOW, CRGB::Black, 0, 200);
            delay(10);
        }
    } else {
        gameState = MEM_MEMORIZE;
        for(int i=0; i<activePucksCount; i++) {
            setPuck(puckGlobalIds[i], EFF_STATIC, puckColors[i], 0, 255);
            delay(10);
        }
    }
    
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
    PuckNetwork::broadcast(snd);
}

void Game_Memory::resetUnsolvedToNeutral() {
    for(int i=0; i<activePucksCount; i++) {
        if (!puckSolved[i]) {
            setPuck(puckGlobalIds[i], EFF_BREATHE_MOD4, CRGB::White, 50, 85);
            delay(10);
        } else {
            setPuck(puckGlobalIds[i], EFF_OFF, CRGB::Black, 0, 0);
        }
    }
}

void Game_Memory::loop() {
    unsigned long now = millis();

    if (gameState == MEM_SEARCH_INIT) {
        if (now - stateStartTime > 3000) {
            gameState = MEM_RUNNING;
            resetUnsolvedToNeutral();
        }
    }
    else if (gameState == MEM_MEMORIZE) {
        if (now - stateStartTime > memorizeTime) {
            gameState = MEM_RUNNING;
            resetUnsolvedToNeutral();
        }
    }
    else if (gameState == MEM_EVALUATE) {
        if (now - stateStartTime > 1500) {
            if (isMatch) {
                puckSolved[firstPuckIdx] = true;
                puckSolved[secondPuckIdx] = true;
                setPuck(puckGlobalIds[firstPuckIdx], EFF_OFF, CRGB::Black, 0, 0);
                setPuck(puckGlobalIds[secondPuckIdx], EFF_OFF, CRGB::Black, 0, 0);
            } else {
                setPuck(puckGlobalIds[firstPuckIdx], EFF_BREATHE_MOD4, CRGB::White, 50, 85);
                setPuck(puckGlobalIds[secondPuckIdx], EFF_BREATHE_MOD4, CRGB::White, 50, 85);
            }
            
            firstPuckIdx = -1;
            secondPuckIdx = -1;
            
            if (pairsFound >= activePucksCount / 2) {
                gameState = MEM_FINISHED;
                stateStartTime = millis();
                for(int i=0; i<activePucksCount; i++) {
                    setPuck(puckGlobalIds[i], EFF_RAINBOW, CRGB::Black, 0, 200);
                }
                CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_HERO;
                PuckNetwork::broadcast(snd);
            } else {
                gameState = MEM_RUNNING;
            }
        }
    }
    else if (gameState == MEM_REVEAL) {
        if (now - stateStartTime > 2000) {
            gameState = MEM_RUNNING;
            resetUnsolvedToNeutral();
        }
    }
    else if (gameState == MEM_FINISHED) {
        if (now - stateStartTime > 4000) {
            if (autoRestart) {
                startGame();
            } else {
                gameState = MEM_IDLE;
                for(int i=0; i<activePucksCount; i++) setPuck(puckGlobalIds[i], EFF_OFF, CRGB::Black, 0, 0);
            }
        }
    }
}

void Game_Memory::handleEvent(int globalPuckIdx, EventPacket event) {
    if (gameState != MEM_RUNNING || event.type != EVT_BTN_CLICK) return;

    int pIdx = -1;
    for(int i=0; i<activePucksCount; i++) {
        if (puckGlobalIds[i] == globalPuckIdx) { pIdx = i; break; }
    }
    if (pIdx == -1 || puckSolved[pIdx]) return; // Ignoriere gelöste Pucks

    // Wenn es der erste Puck ist
    if (firstPuckIdx == -1) {
        firstPuckIdx = pIdx;
        setPuck(globalPuckIdx, EFF_STATIC, puckColors[pIdx], 0, 255);
        sendSound(globalPuckIdx, 80);
    } 
    // Wenn es der zweite Puck ist (und nicht derselbe wie der erste!)
    else if (firstPuckIdx != pIdx && secondPuckIdx == -1) {
        secondPuckIdx = pIdx;
        setPuck(globalPuckIdx, EFF_STATIC, puckColors[pIdx], 0, 255);
        
        gameState = MEM_EVALUATE;
        stateStartTime = millis();
        
        if (puckColors[firstPuckIdx] == puckColors[secondPuckIdx]) {
            isMatch = true;
            pairsFound++;
            setPuck(puckGlobalIds[firstPuckIdx], EFF_WIN, CRGB::Green, 0, 200);
            setPuck(puckGlobalIds[secondPuckIdx], EFF_WIN, CRGB::Green, 0, 200);
            sendSequence(globalPuckIdx, SEQ_DINGDONG);
        } else {
            isMatch = false;
            errors++;
            setPuck(puckGlobalIds[firstPuckIdx], EFF_FAIL, CRGB::Red, 0, 255);
            setPuck(puckGlobalIds[secondPuckIdx], EFF_FAIL, CRGB::Red, 0, 255);
            sendSequence(globalPuckIdx, SEQ_ERROR);
        }
    }
}

void Game_Memory::setPuck(int globalIdx, int effect, CRGB color, int speed, int bright) {
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[globalIdx].mac, cp);
}

void Game_Memory::sendSound(int globalIdx, int duration) {
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = duration;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[globalIdx].mac, snd);
}

void Game_Memory::sendSequence(int globalIdx, int seqID) {
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = seqID;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[globalIdx].mac, snd);
}

String Game_Memory::getStatusJSON() {
    String json = "{";
    json += "\"st\":" + String(gameState) + ",";
    json += "\"pf\":" + String(pairsFound) + ",";
    json += "\"err\":" + String(errors) + ",";
    json += "\"tot\":" + String(activePucksCount / 2);
    json += "}";
    return json;
}