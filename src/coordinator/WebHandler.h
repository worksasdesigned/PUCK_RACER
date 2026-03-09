#ifndef WEBHANDLER_H
#define WEBHANDLER_H
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

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
};
#endif