#include "StatusLed.h"
#include <FastLED.h>
#include "Common.h"
#include "PuckNetwork.h"

#define STATUS_LED_PIN              4
#define STATUS_LED_COUNT            16
#define STATUS_LED_BRIGHTNESS       191      // ~75 %
#define STATUS_LED_CHECK_INTERVAL_MS 10000UL

static CRGB statusLeds[STATUS_LED_COUNT];

StatusLedState StatusLed::currentState = STATUS_BOOT;
unsigned long StatusLed::lastCheck = 0;

void StatusLed::begin() {
    FastLED.addLeds<WS2812B, STATUS_LED_PIN, GRB>(statusLeds, STATUS_LED_COUNT);
    FastLED.setBrightness(STATUS_LED_BRIGHTNESS);
    currentState = STATUS_BOOT;
    render();
}

void StatusLed::setState(StatusLedState newState) {
    if (newState == currentState) return;
    currentState = newState;
    render();
}

void StatusLed::update() {
    unsigned long now = millis();
    if (now - lastCheck < STATUS_LED_CHECK_INTERVAL_MS) return;
    lastCheck = now;

    PuckInfo* pucks = PuckNetwork::getPucks();
    bool any = false;
    for (int i = 0; i < MAX_PEERS; i++) {
        if (pucks[i].active) { any = true; break; }
    }
    setState(any ? STATUS_HAS_PUCK : STATUS_NO_PUCK);
}

void StatusLed::render() {
    for (int i = 0; i < STATUS_LED_COUNT; i++) statusLeds[i] = CRGB::Black;

    switch (currentState) {
        case STATUS_BOOT:
            for (int i = 0; i < STATUS_LED_COUNT; i++) statusLeds[i] = CRGB::Purple;
            break;
        case STATUS_NO_PUCK:
            for (int i = 0; i < STATUS_LED_COUNT; i += 2) statusLeds[i] = CRGB::Yellow;
            break;
        case STATUS_HAS_PUCK:
            for (int i = 0; i < STATUS_LED_COUNT; i += 4) statusLeds[i] = CRGB::Green;
            break;
    }

    FastLED.show();
}
