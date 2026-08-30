#include "invariants.h"

#include <stdio.h>

static uint32_t g_n = 0;
static uint32_t g_bad = 0;

static void chk_at(bool c, int line) {
    ++g_n;
    if (!c) {
        ++g_bad;
        printf("fail line %d\n", line);
    }
}

#define chk(c) chk_at((c), __LINE__)

int main() {
    LabSnap a;
    LabSnap b;
    lab_snap_clear(&a);
    lab_snap_clear(&b);

    chk(lab_inv01(a));
    a.root_ok = 0;
    chk(!lab_inv01(a));

    lab_snap_clear(&a);
    lab_snap_clear(&b);
    snprintf(a.state, 24, "terminal_mode");
    snprintf(b.state, 24, "ghost_down");
    chk(lab_inv02(a, b));
    snprintf(b.state, 24, "game_mode");
    snprintf(a.state, 24, "ghost_down");
    chk(!lab_inv02(a, b));
    snprintf(a.state, 24, "game_mode");
    snprintf(b.state, 24, "danger_mode");
    chk(!lab_inv02(a, b));
    snprintf(a.state, 24, "game_mode");
    snprintf(b.state, 24, "terminal_mode");
    chk(lab_inv02(a, b));
    a.pid = 1;
    b.pid = 2;
    snprintf(a.state, 24, "ghost_down");
    snprintf(b.state, 24, "terminal_mode");
    chk(lab_inv02(a, b));

    lab_snap_clear(&b);
    b.hmac_ok = 0;
    b.accepted = 0;
    snprintf(b.last_policy, 24, "drop");
    chk(lab_inv03(b));
    b.accepted = 1;
    chk(!lab_inv03(b));
    b.accepted = 0;
    snprintf(b.last_policy, 24, "ghost_down");
    chk(!lab_inv03(b));

    lab_snap_clear(&b);
    b.replay_attempt = 1;
    b.accepted = 0;
    chk(lab_inv04(b));
    b.accepted = 1;
    chk(!lab_inv04(b));

    lab_snap_clear(&b);
    b.totp_out = 1;
    b.accepted = 0;
    chk(lab_inv05(b));
    b.accepted = 1;
    chk(!lab_inv05(b));

    lab_snap_clear(&b);
    chk(lab_inv06(b));
    b.asan = 1;
    chk(!lab_inv06(b));
    b.asan = 0;
    b.target_down = 1;
    b.recovered = 0;
    chk(!lab_inv06(b));
    b.recovered = 1;
    chk(lab_inv06(b));

    lab_snap_clear(&a);
    lab_snap_clear(&b);
    b.unsigned_kill = 1;
    a.frozen = 0;
    b.frozen = 1;
    b.uk_froze = 0;
    chk(lab_inv07(a, b));
    b.uk_froze = 1;
    chk(!lab_inv07(a, b));
    b.uk_froze = 0;
    a.uk_froze = 1;
    chk(!lab_inv07(a, b));
    a.uk_froze = 0;
    b.frozen = 0;
    chk(lab_inv07(a, b));

    lab_snap_clear(&a);
    lab_snap_clear(&b);
    a.frozen = 0;
    b.frozen = 1;
    b.snap_ran = 1;
    chk(lab_inv08(a, b));
    b.snap_ran = 0;
    chk(!lab_inv08(a, b));

    char line[LAB_SNAP_LINE];
    lab_snap_clear(&a);
    snprintf(a.state, 24, "terminal_mode");
    chk(lab_snap_encode(a, line, LAB_SNAP_LINE));
    LabSnap c;
    chk(lab_snap_parse(line, &c));
    chk(c.root_ok == 1);
    LabInvResult r = lab_check_pair(a, c);
    chk(r.ok == 1);

    lab_snap_clear(&a);
    lab_snap_clear(&b);
    a.frozen = 0;
    b.frozen = 1;
    b.unsigned_kill = 1;
    b.uk_froze = 1;
    b.hmac_ok = 0;
    snprintf(b.last_policy, 24, "drop");
    r = lab_check_pair(a, b);
    chk(r.ok == 0);

    printf(g_bad == 0 ? "PASS lab_invariants n=%u\n" : "FAIL lab_invariants n=%u bad=%u\n",
           static_cast<unsigned>(g_n), static_cast<unsigned>(g_bad));
    return g_bad == 0 ? 0 : 1;
}
