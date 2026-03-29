# PuckRacer Coordinator — Claude Code Instructions

## AI Knowledge Base (.claude/*.ai)

Before modifying or discussing any game, system component, or frontend file:
1. **Always read the relevant `.claude/*.ai` file first** to understand the architecture, state machine, commands, and JSON format
2. Start with `.claude/index.ai` for the master overview if unsure which file to look at
3. **After making changes**, update the corresponding `.ai` file to keep it current

### File mapping:
- Game discussion → `.claude/game_<name>.ai` (e.g. "Shuttle Run" → `.claude/game_shuttle.ai`)
- WebHandler/API → `.claude/webhandler.ai`
- PuckNetwork/ESP-NOW → `.claude/pucknetwork.ai`
- Game registration/new game → `.claude/gamemanager.ai`
- Statistics → `.claude/statsmanager.ai`
- License/Activation → `.claude/activationmanager.ai`
- Frontend JS → `.claude/script.ai`, `lang.ai`, `player_picker.ai`, `tour.ai`, `training_config.ai`
- WiFi Scanner → `.claude/wifiscanner.ai`
- Overall architecture → `.claude/index.ai`

### Game ID quick reference:
1=ShuttleRun, 2=Counter, 3=Countdown, 4=SimonSays, 5=DisplayMode, 6=SimonRuns, 7=SortingHat, 8=BombSquad, 9=RedGreen, 10=Stopwatch, 11=Zombie, 12=BeepTest, 13=React2P, 14=TTest, 15=Target, 16=Timer, 17=Hunt, 18=Pacemaker, 19=Memory, 20=Domination, 21=CTL, 22=Tabata, 23=Ball, 24=Whac, 25=CatReflex, 26=Musical, 27=TTTT, 28=Pitstop, 29=Custom

## Project conventions
- ESP32-S3, PlatformIO, Arduino framework
- No PSRAM usage (clone compatibility)
- German comments in C++ code are acceptable
- LittleFS for web files (data/) and config storage
- ArduinoJson v7 for JSON parsing
- All games inherit from Game base class (include/Game.h → but actually src/coordinator/Game.h)
- Common.h is in include/ directory (not src/coordinator/)
