#ifndef MEDIUM_WLAN_H
#define MEDIUM_WLAN_H

// =====================================================
// Ghost Hive v1.7.1 — WLAN-Medium
// Spec-Basis: §2.2, §18, §22
// UDP im lokalen LAN. Kein Cloud. Kein WPA-Client-Join.
// In-Process: RAM-Bus (Tests). Host/PSP: UDP wenn listen/connect.
// =====================================================

#include "transport_frame.h"

class MediumWlan {
public:
    MediumWlan();
    ~MediumWlan();

    void init();
    bool listenKernel(uint16_t port);
    bool connectKernel(const char* id, const char* ip, uint16_t port);
    void pump();

    bool registerPeer(const char* id, uint8_t role);
    bool toKernel(const TransportFrame& frame);
    bool fromKernel(TransportFrame& frame);
    bool toPeer(const char* id, const TransportFrame& frame);
    bool fromPeer(const char* id, TransportFrame& frame);

private:
    struct Inbox {
        char id[32];
        uint8_t role;
        TransportFrame slots[TRANSPORT_PEER_SLOTS];
        uint8_t head;
        uint8_t tail;
        uint8_t count;
        bool used;
        uint32_t ip;
        uint16_t port;
        bool udp;
    };

    TransportFrame kernel_rx_[TRANSPORT_KERNEL_SLOTS];
    uint8_t k_head_;
    uint8_t k_tail_;
    uint8_t k_count_;
    Inbox peers_[TRANSPORT_MAX_PEERS];

    int sock_;
    bool listen_;
    char local_id_[32];
    uint32_t kernel_ip_;
    uint16_t kernel_port_;

    Inbox* findPeer(const char* id);
    bool rememberUdp(const char* id, uint8_t role, uint32_t ip, uint16_t port);
    bool sendUdp(uint32_t ip, uint16_t port, const TransportFrame& frame);
    bool openUdp(uint16_t bind_port, bool broadcast);
    void closeUdp();

    static bool pushSlot(TransportFrame* slots, uint8_t cap,
                         uint8_t& head, uint8_t& tail, uint8_t& count,
                         const TransportFrame& frame);
    static bool popSlot(TransportFrame* slots, uint8_t cap,
                        uint8_t& head, uint8_t& tail, uint8_t& count,
                        TransportFrame& frame);
};

#endif
