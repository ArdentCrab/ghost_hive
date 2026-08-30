#include "invariants.h"

#include <stdio.h>

static bool lab_streq(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) return false;
    uint16_t i = 0;
    while (a[i] != '\0' || b[i] != '\0') {
        if (a[i] != b[i]) return false;
        ++i;
        if (i > 32) return false;
    }
    return true;
}

static void lab_cpy(char* dst, uint8_t cap, const char* src) {
    if (dst == nullptr || cap == 0) return;
    uint8_t i = 0;
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1 < cap) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

void lab_snap_clear(LabSnap* s) {
    if (s == nullptr) return;
    s->root_ok = 1;
    s->state[0] = '\0';
    lab_cpy(s->state, 24, "terminal_mode");
    s->game_n = 0;
    lab_cpy(s->last_policy, 24, "drop");
    s->last_counter = 0;
    s->last_totp_win = 0;
    s->frozen = 0;
    s->snap_n = 0;
    s->snap_ran = 0;
    s->kill_sent = 0;
    s->hmac_ok = 1;
    s->accepted = 0;
    s->unsigned_kill = 0;
    s->uk_froze = 0;
    s->replay_attempt = 0;
    s->totp_out = 0;
    s->asan = 0;
    s->target_down = 0;
    s->recovered = 1;
    s->pid = 0;
    s->kind[0] = '\0';
    s->etype[0] = '\0';
    s->now = 0;
}

static bool psp_allowed(const char* a, const char* b) {
    if (lab_streq(a, b)) return true;
    // §10 PSP-State-Machine. ghost_peek is a hop.
    if (lab_streq(a, "game_mode") && lab_streq(b, "terminal_mode")) return true;
    if (lab_streq(a, "terminal_mode") && lab_streq(b, "game_mode")) return true;
    if (lab_streq(a, "terminal_mode") && lab_streq(b, "ghost_down")) return true;
    if (lab_streq(a, "ghost_down") && lab_streq(b, "danger_mode")) return true;
    if (lab_streq(a, "danger_mode") && lab_streq(b, "terminal_mode")) return true;
    if (lab_streq(a, "danger_mode") && lab_streq(b, "ghost_down")) return true;
    if (lab_streq(a, "ghost_down") && lab_streq(b, "low_power_mode")) return true;
    if (lab_streq(a, "low_power_mode") && lab_streq(b, "ghost_peek")) return true;
    if (lab_streq(a, "ghost_peek") && lab_streq(b, "low_power_mode")) return true;
    return false;
}

bool lab_inv01(const LabSnap& s) {
    return s.root_ok != 0;
}

bool lab_inv02(const LabSnap& before, const LabSnap& after) {
    if (before.pid != 0 && after.pid != 0 && before.pid != after.pid) return true;
    if (before.state[0] == '\0' || after.state[0] == '\0') return true;
    return psp_allowed(before.state, after.state);
}

bool lab_inv03(const LabSnap& after) {
    // Invalid HMAC must not be accepted as an authenticated event.
    // last_policy on that event: drop or log_only only.
    if (after.hmac_ok != 0) return true;
    if (after.accepted != 0) return false;
    if (lab_streq(after.last_policy, "drop")) return true;
    if (lab_streq(after.last_policy, "log_only")) return true;
    // P15: frame still dropped; hmac_i names the reject, not accept.
    if (lab_streq(after.last_policy, "hmac_i")) return true;
    return false;
}

bool lab_inv04(const LabSnap& after) {
    if (after.replay_attempt == 0) return true;
    return after.accepted == 0;
}

bool lab_inv05(const LabSnap& after) {
    if (after.totp_out == 0) return true;
    return after.accepted == 0;
}

bool lab_inv06(const LabSnap& after) {
    if (after.asan != 0) return false;
    if (after.target_down != 0 && after.recovered == 0) return false;
    return true;
}

bool lab_inv07(const LabSnap& before, const LabSnap& after) {
    /* SPEC-v1: uk_froze is per-frame latch, not 1 Hz smear. */
    if (before.pid != 0 && after.pid != 0 && before.pid != after.pid) return true;
    return before.uk_froze == 0 && after.uk_froze == 0;
}

bool lab_inv08(const LabSnap& before, const LabSnap& after) {
    if (before.pid != 0 && after.pid != 0 && before.pid != after.pid) return true;
    if (before.frozen != 0 || after.frozen == 0) return true;
    return after.snap_ran != 0;
}

LabInvResult lab_check_pair(const LabSnap& before, const LabSnap& after) {
    LabInvResult r;
    r.ok = 1;
    r.id[0] = '\0';
    if (!lab_inv01(after)) {
        r.ok = 0;
        lab_cpy(r.id, 12, "INV-01");
        return r;
    }
    if (!lab_inv02(before, after)) {
        r.ok = 0;
        lab_cpy(r.id, 12, "INV-02");
        return r;
    }
    if (!lab_inv03(after)) {
        r.ok = 0;
        lab_cpy(r.id, 12, "INV-03");
        return r;
    }
    if (!lab_inv04(after)) {
        r.ok = 0;
        lab_cpy(r.id, 12, "INV-04");
        return r;
    }
    if (!lab_inv05(after)) {
        r.ok = 0;
        lab_cpy(r.id, 12, "INV-05");
        return r;
    }
    if (!lab_inv06(after)) {
        r.ok = 0;
        lab_cpy(r.id, 12, "INV-06");
        return r;
    }
    if (!lab_inv07(before, after)) {
        r.ok = 0;
        lab_cpy(r.id, 12, "INV-07");
        return r;
    }
    if (!lab_inv08(before, after)) {
        r.ok = 0;
        lab_cpy(r.id, 12, "INV-08");
        return r;
    }
    return r;
}

bool lab_snap_encode(const LabSnap& s, char* out, uint16_t cap) {
    if (out == nullptr || cap < 80) return false;
    int n = snprintf(
        out, cap,
        "GHS1 root_ok=%u state=%s game_n=%u last_policy=%s last_counter=%u "
        "last_totp_win=%u frozen=%u snap_n=%u snap_ran=%u kill_sent=%u "
        "hmac_ok=%u accepted=%u unsigned_kill=%u uk_froze=%u replay_attempt=%u totp_out=%u "
        "asan=%u target_down=%u recovered=%u pid=%u kind=%s etype=%s now=%u\n",
        static_cast<unsigned>(s.root_ok),
        s.state[0] != '\0' ? s.state : "terminal_mode",
        s.game_n,
        s.last_policy[0] != '\0' ? s.last_policy : "drop",
        s.last_counter,
        s.last_totp_win,
        static_cast<unsigned>(s.frozen),
        static_cast<unsigned>(s.snap_n),
        static_cast<unsigned>(s.snap_ran),
        static_cast<unsigned>(s.kill_sent),
        static_cast<unsigned>(s.hmac_ok),
        static_cast<unsigned>(s.accepted),
        static_cast<unsigned>(s.unsigned_kill),
        static_cast<unsigned>(s.uk_froze),
        static_cast<unsigned>(s.replay_attempt),
        static_cast<unsigned>(s.totp_out),
        static_cast<unsigned>(s.asan),
        static_cast<unsigned>(s.target_down),
        static_cast<unsigned>(s.recovered),
        s.pid,
        s.kind[0] != '\0' ? s.kind : "-",
        s.etype[0] != '\0' ? s.etype : "-",
        s.now);
    return n > 0 && static_cast<uint16_t>(n) < cap;
}

static const char* lab_find_key(const char* line, const char* key) {
    if (line == nullptr || key == nullptr) return nullptr;
    uint16_t i = 0;
    uint8_t klen = 0;
    while (key[klen] != '\0') ++klen;
    while (line[i] != '\0') {
        uint8_t j = 0;
        while (j < klen && line[i + j] == key[j]) ++j;
        if (j == klen && line[i + j] == '=') {
            return line + i + j + 1;
        }
        ++i;
    }
    return nullptr;
}

static uint32_t lab_parse_u32(const char* p) {
    if (p == nullptr) return 0;
    uint32_t v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10u + static_cast<uint32_t>(*p - '0');
        ++p;
    }
    return v;
}

static void lab_parse_tok(const char* p, char* dst, uint8_t cap) {
    if (dst == nullptr || cap == 0) return;
    uint8_t i = 0;
    if (p == nullptr) {
        dst[0] = '\0';
        return;
    }
    while (p[i] != '\0' && p[i] != ' ' && p[i] != '\n' && i + 1 < cap) {
        dst[i] = p[i];
        ++i;
    }
    dst[i] = '\0';
}

bool lab_snap_parse(const char* line, LabSnap* s) {
    if (line == nullptr || s == nullptr) return false;
    lab_snap_clear(s);
    if (line[0] != 'G' || line[1] != 'H' || line[2] != 'S' || line[3] != '1') {
        return false;
    }
    s->root_ok = static_cast<uint8_t>(lab_parse_u32(lab_find_key(line, "root_ok")));
    lab_parse_tok(lab_find_key(line, "state"), s->state, 24);
    s->game_n = lab_parse_u32(lab_find_key(line, "game_n"));
    lab_parse_tok(lab_find_key(line, "last_policy"), s->last_policy, 24);
    s->last_counter = lab_parse_u32(lab_find_key(line, "last_counter"));
    s->last_totp_win = lab_parse_u32(lab_find_key(line, "last_totp_win"));
    s->frozen = static_cast<uint8_t>(lab_parse_u32(lab_find_key(line, "frozen")));
    s->snap_n = static_cast<uint8_t>(lab_parse_u32(lab_find_key(line, "snap_n")));
    s->snap_ran = static_cast<uint8_t>(lab_parse_u32(lab_find_key(line, "snap_ran")));
    s->kill_sent = static_cast<uint8_t>(lab_parse_u32(lab_find_key(line, "kill_sent")));
    s->hmac_ok = static_cast<uint8_t>(lab_parse_u32(lab_find_key(line, "hmac_ok")));
    s->accepted = static_cast<uint8_t>(lab_parse_u32(lab_find_key(line, "accepted")));
    s->unsigned_kill = static_cast<uint8_t>(
        lab_parse_u32(lab_find_key(line, "unsigned_kill")));
    s->uk_froze = static_cast<uint8_t>(
        lab_parse_u32(lab_find_key(line, "uk_froze")));
    s->replay_attempt = static_cast<uint8_t>(
        lab_parse_u32(lab_find_key(line, "replay_attempt")));
    s->totp_out = static_cast<uint8_t>(lab_parse_u32(lab_find_key(line, "totp_out")));
    s->asan = static_cast<uint8_t>(lab_parse_u32(lab_find_key(line, "asan")));
    s->target_down = static_cast<uint8_t>(
        lab_parse_u32(lab_find_key(line, "target_down")));
    s->recovered = static_cast<uint8_t>(lab_parse_u32(lab_find_key(line, "recovered")));
    s->pid = lab_parse_u32(lab_find_key(line, "pid"));
    lab_parse_tok(lab_find_key(line, "kind"), s->kind, 16);
    lab_parse_tok(lab_find_key(line, "etype"), s->etype, 24);
    s->now = lab_parse_u32(lab_find_key(line, "now"));
    return true;
}
