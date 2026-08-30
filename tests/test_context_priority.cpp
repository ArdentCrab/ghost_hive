#include <cstdio>
#include "../src/psp/context_engine.h"
#include "../src/psp/priority_engine.h"

int main() {
    ContextEngine ctx;
    PriorityEngine prio;

    bool ok = true;

    ContextMode home = ctx.compute("FritzBox", "192.168.1.0/24", true, true);
    ContextMode mobile = ctx.compute("Mobile", "", false, true);
    ContextMode offline = ctx.compute(nullptr, "", false, false);

    if (home != ContextMode::Home) ok = false;
    if (mobile != ContextMode::Mobile) ok = false;
    if (offline != ContextMode::Offline) ok = false;

    Priority w = prio.compute(1, 2, true, true);
    Priority p = prio.compute(3, 3, true, true);

    if (w != Priority::Worker) ok = false;
    if (p != Priority::Psp) ok = false;

    printf(ok ? "PASS context_priority\n" : "FAIL context_priority\n");
    return ok ? 0 : 1;
}
