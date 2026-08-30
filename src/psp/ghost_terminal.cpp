#include "ghost_terminal.h"
#include "decision_pipeline.h"
#include "psp_time.h"
#include "ghost_arm.h"
#include "watch_hud.h"
#include "transport/ghost_transport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__PSP__)
#include <pspdebug.h>
#endif

#if !defined(__PSP__)
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#endif

GhostTerminal::GhostTerminal()
    : mode_(TermMode::Terminal),
      running_(false),
      cursor_(0),
      zahnrad_(0),
      lockIndex_(-1),
      screenLocked_(0),
      watch_page_(0),
      watch_focus_(0),
      down_was_(0),
      watch_hidden_(0),
      force_draw_(0),
      down_t0_(0),
      down_hold_t0_(0),
      nav_cool_t_(0),
      down_hold_page_(0),
      watch_alarm_(0),
      watch_pulse_(0),
      outputCount_(0),
      outputIndex_(0),
      historyCount_(0),
      resumeMode_(TermMode::Terminal),
      now_(0),
      lastActivity_(0),
      registry_(nullptr),
      events_(nullptr),
      vault_(nullptr),
      pipeline_(nullptr),
      xport_(nullptr) {
    watch_text_[0] = '\0';
    frameClear();
}

void GhostTerminal::init(Registry* reg, EventQueue* eq) {
    registry_ = reg;
    events_ = eq;
    running_ = true;
    now_ = 0;
    lastActivity_ = 0;
    buildCommands();
#if defined(GHOST_DEBUG_CLI)
    enterTerminalMode();
#else
    enterWatchMode();
#endif
}

void GhostTerminal::attach(GhostVault* vault, DecisionPipeline* pipeline) {
    vault_ = vault;
    pipeline_ = pipeline;
    down_.attach(vault_, &stealth_);
}

void GhostTerminal::attachTransport(GhostTransport* xport) {
    xport_ = xport;
}

void GhostTerminal::enterTerminalMode() {
    mode_ = TermMode::Terminal;
    screenLocked_ = 0;
    stealth_.enterTerminalMode();
    scanner_.setTerminalMode(true);
}

uint8_t GhostTerminal::watchPage() const { return watch_page_; }

const char* GhostTerminal::watchText() const { return watch_text_; }

#if !defined(__PSP__)
static uint8_t watch_split_lines(const char* text, char line[][49], uint8_t maxn) {
    uint8_t n = 0;
    uint8_t col = 0;
    uint16_t i = 0;
    if (text == nullptr || maxn == 0) return 0;
    while (text[i] != '\0' && n < maxn) {
        if (text[i] == '\n' || col >= 48) {
            while (col < 48) line[n][col++] = ' ';
            line[n][48] = '\0';
            ++n;
            col = 0;
            if (text[i] == '\n') ++i;
            continue;
        }
        line[n][col++] = text[i++];
    }
    if (col > 0 && n < maxn) {
        while (col < 48) line[n][col++] = ' ';
        line[n][48] = '\0';
        ++n;
    }
    while (n > 0) {
        uint8_t k = 0;
        while (k < 48 && line[n - 1][k] == ' ') ++k;
        if (k < 48) break;
        --n;
    }
    return n;
}

static void mini_tty_restore(void) {
    fputs("\033[?25h\033[?1049l", stdout);
    fflush(stdout);
}

static void viewer_alarm_beep(void) {
    (void)fputc('\a', stdout);
    (void)fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        (void)execlp(
            "powershell.exe", "powershell.exe", "-NoProfile", "-Command",
            "[console]::Beep(2000,220);[console]::Beep(2500,220);[console]::Beep(2000,450)",
            static_cast<char*>(nullptr));
        _exit(0);
    }
}

static void dump_watch_inplace(FILE* out, char pages[4][24][49], uint8_t nline[4],
                               uint8_t alarm, uint8_t pulse) {
    static uint8_t tty_armed = 0;
    static uint8_t prev_alarm = 0;
    if (out == nullptr) return;
    if (isatty(fileno(out)) && tty_armed == 0) {
        tty_armed = 1;
        (void)signal(SIGCHLD, SIG_IGN);
        (void)atexit(mini_tty_restore);
        fputs("\033[?1049h\033[?25l", out);
    }
    if (alarm != 0 && prev_alarm == 0) viewer_alarm_beep();
    prev_alarm = alarm;

    struct winsize ws;
    ws.ws_row = 40;
    ws.ws_col = 80;
    if (isatty(fileno(out))) {
        if (ioctl(fileno(out), TIOCGWINSZ, &ws) != 0 || ws.ws_row < 8) ws.ws_row = 24;
        if (ws.ws_col < 48) ws.ws_col = 48;
    }

    fputs("\033[H", out);
    uint8_t row = 1;
    const uint8_t maxr = ws.ws_row > 0 ? static_cast<uint8_t>(ws.ws_row) : 24;

    if (alarm != 0) {
        if (pulse != 0) fputs("\033[41;97;1m", out);
        else fputs("\033[31;1m", out);
        fputs("ALARM  Watch-Headline (Viewer-Beep, kein Down)", out);
        fputs("\033[0m\033[K\n", out);
        ++row;
    } else {
        fputs("\033[2m[GHv2 Mini]\033[0m in-place  L/R auf PSP; hier alle 4 Seiten\033[K\n", out);
        ++row;
    }

    const bool grid = (ws.ws_col >= 97);
    if (grid) {
        uint8_t pair[2][2] = {{0, 1}, {2, 3}};
        for (uint8_t pr = 0; pr < 2; ++pr) {
            uint8_t a = pair[pr][0];
            uint8_t b = pair[pr][1];
            uint8_t h = nline[a];
            if (nline[b] > h) h = nline[b];
            for (uint8_t i = 0; i < h && row < maxr; ++i) {
                const char* la = (i < nline[a]) ? pages[a][i] : "                                                ";
                const char* lb = (i < nline[b]) ? pages[b][i] : "                                                ";
                fputs(la, out);
                fputc('|', out);
                fputs(lb, out);
                fputs("\033[K\n", out);
                ++row;
            }
        }
    } else {
        for (uint8_t p = 0; p < 4u && row < maxr; ++p) {
            for (uint8_t i = 0; i < nline[p] && row < maxr; ++i) {
                fputs(pages[p][i], out);
                fputs("\033[K\n", out);
                ++row;
            }
        }
    }
    fputs("\033[J", out);
    fflush(out);
}
#endif

void GhostTerminal::dumpWatchStacked(FILE* out) {
    if (out == nullptr) return;
    uint8_t saved = watch_page_;
#if defined(__PSP__)
    for (uint8_t p = 0; p < 4u; ++p) {
        watch_page_ = p;
        fillWatch();
        fputs(watch_text_, out);
        fputc('\n', out);
    }
    watch_page_ = saved;
    fillWatch();
#else
    char pages[4][24][49];
    uint8_t nline[4];
    uint8_t tty = isatty(fileno(out)) ? 1 : 0;
    for (uint8_t p = 0; p < 4u; ++p) {
        watch_page_ = p;
        fillWatch();
        nline[p] = watch_split_lines(watch_text_, pages[p], 24);
        if (tty == 0) {
            fputs(watch_text_, out);
            fputc('\n', out);
        }
    }
    watch_page_ = saved;
    fillWatch();
    if (tty != 0) {
        dump_watch_inplace(out, pages, nline, watch_alarm_, watch_pulse_);
    }
#endif
}

void GhostTerminal::enterWatchMode() {
    mode_ = TermMode::Watch;
    screenLocked_ = 0;
    if (!down_.isActive()) {
        stealth_.enterTerminalMode();
        scanner_.setTerminalMode(true);
    }
}

bool GhostTerminal::isRunning() const { return running_; }
TermMode GhostTerminal::mode() const { return mode_; }
bool GhostTerminal::locked() const { return lockIndex_ >= 0; }
uint8_t GhostTerminal::cursor() const { return cursor_; }
uint8_t GhostTerminal::zahnrad() const { return zahnrad_; }
uint8_t GhostTerminal::historyCount() const { return historyCount_; }
uint8_t GhostTerminal::outputCount() const { return outputCount_; }
uint8_t GhostTerminal::outputIndex() const { return outputIndex_; }

const char* GhostTerminal::frameLine(uint8_t row) const {
    if (row >= TERM_ROWS) return "";
    return frame_[row];
}

GhostScanner& GhostTerminal::scanner() { return scanner_; }
GhostStealth& GhostTerminal::stealth() { return stealth_; }
GhostDown& GhostTerminal::down() { return down_; }
GhostPeek& GhostTerminal::peek() { return peek_; }
GhostHeartbeat& GhostTerminal::heartbeat() { return heartbeat_; }

void GhostTerminal::tick(uint32_t now_ms) {
    now_ = now_ms;
    resetIfNeeded();
    down_.tick(now_ / 1000u);

    if (down_.isActive()) {
        if (down_was_ == 0) {
            down_t0_ = now_;
            watch_page_ = 1;
            force_draw_ = 1;
        }
        down_was_ = 1;
        mode_ = TermMode::Watch;
        scanner_.setTerminalMode(false);
        stealth_.radioOff();
    } else {
        down_was_ = 0;
        watch_hidden_ = 0;
    }

    Key key = psp_read_key();
#if defined(__PSP__) || defined(PSP_BUILD)
    if (mode_ == TermMode::Watch &&
        (key == Key::L || key == Key::R || key == Key::Up || key == Key::Down)) {
        if (nav_cool_t_ != 0 && now_ >= nav_cool_t_ &&
            (now_ - nav_cool_t_) < WATCH_NAV_COOL_MS) {
            key = Key::None;
        }
    }
#endif
    if (key != Key::None) lastActivity_ = now_;
    applyEvent(mapKey(key));
#if defined(__PSP__) || defined(PSP_BUILD)
    if (mode_ == TermMode::Watch &&
        (key == Key::L || key == Key::R || key == Key::Up || key == Key::Down)) {
        nav_cool_t_ = now_ == 0 ? 1 : now_;
    }
#endif
    pollManualDown();
}

void GhostTerminal::run() {
    running_ = true;
    while (running_) {
        tick(psp_now_ms());
        sceKernelDelayThread(10 * 1000);
    }
}

void GhostTerminal::buildCommands() {
    const char* names[CMD_COUNT] = {
        "hive status", "hive devices", "hive policies", "hive scan",
        "hive backup", "hive alert", "ghost down", "ghost peek",
        "danger mode", "mine check", "mine block", "time check"
    };
    for (uint8_t i = 0; i < CMD_COUNT; ++i) {
        uint8_t j = 0;
        while (names[i][j] != '\0' && j < (CMD_LEN - 1)) {
            commands_[i][j] = names[i][j];
            ++j;
        }
        commands_[i][j] = '\0';
    }
}

void GhostTerminal::pollManualDown() {
    if (mode_ != TermMode::Watch || down_.isActive() || watch_page_ != 1) {
        down_hold_t0_ = 0;
        down_hold_page_ = 0;
        return;
    }
    if (!psp_combo_held()) {
        down_hold_t0_ = 0;
        down_hold_page_ = 0;
        return;
    }
    if (down_hold_page_ != 1) {
        down_hold_t0_ = 0;
        down_hold_page_ = 1;
    }
    if (down_hold_t0_ == 0) {
        down_hold_t0_ = now_ == 0 ? 1 : now_;
        return;
    }
    if (now_ < down_hold_t0_) {
        down_hold_t0_ = 0;
        down_hold_page_ = 0;
        return;
    }
    if ((now_ - down_hold_t0_) >= DOWN_HOLD_MS) {
        down_.execute(now_ / 1000u);
        if (xport_ != nullptr) (void)xport_->enterHiveDown(now_ / 1000u);
        down_hold_t0_ = 0;
        down_hold_page_ = 0;
    }
}

void GhostTerminal::resetIfNeeded() {
    if (mode_ == TermMode::GhostDown) return;
    if (mode_ == TermMode::Watch) return;
    if (now_ < lastActivity_) {
        lastActivity_ = now_;
        return;
    }
    if (now_ - lastActivity_ > AUTO_RESET_MS) {
        sessionReset();
        lastActivity_ = now_;
    }
}

TermEvent GhostTerminal::mapKey(Key key) const {
    if (key == Key::None) return TermEvent::None;
    if (mode_ == TermMode::GhostDown) return TermEvent::None;

    if (mode_ == TermMode::Watch) {
        if (down_.isActive()) {
            if (key == Key::Square) return TermEvent::WatchRefresh;
            if (key == Key::Home) return TermEvent::DismissWatch;
            if (key == Key::L) return TermEvent::WatchPrev;
            if (key == Key::R) return TermEvent::WatchNext;
            return TermEvent::None;
        }
        if (key == Key::Home) return TermEvent::ExitHive;
        if (key == Key::L) return TermEvent::WatchPrev;
        if (key == Key::R) return TermEvent::WatchNext;
        if (key == Key::Square) return TermEvent::WatchRefresh;
        if (key == Key::O) return TermEvent::O;
        if (watch_page_ == 3) {
            if (key == Key::Up) return TermEvent::WatchFocusPrev;
            if (key == Key::Down) return TermEvent::WatchFocusNext;
        }
        return TermEvent::None;
    }

    if (mode_ == TermMode::Output) {
        if (key == Key::L || key == Key::Left) return TermEvent::OutputPrev;
        if (key == Key::R || key == Key::Right) return TermEvent::OutputNext;
        if (key == Key::Home || key == Key::O) return TermEvent::OutputExit;
        return TermEvent::None;
    }
    if (mode_ == TermMode::Black) {
        if (key == Key::O) return TermEvent::O;
        return TermEvent::None;
    }
    if (mode_ == TermMode::Game) {
        return TermEvent::None;
    }

    switch (key) {
        case Key::Up: return TermEvent::CursorUp;
        case Key::Down: return TermEvent::CursorDown;
        case Key::Left: return TermEvent::GearPrev;
        case Key::Right: return TermEvent::GearNext;
        case Key::X: return TermEvent::LockOrExec;
        case Key::O: return TermEvent::O;
        case Key::Square: return TermEvent::Clear;
        default: return TermEvent::None;
    }
}

void GhostTerminal::applyEvent(TermEvent ev) {
    switch (ev) {
        case TermEvent::CursorUp: moveCursor(-1); break;
        case TermEvent::CursorDown: moveCursor(1); break;
        case TermEvent::GearPrev: gearCli(-1); break;
        case TermEvent::GearNext: gearCli(1); break;
        case TermEvent::LockOrExec: onLockOrExec(); break;
        case TermEvent::O: onO(); break;
        case TermEvent::Clear: clearView(); break;
        case TermEvent::GameToggle:
            if (mode_ == TermMode::Game) leaveGameMode();
            else enterGameMode();
            break;
        case TermEvent::OutputPrev: pageOutput(-1); break;
        case TermEvent::OutputNext: pageOutput(1); break;
        case TermEvent::OutputExit: closeOutput(); break;
        case TermEvent::WatchPrev:
            watch_page_ = static_cast<uint8_t>((watch_page_ + 3u) % 4u);
            force_draw_ = 1;
            break;
        case TermEvent::WatchNext:
            watch_page_ = static_cast<uint8_t>((watch_page_ + 1u) % 4u);
            force_draw_ = 1;
            break;
        case TermEvent::WatchRefresh:
            watch_hidden_ = 0;
            force_draw_ = 1;
            break;
        case TermEvent::WatchFocusPrev: {
            uint8_t n = registry_ != nullptr ? registry_->getDeviceCount() : 0;
            if (n != 0) {
                watch_focus_ = static_cast<uint8_t>((watch_focus_ + n - 1u) % n);
                force_draw_ = 1;
            }
            break;
        }
        case TermEvent::WatchFocusNext: {
            uint8_t n = registry_ != nullptr ? registry_->getDeviceCount() : 0;
            if (n != 0) {
                watch_focus_ = static_cast<uint8_t>((watch_focus_ + 1u) % n);
                force_draw_ = 1;
            }
            break;
        }
        case TermEvent::ExitHive:
            running_ = false;
            break;
        case TermEvent::DismissWatch:
            watch_hidden_ = 1;
            force_draw_ = 1;
            break;
        case TermEvent::None: break;
    }
}

void GhostTerminal::syncLock() {
    if (lockIndex_ >= 0) lockIndex_ = static_cast<int8_t>(cursor_);
}

bool GhostTerminal::onLive() const {
    return cursor_ == historyCount_;
}

void GhostTerminal::goLive() {
    zahnrad_ = 0;
    lockIndex_ = -1;
    cursor_ = historyCount_;
}

void GhostTerminal::moveCursor(int8_t dir) {
    if (mode_ != TermMode::Terminal) return;
    int max = static_cast<int>(historyCount_);
    int n = static_cast<int>(cursor_) + static_cast<int>(dir);
    if (n < 0) n = 0;
    if (n > max) n = max;
    cursor_ = static_cast<uint8_t>(n);
    syncLock();
}

void GhostTerminal::gearCli(int8_t dir) {
    if (dir == 0) return;
    if (!onLive()) return;
    int n = static_cast<int>(zahnrad_) + static_cast<int>(dir);
    if (n < 0) n = static_cast<int>(CMD_COUNT) - 1;
    if (n >= static_cast<int>(CMD_COUNT)) n = 0;
    zahnrad_ = static_cast<uint8_t>(n);
    syncLock();
}

void GhostTerminal::onLockOrExec() {
    if (mode_ != TermMode::Terminal) return;
    if (lockIndex_ < 0) {
        lockIndex_ = static_cast<int8_t>(cursor_);
        return;
    }
    uint8_t idx = static_cast<uint8_t>(lockIndex_);
    lockIndex_ = -1;
    if (idx >= historyCount_) execute(zahnrad_);
    else openHistorySnapshot(static_cast<uint8_t>(historyCount_ - 1u - idx));
}

void GhostTerminal::onO() {
    if (mode_ == TermMode::Output) {
        closeOutput();
        return;
    }
    if (mode_ == TermMode::Black) {
#if defined(GHOST_DEBUG_CLI)
        enterTerminalMode();
#else
        enterWatchMode();
#endif
        force_draw_ = 1;
        screenLocked_ = 0;
        return;
    }
    if (mode_ == TermMode::Watch) {
        enterBlackMode();
        return;
    }
    if (mode_ != TermMode::Terminal) return;
    if (lockIndex_ >= 0) lockIndex_ = -1;
    else enterBlackMode();
}

void GhostTerminal::execute(uint8_t index) {
    if (index >= CMD_COUNT) return;

    char buffer[OUTPUT_LEN];
    buffer[0] = '\0';

    switch (index) {
        case 0:
            if (registry_ != nullptr && events_ != nullptr && vault_ != nullptr) {
                output_.buildStatus(buffer, *registry_, *events_, *vault_, stealth_);
            } else {
                output_.buildStatus(buffer);
            }
            break;
        case 1:
            if (registry_) output_.buildDevices(*registry_, buffer);
            break;
        case 2:
            output_.buildPolicy(policy_, buffer);
            break;
        case 3:
            scanner_.scanWifi();
            output_.buildScan(scanner_, buffer);
            scanner_.releaseBuffer();
            break;
        case 4:
            if (vault_ != nullptr) output_.buildVault(*vault_, buffer);
            else output_.buildBackup(buffer);
            break;
        case 5:
            output_.buildAlert(buffer);
            break;
        case 6:
            output_.buildGhostDown(buffer, down_, now_, stealth_.isGameMode());
            break;
        case 7:
            if (pipeline_ != nullptr) peek_.ingestGuard(pipeline_->replay());
            else peek_.perform();
            output_.buildPeek(peek_, buffer);
            break;
        case 8:
            output_.buildDanger(buffer);
            break;
        case 9:
            output_.buildMineCheck(peek_, buffer);
            break;
        case 10: {
            uint8_t n = 0;
            if (pipeline_ != nullptr) {
                ReplayGuard& rg = pipeline_->replay();
                uint8_t tracked = rg.trackedCount();
                for (uint8_t i = 0; i < tracked; ++i) {
                    const char* mid = rg.mineIdAt(i);
                    if (mid == nullptr || mid[0] == '\0') continue;
                    if (registry_ != nullptr) rg.blockMine(mid, *registry_);
                    else rg.blockMine(mid);
                    ++n;
                }
                if (registry_ != nullptr) {
                    uint8_t dc = registry_->getDeviceCount();
                    for (uint8_t i = 0; i < dc; ++i) {
                        DeviceInfo info = registry_->getDeviceInfo(i);
                        if (info.role != ROLE_MINE || info.id[0] == '\0') continue;
                        if (rg.isBlocked(info.id)) continue;
                        rg.blockMine(info.id, *registry_);
                        ++n;
                    }
                }
            }
            output_.buildMineBlock(n, buffer);
            break;
        }
        case 11:
            output_.buildTime(psp_now_sec(), buffer);
            break;
        default:
            return;
    }

    saveToHistory(commands_[index], buffer);
    openOutput(buffer);
}

void GhostTerminal::openHistorySnapshot(uint8_t index) {
    if (index >= historyCount_) return;
    openOutput(history_[index].snapshot);
}

void GhostTerminal::openOutput(const char* text) {
    if (text == nullptr) return;
    if (outputCount_ >= OUTPUT_MAX) {
        for (uint8_t i = 0; i < (OUTPUT_MAX - 1); ++i) {
            uint16_t n = 0;
            while (n < OUTPUT_LEN) {
                outputs_[i][n] = outputs_[i + 1][n];
                if (outputs_[i][n] == '\0') break;
                ++n;
            }
        }
        outputCount_ = OUTPUT_MAX - 1;
    }
    uint16_t i = 0;
    while (text[i] != '\0' && i < (OUTPUT_LEN - 1)) {
        outputs_[outputCount_][i] = text[i];
        ++i;
    }
    outputs_[outputCount_][i] = '\0';
    outputIndex_ = outputCount_;
    ++outputCount_;
    mode_ = TermMode::Output;
}

void GhostTerminal::closeOutput() {
    goLive();
#if defined(GHOST_DEBUG_CLI)
    enterTerminalMode();
#else
    enterWatchMode();
#endif
}

void GhostTerminal::clearView() {
    outputCount_ = 0;
    outputIndex_ = 0;
    goLive();
#if defined(GHOST_DEBUG_CLI)
    enterTerminalMode();
#else
    enterWatchMode();
#endif
}

void GhostTerminal::sessionReset() {
    cursor_ = 0;
    zahnrad_ = 0;
    lockIndex_ = -1;
    outputCount_ = 0;
    outputIndex_ = 0;
    historyCount_ = 0;
    resumeMode_ = TermMode::Terminal;
#if defined(GHOST_DEBUG_CLI)
    enterTerminalMode();
#else
    watch_page_ = 0;
    enterWatchMode();
#endif
}

void GhostTerminal::enterBlackMode() {
    screenLocked_ = 1;
    mode_ = TermMode::Black;
    force_draw_ = 1;
}

void GhostTerminal::enterGameMode() {
    resumeMode_ = mode_;
    mode_ = TermMode::Game;
    screenLocked_ = 0;
    stealth_.enterGameMode();
    scanner_.setTerminalMode(false);
    force_draw_ = 1;
}

void GhostTerminal::leaveGameMode() {
    if (resumeMode_ == TermMode::Output && outputCount_ > 0) {
        stealth_.enterTerminalMode();
        scanner_.setTerminalMode(true);
        screenLocked_ = 0;
        mode_ = TermMode::Output;
    } else {
#if defined(GHOST_DEBUG_CLI)
        enterTerminalMode();
#else
        enterWatchMode();
#endif
    }
    resumeMode_ = TermMode::Terminal;
    force_draw_ = 1;
}

void GhostTerminal::pageOutput(int8_t dir) {
    if (outputCount_ == 0 || dir == 0) return;
    int n = static_cast<int>(outputIndex_) + static_cast<int>(dir);
    if (n < 0) n = static_cast<int>(outputCount_) - 1;
    if (n >= static_cast<int>(outputCount_)) n = 0;
    outputIndex_ = static_cast<uint8_t>(n);
}

void GhostTerminal::saveToHistory(const char* cmd, const char* snapshot) {
    if (cmd == nullptr || snapshot == nullptr) return;
    if (historyCount_ < SNAPSHOT_MAX) ++historyCount_;
    for (uint8_t i = historyCount_; i > 0; --i) {
        if (i < SNAPSHOT_MAX) history_[i] = history_[i - 1];
    }
    uint8_t i = 0;
    while (cmd[i] != '\0' && i < (CMD_LEN - 1)) {
        history_[0].command[i] = cmd[i];
        ++i;
    }
    history_[0].command[i] = '\0';
    i = 0;
    while (snapshot[i] != '\0' && i < (OUTPUT_LEN - 1)) {
        history_[0].snapshot[i] = snapshot[i];
        ++i;
    }
    history_[0].snapshot[i] = '\0';
}

void GhostTerminal::paintChrome() {
    framePut(0, 0, "ghost:hive");
}

void GhostTerminal::paintPromptRow(uint8_t row, bool on, bool lk, bool live,
                                  const char* cmd) {
    if (lk) framePut(row, 0, "*");
    else if (on) framePut(row, 0, ">");
    framePut(row, 1, "ghost_$");
    if (cmd == nullptr) return;
    if (live) {
        framePut(row, 8, "<");
        framePut(row, 9, cmd);
        uint8_t len = 0;
        while (cmd[len] != '\0' && len < CMD_LEN) ++len;
        framePut(row, static_cast<uint8_t>(9 + len), ">");
    } else {
        framePut(row, 9, cmd);
    }
}

void GhostTerminal::frameClear() {
    for (uint8_t r = 0; r < TERM_ROWS; ++r) {
        for (uint8_t c = 0; c < TERM_COLS; ++c) frame_[r][c] = ' ';
        frame_[r][TERM_COLS] = '\0';
    }
}

void GhostTerminal::framePut(uint8_t row, uint8_t col, const char* text) {
    if (row >= TERM_ROWS || text == nullptr) return;
    uint8_t c = col;
    uint8_t i = 0;
    while (text[i] != '\0' && c < TERM_COLS) {
        frame_[row][c] = text[i];
        ++c;
        ++i;
    }
}

void GhostTerminal::fillWatch() {
    GhostOutput::WatchSrc s;
    s.page = watch_page_;
    s.now_ms = now_;
    s.now_sec = psp_now_sec();
    if (down_.isActive() && now_ >= down_t0_) s.down_elapsed_ms = now_ - down_t0_;
    else s.down_elapsed_ms = 0;
    s.bind_ok = watch_bind_ok() ? 1 : 0;
    s.running = running_ ? 1 : 0;
    s.registry = registry_;
    s.events = events_;
    s.vault = vault_;
    s.stealth = &stealth_;
    s.scanner = &scanner_;
    s.down = &down_;
    s.peek = &peek_;
    if (pipeline_ != nullptr) s.heartbeat = &pipeline_->heartbeat();
    else s.heartbeat = &heartbeat_;
    if (pipeline_ != nullptr) s.policy = &pipeline_->policy();
    else s.policy = &policy_;
    if (pipeline_ != nullptr) s.replay = &pipeline_->replay();
    else s.replay = nullptr;
    s.pipeline = pipeline_;
    s.hmac_i = 0;
    s.hmac_alert = 0;
    s.ack_n = 0;
    s.ack_bud = 0;
    if (xport_ != nullptr) {
        s.hmac_i = xport_->hmacICount();
        s.hmac_alert = xport_->hmacIAlerted();
        s.ack_n = xport_->pendingAckCount();
        s.ack_bud = xport_->ackBudgetUsed();
    }
    {
        uint8_t n = registry_ != nullptr ? registry_->getDeviceCount() : 0;
        if (n == 0) watch_focus_ = 0;
        else if (watch_focus_ >= n) watch_focus_ = 0;
        s.focus = watch_focus_;
    }
    watch_alarm_ = watch_danger_headline(s) ? 1 : 0;
    if (watch_alarm_ != 0 && mode_ == TermMode::Watch && !down_.isActive()) {
        uint8_t pulse = static_cast<uint8_t>((now_ / WATCH_PULSE_MS) & 1u);
        if (pulse != watch_pulse_) {
            watch_pulse_ = pulse;
#if !defined(__PSP__)
            force_draw_ = 1;
#endif
        }
    } else {
        watch_pulse_ = 0;
    }
    output_.buildWatchPage(watch_text_, s);
}

void GhostTerminal::paintWatch() {
    fillWatch();
    uint8_t row = 0;
    uint8_t col = 0;
    uint16_t i = 0;
    while (watch_text_[i] != '\0' && row < TERM_ROWS) {
        if (watch_text_[i] == '\n' || col >= TERM_COLS) {
            ++row;
            col = 0;
            if (watch_text_[i] == '\n') {
                ++i;
                continue;
            }
        }
        if (row >= TERM_ROWS) break;
        frame_[row][col] = watch_text_[i];
        ++col;
        ++i;
    }
}

void GhostTerminal::buildFrame() {
    frameClear();
    if (mode_ == TermMode::Black) {
        return;
    }
    if (mode_ == TermMode::Watch) {
        if (watch_hidden_) {
            return;
        }
        paintWatch();
        return;
    }
    if (mode_ == TermMode::Game) {
        framePut(0, 0, "ghost:hive");
        framePut(TERM_ROWS - 1, 0, "SELECT+START");
        return;
    }
    if (mode_ == TermMode::GhostDown) {
        return;
    }

    paintChrome();

    if (mode_ == TermMode::Output) {
        if (outputCount_ > 0 && outputIndex_ < outputCount_) {
            const char* t = outputs_[outputIndex_];
            uint8_t row = 1;
            uint8_t col = 0;
            uint16_t i = 0;
            while (t[i] != '\0' && row < TERM_ROWS) {
                if (t[i] == '\n' || col >= TERM_COLS) {
                    ++row;
                    col = 0;
                    if (t[i] == '\n') {
                        ++i;
                        continue;
                    }
                }
                if (row >= TERM_ROWS) break;
                frame_[row][col] = t[i];
                ++col;
                ++i;
            }
        }
        return;
    }

    uint8_t row = 1;
    for (uint8_t d = 0; d < historyCount_ && row < TERM_ROWS; ++d, ++row) {
        uint8_t slot = static_cast<uint8_t>(historyCount_ - 1u - d);
        bool on = (cursor_ == d);
        bool lk = (lockIndex_ == static_cast<int8_t>(d));
        paintPromptRow(row, on, lk, false, history_[slot].command);
    }
    if (row < TERM_ROWS) {
        bool on = onLive();
        bool lk = (lockIndex_ == static_cast<int8_t>(historyCount_));
        paintPromptRow(row, on, lk, true, commands_[zahnrad_]);
    }
}

void GhostTerminal::draw() {
    buildFrame();
#if defined(__PSP__)
    static char prev[TERM_ROWS][TERM_COLS + 1];
    static uint8_t have = 0;
    static uint8_t was_black = 0;
    uint8_t changed = (have == 0 || force_draw_ != 0) ? 1 : 0;
    force_draw_ = 0;
    if (changed == 0) {
        for (uint8_t r = 0; r < TERM_ROWS; ++r) {
            uint8_t c = 0;
            while (frame_[r][c] != '\0' && prev[r][c] == frame_[r][c]) ++c;
            if (frame_[r][c] != prev[r][c]) {
                changed = 1;
                break;
            }
        }
    }
    if (changed == 0) return;
    const uint8_t black = (mode_ == TermMode::Black) ? 1 : 0;
    if (black != 0 || was_black != 0 || have == 0) {
        pspDebugScreenClear();
    }
    was_black = black;
    if (black != 0) {
        have = 1;
        return;
    }
    const u32 green = 0x0000FF00;
    const u32 red = 0x000000FF;
    const u32 alarm_a = 0x0000FFFF;
    const uint8_t down_ui = (mode_ == TermMode::Watch && down_.isActive() &&
                             watch_hidden_ == 0) ? 1 : 0;
    const uint8_t alarm_ui = (mode_ == TermMode::Watch && down_ui == 0 &&
                              watch_hidden_ == 0 && watch_alarm_ != 0) ? 1 : 0;
    for (uint8_t r = 0; r < TERM_ROWS; ++r) {
        u32 colr = green;
        if (down_ui && r == 0) colr = red;
        else if (alarm_ui && r == 0) colr = alarm_a;
        pspDebugScreenSetTextColor(colr);
        pspDebugScreenSetXY(0, static_cast<int>(r));
        char line[TERM_COLS + 1];
        uint8_t c = 0;
        while (c < TERM_COLS) {
            char chv = frame_[r][c];
            line[c] = (chv == '\0') ? ' ' : chv;
            ++c;
        }
        line[TERM_COLS] = '\0';
        pspDebugScreenPuts(line);
        c = 0;
        while (c <= TERM_COLS) {
            prev[r][c] = frame_[r][c];
            ++c;
        }
    }
    pspDebugScreenSetTextColor(green);
    have = 1;
#else
    const char* keys = getenv("GHOST_PSP_KEYS");
    if (keys != nullptr && keys[0] == '1') {
        static char prev[TERM_ROWS][TERM_COLS + 1];
        static uint8_t have = 0;
        uint8_t changed = (have == 0) ? 1 : 0;
        for (uint8_t r = 0; r < TERM_ROWS; ++r) {
            uint8_t c = 0;
            while (frame_[r][c] != '\0' && prev[r][c] == frame_[r][c]) ++c;
            if (frame_[r][c] != prev[r][c]) changed = 1;
        }
        if (changed) {
            fputs("\n---\n", stderr);
            for (uint8_t r = 0; r < TERM_ROWS; ++r) {
                if (r == 0 && watch_alarm_ != 0 &&
                    !(mode_ == TermMode::Watch && down_.isActive())) {
                    fputs("\033[7;33m", stderr);
                } else {
                    fputs("\033[32m", stderr);
                }
                fputs(frame_[r], stderr);
                fputs("\033[0m\n", stderr);
                uint8_t c = 0;
                while (c <= TERM_COLS) {
                    prev[r][c] = frame_[r][c];
                    ++c;
                }
            }
            have = 1;
        }
    }
#endif
}
