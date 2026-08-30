#ifndef GHOST_POLICY_H
#define GHOST_POLICY_H

// =====================================================
// Ghost Hive v1.7.1
// Policy Engine
// Spec-Basis: §12.4, §16, §17
// P01–P16, DSL: condition = expression (AND expression)*
// =====================================================

#include "ghost_core.h"

enum class PolicyAction : uint8_t {
    LogOnly,
    Alert,
    Backup,
    Block,
    Kill,
    GhostDown
};

const uint8_t PX_SNAPSHOT   = 1 << 0;
const uint8_t PX_CLASSIFY   = 1 << 1;
const uint8_t PX_DEGRADED   = 1 << 2;
const uint8_t PX_ALERT      = 1 << 3;
const uint8_t PX_VAULT      = 1 << 4;
const uint8_t PX_TIME_ANCHOR= 1 << 5;
const uint8_t PX_CHECK_MINE = 1 << 6;
const uint8_t PX_PASSIVE    = 1 << 7;

struct PolicyRule {
    char id[16];
    char name[32];
    uint8_t scope;
    char condition[128];
    PolicyAction action;
    uint8_t extra;
};

class GhostPolicy {
public:
    GhostPolicy();

    void init();
    void initDefaults();
    PolicyAction evaluate(const Event& event) const;
    uint8_t evaluate(const Event& event, const char* deviceState) const;
    uint8_t evaluateFlags(const Event& event, const char* deviceState) const;
    uint8_t extraFlags(const Event& event) const;
    uint8_t ruleCount() const;
    const PolicyRule* ruleAt(uint8_t index) const;

private:
    PolicyRule rules_[16];
    uint8_t ruleCount_;

    bool matchDsl(const char* dsl, const Event& event) const;
    bool matchExpr(const char* expr, uint8_t len, const Event& event) const;
    static bool strEqRange(const char* a, uint8_t aLen, const char* b);
};

#endif // GHOST_POLICY_H
