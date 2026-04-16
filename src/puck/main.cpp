/*
 * PROJEKT: Puck Race - PUCK FIRMWARE
 * Version 87 LED 15 FPS Cap + Heartbeat/LED Timing-Trennung gegen Flackern
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
#include <esp_task_wdt.h>
#include <math.h>
#include "Common.h"

// Temporär zum Deaktivieren des NTC-Sensors für alte Pucks ohne Sensor.
// Wenn diese Zeile einkommentiert ist, wird die Temperaturmessung übersprungen und immer -999 (Fehler) zurückgegeben.
#define DISABLE_TEMP_SENSOR

// --- HARDWARE ---
#define PIN_BAT     0  // ADC Pin für den Batterie-Spannungsteiler
// GPIO1 ist der letzte freie ADC1-Pin für weitere analoge Sensoren
#define PIN_NTC     6 //3  // NTC Temperatursensor (10kOhm Beta3950, 10kOhm Festwiderstand)
#define PIN_LED     4
#define PIN_BUZZER  5
#define PIN_BTN     3 //6  // Arcade Button (INPUT_PULLUP)
#define NUM_LEDS    35
#define FW_VERSION  88

// --- TEMPERATUR OVERHEAT ---
// Schwellwert in °C – ab diesem Wert wird Overheat-Schutz ausgelöst.
// Zum Feintuning diesen Wert anpassen:
#define TEMP_OVERHEAT_C         60
// NTC Parameter: 10kOhm bei 25°C, Beta 3950, Festwiderstand 10kOhm
#define NTC_R_FIXED             10000.0
#define NTC_R_NOMINAL           10000.0
#define NTC_BETA                3950.0
#define NTC_T_NOMINAL           298.15   // 25°C in Kelvin
#define TEMP_CHECK_INTERVAL_MS  2000
#define OVERHEAT_BEEP_DURATION_MS 30000  // 30 Sekunden aggressives Beepen

// --- BATTERIE ---
#define BAT_CALIBRATION       1.0    // Platzhalter: Feinabstimmung nach Messung (z.B. 1.02)
#define BAT_LOW_BOOT_MV       3700   // Boot-Schwelle: darunter → Notaus
#define BAT_WARNING_MV        3600   // Warnung: darunter → Battery Save (50% Helligkeit)
#define BAT_CRITICAL_MV       3400   // Laufzeit-Schwelle: darunter → Notaus
#define BAT_CHECK_INTERVAL_MS 2000   // Prüfintervall in ms
#define BAT_SAVE_BRIGHTNESS   128    // 50% max Helligkeit im Battery Save Modus

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
uint8_t coordinatorMac[6] = {0};
bool coordinatorMacKnown = false;
String updateSSID = "";
String updatePW = "";

// STABILITÄTS-FIX: Zeitpunkt des letzten empfangenen Coordinator-Befehls.
// Wird bei jedem eingehenden Kommando (CMD_KEEPALIVE, CMD_EFFECT, etc.) aktualisiert.
// Wenn dieser Wert > 15 Sekunden alt ist, geht der Puck davon aus, dass die
// Verbindung verloren wurde, und wechselt auf roten Status + schnelleres Re-Pairing.
unsigned long lastCommandFromCoordinator = 0;

// --- BATTERIE SCHUTZ ---
bool lowBatteryMode = false;
bool batterySaveMode = false;
unsigned long lastBatteryCheck = 0;
#define BAT_AVG_SAMPLES 5
uint16_t batHistory[BAT_AVG_SAMPLES] = {0};
uint8_t  batHistoryIdx = 0;
uint8_t  batHistoryCount = 0;

// --- TEMPERATUR ---
int16_t currentTemp_c10 = -999;  // Aktuelle Temperatur in 0.1°C, -999 = kein Sensor
unsigned long lastTempCheck = 0;
bool overheatMode = false;
unsigned long overheatStartTime = 0;
unsigned long overheatLastToggle = 0;
bool overheatBuzzerOn = false;

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

// --- BRIGHTNESS LIMIT ---
uint8_t maxBrightnessPercent = 100;  // Vom Coordinator einstellbar (10-100%)

// --- LED TIMING ---
// Globales 15 FPS Cap: Minimales Intervall zwischen FastLED.show() Aufrufen.
// Reduziert die Kollisionswahrscheinlichkeit mit ESP-NOW auf dem Single-Core C3.
#define LED_MIN_FRAME_MS    67   // ~15 FPS max
// Nach FastLED.show() warten wir LED_GUARD_MS bevor wir ESP-NOW senden,
// und senden Heartbeats bevorzugt in der Mitte des Frame-Gaps.
#define LED_GUARD_MS        5
#define HEARTBEAT_OFFSET_MS 30   // Heartbeat ~30ms nach letztem show()
unsigned long lastShowTime = 0;  // Zeitpunkt des letzten FastLED.show()

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
    if (!coordinatorMacKnown) return;
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    if (pkt->rx_ctrl.sig_len >= 24) {
        // Sender-MAC (Offset 10 im 802.11 Header)
        uint8_t *mac = pkt->payload + 10;
        // Nur Pakete vom Coordinator akzeptieren
        if (memcmp(mac, coordinatorMac, 6) == 0) {
            currentRSSI = pkt->rx_ctrl.rssi;
        }
    }
}

uint16_t readBatteryMV() {
    uint32_t adc_mv = analogReadMilliVolts(PIN_BAT);
    // Wenn die direkt am ADC gemessene Spannung unter 1.1V liegt, gehen wir davon aus,
    // dass kein Spannungsteiler verbaut ist. Der Pin floatet dann meist auf einem
    // niedrigen Level. In diesem Fall geben wir 0 zurück. Die bestehende Logik
    // (if avgMv < 1000) wird dies als "kein Sensor" erkennen und die Batterie-
    // überwachung für diesen Puck deaktivieren.
    if (adc_mv < 1100) {
        return 0;
    }
    return (uint16_t)(adc_mv * 2 * BAT_CALIBRATION);
}

// NTC Temperatur lesen: Spannungsteiler Vcc -> R_fixed -> ADC -> NTC -> GND
// Rückgabe in 0.1°C Einheiten (z.B. 253 = 25.3°C), -999 bei Fehler
int16_t readTemperature() {
    // Wenn für alte Pucks deaktiviert, immer -999 (kein Sensor) zurückgeben
    #ifdef DISABLE_TEMP_SENSOR
        return -999;
    #endif

    uint32_t adc_mv = analogReadMilliVolts(PIN_NTC);
    if (adc_mv < 10 || adc_mv > 3290) return -999;  // Sensor nicht angeschlossen oder Kurzschluss

    double r_ntc = NTC_R_FIXED * (double)adc_mv / (3300.0 - (double)adc_mv);
    // Steinhart-Hart vereinfacht (Beta-Gleichung):
    // 1/T = 1/T0 + (1/B) * ln(R/R0)
    double steinhart = log(r_ntc / NTC_R_NOMINAL) / NTC_BETA;
    steinhart += 1.0 / NTC_T_NOMINAL;
    double tempK = 1.0 / steinhart;
    double tempC = tempK - 273.15;
    return (int16_t)(tempC * 10.0);
}

void setup() {
    // Buzzer sofort aus (falls Watchdog-Reset während aktivem Ton)
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    noTone(PIN_BUZZER);

    Serial.begin(115200);

    // Initialisiere ADC Pins (nicht zwingend nötig für analogRead, aber sauberer)
    pinMode(PIN_BAT, INPUT);
    pinMode(PIN_NTC, INPUT);

    pinMode(PIN_BTN, INPUT_PULLUP);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    
    FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 1500); // Sicherheitslimit: max 1,5A bei 5V (real <1A, FastLED schätzt konservativ)

    // --- BATTERIE BOOT-CHECK ---
    {
        uint32_t sum = 0;
        for (int i = 0; i < 8; i++) { sum += readBatteryMV(); delay(5); }
        uint16_t avgMv = sum / 8;
        Serial.printf("Boot battery: %u mV\n", avgMv);
        if (avgMv < 1000) {
            Serial.println("No battery sensor detected – skipping battery protection");
        } else if (avgMv < BAT_CRITICAL_MV) {
            lowBatteryMode = true;
            // Countdown: 35 → 0 LEDs in 1 Sekunde
            for (int n = NUM_LEDS; n >= 0; n--) {
                FastLED.clear();
                for (int j = 0; j < n; j++) leds[j] = CRGB::Red;
                FastLED.show();
                delay(1000 / (NUM_LEDS + 1));
            }
            // 2x rot blinken
            for (int b = 0; b < 2; b++) {
                fill_solid(leds, NUM_LEDS, CRGB::Red); FastLED.show(); delay(150);
                FastLED.clear(); FastLED.show(); delay(150);
            }
            Serial.println("LOW BATTERY MODE at boot!");
        } else if (avgMv < BAT_LOW_BOOT_MV) {
            batterySaveMode = true;
            Serial.printf("Battery Save Mode at boot (%u mV)\n", avgMv);
        }
    }

    if (!lowBatteryMode) startSoundSequence(SEQ_MARIO);

    // WATCHDOG: 3s Timeout, automatischer Reboot bei Hänger
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 3000,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);

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
    esp_task_wdt_reset();
    if (updateRequested) { performOTA(); return; }
    
    if (cmdHead != cmdTail) { processIncomingCommands(); }

    handleButton();
    runAnimation();
    runSound(); 

    unsigned long now = millis();

    // --- RUNTIME LOW BATTERY CHECK (alle 2s, Entscheidung nach 5 Messungen = 10s Mittelwert) ---
    if (!lowBatteryMode && now - lastBatteryCheck >= BAT_CHECK_INTERVAL_MS) {
        lastBatteryCheck = now;
        uint16_t batNow = readBatteryMV();

        // Ringpuffer befüllen
        batHistory[batHistoryIdx] = batNow;
        batHistoryIdx = (batHistoryIdx + 1) % BAT_AVG_SAMPLES;
        if (batHistoryCount < BAT_AVG_SAMPLES) batHistoryCount++;

        // Erst auswerten wenn Puffer voll (5 Messungen = 10s)
        if (batHistoryCount >= BAT_AVG_SAMPLES) {
            uint32_t sum = 0;
            for (uint8_t i = 0; i < BAT_AVG_SAMPLES; i++) sum += batHistory[i];
            uint16_t batAvg = sum / BAT_AVG_SAMPLES;

            if (batAvg >= 1000 && batAvg <= BAT_CRITICAL_MV) {
                Serial.printf("CRITICAL: Battery avg %u mV → lowBatteryMode!\n", batAvg);
                noTone(PIN_BUZZER);
                sound.active = false;
                // Countdown-Animation VOR dem Sperren zeigen
                FastLED.setBrightness(200);
                for (int n = NUM_LEDS; n >= 0; n--) {
                    FastLED.clear();
                    for (int j = 0; j < n; j++) leds[j] = CRGB::Red;
                    FastLED.show();
                    delay(1000 / (NUM_LEDS + 1));
                }
                for (int b = 0; b < 2; b++) {
                    fill_solid(leds, NUM_LEDS, CRGB::Red); FastLED.show(); delay(150);
                    FastLED.clear(); FastLED.show(); delay(150);
                }
                // Jetzt sperren
                lowBatteryMode = true;
                anim.id = EFF_OFF;
            } else if (batAvg >= 1000 && batAvg <= BAT_WARNING_MV && !batterySaveMode) {
                batterySaveMode = true;
                Serial.printf("WARNING: Battery avg %u mV → batterySaveMode (50%% brightness)\n", batAvg);
                FastLED.setBrightness(min(anim.brightness, (uint8_t)BAT_SAVE_BRIGHTNESS));
            }
        }
    }

    // --- TEMPERATUR CHECK & OVERHEAT SCHUTZ ---
    if (now - lastTempCheck >= TEMP_CHECK_INTERVAL_MS) {
        lastTempCheck = now;
        currentTemp_c10 = readTemperature();

        if (!overheatMode && currentTemp_c10 != -999 && currentTemp_c10 >= TEMP_OVERHEAT_C * 10) {
            overheatMode = true;
            overheatStartTime = now;
            overheatLastToggle = now;
            overheatBuzzerOn = false;
            // LED Ring aus
            setEffect(EFF_OFF, 0, 0, 0, 0, 0, 0);
            Serial.printf("OVERHEAT: %d.%d°C >= %d°C → Overheat-Schutz aktiv!\n",
                          currentTemp_c10 / 10, abs(currentTemp_c10 % 10), TEMP_OVERHEAT_C);
        }
    }

    // Overheat Beep-Sequenz: aggressives AN/AUS für 30 Sekunden
    if (overheatMode) {
        if (now - overheatStartTime < OVERHEAT_BEEP_DURATION_MS) {
            // 250ms AN, 250ms AUS → aggressives Beepen
            if (now - overheatLastToggle >= 250) {
                overheatLastToggle = now;
                overheatBuzzerOn = !overheatBuzzerOn;
                if (overheatBuzzerOn) tone(PIN_BUZZER, 2000);
                else noTone(PIN_BUZZER);
            }
            // LED Ring bleibt aus, alle anderen Effekte blockieren
            if (anim.id != EFF_OFF) setEffect(EFF_OFF, 0, 0, 0, 0, 0, 0);
        } else {
            // 30 Sekunden vorbei → Buzzer aus, Overheat bleibt aber aktiv
            noTone(PIN_BUZZER);
            overheatBuzzerOn = false;
            // Prüfe ob Temperatur wieder unter Schwelle (mit 5°C Hysterese)
            if (currentTemp_c10 != -999 && currentTemp_c10 < (TEMP_OVERHEAT_C - 5) * 10) {
                overheatMode = false;
                Serial.println("OVERHEAT: Temperatur normalisiert – Schutz deaktiviert.");
            }
        }
    }

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
        coordinatorMacKnown = false;
        lastHeartbeat = 0;  // Sofort EVT_HELLO senden beim nächsten Loop-Durchlauf

        // Visuelles Feedback: Status-LED auf ROT
        setEffect(EFF_STATUS, 255, 0, 0, 0, 255, 0);

        // Sniffer aus (spart CPU während der Reconnect-Phase)
        esp_wifi_set_promiscuous(false);
    }

    // Heartbeat-Versand: Zeitlich vom LED-Refresh trennen.
    // Sende nur, wenn mindestens HEARTBEAT_OFFSET_MS seit dem letzten FastLED.show()
    // vergangen sind, damit ESP-NOW und WS2812B-Timing sich nicht stören.
    {
        unsigned long sinceLast = now - lastShowTime;
        bool safeToSend = (sinceLast >= HEARTBEAT_OFFSET_MS) || (lastShowTime == 0);

        if (!isPaired) {
            if (now - lastHeartbeat > 2000 && safeToSend) {
                sendEvent(EVT_HELLO);
                lastHeartbeat = now;
            }
        } else {
            if (now - lastHeartbeat > 5000 && safeToSend) {
                sendEvent(EVT_HEARTBEAT);
                lastHeartbeat = now;
            }
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
    if (lowBatteryMode) { noTone(PIN_BUZZER); sound.active = false; return; }
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
            coordinatorMacKnown = false;
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
        else if (cmd.cmd == CMD_SET_BRIGHTNESS) {
            maxBrightnessPercent = constrain(cmd.extra, 10, 100);
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

    // Coordinator-MAC beim ersten Paket merken (für Sniffer MAC-Filter)
    if (!coordinatorMacKnown && len == sizeof(CommandPacket)) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        memcpy(coordinatorMac, info->src_addr, 6);
#else
        memcpy(coordinatorMac, mac_addr, 6);
#endif
        coordinatorMacKnown = true;
    }

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
    pkg.temp_c10 = currentTemp_c10;

    // Geglätteten Mittelwert senden (falls Ringpuffer voll), sonst Einzelmessung
    if (batHistoryCount >= BAT_AVG_SAMPLES) {
        uint32_t sum = 0;
        for (uint8_t i = 0; i < BAT_AVG_SAMPLES; i++) sum += batHistory[i];
        pkg.battery_mv = sum / BAT_AVG_SAMPLES;
    } else {
        pkg.battery_mv = readBatteryMV();
    }

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
    // Overheat Guard: nur EFF_OFF erlaubt während Überhitzung
    if (overheatMode && id != EFF_OFF) return;
    // Low Battery Guard: nur EFF_OFF und EFF_STATUS erlaubt
    if (lowBatteryMode && id != EFF_OFF && id != EFF_STATUS) return;

    anim.id = id;
    anim.color1 = CRGB(r, g, b);
    anim.speed = speed;
    anim.brightness = bright;
    anim.step = 0;
    anim.counter = extra;

    if (maxBrightnessPercent < 100) bright = (uint8_t)((uint16_t)bright * maxBrightnessPercent / 100);
    if (batterySaveMode && bright > BAT_SAVE_BRIGHTNESS) bright = BAT_SAVE_BRIGHTNESS;
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

    unsigned long minInterval = (anim.speed > 0) ? (unsigned long)anim.speed : 100; // min 100ms bei speed=0
    if (anim.id == EFF_PROGRESS) minInterval = 0; // Progress: sofort updaten
    // 15 FPS Cap: Egal was anim.speed sagt, nie schneller als LED_MIN_FRAME_MS
    if (minInterval > 0 && minInterval < LED_MIN_FRAME_MS) minInterval = LED_MIN_FRAME_MS;
    if (minInterval > 0 && now - anim.lastUpdate < minInterval) return;
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
    lastShowTime = millis();
    yield();  // WiFi-Stack Verarbeitung nach show() ermöglichen
}

void performOTA() {
    esp_task_wdt_delete(NULL);
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