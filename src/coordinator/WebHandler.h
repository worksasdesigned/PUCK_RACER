#ifndef WEBHANDLER_H
#define WEBHANDLER_H
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <IPAddress.h> // Wichtig für die IP-Speicherung

// RAM watchdog
extern int lowestHeapGameId;
extern int lowestHeapValue;

class WebHandler {
public:
    static void begin();
    static void update();
private:
    static AsyncWebServer server;
    static DNSServer dnsServer;

    // --- NEU: Variablen für den Türsteher (IP-Lock) ---
    static IPAddress activeClientIP;       // Speichert die IP des aktuell berechtigten Geräts
    static unsigned long lastActivityTime; // Zeitstempel der letzten Aktion dieses Geräts
    static String activeClientName;        // Speichert den Klartext-Namen (z.B. "iPhone")
    
    // Die Prüf-Funktion: Darf diese Anfrage durchgelassen werden?
    static bool isClientAllowed(AsyncWebServerRequest *req);
};
#endif