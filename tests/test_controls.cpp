// PSP controls — ghost_$ prompt, no L+R Down, CLI does not start Down.
#include "ghost_terminal.h"
#include "psp_input.h"
#include "registry.h"
#include "event_queue.h"
#include "ghost_vault.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail = 0;

static void chk(bool ok, const char* name) {
    if (ok) printf("PASS %s\n", name);
    else {
        printf("FAIL %s\n", name);
        fail = 1;
    }
}

static void tap(GhostTerminal& t, Key k, uint32_t* now) {
    psp_push_key(k);
    *now += 10;
    t.tick(*now);
}

static bool lineHas(const char* line, const char* needle) {
    return line != nullptr && needle != nullptr && strstr(line, needle) != nullptr;
}

int main() {
    Registry reg;
    EventQueue eq;
    GhostVault vault;
    vault.init();
    GhostTerminal term;
    term.init(&reg, &eq);
    term.attach(&vault, nullptr);
    psp_input_init();

    uint32_t now = 1000;
    term.tick(now);
    term.draw();
    chk(term.mode() == TermMode::Terminal, "boot terminal");
    chk(term.cursor() == 0, "boot cursor live");
    chk(lineHas(term.frameLine(0), "ghost:hive"), "header mark");
    chk(!lineHas(term.frameLine(0), "term"), "header no term");
    chk(!lineHas(term.frameLine(0), "z0"), "header no zahnrad");
    chk(!lineHas(term.frameLine(0), "safe"), "header no safe");
    chk(lineHas(term.frameLine(1), "ghost_$"), "prompt token");
    chk(lineHas(term.frameLine(1), "<hive status>"), "live gear");
    chk(!lineHas(term.frameLine(2), "hive devices"), "no cli menu");
    chk(!lineHas(term.frameLine(0), "DOWN"), "not down");

    tap(term, Key::Right, &now);
    chk(term.cursor() == 0, "gear keeps live cursor");
    chk(term.zahnrad() == 1, "gear right");
    term.draw();
    chk(lineHas(term.frameLine(1), "<hive devices>"), "gear paints");
    tap(term, Key::Left, &now);
    chk(term.zahnrad() == 0, "gear left");
    tap(term, Key::Left, &now);
    chk(term.zahnrad() == 11, "gear wrap left");
    tap(term, Key::Right, &now);
    chk(term.zahnrad() == 0, "gear wrap right");

    tap(term, Key::Down, &now);
    chk(term.cursor() == 0, "down empty hist stays live");
    tap(term, Key::Up, &now);
    chk(term.cursor() == 0, "up empty hist stays live");

    tap(term, Key::Right, &now);
    tap(term, Key::X, &now);
    chk(term.locked(), "lock on");
    term.draw();
    chk(lineHas(term.frameLine(1), "*"), "lock mark on line");
    tap(term, Key::Right, &now);
    chk(term.locked(), "lock follows gear");
    chk(term.zahnrad() == 2, "lock gear 2");
    tap(term, Key::O, &now);
    chk(!term.locked(), "unlock");

    tap(term, Key::L, &now);
    tap(term, Key::R, &now);
    term.draw();
    chk(term.mode() == TermMode::Terminal, "L+R stays terminal");
    chk(!term.down().isActive(), "L+R does not start down");

    tap(term, Key::Triangle, &now);
    chk(term.mode() == TermMode::Terminal, "triangle reserved");

    tap(term, Key::O, &now);
    chk(term.mode() == TermMode::Black, "O black");
    term.draw();
    chk(!lineHas(term.frameLine(0), "ghost:hive"), "black no chrome");
    chk(!lineHas(term.frameLine(TERM_ROWS - 1), "O"), "black blank");
    tap(term, Key::Home, &now);
    chk(term.mode() == TermMode::Black, "home ignored in black");
    tap(term, Key::O, &now);
    chk(term.mode() == TermMode::Terminal, "O back");

    tap(term, Key::SelectStart, &now);
    chk(term.mode() == TermMode::Terminal, "select+start tot");
    tap(term, Key::SelectStart, &now);
    chk(term.mode() == TermMode::Terminal, "select+start still term");

    while (term.zahnrad() != 6) tap(term, Key::Right, &now);
    tap(term, Key::X, &now);
    chk(term.locked(), "x lock");
    tap(term, Key::X, &now);
    chk(term.mode() == TermMode::Output, "ghost down cli opens output");
    chk(!term.down().isActive(), "cli does not start down");
    term.draw();
    chk(lineHas(term.frameLine(1), "down") &&
        (lineHas(term.frameLine(1), "locked") ||
         lineHas(term.frameLine(1), "idle") ||
         lineHas(term.frameLine(1), "observe") ||
         lineHas(term.frameLine(1), "active")),
        "cli status text");
    chk(lineHas(term.frameLine(2), "phase"), "cli phase");
    chk(lineHas(term.frameLine(3), "timer"), "cli timer");
    chk(lineHas(term.frameLine(0), "ghost:hive"), "output same header");
    chk(!lineHas(term.frameLine(0), "DOWN"), "output header no DOWN");
    chk(term.historyCount() >= 1, "history stored");
    chk(!lineHas(term.frameLine(0), "1/1"), "no pager");
    chk(!lineHas(term.frameLine(0), "out"), "header no out word");

    tap(term, Key::SelectStart, &now);
    chk(term.mode() == TermMode::Output, "select+start tot in output");
    tap(term, Key::O, &now);
    chk(term.mode() == TermMode::Terminal, "O exits output");

    tap(term, Key::Home, &now);
    chk(term.mode() == TermMode::Terminal, "home exits output");
    chk(term.cursor() == term.historyCount(), "back on live");
    chk(term.zahnrad() == 0, "new line gear reset");
    term.draw();
    chk(lineHas(term.frameLine(1), "ghost down"), "snapshot keeps cmd");
    chk(!lineHas(term.frameLine(1), "<ghost down>"), "snapshot no gear");
    chk(lineHas(term.frameLine(2), "<hive status>"), "new live gear");

    tap(term, Key::Up, &now);
    chk(term.cursor() == 0, "up onto snapshot");
    uint8_t z = term.zahnrad();
    tap(term, Key::Right, &now);
    chk(term.zahnrad() == z, "gear dead on snapshot");
    tap(term, Key::Down, &now);
    chk(term.cursor() == 1, "down back to live");

    tap(term, Key::X, &now);
    tap(term, Key::X, &now);
    chk(term.mode() == TermMode::Output, "status output");
    tap(term, Key::O, &now);
    tap(term, Key::Right, &now);
    tap(term, Key::X, &now);
    tap(term, Key::X, &now);
    chk(term.outputCount() >= 2, "two output pages");
    uint8_t pg = term.outputIndex();
    tap(term, Key::Right, &now);
    chk(term.outputIndex() != pg, "output d-pad pages");
    tap(term, Key::Left, &now);
    tap(term, Key::Left, &now);
    chk(term.mode() == TermMode::Output, "output wrap stays");

    tap(term, Key::O, &now);
    chk(term.mode() == TermMode::Terminal, "output exit");
    now += AUTO_RESET_MS + 1;
    term.tick(now);
    chk(term.mode() == TermMode::Terminal, "auto-reset terminal");
    chk(term.cursor() == 0, "auto-reset cursor");
    chk(term.zahnrad() == 0, "auto-reset zahnrad");
    chk(!term.locked(), "auto-reset unlock");
    chk(term.historyCount() == 0, "auto-reset history");

    (void)unsetenv("GHOST_ARMING");
    if (setenv("GHOST_DOWN_ARMED", "0", 1) == 0) {
        GhostDown inert;
        inert.execute(9000);
        chk(inert.isActive(), "locked conceal active");
        chk(!inert.killSent(), "locked no kill");
    }

    if (fail) {
        printf("FAIL test_controls\n");
        return 1;
    }
    printf("PASS test_controls\n");
    return 0;
}
