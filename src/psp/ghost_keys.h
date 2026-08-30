#ifndef GHOST_KEYS_H
#define GHOST_KEYS_H

// =====================================================
// Ghost Hive v1.7.1
// Schlüssel (§32 Bootstrap, §33 Krypto)
// PSP hält Root. Rotation manuell. Reset physisch.
// Keine Dummy-Key-Bytes.
// =====================================================

#include "ghost_core.h"
#include "ghost_crypto.h"

const uint8_t KEY_LEN = 32;

class GhostKeys {
public:
    GhostKeys();

    void initEmpty();
    void physicalReset();

    bool hasRoot() const;
    bool hasDevice() const;
    bool hasLog() const;
    bool hasSession() const;
    bool hasMine() const;
    bool hasTotpSeed() const;

    bool provisionRoot(const uint8_t* key, uint8_t len);
    bool provisionDevice(const uint8_t* key, uint8_t len);
    bool provisionLog(const uint8_t* key, uint8_t len);
    bool provisionSession(const uint8_t* key, uint8_t len);
    bool provisionMine(const uint8_t* key, uint8_t len);
    bool provisionTotpSeed(const uint8_t* seed, uint8_t len);
    // §33: Device/Log/Session/Mine/TOTP aus Root-Material. Setzt Root nicht.
    bool provisionDerived(const uint8_t* root, uint8_t len);

    // Peer-Bind: Derived ohne Root. Nie Root-Bytes.
    static const uint8_t PEER_BIND_LEN = 112;
    bool exportPeerBind(uint8_t* dst, uint8_t dstLen) const;
    bool importPeerBind(const uint8_t* src, uint8_t srcLen);
    bool generateRoot();

    const uint8_t* root() const;
    const uint8_t* device() const;
    const uint8_t* log() const;
    const uint8_t* session() const;
    const uint8_t* mine() const;
    const uint8_t* totpSeed() const;

    bool rotateManual(const uint8_t* newRoot, uint8_t len);

private:
    uint8_t root_[KEY_LEN];
    uint8_t device_[KEY_LEN];
    uint8_t log_[KEY_LEN];
    uint8_t session_[KEY_LEN];
    uint8_t mine_[KEY_LEN];
    uint8_t totpSeed_[TOTP_SEED_LEN];
    bool hasRoot_;
    bool hasDevice_;
    bool hasLog_;
    bool hasSession_;
    bool hasMine_;
    bool hasTotpSeed_;

    static void zero(uint8_t* buf, uint8_t len);
    static bool copyKey(uint8_t* dst, uint8_t dstLen,
                        const uint8_t* src, uint8_t srcLen);
};

#endif // GHOST_KEYS_H
