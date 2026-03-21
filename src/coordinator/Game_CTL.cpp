#include "Game_CTL.h"
#include "PuckNetwork.h"

void Game_CTL::setup() {
    Serial.println("GAME: Setup Catch the Light Pro");
    initGame();
}

void Game_CTL::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "show_layout") showLayout();
    else if (cmd == "cfg_pucks") activePucksCount = constrain(value, 3, MAX_PEERS);
    else if (cmd == "cfg_diff") difficulty = constrain(value, 1, 3);
    else if (cmd == "cfg_time") durationMs = value * 1000UL;
    else if (cmd == "cfg_hold") holdToStart = (value == 1);
    else if (cmd == "cfg_speedup") speedupMode = (value == 1);

    else if (cmd == "cfg_color") targetColorIdx = constrain(value, 0, 7);
    else if (cmd == "cfg_sound") soundOn = (value == 1);
    else if (cmd == "cfg_fake") fakeColors = (value == 1);

    else if (cmd == "start") startGameSequence();
    else if (cmd == "stop") {
        gameState = CTL_FINISHED;
        stateStartTime = millis();
        showLayout();
    }
    else if (cmd == "reset") initGame();
    else if (cmd == "exit") {
        CommandPacket cp; memset(&cp, 0, sizeof(cp));
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
        PuckNetwork::broadcast(cp);
        gameState = CTL_SETUP;
    }
}

void Game_CTL::initGame() {
    PuckInfo* netPucks = PuckNetwork::getPucks();
    int assigned = 0;

    for(int i=0; i<MAX_PEERS; i++) {
        pucks[i].isActive = false;
        pucks[i].isTarget = false;
        pucks[i].hits = 0;
        pucks[i].misses = 0;
        pucks[i].totalReactionTime = 0;

        if(netPucks[i].active && assigned < activePucksCount) {
            pucks[assigned].globalIdx = i;
            assigned++;
        }
    }
    activePucksCount = assigned;

    totalHits = 0;
    totalMisses = 0;
    globalTotalRT = 0;

    gameState = CTL_SETUP;
}

void Game_CTL::applyDifficulty() {
    if (difficulty == 1) {
        hitTimeWindow = 2500;
        currentSpawnInterval = 1500;
        minSpawnInterval = 800;
    } else if (difficulty == 2) {
        hitTimeWindow = 1200;
        currentSpawnInterval = 1000;
        minSpawnInterval = 500;
    } else {
        hitTimeWindow = 800;
        currentSpawnInterval = 700;
        minSpawnInterval = 300;
    }
}

CRGB Game_CTL::getPuckColor(int idx) {
    return BLOCK_COLORS[(idx / 4) % 8];
}

void Game_CTL::showLayout() {
    for(int i=0; i<activePucksCount; i++) {
        int fillLevel = ((i % 4) + 1) * 63;
        setPuck(pucks[i].globalIdx, EFF_PROGRESS, getPuckColor(i), fillLevel, 150);
    }
}

void Game_CTL::startGameSequence() {
    totalHits = 0;
    totalMisses = 0;
    globalTotalRT = 0;

    for(int i=0; i<activePucksCount; i++) {
        pucks[i].hits = 0;
        pucks[i].misses = 0;
        pucks[i].totalReactionTime = 0;
        pucks[i].isActive = false;

        setPuck(pucks[i].globalIdx, EFF_OFF, CRGB::Black, 0, 0);
    }

    applyDifficulty();

    if (holdToStart) {
        gameState = CTL_WAIT_HANDS;
        setPuck(pucks[0].globalIdx, EFF_SINGLE_CHASE, GAME_COLORS[targetColorIdx], 40, 255);
    } else {
        // Kurzer Beep + Hit-Farbe auf allen Pucks zeigen
        gameState = CTL_SHOW_COLORS;
        stateStartTime = millis();
        if(soundOn) sendSound(pucks[0].globalIdx, 80);
        for(int i=0; i<activePucksCount; i++) {
            setPuck(pucks[i].globalIdx, EFF_STATIC, GAME_COLORS[targetColorIdx], 0, 255);
        }
    }
}

void Game_CTL::loop() {
    unsigned long now = millis();

    if (gameState == CTL_SHOW_COLORS) {
        // 2s Farben zeigen, dann Countdown starten
        if (now - stateStartTime > 2000) {
            for(int i=0; i<activePucksCount; i++) {
                setPuck(pucks[i].globalIdx, EFF_OFF, CRGB::Black, 0, 0);
            }
            gameState = CTL_COUNTDOWN;
            stateStartTime = millis();
            if(soundOn) sendSequence(pucks[0].globalIdx, SEQ_SKI);
        }
    }
    else if (gameState == CTL_COUNTDOWN) {
        if (now - stateStartTime > 3100) {
            gameState = CTL_RUNNING;
            runStartTime = now;
            nextSpawnTime = now + 200;
        }
    }
    else if (gameState == CTL_FALSE_START) {
        if (now - stateStartTime > 2000) {
            gameState = CTL_WAIT_HANDS;
            for(int i=0; i<activePucksCount; i++) setPuck(pucks[i].globalIdx, EFF_OFF, CRGB::Black);
            setPuck(pucks[0].globalIdx, EFF_SINGLE_CHASE, GAME_COLORS[targetColorIdx], 40, 255);
        }
    }
    else if (gameState == CTL_RUNNING) {
        if (now - runStartTime >= durationMs) {
            gameState = CTL_FINISHED;
            stateStartTime = millis();
            if(soundOn) sendSequence(pucks[0].globalIdx, SEQ_FANFARE);
            showLayout();
            return;
        }

        for(int i=0; i<activePucksCount; i++) {
            if (pucks[i].isActive && now >= pucks[i].expireTime) {
                expirePuck(i);
            }
        }

        if (now >= nextSpawnTime) {
            spawnPuck();
            nextSpawnTime = now + currentSpawnInterval;

            if (speedupMode && currentSpawnInterval > minSpawnInterval) {
                currentSpawnInterval = (currentSpawnInterval * 98) / 100;
                if (currentSpawnInterval < minSpawnInterval) currentSpawnInterval = minSpawnInterval;
            }
        }
    }
}

void Game_CTL::spawnPuck() {
    int available[MAX_PEERS];
    int availCount = 0;

    for(int i=0; i<activePucksCount; i++) {
        if (!pucks[i].isActive) {
            available[availCount] = i;
            availCount++;
        }
    }

    if (availCount > 0) {
        int r = random(availCount);
        int idx = available[r];

        pucks[idx].isActive = true;
        pucks[idx].spawnTime = millis();
        pucks[idx].expireTime = millis() + hitTimeWindow;

        bool spawnFake = fakeColors && (random(100) < 30);
        CRGB spawnCol = GAME_COLORS[targetColorIdx];

        if (spawnFake) {
            pucks[idx].isTarget = false;
            int fakeIdx;
            do {
                fakeIdx = random(8);
            } while (fakeIdx == targetColorIdx);
            spawnCol = GAME_COLORS[fakeIdx];
        } else {
            pucks[idx].isTarget = true;
        }

        setPuck(pucks[idx].globalIdx, EFF_STATIC, spawnCol, 0, 255);
    }
}

void Game_CTL::expirePuck(int idx) {
    pucks[idx].isActive = false;

    if (pucks[idx].isTarget) {
        pucks[idx].misses++;
        totalMisses++;
    }
    setPuck(pucks[idx].globalIdx, EFF_OFF, CRGB::Black, 0, 0);
}

void Game_CTL::handleEvent(int globalIdx, EventPacket event) {
    if (event.type != EVT_BTN_CLICK && event.type != EVT_BTN_RELEASE) return;

    unsigned long now = millis();
    int pIdx = -1;

    for(int i=0; i<activePucksCount; i++) {
        if (pucks[i].globalIdx == globalIdx) { pIdx = i; break; }
    }
    if (pIdx == -1) return;

    // --- Pre-Game States: Nur Puck 0 reagiert ---
    if (gameState == CTL_WAIT_HANDS || gameState == CTL_SHOW_COLORS || gameState == CTL_COUNTDOWN) {
        if (pIdx == 0) {
            if (event.type == EVT_BTN_CLICK) {
                setPuck(globalIdx, EFF_DOUBLE_CHASE, GAME_COLORS[targetColorIdx], 20, 255);
                if (gameState == CTL_WAIT_HANDS) {
                    // Hold-to-Start: Beep + Farben zeigen
                    gameState = CTL_SHOW_COLORS;
                    stateStartTime = millis();
                    if(soundOn) sendSound(globalIdx, 80);
                    for(int i=0; i<activePucksCount; i++) {
                        setPuck(pucks[i].globalIdx, EFF_STATIC, GAME_COLORS[targetColorIdx], 0, 255);
                    }
                }
            } else if (event.type == EVT_BTN_RELEASE) {
                if (gameState == CTL_WAIT_HANDS) {
                    setPuck(globalIdx, EFF_SINGLE_CHASE, GAME_COLORS[targetColorIdx], 40, 200);
                } else if (gameState == CTL_SHOW_COLORS || gameState == CTL_COUNTDOWN) {
                    // FIX: Release während Farb-Anzeige ODER Countdown = False Start
                    if (holdToStart) {
                        triggerFalseStart();
                    }
                }
            }
        }
        return;
    }

    // --- Laufendes Spiel ---
    if (gameState == CTL_RUNNING && event.type == EVT_BTN_CLICK) {
        if (pucks[pIdx].isActive) {
            pucks[pIdx].isActive = false;

            if (pucks[pIdx].isTarget) {
                pucks[pIdx].hits++;
                totalHits++;

                unsigned long rt = now - pucks[pIdx].spawnTime;
                pucks[pIdx].totalReactionTime += rt;
                globalTotalRT += rt;

                setPuck(globalIdx, EFF_OFF, CRGB::Black, 0, 0);
                if(soundOn) sendSound(globalIdx, 80);
            } else {
                pucks[pIdx].misses++;
                totalMisses++;

                setPuck(globalIdx, EFF_FLASH, CRGB::Red, 100, 255);
                if(soundOn) sendSequence(globalIdx, SEQ_ERROR);
            }
        }
    }
}

void Game_CTL::triggerFalseStart() {
    gameState = CTL_FALSE_START;
    stateStartTime = millis();
    setPuck(pucks[0].globalIdx, EFF_POLICE, CRGB::Red, 0, 255);
    if(soundOn) sendSequence(pucks[0].globalIdx, SEQ_ERROR);
}

void Game_CTL::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, cp);
}
void Game_CTL::sendSound(int index, int duration) {
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = duration;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, snd);
}
void Game_CTL::sendSequence(int index, int seqID) {
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = seqID;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, snd);
}

String Game_CTL::getStatusJSON() {
    String json = "{";
    json += "\"st\":" + String(gameState) + ",";

    long t = 0;
    if (gameState == CTL_RUNNING) t = durationMs - (millis() - runStartTime);
    if (t < 0 || gameState == CTL_FINISHED) t = 0;
    // FIX: SHOW_COLORS hat State-Wert 6, daher explizit abfangen
    if (gameState <= CTL_COUNTDOWN || gameState == CTL_SHOW_COLORS) t = durationMs;

    json += "\"t\":" + String(t) + ",";
    json += "\"hits\":" + String(totalHits) + ",";
    json += "\"miss\":" + String(totalMisses) + ",";

    long avg = (totalHits > 0) ? (globalTotalRT / totalHits) : 0;
    json += "\"avg\":" + String(avg) + ",";

    json += "\"pucks\":[";
    for(int i=0; i<activePucksCount; i++) {
        if(i>0) json += ",";
        long pAvg = (pucks[i].hits > 0) ? (pucks[i].totalReactionTime / pucks[i].hits) : 0;
        json += "{";
        json += "\"id\":" + String(i) + ",";
        json += "\"h\":" + String(pucks[i].hits) + ",";
        json += "\"m\":" + String(pucks[i].misses) + ",";
        json += "\"a\":" + String(pAvg);
        json += "}";
    }
    json += "]}";
    return json;
}