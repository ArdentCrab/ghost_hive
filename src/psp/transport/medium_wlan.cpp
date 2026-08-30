#include "medium_wlan.h"
#include "hive_net.h"

#if defined(__PSP__)
#include <pspnet.h>
#include <pspnet_inet.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#endif

static bool parse_ipv4(const char* s, uint32_t& host) {
    if (s == nullptr || s[0] == '\0') return false;
    uint32_t acc[4];
    uint8_t n = 0;
    uint32_t v = 0;
    bool digit = false;
    uint8_t i = 0;
    while (s[i] != '\0') {
        if (s[i] >= '0' && s[i] <= '9') {
            v = v * 10u + static_cast<uint32_t>(s[i] - '0');
            if (v > 255u) return false;
            digit = true;
        } else if (s[i] == '.' && digit && n < 3) {
            acc[n++] = v;
            v = 0;
            digit = false;
        } else {
            return false;
        }
        ++i;
    }
    if (!digit || n != 3) return false;
    acc[3] = v;
    host = (acc[0] << 24) | (acc[1] << 16) | (acc[2] << 8) | acc[3];
    return true;
}

static uint32_t ghost_htonl(uint32_t host) {
    return ((host & 0x000000FFu) << 24) |
           ((host & 0x0000FF00u) << 8) |
           ((host & 0x00FF0000u) >> 8) |
           ((host & 0xFF000000u) >> 24);
}

static uint16_t ghost_htons(uint16_t host) {
    return static_cast<uint16_t>(((host & 0x00FFu) << 8) | ((host & 0xFF00u) >> 8));
}

static uint16_t ghost_ntohs(uint16_t net) {
    return ghost_htons(net);
}

MediumWlan::MediumWlan() : sock_(-1), listen_(false), kernel_ip_(0), kernel_port_(0) {
    local_id_[0] = '\0';
    init();
}

MediumWlan::~MediumWlan() {
    closeUdp();
}

void MediumWlan::init() {
    closeUdp();
    listen_ = false;
    local_id_[0] = '\0';
    kernel_ip_ = 0;
    kernel_port_ = 0;
    k_head_ = 0;
    k_tail_ = 0;
    k_count_ = 0;
    for (uint8_t i = 0; i < TRANSPORT_KERNEL_SLOTS; ++i) {
        transport_clear_frame(kernel_rx_[i]);
    }
    for (uint8_t i = 0; i < TRANSPORT_MAX_PEERS; ++i) {
        peers_[i].used = false;
        peers_[i].id[0] = '\0';
        peers_[i].role = 0;
        peers_[i].head = 0;
        peers_[i].tail = 0;
        peers_[i].count = 0;
        peers_[i].ip = 0;
        peers_[i].port = 0;
        peers_[i].udp = false;
        for (uint8_t s = 0; s < TRANSPORT_PEER_SLOTS; ++s) {
            transport_clear_frame(peers_[i].slots[s]);
        }
    }
}

void MediumWlan::closeUdp() {
    if (sock_ < 0) return;
#if defined(__PSP__)
    sceNetInetClose(sock_);
#else
    close(sock_);
#endif
    sock_ = -1;
}

bool MediumWlan::openUdp(uint16_t bind_port, bool broadcast) {
    closeUdp();

#if defined(__PSP__)
    (void)sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    (void)sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
    (void)sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
    (void)sceNetInetInit();
    sock_ = sceNetInetSocket(AF_INET, SOCK_DGRAM, 0);
#else
    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
#endif
    if (sock_ < 0) return false;

    int yes = 1;
#if defined(__PSP__)
    (void)sceNetInetSetsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (broadcast) {
        (void)sceNetInetSetsockopt(sock_, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    }
#ifdef SO_NONBLOCK
    int nb = 1;
    (void)sceNetInetSetsockopt(sock_, SOL_SOCKET, SO_NONBLOCK, &nb, sizeof(nb));
#endif
#else
    (void)setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (broadcast) {
        (void)setsockopt(sock_, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    }
    int flags = fcntl(sock_, F_GETFL, 0);
    if (flags >= 0) (void)fcntl(sock_, F_SETFL, flags | O_NONBLOCK);
#endif

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ghost_htons(bind_port);
#if defined(__PSP__)
    if (bind_port != 0) {
        addr.sin_addr.s_addr = ghost_htonl(HIVE_KERNEL_IPV4);
    } else {
        addr.sin_addr.s_addr = ghost_htonl(0);
    }
#else
    addr.sin_addr.s_addr = ghost_htonl(0);
#endif

#if defined(__PSP__)
    if (sceNetInetBind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        closeUdp();
        return false;
    }
#else
    if (bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closeUdp();
        return false;
    }
#endif
    return true;
}

bool MediumWlan::listenKernel(uint16_t port) {
    if (!openUdp(port, true)) return false;
    listen_ = true;
    kernel_port_ = port;
    return true;
}

bool MediumWlan::connectKernel(const char* id, const char* ip, uint16_t port) {
    uint32_t host = 0xFFFFFFFFu;
    if (ip != nullptr && ip[0] != '\0') {
        if (!parse_ipv4(ip, host)) return false;
    }
    if (!openUdp(0, true)) return false;
    listen_ = false;
    kernel_ip_ = ghost_htonl(host);
    kernel_port_ = port;
    transport_copy_id(local_id_, id);
    if (id != nullptr && id[0] != '\0') {
        (void)registerPeer(id, ROLE_WORKER);
    }
    return true;
}

bool MediumWlan::sendUdp(uint32_t ip, uint16_t port, const TransportFrame& frame) {
    if (sock_ < 0 || ip == 0 || port == 0) return false;
    uint8_t wire[TRANSPORT_WIRE_LEN];
    if (!transport_encode(frame, wire, TRANSPORT_WIRE_LEN)) return false;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ghost_htons(port);
    addr.sin_addr.s_addr = ip;

#if defined(__PSP__)
    int n = sceNetInetSendto(sock_, wire, TRANSPORT_WIRE_LEN, 0,
                             reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    return n == static_cast<int>(TRANSPORT_WIRE_LEN);
#else
    ssize_t n = sendto(sock_, wire, TRANSPORT_WIRE_LEN, 0,
                       reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    return n == static_cast<ssize_t>(TRANSPORT_WIRE_LEN);
#endif
}

bool MediumWlan::rememberUdp(const char* id, uint8_t role, uint32_t ip, uint16_t port) {
    if (!registerPeer(id, role)) return false;
    Inbox* p = findPeer(id);
    if (p == nullptr) return false;
    p->ip = ip;
    p->port = port;
    p->udp = true;
    return true;
}

void MediumWlan::pump() {
    if (sock_ < 0) return;

    uint8_t n = 0;
    while (n < TRANSPORT_KERNEL_SLOTS) {
        uint8_t wire[TRANSPORT_WIRE_LEN];
        sockaddr_in from = {};
#if defined(__PSP__)
        uint32_t flen = sizeof(from);
        int r = sceNetInetRecvfrom(sock_, wire, TRANSPORT_WIRE_LEN, 0,
                                   reinterpret_cast<sockaddr*>(&from), &flen);
        if (r <= 0) break;
#else
        socklen_t flen = sizeof(from);
        ssize_t r = recvfrom(sock_, wire, TRANSPORT_WIRE_LEN, 0,
                             reinterpret_cast<sockaddr*>(&from), &flen);
        if (r <= 0) break;
#endif
        if (r != static_cast<int>(TRANSPORT_WIRE_LEN)) {
            ++n;
            continue;
        }

        TransportFrame frame;
        if (!transport_decode(wire, TRANSPORT_WIRE_LEN, frame)) {
            ++n;
            continue;
        }

        uint32_t ip = from.sin_addr.s_addr;
        uint16_t port = ghost_ntohs(from.sin_port);

        if (listen_) {
            if (frame.src_id[0] != '\0' && transport_is_bidi(frame.src_role)) {
                (void)rememberUdp(frame.src_id, frame.src_role, ip, port);
            }
            (void)pushSlot(kernel_rx_, TRANSPORT_KERNEL_SLOTS,
                           k_head_, k_tail_, k_count_, frame);
        } else {
            if (frame.src_role == ROLE_KERNEL && kernel_ip_ == 0) {
                kernel_ip_ = ip;
            }
            Inbox* p = findPeer(local_id_);
            if (p != nullptr) {
                (void)pushSlot(p->slots, TRANSPORT_PEER_SLOTS,
                               p->head, p->tail, p->count, frame);
            }
        }
        ++n;
    }
}

bool MediumWlan::pushSlot(TransportFrame* slots, uint8_t cap,
                          uint8_t& head, uint8_t& tail, uint8_t& count,
                          const TransportFrame& frame) {
    (void)head;
    if (count >= cap) return false;
    slots[tail] = frame;
    tail = static_cast<uint8_t>((tail + 1) % cap);
    ++count;
    return true;
}

bool MediumWlan::popSlot(TransportFrame* slots, uint8_t cap,
                         uint8_t& head, uint8_t& tail, uint8_t& count,
                         TransportFrame& frame) {
    (void)tail;
    if (count == 0) return false;
    frame = slots[head];
    head = static_cast<uint8_t>((head + 1) % cap);
    --count;
    return true;
}

MediumWlan::Inbox* MediumWlan::findPeer(const char* id) {
    for (uint8_t i = 0; i < TRANSPORT_MAX_PEERS; ++i) {
        if (peers_[i].used && transport_same_id(peers_[i].id, id)) {
            return &peers_[i];
        }
    }
    return nullptr;
}

bool MediumWlan::registerPeer(const char* id, uint8_t role) {
    if (id == nullptr || id[0] == '\0') return false;
    if (!transport_is_bidi(role)) return false;
    Inbox* existing = findPeer(id);
    if (existing != nullptr) {
        existing->role = role;
        return true;
    }
    for (uint8_t i = 0; i < TRANSPORT_MAX_PEERS; ++i) {
        if (!peers_[i].used) {
            transport_copy_id(peers_[i].id, id);
            peers_[i].role = role;
            peers_[i].head = 0;
            peers_[i].tail = 0;
            peers_[i].count = 0;
            peers_[i].used = true;
            return true;
        }
    }
    return false;
}

bool MediumWlan::toKernel(const TransportFrame& frame) {
    if (sock_ >= 0 && !listen_ && kernel_port_ != 0) {
        uint32_t ip = kernel_ip_;
        if (ip == 0) ip = ghost_htonl(0xFFFFFFFFu);
        if (sendUdp(ip, kernel_port_, frame)) return true;
    }
    return pushSlot(kernel_rx_, TRANSPORT_KERNEL_SLOTS,
                    k_head_, k_tail_, k_count_, frame);
}

bool MediumWlan::fromKernel(TransportFrame& frame) {
    pump();
    return popSlot(kernel_rx_, TRANSPORT_KERNEL_SLOTS,
                   k_head_, k_tail_, k_count_, frame);
}

bool MediumWlan::toPeer(const char* id, const TransportFrame& frame) {
    Inbox* p = findPeer(id);
    if (p == nullptr) return false;
    if (sock_ >= 0 && p->udp && p->ip != 0 && p->port != 0) {
        if (sendUdp(p->ip, p->port, frame)) return true;
    }
    return pushSlot(p->slots, TRANSPORT_PEER_SLOTS,
                    p->head, p->tail, p->count, frame);
}

bool MediumWlan::fromPeer(const char* id, TransportFrame& frame) {
    pump();
    Inbox* p = findPeer(id);
    if (p == nullptr) return false;
    return popSlot(p->slots, TRANSPORT_PEER_SLOTS,
                   p->head, p->tail, p->count, frame);
}
