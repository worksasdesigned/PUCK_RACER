#ifndef WIFI_SCANNER_H
#define WIFI_SCANNER_H

#include <Arduino.h>

struct ScanResult {
    int channel;
    int packets;
    int busy_ms;
    int total_ms;
};

class WifiScanner {
public:
    static void loop();  // Call from main loop()

    // Non-blocking API
    static void startScan(int channel, int durationSec);
    static bool isScanning();
    static bool hasResult();
    static ScanResult getResult();
    static void restoreChannel(int channel);

    // Promisc callback counters (public for C callback)
    static volatile int packetCount;
    static volatile int totalBytes;

private:
    static bool _scanning;
    static bool _resultReady;
    static int _scanChannel;
    static int _scanDurationMs;
    static unsigned long _scanStart;
    static ScanResult _lastResult;
};

#endif