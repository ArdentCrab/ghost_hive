#ifndef HIVE_LOG_CLIENT_H
#define HIVE_LOG_CLIENT_H

// =====================================================
// hive-log-client — read-only Backup/Log
// Spec-Basis: §5, §7, §11, §28
// Laptop hat R, nicht B. Kein Vault-Flush.
// =====================================================

#include "ghost_core.h"

const uint8_t LOG_CLIENT_SLOTS = 16;

class HiveLogClient {
public:
    HiveLogClient();

    void init();
    bool ingest(const Event& event);
    bool pullBackup(const Event& event);
    uint8_t storedCount() const;
    const Event* peek(uint8_t index) const;
    bool canFlush() const;

private:
    Event ram_[LOG_CLIENT_SLOTS];
    uint8_t count_;
};

#endif
