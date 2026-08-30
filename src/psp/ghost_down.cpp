#include "ghost_down.h"
#include "transport/ghost_transport.h"
#include "ghost_arm.h"

GhostDown::GhostDown() {
    init();
}

void GhostDown::init() {
    step_ = DownStep::Idle;
    active_ = false;
    killSent_ = false;
    nasTimedOut_ = false;
    storageDone_ = false;
    storageRetryDone_ = false;
    coldStartAvoided_ = true;
    nasDue_ = 0;
    storageDue_ = 0;
    snapshotCount_ = 0;
    vault_ = nullptr;
    stealth_ = nullptr;
    transport_ = nullptr;
}

void GhostDown::attach(GhostVault* vault, GhostStealth* stealth) {
    vault_ = vault;
    stealth_ = stealth;
}

void GhostDown::attachTransport(GhostTransport* transport) {
    transport_ = transport;
}

void GhostDown::execute(GhostVault& vault, GhostStealth& stealth, uint32_t now) {
    attach(&vault, &stealth);
    execute(now);
}

void GhostDown::concealGame() {
    // §12.6 / §40.6: Game-Mode-Look. Kein Invisible (kein Spiel-Look).
    if (stealth_ != nullptr) {
        stealth_->enterGameMode();
    }
}

void GhostDown::execute(uint32_t now) {
    active_ = true;
    killSent_ = false;
    nasTimedOut_ = false;
    storageDone_ = false;
    storageRetryDone_ = false;
    coldStartAvoided_ = true;
    snapshotCount_ = 0;

    step_ = DownStep::StealthConceal;
    concealGame();
    if (vault_ != nullptr) {
        snapshotCount_ = vault_->snapshot(snapshot_, VAULT_RAM_SLOTS);
    }
    if (!ghost_down_armed()) {
        step_ = DownStep::Done;
        return;
    }

    step_ = DownStep::RamSnapshot;
    if (vault_ != nullptr) {
        snapshotCount_ = vault_->snapshot(snapshot_, VAULT_RAM_SLOTS);
    }

    step_ = DownStep::FinalSnapshot;
    if (vault_ != nullptr) {
        snapshotCount_ = vault_->snapshot(snapshot_, VAULT_RAM_SLOTS);
    }

    step_ = DownStep::NasFlushWait;
    nasDue_ = now + NAS_FLUSH_TIMEOUT_SEC;
    if (transport_ != nullptr) {
        (void)transport_->flushNas(now);
    }
}

void GhostDown::tick(uint32_t now) {
    if (!active_) return;
    if (!ghost_down_armed()) return;

    if (step_ == DownStep::NasFlushWait) {
        if (transport_ != nullptr && transport_->nasFlushAcked()) {
            nasTimedOut_ = false;
            step_ = DownStep::Kill;
            if (snapshotCount_ > 0 && transport_ != nullptr) {
                killSent_ = transport_->sendKill(now);
            }
            step_ = DownStep::StealthConceal;
            concealGame();
            step_ = DownStep::Stop;
            storageDue_ = now + STORAGE_FLUSH_DELAY_SEC;
            step_ = DownStep::StorageFlushWait;
            return;
        }
        if (now < nasDue_) return;
        nasTimedOut_ = true;

        // §40.5 / §43 Kill — Snapshot zuerst, nie gegen PSP
        step_ = DownStep::Kill;
        if (snapshotCount_ > 0 && transport_ != nullptr) {
            killSent_ = transport_->sendKill(now);
        }

        // §40.6 Game-Mode-Tarnung, PSP-State bleibt ghost_down (§9)
        step_ = DownStep::StealthConceal;
        concealGame();

        // §40.7 Stop
        step_ = DownStep::Stop;
        storageDue_ = now + STORAGE_FLUSH_DELAY_SEC;
        step_ = DownStep::StorageFlushWait;
        return;
    }

    if (step_ == DownStep::StorageFlushWait) {
        if (now < storageDue_) return;
        if (vault_ != nullptr) {
            storageDone_ = vault_->flushToStorage(now);
        } else {
            storageDone_ = false;
        }
        if (storageDone_) {
            step_ = DownStep::Done;
            active_ = true;
            return;
        }
        step_ = DownStep::StorageFlushRetry;
        storageDue_ = now + STORAGE_FLUSH_DELAY_SEC;
        return;
    }

    if (step_ == DownStep::StorageFlushRetry) {
        if (now < storageDue_) return;
        if (vault_ != nullptr) {
            storageDone_ = vault_->flushToStorage(now);
        }
        storageRetryDone_ = true;
        step_ = DownStep::Done;
    }
}

bool GhostDown::isActive() const {
    return active_;
}

DownStep GhostDown::step() const {
    return step_;
}

bool GhostDown::peekAllowed() const {
    // §41 / §10: Peek nach Down, low_power, kein Kaltstart
    return step_ == DownStep::Stop ||
           step_ == DownStep::StorageFlushWait ||
           step_ == DownStep::StorageFlushRetry ||
           step_ == DownStep::Done;
}

bool GhostDown::coldStartAvoided() const {
    return coldStartAvoided_;
}

bool GhostDown::nasFlushTimedOut() const {
    return nasTimedOut_;
}

bool GhostDown::storageFlushDone() const {
    return storageDone_;
}

uint8_t GhostDown::snapshotCount() const {
    return snapshotCount_;
}

bool GhostDown::killSent() const {
    return killSent_;
}

uint32_t GhostDown::nasDueMs() const {
    return nasDue_ * 1000u;
}

uint32_t GhostDown::storageDueMs() const {
    return storageDue_ * 1000u;
}
