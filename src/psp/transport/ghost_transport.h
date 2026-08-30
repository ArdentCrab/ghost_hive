#ifndef GHOST_TRANSPORT_H
#define GHOST_TRANSPORT_H

// =====================================================
// Ghost Hive v1.7.1 — Kernel Transport
// Spec-Basis: §2.2, §18, §22, §31, §32, §44
// PSP empfängt alles. ACK nur bidirektional. Minen ohne RX.
// =====================================================

#include "transport_frame.h"
#include "medium_wlan.h"
#include "medium_ir.h"

class Registry;
class DecisionPipeline;
class GhostVault;
class GhostDown;
class GhostPeek;
class GhostIR;

class GhostTransport {
public:
    GhostTransport();

    void attach(Registry* registry,
                DecisionPipeline* pipeline,
                GhostVault* vault,
                GhostDown* down,
                GhostPeek* peek,
                GhostIR* ir,
                MediumWlan* wlan,
                MediumIr* mediumIr);

    uint8_t hmacICount() const;
    uint8_t hmacIAlerted() const;
    uint8_t pendingAckCount() const;
    uint8_t ackBudgetUsed() const;

    void setDangerMode(bool on);
    bool dangerMode() const;
    void setKernelDown(bool on);
    bool kernelDown() const;
    void setTerminalMode(bool on);
    bool terminalMode() const;

    // §40 / Noah: Ghost Down ist global. Snapshot → Kill, Funk danach aus.
    bool enterHiveDown(uint32_t now);
    bool hiveFrozen() const;

    void openPeekWindow();
    void openPeekWindow(uint32_t now);
    bool peekWindowOpen(uint32_t now) const;

    void rx(uint32_t now);
    /* One frame, no pump. Lab INV-07 / SPEC-v1: latch on this frame only. */
    void ingest(const TransportFrame& frame, uint32_t now);
    void tick(uint32_t now);

    bool route(const Event& event, uint32_t now);
    bool sendAck(const Event& event, uint32_t now);
    bool sendKill(uint32_t now);
    bool flushVault(uint32_t now);
    bool flushNas(uint32_t now);
    bool nasFlushAcked() const;

private:
    struct Pending {
        char dst_id[32];
        Event event;
        uint32_t sent_at;
        uint8_t retries;
        bool used;
    };

    Registry* registry_;
    DecisionPipeline* pipeline_;
    GhostVault* vault_;
    GhostDown* down_;
    GhostPeek* peek_;
    MediumWlan* wlan_;
    MediumIr* ir_;
    bool danger_;
    bool kernelDown_;
    bool hiveFrozen_;
    bool terminalMode_;
    uint32_t peek_until_;
    bool nas_flush_acked_;
    Pending pending_[TRANSPORT_PENDING];

    struct ScanRate {
        char id[32];
        uint32_t window_start;
        uint8_t count;
    };
    ScanRate scanRate_[16];
    uint32_t hmacI_window_;
    uint8_t hmacI_count_;
    uint8_t hmacI_alerted_;
    uint32_t ack_sec_;
    uint8_t ack_n_;

    void handleFrame(const TransportFrame& frame, uint32_t now);
    void handleEvent(const Event& event, uint32_t now);
    void handleMine(const MinePayload& payload, uint32_t now);
    void handleAck(const TransportFrame& frame);
    bool admitWorker(const TransportFrame& frame);
    bool admitPeer(const TransportFrame& frame, uint8_t role, uint8_t trust,
                   bool* parked);
    bool admitMine(const MinePayload& payload, uint32_t now);
    bool sensorSpam(const char* id, uint32_t now);
    void noteUnknown(const char* id, uint32_t now);
    void rejectHmacI(Event ev, uint32_t now);
    bool txToDevice(const char* id, uint8_t role, TransportKind kind,
                    const Event& event, uint32_t now);
    bool txToRole(uint8_t role, TransportKind kind,
                  const Event& event, uint32_t now);
    void rememberPending(const char* dst, const Event& event, uint32_t now);
    const Device* enrolled(const char* id) const;
};

#endif
