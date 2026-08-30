#ifndef GHOST_DOWN_H
#define GHOST_DOWN_H

// =====================================================
// Ghost Hive v1.7.2
// Ghost Down
// Spec-Basis: §12.6, §40, §41, §44
// §9 verbietet ghost_down → game_mode als Zustandswechsel.
// §12.6 / §40.6 Game-Mode ist die Tarn-Aktion (Funk tot, sieht aus
// wie ein Spiel). PSP-State bleibt ghost_down.
// Noah: self-start — Tarnung sofort im kritischen Zustand, nicht
// erst nach NAS-Flush, sonst bleibt Funk offen.
// =====================================================

#include "ghost_core.h"
#include "ghost_vault.h"
#include "ghost_stealth.h"

class GhostTransport;

const uint32_t NAS_FLUSH_TIMEOUT_SEC = 5;
const uint32_t STORAGE_FLUSH_DELAY_SEC = 30;

enum class DownStep : uint8_t {
    Idle,
    RamSnapshot,
    FinalSnapshot,
    NasFlushWait,
    Kill,
    StealthConceal,
    Stop,
    StorageFlushWait,
    StorageFlushRetry,
    Done
};

class GhostDown {
public:
    GhostDown();

    void init();
    void attach(GhostVault* vault, GhostStealth* stealth);
    void attachTransport(GhostTransport* transport);
    void execute(uint32_t now);
    void execute(GhostVault& vault, GhostStealth& stealth, uint32_t now);
    void tick(uint32_t now);

    bool isActive() const;
    DownStep step() const;
    bool peekAllowed() const;
    bool coldStartAvoided() const;
    bool nasFlushTimedOut() const;
    bool storageFlushDone() const;
    uint8_t snapshotCount() const;
    bool killSent() const;
    uint32_t nasDueMs() const;
    uint32_t storageDueMs() const;

private:
    void concealGame();

    DownStep step_;
    bool active_;
    bool killSent_;
    bool nasTimedOut_;
    bool storageDone_;
    bool storageRetryDone_;
    bool coldStartAvoided_;
    uint32_t nasDue_;
    uint32_t storageDue_;
    Event snapshot_[VAULT_RAM_SLOTS];
    uint8_t snapshotCount_;
    GhostVault* vault_;
    GhostStealth* stealth_;
    GhostTransport* transport_;
};

#endif // GHOST_DOWN_H
