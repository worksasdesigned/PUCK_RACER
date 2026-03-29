#ifndef GAME_CUSTOM_H
#define GAME_CUSTOM_H

#include "Game.h"

#define CG_MAX_STATES  30
#define CG_MAX_ACTIONS 12
#define CG_MAX_TRANS    4

struct CG_Action {
    int8_t   puck;       // 0-19 = specific puck, -1 = all
    uint8_t  effectID;
    uint8_t  r, g, b;
    uint16_t duration;
    uint8_t  soundType;  // 0=none, 1=beep, 2=ski
    uint16_t soundDur;
};

struct CG_Transition {
    uint8_t  trigger;    // 0=PRESS, 1=TIMEOUT
    int8_t   puck;       // for PRESS: which puck (-1=any)
    uint16_t timeDs;     // for TIMEOUT: deciseconds (0.1s units)
    int8_t   gotoState;  // -1 = end game
};

struct CG_State {
    char          name[16];
    CG_Action     actions[CG_MAX_ACTIONS];
    uint8_t       actionCount;
    CG_Transition transitions[CG_MAX_TRANS];
    uint8_t       transCount;
};

class Game_Custom : public Game {
public:
    void    setup() override;
    void    loop() override;
    void    handleEvent(int puckIndex, EventPacket event) override;
    String  getName() override { return String(gameName); }
    String  getStatusJSON() override;
    void    processCommand(String cmd, int value) override;

private:
    char      gameName[32] = "Custom Game";
    CG_State  states[CG_MAX_STATES];
    uint8_t   stateCount = 0;

    // Runtime
    int8_t        currentState = -1;
    unsigned long stateEnteredAt = 0;
    unsigned long gameStartTime  = 0;
    int           activePucks = 8;
    int           timeLimitSeconds = 0;

    enum Phase { CS_SETUP, CS_RUNNING, CS_FINISHED } phase = CS_SETUP;

    bool    loadJSON();
    void    enterState(int stateIdx);
    void    sendAction(const CG_Action& action);
    uint8_t effectFromString(const char* name);
    void    exitGame();
};

#endif
