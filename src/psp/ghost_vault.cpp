#include "ghost_vault.h"
#include "ghost_crypto.h"
#include "transport/ghost_transport.h"

#include <stdio.h>

#if defined(__PSP__)
#include <pspiofilemgr.h>
#else
#include <sys/stat.h>
#endif

GhostVault::GhostVault() {
    init();
}

void GhostVault::init() {
    count_ = 0;
    keys_ = nullptr;
    transport_ = nullptr;
    flushDue_ = 0;
    lastFlushAt_ = 0;
    lastFlushOk_ = false;
    pendingFlush_ = false;
    safeMode_ = false;
    frozen_ = false;
}

void GhostVault::attachKeys(GhostKeys* keys) {
    keys_ = keys;
}

void GhostVault::attachTransport(GhostTransport* transport) {
    transport_ = transport;
}

bool GhostVault::store(const Event& event) {
    return store(event, event.timestamp);
}

static bool vault_has_root(const GhostKeys* keys) {
    return keys != nullptr && keys->hasRoot();
}

static bool vault_has_log(const GhostKeys* keys) {
    return keys != nullptr && keys->hasLog();
}

static bool vault_session_event(const Event& event) {
    switch (event.type) {
        case EventType::ScanResult:
        case EventType::ProfileUpdate:
        case EventType::AnomalyDetected:
        case EventType::PolicyViolation:
        case EventType::BackupWritten:
        case EventType::AlertSent:
            return true;
        default:
            return false;
    }
}

static void vault_put_u32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}

static void vault_hex20(const uint8_t in[20], char out[41]) {
    const char* d = "0123456789abcdef";
    for (uint8_t i = 0; i < 20; ++i) {
        out[i * 2] = d[in[i] >> 4];
        out[i * 2 + 1] = d[in[i] & 0x0f];
    }
    out[40] = '\0';
}

static bool vault_hex40(const char* s) {
    if (s == nullptr) return false;
    for (uint8_t i = 0; i < VAULT_MAC_HEX; ++i) {
        char c = s[i];
        bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!ok) return false;
    }
    return true;
}

static bool vault_mac_eq(const char* a, const char* b) {
    uint8_t diff = 0;
    for (uint8_t i = 0; i < VAULT_MAC_HEX; ++i) {
        diff = static_cast<uint8_t>(diff | (static_cast<uint8_t>(a[i]) ^ static_cast<uint8_t>(b[i])));
    }
    return diff == 0;
}

static uint32_t vault_event_mac_msg(const Event& event, uint8_t* msg) {
    uint32_t n = 0;
    msg[n++] = 0;
    msg[n++] = static_cast<uint8_t>(event.type);
    for (uint8_t i = 0; i < 32; ++i) msg[n++] = static_cast<uint8_t>(event.source_device_id[i]);
    vault_put_u32(msg + n, event.timestamp);
    n += 4;
    msg[n++] = static_cast<uint8_t>(event.severity);
    for (uint8_t i = 0; i < VAULT_MAC_OFF; ++i) {
        msg[n++] = static_cast<uint8_t>(event.payload[i]);
    }
    return n;
}

static uint32_t vault_mine_mac_msg(const MinePayload& mine, uint8_t* msg) {
    uint32_t n = 0;
    msg[n++] = 1;
    for (uint8_t i = 0; i < 32; ++i) msg[n++] = static_cast<uint8_t>(mine.mine_id[i]);
    vault_put_u32(msg + n, mine.counter);
    n += 4;
    vault_put_u32(msg + n, mine.totp);
    n += 4;
    msg[n++] = static_cast<uint8_t>(mine.event);
    vault_put_u32(msg + n, mine.timestamp);
    n += 4;
    return n;
}

bool GhostVault::keysAttached() const {
    return keys_ != nullptr;
}

bool GhostVault::authBound() const {
    return vault_has_root(keys_) && keys_->hasDevice() && keys_->hasSession();
}

bool GhostVault::totpBound() const {
    return keys_ != nullptr && keys_->hasMine() && keys_->hasTotpSeed();
}

bool GhostVault::eventHasMac(const Event& event) const {
    return vault_hex40(event.payload + VAULT_MAC_OFF);
}

bool GhostVault::signEvent(Event& event) const {
    if (keys_ == nullptr) return true;
    if (!keys_->hasDevice() || !keys_->hasSession()) return false;
    for (uint8_t i = VAULT_AUTH_OFF; i < 128; ++i) event.payload[i] = 0;
    uint8_t msg[160];
    uint32_t n = vault_event_mac_msg(event, msg);
    const uint8_t* key = vault_session_event(event) ? keys_->session() : keys_->device();
    uint8_t mac[20];
    ghost_hmac_sha1(key, KEY_LEN, msg, n, mac);
    char hex[41];
    vault_hex20(mac, hex);
    for (uint8_t i = 0; i < VAULT_MAC_HEX; ++i) {
        event.payload[VAULT_MAC_OFF + i] = hex[i];
    }
    return true;
}

bool GhostVault::verifyEvent(const Event& event) const {
    if (keys_ == nullptr) return true;
    if (!keys_->hasDevice() || !keys_->hasSession()) return false;
    if (!eventHasMac(event)) return false;
    Event work = event;
    char got[41];
    for (uint8_t i = 0; i < VAULT_MAC_HEX; ++i) {
        got[i] = work.payload[VAULT_MAC_OFF + i];
        work.payload[VAULT_MAC_OFF + i] = 0;
    }
    got[40] = '\0';
    uint8_t msg[160];
    uint32_t n = vault_event_mac_msg(work, msg);
    const uint8_t* key = vault_session_event(work) ? keys_->session() : keys_->device();
    uint8_t mac[20];
    ghost_hmac_sha1(key, KEY_LEN, msg, n, mac);
    char hex[41];
    vault_hex20(mac, hex);
    return vault_mac_eq(got, hex);
}

bool GhostVault::signMine(MinePayload& mine) const {
    if (keys_ == nullptr) return true;
    if (!totpBound()) return false;
    uint8_t msg[80];
    uint32_t n = vault_mine_mac_msg(mine, msg);
    uint8_t mac[20];
    ghost_hmac_sha1(keys_->mine(), KEY_LEN, msg, n, mac);
    char hex[41];
    vault_hex20(mac, hex);
    for (uint8_t i = 0; i < 64; ++i) mine.hash[i] = 0;
    for (uint8_t i = 0; i < VAULT_MAC_HEX; ++i) mine.hash[i] = hex[i];
    return true;
}

bool GhostVault::verifyMine(const MinePayload& mine) const {
    if (keys_ == nullptr) return true;
    if (!totpBound()) return false;
    if (!vault_hex40(mine.hash)) return false;
    uint8_t msg[80];
    uint32_t n = vault_mine_mac_msg(mine, msg);
    uint8_t mac[20];
    ghost_hmac_sha1(keys_->mine(), KEY_LEN, msg, n, mac);
    char hex[41];
    vault_hex20(mac, hex);
    return vault_mac_eq(mine.hash, hex);
}

const uint8_t* GhostVault::totpSeed() const {
    if (!totpBound()) return nullptr;
    return keys_->totpSeed();
}

bool GhostVault::copyPlain(uint8_t index, Event* out) const {
    if (out == nullptr || index >= count_) return false;
    const VaultRecord& rec = ram_[index];
    *out = rec.event;
    if (rec.encrypted) {
        if (!vault_has_root(keys_) || !vault_has_log(keys_)) return false;
        uint8_t* p = reinterpret_cast<uint8_t*>(out);
        ghost_xor(p, static_cast<uint32_t>(sizeof(Event)), keys_->log(), KEY_LEN);
    }
    return true;
}

char GhostVault::hmacMark(uint8_t index) const {
    Event plain{};
    if (!copyPlain(index, &plain)) return 'N';
    char m = plain.payload[VAULT_AUTH_OFF];
    if (m != 'V' && m != 'I' && m != 'N') return 'N';
    return m;
}

char GhostVault::totpMark(uint8_t index) const {
    Event plain{};
    if (!copyPlain(index, &plain)) return 'N';
    char m = plain.payload[VAULT_TOTP_OFF];
    if (m != 'V' && m != 'I' && m != 'N') return 'N';
    return m;
}

bool GhostVault::safeMode() const {
    return safeMode_;
}

bool GhostVault::rootBound() const {
    return vault_has_root(keys_);
}

void GhostVault::freeze() {
    frozen_ = true;
}

bool GhostVault::frozen() const {
    return frozen_;
}

void GhostVault::noteIoFailure() {
    safeMode_ = true;
    Event ev{};
    ev.type = EventType::PolicyViolation;
    uint8_t i = 0;
    while (KERNEL_SOURCE_ID[i] != '\0' && i < 31) {
        ev.source_device_id[i] = KERNEL_SOURCE_ID[i];
        ++i;
    }
    ev.source_device_id[i] = '\0';
    ev.timestamp = lastFlushAt_;
    ev.severity = Severity::High;
    ev.payload[0] = '\0';
    (void)signEvent(ev);
    if (!frozen_ && count_ < VAULT_RAM_SLOTS) {
        (void)store(ev, lastFlushAt_);
    }
    freeze();
}

// Local forensic only: overwrite [84]/[85] after HMAC verify. Not on the wire.
static void vault_stamp_auth(Event& event, const GhostVault* vault) {
    char hmac = 'N';
    char totpSt = 'N';
    if (vault != nullptr && vault->keysAttached()) {
        // Device+Session gebunden: fehlendes oder falsches MAC → I (§15 / §30).
        hmac = vault->verifyEvent(event) ? 'V' : 'I';
    }
    if (event.type == EventType::MineEvent) {
        if (vault != nullptr && vault->totpBound() && vault->totpSeed() != nullptr) {
            uint32_t code = 0;
            uint8_t i = 0;
            const char* p = event.payload;
            while (i < VAULT_AUTH_OFF && p[i] >= '0' && p[i] <= '9') ++i;
            if (p[i] == ':') {
                ++i;
                while (i < VAULT_AUTH_OFF && p[i] >= '0' && p[i] <= '9') {
                    code = code * 10u + static_cast<uint32_t>(p[i] - '0');
                    ++i;
                }
            }
            uint32_t expect = ghost_totp(vault->totpSeed(), TOTP_SEED_LEN, event.timestamp);
            totpSt = (code == expect) ? 'V' : 'I';
        } else if (vault != nullptr && vault->keysAttached()) {
            totpSt = 'I';
        }
    }
    event.payload[VAULT_AUTH_OFF] = hmac;
    event.payload[VAULT_TOTP_OFF] = totpSt;
}

bool GhostVault::store(const Event& event, uint32_t now) {
    if (frozen_) return false;
    if (count_ >= VAULT_RAM_SLOTS) return false;
    uint8_t mine = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        uint8_t k = 0;
        while (k < 32 && ram_[i].event.source_device_id[k] == event.source_device_id[k]) {
            if (event.source_device_id[k] == '\0') break;
            ++k;
        }
        bool a_end = (k == 32) || (ram_[i].event.source_device_id[k] == '\0');
        bool b_end = (k == 32) || (event.source_device_id[k] == '\0');
        if (a_end && b_end) {
            ++mine;
            if (mine >= VAULT_SLOTS_PER_PEER) return false;
        }
    }

    VaultRecord& rec = ram_[count_];
    rec.event = event;
    vault_stamp_auth(rec.event, this);
    rec.encrypted = false;
    rec.checksum = ghost_checksum32(
        reinterpret_cast<const uint8_t*>(&rec.event),
        static_cast<uint32_t>(sizeof(Event)));

    encryptRecord(rec);
    ++count_;

    pendingFlush_ = true;
    flushDue_ = now + VAULT_FLUSH_DELAY_SEC;
    // §28 RAM zuerst; Storage nur mit Root (§33)
    (void)persist();
    return true;
}

void GhostVault::encryptRecord(VaultRecord& rec) const {
    if (!vault_has_root(keys_) || !vault_has_log(keys_)) return;
    uint8_t* p = reinterpret_cast<uint8_t*>(&rec.event);
    ghost_xor(p, static_cast<uint32_t>(sizeof(Event)), keys_->log(), KEY_LEN);
    rec.encrypted = true;
}

void GhostVault::tick(uint32_t now) {
    if (pendingFlush_ && now >= flushDue_) {
        flushToStorage(now);
    }
}

bool GhostVault::flushToStorage(uint32_t now) {
    lastFlushAt_ = now;
    pendingFlush_ = false;
    bool ok = flush();
    if (ok && transport_ != nullptr) {
        (void)transport_->flushVault(now);
    }
    return ok;
}

bool GhostVault::verify(uint8_t index) const {
    if (index >= count_) return false;
    const VaultRecord& rec = ram_[index];
    Event plain = rec.event;
    if (rec.encrypted && vault_has_root(keys_) && vault_has_log(keys_)) {
        uint8_t* p = reinterpret_cast<uint8_t*>(&plain);
        ghost_xor(p, static_cast<uint32_t>(sizeof(Event)), keys_->log(), KEY_LEN);
    }
    uint32_t sum = ghost_checksum32(
        reinterpret_cast<const uint8_t*>(&plain),
        static_cast<uint32_t>(sizeof(Event)));
    return sum == rec.checksum;
}

uint8_t GhostVault::getStoredCount() const {
    return count_;
}

bool GhostVault::isFull() const {
    return count_ >= VAULT_RAM_SLOTS;
}

const Event* GhostVault::peekRam(uint8_t index) const {
    if (index >= count_) return nullptr;
    return &ram_[index].event;
}

uint8_t GhostVault::snapshot(Event* dst, uint8_t max) const {
    if (dst == nullptr) return 0;
    uint8_t n = (count_ < max) ? count_ : max;
    for (uint8_t i = 0; i < n; ++i) {
        dst[i] = ram_[i].event;
        if (ram_[i].encrypted && vault_has_root(keys_) && vault_has_log(keys_)) {
            uint8_t* p = reinterpret_cast<uint8_t*>(&dst[i]);
            ghost_xor(p, static_cast<uint32_t>(sizeof(Event)), keys_->log(), KEY_LEN);
        }
    }
    return n;
}

uint32_t GhostVault::lastFlushAt() const {
    return lastFlushAt_;
}

bool GhostVault::lastFlushOk() const {
    return lastFlushOk_;
}

uint32_t GhostVault::checksum() const {
    GhostCrypto crypto;
    uint32_t acc = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        acc ^= ram_[i].checksum;
    }
    (void)crypto;
    return acc;
}

bool GhostVault::flush() {
    // §33: ohne Root-Key kein Vault-Flush. Tests ohne attachKeys bleiben frei.
    if (keys_ != nullptr && !keys_->hasRoot()) {
        lastFlushOk_ = false;
        return false;
    }
    for (uint8_t i = 0; i < count_; ++i) {
        if (!verify(i)) {
            lastFlushOk_ = false;
            return false;
        }
    }
    pendingFlush_ = false;
    (void)persist();
    lastFlushOk_ = true;
    return true;
}

#if defined(__PSP__)
static const char VAULT_DIR_PSP[] = "ms0:/ghost_hive";
static const char VAULT_BIN_PSP[] = "ms0:/ghost_hive/vault.bin";
#endif

static const char* vault_bin_path() {
#if defined(__PSP__)
    return VAULT_BIN_PSP;
#else
    return VAULT_BIN_PATH;
#endif
}

static bool vault_put_u8(FILE* f, uint8_t v) {
    return fwrite(&v, 1, 1, f) == 1;
}

static bool vault_put_u32(FILE* f, uint32_t v) {
    uint8_t b[4];
    b[0] = static_cast<uint8_t>(v);
    b[1] = static_cast<uint8_t>(v >> 8);
    b[2] = static_cast<uint8_t>(v >> 16);
    b[3] = static_cast<uint8_t>(v >> 24);
    return fwrite(b, 1, 4, f) == 4;
}

static bool vault_put_bytes(FILE* f, const char* src, uint16_t n) {
    return fwrite(src, 1, n, f) == n;
}

static bool vault_get_u8(FILE* f, uint8_t& v) {
    return fread(&v, 1, 1, f) == 1;
}

static bool vault_get_u32(FILE* f, uint32_t& v) {
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return false;
    v = static_cast<uint32_t>(b[0]) |
        (static_cast<uint32_t>(b[1]) << 8) |
        (static_cast<uint32_t>(b[2]) << 16) |
        (static_cast<uint32_t>(b[3]) << 24);
    return true;
}

static bool vault_get_bytes(FILE* f, char* dst, uint16_t n) {
    return fread(dst, 1, n, f) == n;
}

static bool vault_ensure_dir() {
#if defined(__PSP__)
    (void)sceIoMkdir(VAULT_DIR_PSP, 0777);
    return true;
#else
    struct stat st;
    if (stat(HIVE_PERSIST_DIR, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return mkdir(HIVE_PERSIST_DIR, 0755) == 0;
#endif
}

bool GhostVault::persist() const {
    if (keys_ != nullptr && !keys_->hasRoot()) return false;
    if (!vault_ensure_dir()) return false;
    FILE* f = fopen(vault_bin_path(), "wb");
    if (f == nullptr) return false;
    bool ok = vault_put_u8(f, 1) && vault_put_u8(f, count_);
    for (uint8_t i = 0; ok && i < count_; ++i) {
        const VaultRecord& rec = ram_[i];
        ok = vault_put_u8(f, static_cast<uint8_t>(rec.event.type)) &&
             vault_put_bytes(f, rec.event.source_device_id, 32) &&
             vault_put_u32(f, rec.event.timestamp) &&
             vault_put_bytes(f, rec.event.payload, 128) &&
             vault_put_u8(f, static_cast<uint8_t>(rec.event.severity)) &&
             vault_put_u32(f, rec.checksum) &&
             vault_put_u8(f, rec.encrypted ? 1 : 0);
    }
    if (fclose(f) != 0) ok = false;
    return ok;
}

bool GhostVault::load() {
    FILE* f = fopen(vault_bin_path(), "rb");
    if (f == nullptr) return false;
    uint8_t version = 0;
    uint8_t n = 0;
    bool ok = vault_get_u8(f, version) && vault_get_u8(f, n);
    if (!ok || version != 1 || n > VAULT_RAM_SLOTS) {
        fclose(f);
        noteIoFailure();
        return false;
    }

    VaultRecord loaded[VAULT_RAM_SLOTS];
    for (uint8_t i = 0; i < n; ++i) {
        uint8_t type = 0;
        uint8_t sev = 0;
        uint8_t enc = 0;
        loaded[i].event.source_device_id[0] = '\0';
        loaded[i].event.payload[0] = '\0';
        ok = vault_get_u8(f, type) &&
             vault_get_bytes(f, loaded[i].event.source_device_id, 32) &&
             vault_get_u32(f, loaded[i].event.timestamp) &&
             vault_get_bytes(f, loaded[i].event.payload, 128) &&
             vault_get_u8(f, sev) &&
             vault_get_u32(f, loaded[i].checksum) &&
             vault_get_u8(f, enc);
        if (!ok) {
            fclose(f);
            noteIoFailure();
            return false;
        }
        loaded[i].event.source_device_id[31] = '\0';
        loaded[i].event.type = static_cast<EventType>(type);
        loaded[i].event.severity = static_cast<Severity>(sev);
        loaded[i].encrypted = (enc != 0);
    }
    int extra = fgetc(f);
    fclose(f);
    if (extra != EOF) {
        noteIoFailure();
        return false;
    }

    // §33 Reset: ohne Root/Log sind verschlüsselte Records unlesbar.
    for (uint8_t i = 0; i < n; ++i) {
        if (loaded[i].encrypted) {
            if (!vault_has_root(keys_) || !vault_has_log(keys_)) {
                safeMode_ = true;
                freeze();
                return false;
            }
        }
    }

    uint8_t savedCount = count_;
    VaultRecord saved[VAULT_RAM_SLOTS];
    for (uint8_t i = 0; i < savedCount; ++i) saved[i] = ram_[i];

    count_ = n;
    for (uint8_t i = 0; i < n; ++i) {
        ram_[i] = loaded[i];
    }
    for (uint8_t i = 0; i < n; ++i) {
        if (!verify(i)) {
            count_ = savedCount;
            for (uint8_t j = 0; j < savedCount; ++j) ram_[j] = saved[j];
            noteIoFailure();
            return false;
        }
    }
    pendingFlush_ = false;
    lastFlushOk_ = true;
    safeMode_ = false;
    return true;
}
