/*
 * PROJEKT: Puck Race - PUCK FIRMWARE
 * VERSION: 74 (FIX: CMD_PING Re-Pairing, SEQ_OFF stoppt Ton, EFF_BREATHE globale Helligkeit)
 *
 * ÄNDERUNGEN v73:
 *  [FIX 1] Race Condition: portMUX_TYPE Spinlock um alle cmdQueue-Zugriffe.
 *          OnDataRecv (Core 0) und processIncomingCommands (Core 1) griffen
 *          gleichzeitig auf den Ringpuffer zu -> halbüberschriebene Pakete
 *          -> Dauerton / eingefrorene LEDs.
 *  [FIX 2] tone(pin, freq, duration) ersetzt durch tone(pin, freq) ohne
 *          duration-Parameter. Der interne FreeRTOS-Timer von tone() kollidierte
 *          bei schnell aufeinanderfolgenden Sequenzen und brach diese ab.
 *          noTone() wird jetzt explizit von der Sound Engine aufgerufen.
 *  [FIX 3] lastReceivedSeq startet bei 255 statt 0, damit das erste empfangene
 *          Paket (seqNr=0) nicht fälschlicherweise als Duplikat verworfen wird.
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
#define PIN_LED     4
#define PIN_BTN     3 
#define PIN_BUZZER  5
#define NUM_LEDS    35
#define FW_VERSION  78

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
int currentRSSI = 0;
uint8_t globalSeqCounter = 0;

// FIX 3: Start bei 255 damit seqNr=0 (erstes Paket) nicht als Duplikat gilt
uint8_t lastReceivedSeq = 255;

// --- BEFEHLS-QUEUE (Ringpuffer) ---
#define CMD_QUEUE_SIZE 10
volatile CommandPacket cmdQueue[CMD_QUEUE_SIZE];
volatile int cmdHead = 0;
volatile int cmdTail = 0;

// FIX 1: Spinlock für thread-sicheren Queue-Zugriff zwischen Core 0 und Core 1
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
void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *data, int len);

void setup() {
    Serial.begin(115200);
    pinMode(PIN_BTN, INPUT_PULLUP);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    
    FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
    
    startSoundSequence(SEQ_MARIO);

    WiFi.mode(WIFI_STA); 
    WiFi.disconnect();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

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
    
    // Snapshot von cmdHead holen (ohne Lock reicht hier, da nur gelesen wird)
    if (cmdHead != cmdTail) { processIncomingCommands(); }

    handleButton();
    runAnimation();
    runSound(); 

    unsigned long now = millis();
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

// FIX 2: tone() ohne duration-Parameter - kein konkurrierender interner FreeRTOS-Timer
void runSound() {
    if (!sound.active) return;
    unsigned long now = millis();

    if (sound.isExplosion) {
        if (now >= sound.nextNoteTime) {
            if (sound.seqIndex++ < 100) { 
                tone(PIN_BUZZER, random(50, 400)); // Kein duration!
                sound.nextNoteTime = now + 5; 
            } else {
                noTone(PIN_BUZZER); // Explizit stoppen
                sound.active = false;
            }
        }
        return;
    }

    if (now >= sound.nextNoteTime) {
        if (sound.seqIndex < sound.seqLength) {
            int f = sound.currentSeq[sound.seqIndex].freq;
            int d = sound.currentSeq[sound.seqIndex].duration;
            
            if (f > 0) tone(PIN_BUZZER, f); // FIX 2: Kein duration-Parameter!
            else noTone(PIN_BUZZER);
            
            sound.nextNoteTime = now + d + 20; 
            sound.seqIndex++;
        } else {
            noTone(PIN_BUZZER); // FIX 2: Explizit stoppen
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
    noTone(PIN_BUZZER); // Zuerst alles stoppen (verhindert Timer-Konflikte)
    sound.active = true;
    sound.isExplosion = false;
    sound.seqIndex = 0;
    sound.seqLength = 0;
    sound.nextNoteTime = millis();

    switch(id) {
        case SEQ_OFF:
            // FIX 7: Explizit alles stoppen. Ohne diesen Case blieb ein
            // laufender Ton aktiv, weil sound.active=true gesetzt wurde
            // aber seqLength=0 blieb und nie noTone() aufgerufen wurde.
            sound.active = false;
            noTone(PIN_BUZZER);
            return; // Früh raus, kein weiteres Setup nötig
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
            // FIX 7: Placeholder – Noten können hier ergänzt werden.
            // Ohne diesen Case würde SEQ_TETRIS lautlos bleiben aber
            // sound.active=true lassen, was andere Töne unterdrückt.
            addNote(NOTE_E5, 120); addNote(NOTE_B4, 60);  addNote(NOTE_C5, 60);
            addNote(NOTE_D5, 120); addNote(NOTE_C5, 60);  addNote(NOTE_B4, 60);
            addNote(NOTE_A4, 120); addNote(NOTE_A4, 60);  addNote(NOTE_C5, 60);
            addNote(NOTE_E5, 120); addNote(NOTE_D5, 60);  addNote(NOTE_C5, 60);
            addNote(NOTE_B4, 180); addNote(NOTE_C5, 60);  addNote(NOTE_D5, 120);
            addNote(NOTE_E5, 120); addNote(NOTE_C5, 120); addNote(NOTE_A4, 120);
            break;
    }
}

// FIX 1: Spinlock um alle Queue-Zugriffe - verhindert Race Condition zwischen Core 0/1
void processIncomingCommands() {
    while (true) {
        // Kritischen Abschnitt so kurz wie möglich halten: nur lesen + Index vorrücken
        portENTER_CRITICAL(&queueMux);
        bool hasData = (cmdTail != cmdHead);
        CommandPacket cmd;
        if (hasData) {
            memcpy(&cmd, (void*)&cmdQueue[cmdTail], sizeof(CommandPacket));
            cmdTail = (cmdTail + 1) % CMD_QUEUE_SIZE;
        }
        portEXIT_CRITICAL(&queueMux);

        if (!hasData) break;

        // Verarbeitung außerhalb des kritischen Abschnitts
        if (cmd.cmd == CMD_PING) {
            // FIX 5: Coordinator hat neu gestartet → isPaired zurücksetzen,
            // damit der Puck sofort EVT_HELLO sendet und sich neu anmeldet.
            isPaired = false;
            lastHeartbeat = 0; // Sofort senden
            Serial.println("PING empfangen – Re-Pairing...");
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

// FIX 1: Spinlock beim Schreiben in die Queue (läuft auf Core 0 / WiFi-Task)
void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *data, int len) {
    if (info->rx_ctrl) currentRSSI = info->rx_ctrl->rssi;
    
    if (len == sizeof(CommandPacket)) {
        CommandPacket* incoming = (CommandPacket*)data;

        // Deduplication Check (lastReceivedSeq nur hier geschrieben -> kein Lock nötig)
        if (incoming->seqNr == lastReceivedSeq) {
            return;
        }
        lastReceivedSeq = incoming->seqNr;

        // FIX 1: Kritischer Abschnitt für Queue-Schreibzugriff
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
    pkg.battery_mv = 3700; 
    
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

    // FIX 3: Helligkeit IMMER sofort zurücksetzen, bevor der neue Effekt startet.
    // Ohne diesen Reset würde EFF_BREATHE (das FastLED.setBrightness() dynamisch
    // verändert) die globale Helligkeit für alle nachfolgenden Effekte korrumpieren.
    FastLED.setBrightness(bright);
    
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
            // FIX 3: nscale8() auf die LED-Objekte statt FastLED.setBrightness(),
            // damit die globale Helligkeit nicht für andere Effekte korrumpiert wird.
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
