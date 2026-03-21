/*
 * PROJEKT: Puck Race - PUCK FIRMWARE
 * Version 81 stability fixes, bidirectional heartbeats
 * Version: 80 Battery Update + Sound Fixes + Core 3.x Support
 * VERSION: 75 (FIX: ESP-NOW Core 2.x Kompatibilität + Promiscuous RSSI Sniffer)
 *
 * ÄNDERUNGEN v75:
 * [FIX] esp_now_recv_cb_t Signatur auf Core 2.x Standard angepasst, um
 * Kompilierungsfehler im PlatformIO esp32-c3 Environment zu beheben.
 * [NEW] Promiscuous Sniffer hinzugefügt, um den RSSI-Wert auszulesen,
 * da der Core 2.x ESP-NOW Callback dies nicht nativ unterstützt.
 *
 * ÄNDERUNGEN v73/74:
 * [FIX 1] Race Condition: portMUX_TYPE Spinlock um alle cmdQueue-Zugriffe.
 * [FIX 2] tone() ohne duration-Parameter gegen FreeRTOS Timer Konflikte.
 * [FIX 3] lastReceivedSeq startet bei 255.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <FastLED.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include "Common.h"

// --- HARDWARE ---
#define PIN_BAT     2  // ADC Pin für den Batterie-Spannungsteiler
#define PIN_LED     4
#define PIN_BTN     3 
#define PIN_BUZZER  5
#define NUM_LEDS    35
#define FW_VERSION  81

// --- AUDIO NOTEN ---
#define NOTE_B0  31
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_G5  784
#define NOTE_A5  880
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_E6  1319

CRGB leds[NUM_LEDS];
const uint8_t broadcastMac[] = BROADCAST_MAC;

// --- STATUS ---
bool isPaired = false;
bool updateRequested = false;
unsigned long lastHeartbeat = 0;
String updateSSID = "";
String updatePW = "";

// STABILITÄTS-FIX: Zeitpunkt des letzten empfangenen Coordinator-Befehls.
// Wird bei jedem eingehenden Kommando (CMD_KEEPALIVE, CMD_EFFECT, etc.) aktualisiert.
// Wenn dieser Wert > 15 Sekunden alt ist, geht der Puck davon aus, dass die
// Verbindung verloren wurde, und wechselt auf roten Status + schnelleres Re-Pairing.
unsigned long lastCommandFromCoordinator = 0;

// STABILITÄTS-FIX: Timeout-Schwelle in ms, ab der der Puck die Verbindung
// als verloren betrachtet. Der Coordinator sendet alle 10s einen CMD_KEEPALIVE,
// also sollte bei stabiler Verbindung nie ein 15s-Timeout auftreten.
#define COORDINATOR_TIMEOUT_MS 15000

// GLOBALE RSSI VARIABLE (wird nun vom Sniffer befüllt)
volatile int currentRSSI = 0;

uint8_t globalSeqCounter = 0;
uint8_t lastReceivedSeq = 255;

// --- BEFEHLS-QUEUE (Ringpuffer) ---
#define CMD_QUEUE_SIZE 10
volatile CommandPacket cmdQueue[CMD_QUEUE_SIZE];
volatile int cmdHead = 0;
volatile int cmdTail = 0;

portMUX_TYPE queueMux = portMUX_INITIALIZER_UNLOCKED;

// --- BUTTON (GLITCH FILTER) ---
struct {
    bool state;
    bool lastReading;
    unsigned long lastDebounceTime;
} button = {false, false, 0};

// --- ANIMATION ENGINE ---
struct {
    uint8_t id = EFF_OFF;
    CRGB color1 = CRGB::Black;
    int speed = 0;
    uint8_t brightness = 50;
    unsigned long lastUpdate = 0;
    int step = 0;
    int counter = 0; 
} anim;

// --- SOUND ENGINE ---
struct Note {
    int freq;
    int duration;
};
#define MAX_SEQ_LEN 30 

struct {
    bool active = false;
    bool isExplosion = false; 
    int seqIndex = 0;
    int seqLength = 0;
    unsigned long nextNoteTime = 0;
    Note currentSeq[MAX_SEQ_LEN];
} sound;

// --- PROTOTYPEN ---
void runAnimation();
void runSound();
void startSoundSequence(uint8_t id);
void setEffect(uint8_t id, uint8_t r, uint8_t g, uint8_t b, int speed, uint8_t bright, uint8_t extra);
void handleButton();
void processIncomingCommands();
void performOTA();
void sendEvent(uint8_t type);

// FIX: Signatur für ESP-NOW Empfang (Unterstützt Core 2.x und 3.x)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *data, int len);
#else
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *data, int len); 
#endif


// =========================================================================
// NEU: RSSI SNIFFER FÜR CORE 2.X
// Belauscht die WLAN Pakete im Hintergrund, um die Signalstärke zu lesen
// =========================================================================
void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    // Prüfen, ob das Paket lang genug ist (MAC-Header)
    if (pkt->rx_ctrl.sig_len >= 24) { 
        // Lese die Sender-MAC (Offset 10 im 802.11 Header)
        uint8_t *mac = pkt->payload + 10; 
        
        // Da wir nur vom Coordinator Pakete annehmen, reicht es, wenn wir
        // bei jedem Management/Data Paket die RSSI Variable aktualisieren.
        // Ein genauerer MAC-Filter ist für dieses Projekt nicht zwingend nötig.
        currentRSSI = pkt->rx_ctrl.rssi;
    }
}


void setup() {
    Serial.begin(115200);
    
    // Initialisiere ADC Pin (nicht zwingend nötig für analogRead, aber sauberer)
    pinMode(PIN_BAT, INPUT);
    
    pinMode(PIN_BTN, INPUT_PULLUP);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    
    FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
    
    startSoundSequence(SEQ_MARIO);

    // WIFI SETUP
    WiFi.mode(WIFI_STA); 
    WiFi.disconnect();
    
    // RSSI Sniffer: Callback registrieren, aber standardmäßig DEAKTIVIERT lassen.
    // -------------------------------------------------------------------------
    // STABILITÄTS-FIX: Der Promiscuous Mode war bisher permanent aktiv. Das bedeutet,
    // dass JEDES WiFi-Paket in der Luft (auch von fremden Netzwerken, Handys, etc.)
    // einen ISR-Callback auf dem Puck auslöst. Bei 4 Pucks in einer WiFi-dichten
    // Umgebung erzeugt das erhebliche CPU-Last und kann dazu führen, dass ESP-NOW
    // Pakete (Heartbeats, Button-Events) verloren gehen.
    //
    // Der Sniffer wird jetzt nur noch aktiviert, wenn EFF_STATUS gesetzt wird
    // (dort wird currentRSSI für die Rot/Grün-Anzeige gebraucht), und bei allen
    // anderen Effekten wieder deaktiviert. Siehe setEffect().
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false); // Standardmäßig AUS → weniger CPU-Last

    if (esp_now_init() != ESP_OK) ESP.restart();
    esp_now_register_recv_cb(OnDataRecv);
    
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastMac, 6);
    peerInfo.channel = WIFI_CHANNEL; 
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    button.state = (digitalRead(PIN_BTN) == LOW);
    button.lastReading = button.state;
    button.lastDebounceTime = millis();
}

void loop() {
    if (updateRequested) { performOTA(); return; }
    
    if (cmdHead != cmdTail) { processIncomingCommands(); }

    handleButton();
    runAnimation();
    runSound(); 

    unsigned long now = millis();

    // =========================================================================
    // DISCONNECT-ERKENNUNG: Coordinator → Puck Verbindungsüberwachung
    // =========================================================================
    // Der Coordinator sendet alle 10s einen CMD_KEEPALIVE Broadcast.
    // Wenn wir seit COORDINATOR_TIMEOUT_MS (15s) keinen einzigen Befehl mehr
    // empfangen haben, ist die Verbindung sehr wahrscheinlich unterbrochen.
    //
    // Reaktion:
    //   1. LED auf ROT setzen → visuelles Feedback für den Benutzer
    //   2. isPaired = false → Puck sendet jetzt alle 2s EVT_HELLO statt
    //      alle 5s EVT_HEARTBEAT, was das Re-Pairing beschleunigt
    //   3. Promiscuous Sniffer deaktivieren → CPU-Last reduzieren
    //
    // lastCommandFromCoordinator == 0 bedeutet: noch nie ein Befehl empfangen
    // (Startup-Phase), dann wird kein Timeout ausgelöst.
    if (isPaired && lastCommandFromCoordinator > 0 &&
        now - lastCommandFromCoordinator > COORDINATOR_TIMEOUT_MS) {

        Serial.printf("DISCONNECT: Kein Coordinator-Signal seit %lums → Verbindung verloren!\n",
                      now - lastCommandFromCoordinator);
        isPaired = false;
        lastHeartbeat = 0;  // Sofort EVT_HELLO senden beim nächsten Loop-Durchlauf

        // Visuelles Feedback: Status-LED auf ROT
        setEffect(EFF_STATUS, 255, 0, 0, 0, 255, 0);

        // Sniffer aus (spart CPU während der Reconnect-Phase)
        esp_wifi_set_promiscuous(false);
    }

    if (!isPaired) {
        if (now - lastHeartbeat > 2000) {
            sendEvent(EVT_HELLO);
            lastHeartbeat = now;
        }
    } else {
        if (now - lastHeartbeat > 5000) {
            sendEvent(EVT_HEARTBEAT);
            lastHeartbeat = now;
        }
    }
}

void runSound() {
    if (!sound.active) return;
    unsigned long now = millis();

    if (sound.isExplosion) {
        if (now >= sound.nextNoteTime) {
            if (sound.seqIndex++ < 100) { 
                tone(PIN_BUZZER, random(50, 400)); 
                sound.nextNoteTime = now + 5; 
            } else {
                noTone(PIN_BUZZER); 
                sound.active = false;
            }
        }
        return;
    }

    if (now >= sound.nextNoteTime) {
        if (sound.seqIndex < sound.seqLength) {
            int f = sound.currentSeq[sound.seqIndex].freq;
            int d = sound.currentSeq[sound.seqIndex].duration;
            
            if (f > 0) tone(PIN_BUZZER, f); 
            else noTone(PIN_BUZZER);
            
            sound.nextNoteTime = now + d + 20; 
            sound.seqIndex++;
        } else {
            noTone(PIN_BUZZER); 
            sound.active = false;
        }
    }
}

void addNote(int freq, int dur) {
    if(sound.seqLength < MAX_SEQ_LEN) {
        sound.currentSeq[sound.seqLength].freq = freq;
        sound.currentSeq[sound.seqLength].duration = dur;
        sound.seqLength++;
    }
}

void startSoundSequence(uint8_t id) {
    noTone(PIN_BUZZER); 
    sound.active = true;
    sound.isExplosion = false;
    sound.seqIndex = 0;
    sound.seqLength = 0;
    sound.nextNoteTime = millis();

    switch(id) {
        case SEQ_OFF:
            sound.active = false;
            noTone(PIN_BUZZER);
            return; 
        case SEQ_FANFARE: 
            addNote(NOTE_C5, 100); addNote(NOTE_E5, 100); addNote(NOTE_G5, 100);
            addNote(0, 50); addNote(NOTE_C6, 400);
            break;
        case SEQ_ERROR: 
            addNote(NOTE_G4, 400);
            break;
        case SEQ_EXPLOSION:
            sound.isExplosion = true;
            break;
        case SEQ_HERO: 
            addNote(NOTE_C4, 200); addNote(NOTE_G4, 200); addNote(NOTE_C5, 600);
            break;
        case SEQ_MARIO: 
            addNote(NOTE_E5, 100); addNote(0, 50); addNote(NOTE_E5, 100); 
            addNote(0, 100); addNote(NOTE_E5, 100); addNote(0, 100);
            addNote(NOTE_C5, 100); addNote(NOTE_E5, 100);
            break;
        case SEQ_RACE: 
            addNote(NOTE_C4, 400); addNote(0, 400); 
            addNote(NOTE_C4, 400); addNote(0, 400); 
            addNote(NOTE_C5, 800); 
            break;
        case SEQ_SKI: 
            addNote(600, 100); addNote(0, 900);
            addNote(600, 100); addNote(0, 900);
            addNote(600, 100); addNote(0, 900);
            addNote(1200, 800);
            break;
        case SEQ_DINGDONG:
            addNote(NOTE_E5, 150); addNote(NOTE_C6, 400);
            break;
        case SEQ_TETRIS:
            addNote(NOTE_E5, 120); addNote(NOTE_B4, 60);  addNote(NOTE_C5, 60);
            addNote(NOTE_D5, 120); addNote(NOTE_C5, 60);  addNote(NOTE_B4, 60);
            addNote(NOTE_A4, 120); addNote(NOTE_A4, 60);  addNote(NOTE_C5, 60);
            addNote(NOTE_E5, 120); addNote(NOTE_D5, 60);  addNote(NOTE_C5, 60);
            addNote(NOTE_B4, 180); addNote(NOTE_C5, 60);  addNote(NOTE_D5, 120);
            addNote(NOTE_E5, 120); addNote(NOTE_C5, 120); addNote(NOTE_A4, 120);
            break;
    }
}

void processIncomingCommands() {
    while (true) {
        portENTER_CRITICAL(&queueMux);
        bool hasData = (cmdTail != cmdHead);
        CommandPacket cmd;
        if (hasData) {
            memcpy(&cmd, (void*)&cmdQueue[cmdTail], sizeof(CommandPacket));
            cmdTail = (cmdTail + 1) % CMD_QUEUE_SIZE;
        }
        portEXIT_CRITICAL(&queueMux);

        if (!hasData) break;

        // STABILITÄTS-FIX: Bei JEDEM empfangenen Befehl den Zeitstempel aktualisieren.
        // So weiß der Puck, dass der Coordinator noch erreichbar ist.
        // Wird in loop() gegen COORDINATOR_TIMEOUT_MS geprüft.
        lastCommandFromCoordinator = millis();

        if (cmd.cmd == CMD_PING) {
            isPaired = false;
            lastHeartbeat = 0;
            Serial.println("PING empfangen – Re-Pairing...");
        }
        else if (cmd.cmd == CMD_KEEPALIVE) {
            // Bidirektionaler Heartbeat vom Coordinator (alle ~10 Sekunden).
            // Hauptzweck: lastCommandFromCoordinator wird oben bereits aktualisiert.
            // Keine weitere Aktion nötig – der Puck weiß jetzt, dass die Verbindung steht.
        }
        else if (cmd.cmd == CMD_PAIR_ACK) {
            if (!isPaired) isPaired = true;
        }
        else if (cmd.cmd == CMD_EFFECT) {
        if (cmd.effectID != EFF_FLASH && cmd.effectID != EFF_STATIC && cmd.effectID != EFF_STATUS && cmd.effectID != EFF_FLASH_BEEP) {
                if (anim.id == cmd.effectID &&
                    anim.color1.r == cmd.r && anim.color1.g == cmd.g && anim.color1.b == cmd.b &&
                    anim.speed == cmd.duration &&
                    anim.brightness == cmd.extra) {
                    continue;
                }
            }
            setEffect(cmd.effectID, cmd.r, cmd.g, cmd.b, cmd.duration, cmd.extra, cmd.extra);
        }
        else if (cmd.cmd == CMD_SOUND) {
            sound.active = true; sound.isExplosion = false;
            sound.seqIndex = 0; sound.seqLength = 0;
            addNote(600, cmd.duration); 
            sound.nextNoteTime = millis();
        }
        else if (cmd.cmd == CMD_SEQUENCE) {
            startSoundSequence(cmd.extra); 
        }
        else if (cmd.cmd == CMD_RESET) {
            ESP.restart();
        }
    }
}


// =========================================================================
// ESP-NOW CALLBACK (Universal für Core 2.x und 3.x)
// Nutzt die passende Signatur. Der RSSI Wert wird vom Promiscuous Sniffer geliefert.
// =========================================================================
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *data, int len) {
#else
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *data, int len) {
#endif

    if (len == sizeof(CommandPacket)) {
        CommandPacket* incoming = (CommandPacket*)data;

        if (incoming->seqNr == lastReceivedSeq) {
            return;
        }
        lastReceivedSeq = incoming->seqNr;

        portENTER_CRITICAL(&queueMux);
        int nextHead = (cmdHead + 1) % CMD_QUEUE_SIZE;
        if (nextHead != cmdTail) {
            memcpy((void*)&cmdQueue[cmdHead], data, sizeof(CommandPacket));
            cmdHead = nextHead;
        }
        portEXIT_CRITICAL(&queueMux);
    }
    else if (len == sizeof(UpdateCredentialsPacket)) {
        UpdateCredentialsPacket* creds = (UpdateCredentialsPacket*)data;
        if (creds->cmd == CMD_UPDATE_MODE) {
            updateSSID = String(creds->ssid);
            updatePW = String(creds->password);
            setEffect(EFF_STATIC, 0, 0, 255, 0, 100, 0); 
            updateRequested = true;
        }
    }
}

void sendEvent(uint8_t type) {
    EventPacket pkg;
    pkg.type = type;
    pkg.version = FW_VERSION; 
    
    // BERECHNUNG DER BATTERIESPANNUNG
    // Liest den analogen Wert in Millivolt. Da der Spannungsteiler (100k/100k) 
    // die Spannung halbiert, müssen wir den gemessenen Wert mit 2 multiplizieren.
    uint32_t adc_mv = analogReadMilliVolts(PIN_BAT);
    pkg.battery_mv = (uint16_t)(adc_mv * 2); 
    
    globalSeqCounter++;
    pkg.seqNr = globalSeqCounter;

    if (type == EVT_BTN_CLICK || type == EVT_BTN_DOUBLE || type == EVT_BTN_HOLD || type == EVT_BTN_RELEASE) {
        for(int i=0; i<3; i++) {
            esp_now_send(broadcastMac, (uint8_t *) &pkg, sizeof(pkg));
            delay(2); 
        }
    } else {
        for(int i=0; i<2; i++) {
            esp_now_send(broadcastMac, (uint8_t *) &pkg, sizeof(pkg));
            delay(2);
        }
    }
}

void handleButton() {
    bool reading = (digitalRead(PIN_BTN) == LOW);

    if (reading != button.lastReading) {
        button.lastDebounceTime = millis();
    }
    button.lastReading = reading;

    if ((millis() - button.lastDebounceTime) > 15) {
        if (reading != button.state) {
            button.state = reading;
            
            if (button.state == true) {
                sendEvent(EVT_BTN_CLICK);
            } else {
                sendEvent(EVT_BTN_RELEASE);
            }
        }
    }
}

void setEffect(uint8_t id, uint8_t r, uint8_t g, uint8_t b, int speed, uint8_t bright, uint8_t extra) {
    anim.id = id;
    anim.color1 = CRGB(r, g, b);
    anim.speed = speed;
    anim.brightness = bright;
    anim.step = 0;
    anim.counter = extra;

    FastLED.setBrightness(bright);

    // STABILITÄTS-FIX: Promiscuous Sniffer nur bei EFF_STATUS aktivieren.
    // EFF_STATUS nutzt currentRSSI (Zeile in runAnimation), um bei schwachem
    // Signal die LED rot statt grün anzuzeigen. Für alle anderen Effekte wird
    // der Sniffer deaktiviert, um CPU-Last zu reduzieren und ESP-NOW Stabilität
    // zu verbessern.
    if (id == EFF_STATUS) {
        esp_wifi_set_promiscuous(true);
    } else {
        esp_wifi_set_promiscuous(false);
    }

    if (id == EFF_FLASH_BEEP) {
        startSoundSequence(SEQ_DINGDONG);
        fill_solid(leds, NUM_LEDS, anim.color1);
        FastLED.show();
        anim.lastUpdate = millis();
    }
    else if (id != EFF_STATIC && id != EFF_STATUS && id != EFF_PROGRESS) {
        FastLED.clear();
    }
}

void runAnimation() {
    unsigned long now = millis();
    
    if (anim.id == EFF_FLASH_BEEP) {
        if (now - anim.lastUpdate > (unsigned long)anim.speed) {
            anim.id = EFF_OFF;
            FastLED.clear(); FastLED.show();
        }
        return;
    }

    if (anim.speed > 0 && now - anim.lastUpdate < (unsigned long)anim.speed && anim.id != EFF_PROGRESS) return;
    anim.lastUpdate = now;

    switch (anim.id) {
        case EFF_OFF: FastLED.clear(); break;
        case EFF_STATIC: fill_solid(leds, NUM_LEDS, anim.color1); break;
        case EFF_BLINK: 
            anim.step = !anim.step; 
            fill_solid(leds, NUM_LEDS, anim.step ? anim.color1 : CRGB::Black); 
            break;
        case EFF_FLASH: 
            if (anim.step % 4 == 0) fill_solid(leds, NUM_LEDS, anim.color1);
            else FastLED.clear();
            anim.step++; 
            break;    
        case EFF_BLINK_COUNT:
            if (anim.counter > 0) {
                anim.step = !anim.step; 
                fill_solid(leds, NUM_LEDS, anim.step ? anim.color1 : CRGB::Black);
                if (!anim.step) anim.counter--; 
            } else {
                FastLED.clear(); 
            }
            break;
        case EFF_STATUS: 
            FastLED.clear();
            {
                CRGB statusColor = anim.color1;
                if(currentRSSI < -80 && currentRSSI != 0) statusColor = CRGB::Red;
                leds[0] = statusColor; leds[0].nscale8(85);
                leds[1] = statusColor; leds[1].nscale8(170);
                leds[2] = statusColor; leds[2].nscale8(85);
            }
            break;
        case EFF_PROGRESS: 
            {   
                int val = anim.speed; 
                if(val > 255) val = 255;
                int numLit = (val * NUM_LEDS) / 255;
                FastLED.clear();
                for(int i=0; i<numLit; i++) leds[i] = anim.color1;
                break; 
            }
        case EFF_BREATHE: { 
            uint8_t val = (uint8_t)((exp(sin(anim.step/50.0*PI)) - 0.36787944)*108.0);
            fill_solid(leds, NUM_LEDS, anim.color1);
            nscale8(leds, NUM_LEDS, val);
            anim.step++; break; 
        }
        case EFF_BREATHE_MOD2: { 
            uint8_t val = (uint8_t)((exp(sin(anim.step/50.0*PI)) - 0.36787944)*108.0);
            FastLED.clear();
            for(int i=0; i<NUM_LEDS; i+=2) leds[i] = anim.color1;
            nscale8(leds, NUM_LEDS, val);
            anim.step++; break;
        }
        case EFF_BREATHE_MOD4: { 
            uint8_t val = (uint8_t)((exp(sin(anim.step/50.0*PI)) - 0.36787944)*108.0);
            FastLED.clear();
            for(int i=0; i<NUM_LEDS; i+=4) leds[i] = anim.color1;
            nscale8(leds, NUM_LEDS, val);
            anim.step++; break;
        }
        case EFF_SINGLE_CHASE: 
            fadeToBlackBy(leds, NUM_LEDS, 64); leds[anim.step % NUM_LEDS] = anim.color1; anim.step++; break;
        case EFF_DOUBLE_CHASE: 
            fadeToBlackBy(leds, NUM_LEDS, 64); leds[anim.step % NUM_LEDS] = anim.color1; leds[(anim.step + NUM_LEDS/2) % NUM_LEDS] = anim.color1; anim.step++; break;
        case EFF_SPLIT_ROT: 
            FastLED.clear(); for(int i=0; i<NUM_LEDS/2; i++) { int idx = (anim.step + i) % NUM_LEDS; leds[idx] = anim.color1; } anim.step++; break;
        case EFF_RAINBOW: 
            fill_rainbow(leds, NUM_LEDS, anim.step, 7); anim.step++; break;
        case EFF_COUNTDOWN: 
            { int visible = NUM_LEDS - (anim.step % NUM_LEDS); FastLED.clear(); for(int i=0; i<visible; i++) leds[i] = anim.color1; anim.step++; break; }
        case EFF_LOADING: 
            { fadeToBlackBy(leds, NUM_LEDS, 40); int pos = beatsin16(30, 0, NUM_LEDS-1); leds[pos] = anim.color1; break; }
        case EFF_WIN: 
            fill_solid(leds, NUM_LEDS, CRGB::DarkGreen); if(random8() < 40) leds[random16(NUM_LEDS)] = CRGB::White; break;
        case EFF_FAIL: 
            fill_solid(leds, NUM_LEDS, CRGB::Red); if(random8() < 80) FastLED.clear(); break;
        case EFF_SPARKLE: 
            fill_solid(leds, NUM_LEDS, anim.color1); if(random8() < 30) leds[random16(NUM_LEDS)] = CRGB::White; break;
        case EFF_POLICE: 
            if ((millis() / 200) % 2 == 0) fill_solid(leds, NUM_LEDS, CRGB::Red); else fill_solid(leds, NUM_LEDS, CRGB::Blue); break;
    }
    FastLED.show();
}

void performOTA() {
    Serial.println("\n--- OTA START ---");
    fill_solid(leds, NUM_LEDS, CRGB::Purple); FastLED.show();
    
    esp_now_deinit(); 
    WiFi.disconnect();
    delay(100);

    if(updateSSID == "") { updateSSID = "PuckRace_Trainer"; updatePW = ""; }
    
    WiFi.mode(WIFI_STA);
    if (updatePW.length() > 0) WiFi.begin(updateSSID.c_str(), updatePW.c_str());
    else WiFi.begin(updateSSID.c_str(), NULL);
    
    unsigned long start = millis();
    bool connected = false;
    
    while (millis() - start < 20000) { 
        if(WiFi.status() == WL_CONNECTED) { connected = true; break; }
        leds[0] = (millis() % 200 < 100) ? CRGB::Purple : CRGB::Black;
        FastLED.show();
        delay(100);
    }
    
    if (connected) {
        fill_solid(leds, NUM_LEDS, CRGB::Blue); FastLED.show();
        delay(1000); 
        WiFiClient client;
        client.setTimeout(15000); 
        String serverIP = WiFi.gatewayIP().toString();
        String url = "http://" + serverIP + "/puck_update.bin";
        fill_solid(leds, NUM_LEDS, CRGB::Yellow); FastLED.show();
        t_httpUpdate_return ret = httpUpdate.update(client, url);
        if (ret == HTTP_UPDATE_OK) {
            fill_solid(leds, NUM_LEDS, CRGB::Green); FastLED.show();
            delay(2000);
            ESP.restart(); 
        } else {
            fill_solid(leds, NUM_LEDS, CRGB::Red); FastLED.show(); delay(5000);
            ESP.restart();
        }
    } else {
        fill_solid(leds, NUM_LEDS, CRGB::Red); FastLED.show(); delay(5000);
        ESP.restart();
    }
}