#include "Game_TTTT.h"
#include "PuckNetwork.h"

void Game_TTTT::setup() {
    Serial.println("GAME: Setup TTTT");
    initGame();
}

void Game_TTTT::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "cfg_ply") numPlayers = constrain(value, 1, 3);
    else if (cmd == "cfg_pucks") numPucks = value; // Erwartet 4, 6, 7 oder 10
    else if (cmd == "cfg_hard") hardMode = (value == 1);
    else if (cmd == "cfg_auto") autoRestart = (value == 1);
    else if (cmd == "show_layout") showLayout();
    
    else if (cmd == "start") startGame();
    else if (cmd == "stop") {
        state = TTTT_FINISHED;
        finishedTime = millis();
    }
    else if (cmd == "reset") initGame(); 
    else if (cmd == "exit") {
        CommandPacket cp; memset(&cp, 0, sizeof(cp));
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
        PuckNetwork::broadcast(cp);
    }
}

void Game_TTTT::initGame() {
    state = TTTT_SETUP;
    currentPlayer = 0;
    
    // Alle Felder auf neutral (-1) setzen
    for(int i=0; i<9; i++) grid[i] = -1;
    for(int i=0; i<3; i++) winIndices[i] = -1;
    
    // Timer für die Blockzeit zurücksetzen
    for(int i=0; i<MAX_PEERS; i++) lastInput[i] = 0;
    
    // Alle Pucks ausschalten (bis auf den Aufbau-Status in names.html)
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = EFF_OFF;
    PuckNetwork::broadcast(cp);
}

void Game_TTTT::showLayout() {
    // Puck 0 = Auswahlpuck -> Rainbow
    setPuck(0, EFF_RAINBOW, CRGB::White, 0, 150);
    
    // Grid-Pucks (1 bis numPucks-1) mit Progress-Effekt zur Orientierung anzeigen
    // Immer 4 Pucks teilen sich eine Farbe, Füllstand 25%, 50%, 75%, 100%
    CRGB colors[] = { CRGB::Red, CRGB::Blue, CRGB::Green };
    int gridPucksCount = numPucks - 1;
    
    for(int i=0; i<gridPucksCount; i++) {
        int colorIdx = i / 4;
        int fill = ((i % 4) + 1) * 63; // ca. 25%, 50%, 75%, 100% (255)
        if (colorIdx > 2) colorIdx = 2; // Fallback
        
        setPuck(i + 1, EFF_PROGRESS, colors[colorIdx], fill, 200);
    }
}

void Game_TTTT::startGame() {
    state = TTTT_RUNNING;
    runStartTime = millis();
    currentPlayer = 0;
    for(int i=0; i<9; i++) grid[i] = -1;
    for(int i=0; i<MAX_PEERS; i++) lastInput[i] = 0; // Entprell-Timer zum Start nullen
    
    // Start-Signal
    CommandPacket snd; snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
    PuckNetwork::broadcast(snd);
    
    updateSelectorPuck();
    
    // Alle Spiel-Pucks auf neutrales Atmen setzen
    for(int i=1; i<numPucks; i++) {
        setPuck(i, EFF_BREATHE_MOD4, CRGB::White, 50, 100);
    }
}

void Game_TTTT::updateSelectorPuck() {
    // Auswahlpuck zeigt die Farbe des aktuellen Spielers
    setPuck(0, EFF_STATIC, PLAYER_COLORS[currentPlayer], 0, 255);
}

void Game_TTTT::applyGridColor(int gridIdx) {
    int puckIdx = gridIdx + 1; // Mappen auf echte Puck-ID
    if (grid[gridIdx] == -1) {
        // Neutral
        setPuck(puckIdx, EFF_BREATHE_MOD4, CRGB::White, 50, 100);
    } else {
        // Spieler gehört
        setPuck(puckIdx, EFF_STATIC, PLAYER_COLORS[grid[gridIdx]], 0, 255);
    }
}

void Game_TTTT::loop() {
    if (state == TTTT_FINISHED && autoRestart) {
        if (millis() - finishedTime > 5000) { // 5 Sekunden Pause nach Sieg
            startGame();
        }
    }
}

void Game_TTTT::handleEvent(int puckIndex, EventPacket event) {
    if (event.type != EVT_BTN_CLICK || state != TTTT_RUNNING) return;
    
    // Außerhalb der konfigurierten Pucks ignorieren
    if (puckIndex >= numPucks) return;

    unsigned long now = millis();
    
    // 500ms Blockzeit (Debounce) pro Puck prüfen
    if (now - lastInput[puckIndex] < 500) return;
    lastInput[puckIndex] = now;

    // Beep Feedback
    CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 80;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);

    // Auswahl-Puck (Index 0) gedrückt = Nächster Spieler ist dran (Fehlwurf)
    if (puckIndex == 0) {
        nextPlayer();
        return;
    }
    
    // Spiel-Puck gedrückt (Index 1 bis N)
    int gridIdx = puckIndex - 1;
    
    if (hardMode) {
        // Im Hard-Mode wird bei JEDEM Treffer eines eingefärbten Pucks dieser neutral.
        if (grid[gridIdx] == -1) {
            grid[gridIdx] = currentPlayer; // War neutral -> Einfärben
        } else {
            grid[gridIdx] = -1; // War farbig -> Neutralisieren
        }
    } else {
        // Im Normal-Mode wird direkt überschrieben, ABER wenn ich meinen eigenen treffe, passiert nichts.
        if (grid[gridIdx] != currentPlayer) {
            grid[gridIdx] = currentPlayer;
        } else {
            return; // Eigener Puck, ignorieren
        }
    }
    
    applyGridColor(gridIdx);
    checkWin();
    
    // Wenn das Spiel noch läuft, Spieler wechseln, weil geworfen wurde.
    if (state == TTTT_RUNNING) {
        nextPlayer();
    }
}

void Game_TTTT::nextPlayer() {
    currentPlayer = (currentPlayer + 1) % numPlayers;
    updateSelectorPuck();
}

void Game_TTTT::checkWin() {
    bool hasWon = false;
    
    // Arrays zur Definition der Gewinn-Reihen je nach Modus
    // Index mapping: 0-8 entspricht Puck 1-9
    
    if (numPucks == 10) { // 3x3 TicTacToe (Indices 0 bis 8)
        int lines[8][3] = {
            {0,1,2}, {3,4,5}, {6,7,8}, // Horizontal
            {0,3,6}, {1,4,7}, {2,5,8}, // Vertikal
            {0,4,8}, {2,4,6}           // Diagonal
        };
        for(int i=0; i<8; i++) {
            if (grid[lines[i][0]] != -1 && 
                grid[lines[i][0]] == grid[lines[i][1]] && 
                grid[lines[i][1]] == grid[lines[i][2]]) {
                hasWon = true;
                winIndices[0] = lines[i][0]; winIndices[1] = lines[i][1]; winIndices[2] = lines[i][2];
                break;
            }
        }
    } 
    else if (numPucks == 7) { // 2x3 Grid (Indices 0 bis 5) -> Nur Horizontal
        int lines[2][3] = { {0,1,2}, {3,4,5} };
        for(int i=0; i<2; i++) {
            if (grid[lines[i][0]] != -1 && 
                grid[lines[i][0]] == grid[lines[i][1]] && 
                grid[lines[i][1]] == grid[lines[i][2]]) {
                hasWon = true;
                winIndices[0] = lines[i][0]; winIndices[1] = lines[i][1]; winIndices[2] = lines[i][2];
                break;
            }
        }
    }
    else if (numPucks == 6) { // 5 auf dem Würfel (Indices 0 bis 4) -> Nur Diagonal
        int lines[2][3] = { {0,2,4}, {1,2,3} };
        for(int i=0; i<2; i++) {
            if (grid[lines[i][0]] != -1 && 
                grid[lines[i][0]] == grid[lines[i][1]] && 
                grid[lines[i][1]] == grid[lines[i][2]]) {
                hasWon = true;
                winIndices[0] = lines[i][0]; winIndices[1] = lines[i][1]; winIndices[2] = lines[i][2];
                break;
            }
        }
    }
    else if (numPucks == 4) { // 1x3 Reihe (Indices 0 bis 2)
        if (grid[0] != -1 && grid[0] == grid[1] && grid[1] == grid[2]) {
            hasWon = true;
            winIndices[0] = 0; winIndices[1] = 1; winIndices[2] = 2;
        }
    }

    if (hasWon) {
        state = TTTT_FINISHED;
        finishedTime = millis();
        int winner = grid[winIndices[0]];
        
        // Alle Spielfeld-Pucks in der Farbe des Gewinners anzeigen (Solid)
        for(int i=1; i<numPucks; i++) {
            setPuck(i, EFF_STATIC, PLAYER_COLORS[winner], 0, 100);
        }
        
        // Die 3 Gewinner-Pucks blinken/Rainbow
        for(int i=0; i<3; i++) {
            setPuck(winIndices[i] + 1, EFF_RAINBOW, CRGB::White, 0, 255);
        }
        
        // Fanfare
        CommandPacket snd; snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_FANFARE;
        PuckNetwork::broadcast(snd);
    }
}

void Game_TTTT::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, cp);
}

String Game_TTTT::getStatusJSON() {
    String json = "{";
    json += "\"st\":" + String(state) + ",";
    json += "\"t\":" + String((state == TTTT_RUNNING) ? (millis() - runStartTime) : 0) + ",";
    json += "\"cp\":" + String(currentPlayer) + ",";
    
    // Zählen, wie viele Pucks jedem Spieler gehören
    int counts[3] = {0, 0, 0};
    int gridCount = numPucks - 1;
    for(int i=0; i<gridCount; i++) {
        if(grid[i] >= 0 && grid[i] < 3) counts[grid[i]]++;
    }
    
    json += "\"sc\":[" + String(counts[0]) + "," + String(counts[1]) + "," + String(counts[2]) + "]";
    json += "}";
    return json;
}