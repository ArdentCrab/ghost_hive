#ifndef GHOST_VAULT_H
#define GHOST_VAULT_H

// =====================================================
// Ghost Hive v1.7.1
// Vault
// Spec-Basis: §12.3, §24, §28, §40
// RAM zuerst, encrypt, Integrität, Flush verzögert
// =====================================================

#include "ghost_core.h"
#include "ghost_keys.h"

class GhostTransport;

const uint8_t VAULT_RAM_SLOTS = 64;
const uint8_t VAULT_SLOTS_PER_PEER = 8;
const uint32_t VAULT_FLUSH_DELAY_SEC = 5;
// Wire HMAC covers payload[0..87]. [88..127] is the MAC hex itself.
// [84]/[85] are local V/I/N stamps written at store() after verify (§15 / §33).
// On the wire they must be the signed zeros; they are not trust/routing/policy.
const uint8_t VAULT_AUTH_OFF = 84;
const uint8_t VAULT_TOTP_OFF = 85;
const uint8_t VAULT_MAC_OFF = 88;
const uint8_t VAULT_MAC_HEX = 40;
const char HIVE_PERSIST_DIR[] = "/tmp/ghost_hive";
const char VAULT_BIN_PATH[] = "/tmp/ghost_hive/vault.bin";

struct VaultRecord {
    Event event;
    uint32_t checksum;
    bool encrypted;
};

class GhostVault {
public:
    GhostVault();

    void init();
    void attachKeys(GhostKeys* keys);
    void attachTransport(GhostTransport* transport);
    bool store(const Event& event);
    bool store(const Event& event, uint32_t now);
    bool load();
    void tick(uint32_t now);
    bool flushToStorage(uint32_t now);
    bool verify(uint8_t index) const;

    uint8_t getStoredCount() const;
    bool isFull() const;
    const Event* peekRam(uint8_t index) const;
    uint8_t snapshot(Event* dst, uint8_t max) const;
    uint32_t lastFlushAt() const;
    bool lastFlushOk() const;
    uint32_t checksum() const;
    bool flush();

    // §15 / §33 Frame-Auth: HMAC-SHA1. Ohne attachKeys Dummy-Frames erlaubt.
    bool keysAttached() const;
    bool authBound() const;
    bool totpBound() const;
    bool signEvent(Event& event) const;
    bool verifyEvent(const Event& event) const;
    bool signMine(MinePayload& mine) const;
    bool verifyMine(const MinePayload& mine) const;
    bool eventHasMac(const Event& event) const;
    const uint8_t* totpSeed() const;

    // §15 / §30 / §31: HMAC/TOTP-Stempel V/I/N. I/O-Fehler → safe_mode.
    bool copyPlain(uint8_t index, Event* out) const;
    char hmacMark(uint8_t index) const;
    char totpMark(uint8_t index) const;
    bool safeMode() const;
    bool rootBound() const;
    void freeze();
    bool frozen() const;

private:
    VaultRecord ram_[VAULT_RAM_SLOTS];
    uint8_t count_;
    GhostKeys* keys_;
    GhostTransport* transport_;
    uint32_t flushDue_;
    uint32_t lastFlushAt_;
    bool lastFlushOk_;
    bool pendingFlush_;
    bool safeMode_;
    bool frozen_;

    void encryptRecord(VaultRecord& rec) const;
    bool persist() const;
    void noteIoFailure();
};

#endif // GHOST_VAULT_H
