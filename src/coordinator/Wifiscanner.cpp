#include "WifiScanner.h"
#include "Common.h"
#include <esp_wifi.h>

volatile int WifiScanner::packetCount = 0;
volatile int WifiScanner::totalBytes = 0;
bool WifiScanner::_scanning = false;
bool WifiScanner::_resultReady = false;
int WifiScanner::_scanChannel = 1;
int WifiScanner::_scanDurationMs = 3000;
unsigned long WifiScanner::_scanStart = 0;
ScanResult WifiScanner::_lastResult = {0, 0, 0, 0};

// Promiscuous callback – läuft auf dem WiFi Task Core. 
// Muss extrem kurz und effizient gehalten werden, da er bei jedem WLAN-Paket in der Luft feuert.
static void IRAM_ATTR promiscCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    WifiScanner::packetCount++;
    WifiScanner::totalBytes += pkt->rx_ctrl.sig_len;
}

void WifiScanner::startScan(int channel, int durationSec) {
    // Verhindere den Start eines neuen Scans, wenn bereits einer läuft
    if (_scanning) return;

    // --- WERTE BEREINIGEN (Clamp) ---
    if (channel < 1) channel = 1;
    if (channel > 13) channel = 13;
    if (durationSec < 1) durationSec = 1;
    
    // ANPASSUNG: Das alte Limit von 10 Sekunden wurde auf 60 Sekunden erhöht, 
    // um die gewünschten 30-Sekunden-Scans zu ermöglichen.
    if (durationSec > 60) durationSec = 60; 

    _scanChannel = channel;
    _scanDurationMs = durationSec * 1000;
    _resultReady = false;

    // Zähler für den neuen Durchlauf zurücksetzen
    packetCount = 0;
    totalBytes = 0;

    // Kanal wechseln und Promiscuous Mode (Mithören aller Pakete) aktivieren
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous_rx_cb(promiscCallback);
    esp_wifi_set_promiscuous(true);

    _scanStart = millis();
    _scanning = true;

    Serial.printf("SCAN: Started CH%d for %ds\n", channel, durationSec);
}

void WifiScanner::loop() {
    if (!_scanning) return;

    // Prüfen, ob die gewünschte Scan-Dauer erreicht ist
    if (millis() - _scanStart >= (unsigned long)_scanDurationMs) {
        
        // Scan beendet – Promiscuous Mode deaktivieren
        esp_wifi_set_promiscuous(false);

        // Ergebnisse speichern
        _lastResult.channel = _scanChannel;
        _lastResult.packets = packetCount;
        _lastResult.total_ms = _scanDurationMs;

        // Schätzung der Airtime (Kanalbelegung in Mikrosekunden):
        // ~200µs Overhead pro Paket (Präambel, Interframe Space, ACK)
        // ~0.5µs pro Byte (Durchschnittswert für gemischte 802.11 Übertragungsraten)
        long airtime_us = ((long)packetCount * 200L) + ((long)totalBytes / 2L);
        _lastResult.busy_ms = (int)(airtime_us / 1000L);
        
        // Logische Korrektur: Die berechnete Auslastung kann nie größer als die Gesamtzeit sein
        if (_lastResult.busy_ms > _lastResult.total_ms) {
            _lastResult.busy_ms = _lastResult.total_ms;
        }

        _scanning = false;
        _resultReady = true;

        Serial.printf("SCAN: CH%d done – %d pkts, %d bytes, ~%dms airtime\n",
                      _scanChannel, packetCount, (int)totalBytes, _lastResult.busy_ms);
    }
}

bool WifiScanner::isScanning() { return _scanning; }
bool WifiScanner::hasResult() { return _resultReady; }

ScanResult WifiScanner::getResult() {
    // Ergebnis abholen und Flag zurücksetzen
    _resultReady = false;  
    return _lastResult;
}

void WifiScanner::restoreChannel(int channel) {
    // Falls noch ein Scan läuft, diesen hart abbrechen
    if (_scanning) {
        esp_wifi_set_promiscuous(false);
        _scanning = false;
    }
    // Wieder auf den Arbeitskanal des Systems wechseln
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    Serial.printf("SCAN: Restored to CH%d\n", channel);
}