// Host-only UDP relay: LAN :17471 <-> Kernel IBSS. No PSP module. No IP routing.
#include "transport/transport_frame.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

static const uint8_t RELAY_SLOTS = 8;

struct Slot {
    char id[32];
    uint32_t ip;
    uint16_t port;
    uint8_t used;
};

static int open_udp(uint16_t port) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return -1;
    int yes = 1;
    (void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    (void)setsockopt(s, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    int fl = fcntl(s, F_GETFL, 0);
    if (fl >= 0) (void)fcntl(s, F_SETFL, fl | O_NONBLOCK);
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(0);
    if (bind(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) {
        close(s);
        return -1;
    }
    return s;
}

static bool parse_ip(const char* s, uint32_t* host) {
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (s == nullptr) return false;
    if (sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return false;
    if (a > 255 || b > 255 || c > 255 || d > 255) return false;
    *host = (a << 24) | (b << 16) | (c << 8) | d;
    return true;
}

static int8_t find_id(Slot* t, const char* id) {
    for (uint8_t i = 0; i < RELAY_SLOTS; ++i) {
        if (t[i].used && transport_same_id(t[i].id, id)) return static_cast<int8_t>(i);
    }
    return -1;
}

static int8_t remember(Slot* t, const char* id, uint32_t ip, uint16_t port) {
    if (id == nullptr || id[0] == '\0') return -1;
    int8_t x = find_id(t, id);
    if (x < 0) {
        for (uint8_t i = 0; i < RELAY_SLOTS; ++i) {
            if (!t[i].used) {
                x = static_cast<int8_t>(i);
                transport_copy_id(t[i].id, id);
                t[i].used = 1;
                break;
            }
        }
    }
    if (x < 0) return -1;
    t[x].ip = ip;
    t[x].port = port;
    return x;
}

int main(int argc, char** argv) {
    uint32_t k_host = 0x0A112F01u;
    if (argc > 1 && !parse_ip(argv[1], &k_host)) return 1;

    int lan = open_udp(GHOST_UDP_PORT);
    int ibss = socket(AF_INET, SOCK_DGRAM, 0);
    if (lan < 0 || ibss < 0) return 1;
    int fl = fcntl(ibss, F_GETFL, 0);
    if (fl >= 0) (void)fcntl(ibss, F_SETFL, fl | O_NONBLOCK);
    int yes = 1;
    (void)setsockopt(ibss, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));

    sockaddr_in kern;
    memset(&kern, 0, sizeof(kern));
    kern.sin_family = AF_INET;
    kern.sin_port = htons(GHOST_UDP_PORT);
    kern.sin_addr.s_addr = htonl(k_host);

    Slot tab[RELAY_SLOTS];
    for (uint8_t i = 0; i < RELAY_SLOTS; ++i) {
        tab[i].used = 0;
        tab[i].id[0] = '\0';
        tab[i].ip = 0;
        tab[i].port = 0;
    }

    for (;;) {
        uint8_t wire[TRANSPORT_WIRE_LEN];
        sockaddr_in from;
        socklen_t flen = sizeof(from);
        ssize_t n = recvfrom(lan, wire, TRANSPORT_WIRE_LEN, 0,
                             reinterpret_cast<sockaddr*>(&from), &flen);
        if (n == static_cast<ssize_t>(TRANSPORT_WIRE_LEN)) {
            if (ntohl(from.sin_addr.s_addr) != k_host) {
                TransportFrame fr;
                if (transport_decode(wire, TRANSPORT_WIRE_LEN, fr)) {
                    (void)remember(tab, fr.src_id, ntohl(from.sin_addr.s_addr),
                                   ntohs(from.sin_port));
                }
                (void)sendto(ibss, wire, TRANSPORT_WIRE_LEN, 0,
                             reinterpret_cast<sockaddr*>(&kern), sizeof(kern));
            }
        }
        flen = sizeof(from);
        n = recvfrom(ibss, wire, TRANSPORT_WIRE_LEN, 0,
                     reinterpret_cast<sockaddr*>(&from), &flen);
        if (n == static_cast<ssize_t>(TRANSPORT_WIRE_LEN)) {
            TransportFrame fr;
            if (transport_decode(wire, TRANSPORT_WIRE_LEN, fr)) {
                int8_t x = find_id(tab, fr.dst_id);
                if (x < 0) x = find_id(tab, fr.event.source_device_id);
                if (x >= 0) {
                    sockaddr_in to;
                    memset(&to, 0, sizeof(to));
                    to.sin_family = AF_INET;
                    to.sin_port = htons(tab[x].port);
                    to.sin_addr.s_addr = htonl(tab[x].ip);
                    (void)sendto(lan, wire, TRANSPORT_WIRE_LEN, 0,
                                 reinterpret_cast<sockaddr*>(&to), sizeof(to));
                }
            }
        }
        usleep(2000);
    }
    return 0;
}
