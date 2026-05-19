#ifndef STATUSLED_H
#define STATUSLED_H

#include <Arduino.h>

enum StatusLedState {
    STATUS_BOOT,      // Lila auf allen 16 LEDs
    STATUS_NO_PUCK,   // Gelb, jede 2. LED
    STATUS_HAS_PUCK   // Grün, jede 4. LED
};

// Sehr sparsamer Status-Indikator am WS2812B Ring (16 LEDs an GPIO 4).
// FastLED.show() läuft NUR bei echtem Zustandswechsel — keine zyklische
// Refresh-Logik, damit ESP-NOW nicht durch RMT/Interrupts gestört wird.
class StatusLed {
public:
    static void begin();
    static void update();
    static void setState(StatusLedState newState);

private:
    static StatusLedState currentState;
    static unsigned long lastCheck;
    static void render();
};

#endif
