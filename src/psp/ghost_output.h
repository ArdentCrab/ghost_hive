#ifndef GHOST_OUTPUT_H
#define GHOST_OUTPUT_H

// =====================================================
// Ghost Hive v1.7.1
// Output Engine — P4 FINAL
// Spec-Basis: §34 CLI, §35 Views
// Views: Geräte, Events, Heartbeat, Vault, Policy, Minen, Replay
// =====================================================

#include "ghost_core.h"
#include "ghost_data.h"
#include "registry.h"
#include "event_queue.h"
#include "ghost_scanner.h"
#include "ghost_heartbeat.h"
#include "ghost_stealth.h"
#include "ghost_peek.h"
#include "ghost_vault.h"
#include "replay_guard.h"
#include "ghost_policy.h"
#include "ghost_down.h"

class DecisionPipeline;

const uint16_t OUTPUT_BUFFER_LEN = 1024;

enum class OutputLevel : uint8_t {
    Normal = 0,
    Verbose = 1,
    Trace = 2
};

class GhostOutput {
public:
    GhostOutput();

    void setLevel(OutputLevel level);
    OutputLevel level() const;

    void buildStatus(char* buffer);
    void buildStatus(char* buffer,
                     Registry& registry,
                     EventQueue& queue,
                     GhostVault& vault,
                     GhostStealth& stealth);

    void buildDevices(Registry& registry, char* buffer);
    void buildEvents(EventQueue& queue, char* buffer);
    void buildHeartbeat(GhostHeartbeat& heartbeat,
                        const char* deviceId,
                        char* buffer);
    void buildHeartbeat(GhostHeartbeat& heartbeat,
                        Registry& registry,
                        char* buffer);
    void buildVault(GhostVault& vault, char* buffer);
    void buildPolicy(GhostPolicy& policy, char* buffer);
    void buildPolicyView(GhostPolicy& policy, char* buffer);
    void buildPolicies(char* buffer);
    void buildMines(ReplayGuard& guard, char* buffer);
    void buildReplay(ReplayGuard& guard, char* buffer);

    void buildScan(GhostScanner& scanner, char* buffer);
    void buildBackup(char* buffer);
    void buildAlert(char* buffer);
    void buildStealth(GhostStealth& stealth, char* buffer);
    void buildPeek(GhostPeek& peek, char* buffer);
    void buildMineCheck(GhostPeek& peek, char* buffer);
    void buildDanger(char* buffer);
    void buildMineBlock(char* buffer);
    void buildMineBlock(uint8_t blocked, char* buffer);
    void buildTime(uint32_t nowSec, char* buffer);
    void buildGhostDown(char* buffer, const GhostDown& down, uint32_t now_ms,
                        bool game);

    struct WatchSrc {
        uint8_t page;
        uint32_t now_ms;
        uint32_t now_sec;
        uint32_t down_elapsed_ms;
        uint8_t bind_ok;
        uint8_t running;
        Registry* registry;
        EventQueue* events;
        GhostVault* vault;
        GhostStealth* stealth;
        GhostScanner* scanner;
        GhostDown* down;
        GhostPeek* peek;
        GhostHeartbeat* heartbeat;
        GhostPolicy* policy;
        ReplayGuard* replay;
        DecisionPipeline* pipeline;
        uint8_t focus;
        uint8_t hmac_i;
        uint8_t hmac_alert;
        uint8_t ack_n;
        uint8_t ack_bud;
    };

    void wlab(char* buffer, const char* lab, const char* val);
    void wlabn(char* buffer, const char* lab, int32_t val);
    void wraw(char* buffer, const char* line);
    void buildWatchPage(char* buffer, const WatchSrc& src);

private:
    OutputLevel level_;

    void append(char* buffer, const char* text);
    void appendNumber(char* buffer, int32_t value);
    void appendU32(char* buffer, uint32_t value);
    void appendHex(char* buffer, uint32_t value);
    void appendEventType(char* buffer, EventType type);
    void kv(char* buffer, const char* key, const char* val);
    void kvn(char* buffer, const char* key, int32_t val);
    void cell(char* buffer, const char* text, uint8_t width);
    void cellN(char* buffer, int32_t value, uint8_t width);
    const char* roleName(uint8_t role) const;
    const char* stateName(DeviceState state) const;
    const char* actionName(PolicyAction action) const;
    const char* encName(uint8_t enc) const;
    void appendRole(char* buffer, uint8_t role);
    void appendState(char* buffer, DeviceState state);
    void appendAction(char* buffer, PolicyAction action);
};

#endif // GHOST_OUTPUT_H
