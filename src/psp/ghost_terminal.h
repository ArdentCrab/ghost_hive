#ifndef GHOST_TERMINAL_H
#define GHOST_TERMINAL_H

// =====================================================
// Ghost Hive v1.7.3 — ghost-terminal (§11, §10, §34)
// Batch 30: ghost_$ prompt, snapshot lines, output full-frame (§34/§35)
// TermEvent is local to the terminal, not a SPEC EventType.
// =====================================================

#include "ghost_core.h"
#include "psp_input.h"
#include "ghost_output.h"
#include "registry.h"
#include "event_queue.h"
#include "ghost_policy.h"
#include "ghost_heartbeat.h"
#include "ghost_scanner.h"
#include "ghost_stealth.h"
#include "ghost_down.h"
#include "ghost_peek.h"
#include "ghost_vault.h"

#include <stdio.h>

class DecisionPipeline;
class GhostTransport;

const uint8_t CMD_COUNT = 12;
const uint8_t CMD_LEN = 32;

const uint16_t OUTPUT_LEN = 1536;
const uint8_t OUTPUT_MAX = 6;

const uint8_t SNAPSHOT_MAX = 6;
const uint32_t AUTO_RESET_MS = 600000;
const uint32_t DOWN_HOLD_MS = 3000;
const uint32_t WATCH_NAV_COOL_MS = 180;
const uint32_t WATCH_PULSE_MS = 400;
const uint8_t TERM_ROWS = 24;
const uint8_t TERM_COLS = 48;
const uint8_t WATCH_GLYPH_W = 9;
const uint8_t WATCH_GLYPH_H = 11;
const uint16_t WATCH_FB_W = 480;
const uint16_t WATCH_FB_H = 272;

enum class TermMode : uint8_t {
    Terminal,
    Output,
    Black,
    Game,
    GhostDown,
    Watch
};

enum class TermEvent : uint8_t {
    None,
    CursorUp,
    CursorDown,
    GearPrev,
    GearNext,
    LockOrExec,
    O,
    Clear,
    GameToggle,
    OutputPrev,
    OutputNext,
    OutputExit,
    WatchPrev,
    WatchNext,
    WatchRefresh,
    WatchFocusPrev,
    WatchFocusNext,
    ExitHive,
    DismissWatch
};

struct HistoryEntry {
    char command[CMD_LEN];
    char snapshot[OUTPUT_LEN];
};

class GhostTerminal {
public:
    GhostTerminal();

    void init(Registry* reg, EventQueue* eq);
    void attach(GhostVault* vault, DecisionPipeline* pipeline);
    void attachTransport(GhostTransport* xport);
    void tick(uint32_t now_ms);
    void run();
    bool isRunning() const;
    void enterTerminalMode();
    void draw();
    TermMode mode() const;
    bool locked() const;
    uint8_t cursor() const;
    uint8_t zahnrad() const;
    uint8_t historyCount() const;
    uint8_t outputCount() const;
    uint8_t outputIndex() const;
    uint8_t watchPage() const;
    const char* frameLine(uint8_t row) const;
    const char* watchText() const;
    void dumpWatchStacked(FILE* out);

    GhostScanner& scanner();
    GhostStealth& stealth();
    GhostDown& down();
    GhostPeek& peek();
    GhostHeartbeat& heartbeat();

private:
    TermMode mode_;
    bool running_;
    uint8_t cursor_;
    uint8_t zahnrad_;
    int8_t lockIndex_;
    uint8_t screenLocked_;
    uint8_t watch_page_;
    uint8_t watch_focus_;
    uint8_t down_was_;
    uint8_t watch_hidden_;
    uint8_t force_draw_;
    uint32_t down_t0_;
    uint32_t down_hold_t0_;
    uint32_t nav_cool_t_;
    uint8_t down_hold_page_;
    uint8_t watch_alarm_;
    uint8_t watch_pulse_;
    char watch_text_[OUTPUT_LEN];

    char commands_[CMD_COUNT][CMD_LEN];

    char outputs_[OUTPUT_MAX][OUTPUT_LEN];
    uint8_t outputCount_;
    uint8_t outputIndex_;

    HistoryEntry history_[SNAPSHOT_MAX];
    uint8_t historyCount_;
    TermMode resumeMode_;

    uint32_t now_;
    uint32_t lastActivity_;
    char frame_[TERM_ROWS][TERM_COLS + 1];

    Registry* registry_;
    EventQueue* events_;
    GhostVault* vault_;
    DecisionPipeline* pipeline_;
    GhostTransport* xport_;
    GhostPolicy policy_;
    GhostHeartbeat heartbeat_;
    GhostScanner scanner_;
    GhostStealth stealth_;
    GhostDown down_;
    GhostPeek peek_;
    GhostOutput output_;

    void buildCommands();
    void resetIfNeeded();
    TermEvent mapKey(Key key) const;
    void applyEvent(TermEvent ev);
    void syncLock();
    void gearCli(int8_t dir);
    void moveCursor(int8_t dir);
    void onLockOrExec();
    void onO();
    void execute(uint8_t index);
    void openHistorySnapshot(uint8_t index);
    void openOutput(const char* text);
    void closeOutput();
    void clearView();
    void sessionReset();
    void enterBlackMode();
    void enterWatchMode();
    void fillWatch();
    void paintWatch();
    void pollManualDown();
    void enterGameMode();
    void leaveGameMode();
    void pageOutput(int8_t dir);
    void saveToHistory(const char* cmd, const char* snapshot);
    void goLive();
    bool onLive() const;
    void paintChrome();
    void paintPromptRow(uint8_t row, bool on, bool lk, bool live, const char* cmd);
    void frameClear();
    void framePut(uint8_t row, uint8_t col, const char* text);
    void buildFrame();
};

#endif
