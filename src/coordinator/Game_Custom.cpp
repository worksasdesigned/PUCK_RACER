#include "Game_Custom.h"
#include "PuckNetwork.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

// --- Effect name → ID mapping (reduced set for custom games) ---
uint8_t Game_Custom::effectFromString(const char* name) {
    if (!name) return EFF_OFF;
    if (strcmp(name, "STATIC") == 0)       return EFF_STATIC;
    if (strcmp(name, "SINGLE_CHASE") == 0) return EFF_SINGLE_CHASE;
    if (strcmp(name, "DOUBLE_CHASE") == 0) return EFF_DOUBLE_CHASE;
    if (strcmp(name, "STATUS") == 0)       return EFF_STATUS;
    if (strcmp(name, "COUNTDOWN") == 0)    return EFF_COUNTDOWN;
    if (strcmp(name, "BREATHE_MOD2") == 0) return EFF_BREATHE_MOD2;
    if (strcmp(name, "BREATHE_MOD4") == 0) return EFF_BREATHE_MOD4;
    return EFF_OFF; // "OFF" or unknown
}

// --- Load and parse JSON from LittleFS ---
bool Game_Custom::loadJSON() {
    File f = LittleFS.open("/custom_game.json", "r");
    if (!f) { Serial.println("CG: /custom_game.json not found"); return false; }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("CG: JSON parse error: %s\n", err.c_str());
        return false;
    }

    strlcpy(gameName, doc["name"] | "Custom Game", sizeof(gameName));
    stateCount = 0;

    JsonArray statesArr = doc["states"];
    for (JsonObject s : statesArr) {
        if (stateCount >= CG_MAX_STATES) break;
        CG_State& st = states[stateCount];
        memset(&st, 0, sizeof(CG_State));
        strlcpy(st.name, s["name"] | "", sizeof(st.name));

        // --- Actions ---
        st.actionCount = 0;
        JsonArray acts = s["actions"];
        if (acts) {
            for (JsonObject a : acts) {
                if (st.actionCount >= CG_MAX_ACTIONS) break;
                CG_Action& act = st.actions[st.actionCount];
                act.puck     = a["puck"] | (int8_t)-1;
                act.effectID = effectFromString(a["effect"]);
                act.r        = a["r"] | 0;
                act.g        = a["g"] | 0;
                act.b        = a["b"] | 0;
                act.duration = a["dur"] | (uint16_t)0;

                const char* stype = a["sound"];
                act.soundType = 0;
                act.soundDur  = 0;
                if (stype) {
                    if (strcmp(stype, "BEEP") == 0) {
                        act.soundType = 1;
                        act.soundDur  = a["soundDur"] | 100;
                    } else if (strcmp(stype, "SKI") == 0) {
                        act.soundType = 2;
                    }
                }
                st.actionCount++;
            }
        }

        // --- Transitions ---
        st.transCount = 0;
        JsonArray trs = s["transitions"];
        if (trs) {
            for (JsonObject t : trs) {
                if (st.transCount >= CG_MAX_TRANS) break;
                CG_Transition& tr = st.transitions[st.transCount];
                const char* on = t["on"] | "TIMEOUT";
                tr.trigger   = (strcmp(on, "PRESS") == 0) ? 0 : 1;
                tr.puck      = t["puck"] | (int8_t)-1;
                tr.timeDs    = t["time"] | (uint16_t)0;
                tr.gotoState = t["goto"] | (int8_t)-1;
                st.transCount++;
            }
        }
        stateCount++;
    }

    Serial.printf("CG: Loaded '%s' with %d states\n", gameName, stateCount);
    return true;
}

// --- Game lifecycle ---

void Game_Custom::setup() {
    Serial.println("GAME: Custom Game Setup");
    phase = CS_SETUP;
    currentState = -1;
    stateCount = 0;

    CommandPacket cp = {};
    cp.cmd = CMD_EFFECT;
    cp.effectID = EFF_STATUS;
    cp.r = 0; cp.g = 255; cp.b = 0;
    cp.extra = 255;
    PuckNetwork::broadcast(cp);
}

void Game_Custom::processCommand(String cmd, int value) {
    if (cmd == "load_json") {
        if (loadJSON()) {
            PuckInfo* pucks = PuckNetwork::getPucks();
            for (int i = 0; i < activePucks && i < MAX_PEERS; i++) {
                if (!pucks[i].active) continue;
                CommandPacket cp = {};
                cp.cmd = CMD_EFFECT;
                cp.effectID = EFF_BREATHE_MOD4;
                cp.r = 0; cp.g = 200; cp.b = 255;
                cp.duration = 50; cp.extra = 100;
                PuckNetwork::sendToPuck(pucks[i].mac, cp);
            }
        }
    }
    else if (cmd == "config_pucks") {
        if (value > 0 && value <= 12) activePucks = value;
    }
    else if (cmd == "config_time") {
        timeLimitSeconds = value;
    }
    else if (cmd == "start") {
        if (stateCount == 0) return;
        phase = CS_RUNNING;
        gameStartTime = millis();
        enterState(0);
    }
    else if (cmd == "stop" || cmd == "reset") {
        phase = CS_SETUP;
        currentState = -1;
        CommandPacket cp = {};
        cp.cmd = CMD_EFFECT;
        cp.effectID = EFF_BREATHE_MOD4;
        cp.r = 0; cp.g = 200; cp.b = 255;
        cp.duration = 50; cp.extra = 100;
        PuckNetwork::broadcast(cp);
    }
    else if (cmd == "exit") {
        exitGame();
    }
}

// --- State machine ---

void Game_Custom::enterState(int stateIdx) {
    if (stateIdx < 0 || stateIdx >= stateCount) {
        // End of game
        phase = CS_FINISHED;
        CommandPacket snd = {};
        snd.cmd = CMD_SEQUENCE;
        snd.effectID = SEQ_FANFARE;
        PuckNetwork::broadcast(snd);

        CommandPacket cp = {};
        cp.cmd = CMD_EFFECT;
        cp.effectID = EFF_RAINBOW;
        cp.extra = 200;
        PuckNetwork::broadcast(cp);
        return;
    }

    currentState = stateIdx;
    stateEnteredAt = millis();

    CG_State& st = states[stateIdx];
    for (int i = 0; i < st.actionCount; i++) {
        sendAction(st.actions[i]);
    }

    Serial.printf("CG: → State %d '%s'\n", stateIdx, st.name);
}

void Game_Custom::sendAction(const CG_Action& action) {
    PuckInfo* pucks = PuckNetwork::getPucks();

    CommandPacket cp = {};
    cp.cmd = CMD_EFFECT;
    cp.effectID = action.effectID;
    cp.r = action.r; cp.g = action.g; cp.b = action.b;
    cp.duration = action.duration;
    cp.extra = 200;

    // Sound command (prepared but only sent if needed)
    CommandPacket snd = {};
    bool hasSound = false;
    if (action.soundType == 1) {
        snd.cmd = CMD_SOUND;
        snd.duration = action.soundDur;
        hasSound = true;
    } else if (action.soundType == 2) {
        snd.cmd = CMD_SEQUENCE;
        snd.effectID = SEQ_SKI;
        hasSound = true;
    }

    if (action.puck == -1) {
        PuckNetwork::broadcast(cp);
        if (hasSound) PuckNetwork::broadcast(snd);
    } else {
        int idx = action.puck;
        if (idx >= 0 && idx < MAX_PEERS && pucks[idx].active) {
            PuckNetwork::sendToPuck(pucks[idx].mac, cp);
            if (hasSound) PuckNetwork::sendToPuck(pucks[idx].mac, snd);
        }
    }
}

// --- Main loop: check timeout transitions ---

void Game_Custom::loop() {
    if (phase != CS_RUNNING || currentState < 0) return;

    unsigned long now = millis();

    // Global time limit
    if (timeLimitSeconds > 0 && (now - gameStartTime) / 1000 >= (unsigned long)timeLimitSeconds) {
        phase = CS_FINISHED;
        CommandPacket snd = {};
        snd.cmd = CMD_SOUND;
        snd.duration = 1000;
        PuckNetwork::broadcast(snd);
        return;
    }

    // Timeout transitions for current state
    CG_State& st = states[currentState];
    for (int i = 0; i < st.transCount; i++) {
        CG_Transition& tr = st.transitions[i];
        if (tr.trigger == 1) { // TIMEOUT
            unsigned long timeoutMs = (unsigned long)tr.timeDs * 100;
            if (now - stateEnteredAt >= timeoutMs) {
                enterState(tr.gotoState);
                return; // state changed, exit loop iteration
            }
        }
    }
}

// --- Button press handling ---

void Game_Custom::handleEvent(int puckIndex, EventPacket event) {
    if (event.type != EVT_BTN_CLICK) return;
    if (phase != CS_RUNNING || currentState < 0) return;
    if (puckIndex >= activePucks) return;

    CG_State& st = states[currentState];
    for (int i = 0; i < st.transCount; i++) {
        CG_Transition& tr = st.transitions[i];
        if (tr.trigger == 0) { // PRESS
            if (tr.puck == -1 || tr.puck == puckIndex) {
                enterState(tr.gotoState);
                return;
            }
        }
    }
}

// --- Cleanup ---

void Game_Custom::exitGame() {
    phase = CS_SETUP;
    currentState = -1;
    CommandPacket cp = {};
    cp.cmd = CMD_EFFECT;
    cp.effectID = EFF_STATUS;
    cp.r = 0; cp.g = 255; cp.b = 0;
    cp.extra = 255;
    PuckNetwork::broadcast(cp);
    Serial.println("CG: Exit → Status Light");
}

// --- Status JSON for web UI ---

String Game_Custom::getStatusJSON() {
    String json = "{\"phase\":";
    json += String(phase);
    json += ",\"state\":";
    json += String(currentState);
    json += ",\"states\":";
    json += String(stateCount);
    json += ",\"name\":\"";
    json += gameName;
    json += "\",\"time\":";

    long elapsed = 0;
    if (phase == CS_RUNNING)  elapsed = millis() - gameStartTime;
    if (phase == CS_FINISHED) elapsed = millis() - gameStartTime;
    json += String(elapsed);

    json += ",\"sName\":\"";
    if (currentState >= 0 && currentState < stateCount)
        json += states[currentState].name;
    json += "\"}";
    return json;
}
