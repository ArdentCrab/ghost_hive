#ifndef DECISION_PIPELINE_H
#define DECISION_PIPELINE_H

// =====================================================
// Ghost Hive v1.7.1
// Decision Pipeline
// Spec-Basis: §15, §17, §18, §20, §44
// =====================================================

#include "ghost_core.h"
#include "registry.h"
#include "replay_guard.h"
#include "ghost_policy.h"
#include "priority_engine.h"
#include "fallback_engine.h"
#include "ghost_heartbeat.h"
#include "event_queue.h"
#include "ghost_vault.h"

class GhostTransport;

const uint8_t TELEM_REPLAY_SLOTS = 64;

enum class PipelineResult : uint8_t {
    Accepted,
    Rejected,
    Escalated,
    Blocked
};

class DecisionPipeline {
public:
    DecisionPipeline();
    ~DecisionPipeline();

    void attach(Registry* registry, GhostVault* vault);
    void attachTransport(GhostTransport* transport);
    void drain(EventQueue& queue, uint32_t now);
    PipelineResult process(const Event& event, uint32_t now);

    ReplayGuard& replay();
    GhostHeartbeat& heartbeat();
    GhostPolicy& policy();
    uint32_t telemDenseDrops(const char* id) const;

private:
    ReplayGuard replay_;
    GhostPolicy policy_;
    PriorityEngine priority_;
    FallbackEngine fallback_;
    GhostHeartbeat heartbeat_;
    Registry* registry_;
    GhostVault* vault_;
    GhostTransport* transport_;
    Priority lastPriority_;
    TaskTarget lastFallback_;
    uint8_t classRoute_;
    bool priorityHasPeer_;

    struct TelemRepSlot {
        uint32_t ts;
        uint8_t blob[10];
        uint8_t used;
    };
    struct TelemRepDev {
        char id[32];
        TelemRepSlot slot[TELEM_REPLAY_SLOTS];
        uint8_t next;
        uint8_t live;
        uint8_t rate_used;
        uint32_t last_ok;
        uint32_t last_event_ts;
        uint32_t dense_drops;
    };
    TelemRepDev telem_rep_[MAX_DEVICES];

    static void onDeviceRemoved(void* ctx, const char* id);
    void recycleTelem(const char* id);
    void clearTelemDev(uint8_t i);

    int16_t telemDevIndex(const char* id) const;
    bool telemReplayOk(const Event& event, uint32_t now);
    bool handleTelemetryUpdate(Event& event, uint32_t now);

    bool validate(const Event& event) const;
    bool replayGuard(const Event& event, uint32_t now);
    bool enrich(Event& event, uint32_t now);
    bool classify(Event& event);
    bool checkPolicies(const Event& event);
    bool computePriority(const Event& event);
    bool computeFallback(const Event& event);
    bool computeEscalation(const Event& event);
    bool route(const Event& event, uint32_t now);
    bool store(const Event& event, uint32_t now);
    bool ack(const Event& event, uint32_t now);

    void applyTimeAnchor(const Event& event);
    void applyFallback(const Event& event);
    void degradeOnline(const char* id);
    bool payloadIs(const Event& event, const char* tag) const;

    void parseMinePayload(const char* payload,
                          uint32_t& counter,
                          uint32_t& totp) const;
};

#endif // DECISION_PIPELINE_H
