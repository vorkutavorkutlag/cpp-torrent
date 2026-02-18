#ifndef CLIENT_H
#define CLIENT_H

#include <array>

#include "constants.h"

struct Peer {
    uint32_t ip;
    uint16_t port;

    Peer(uint32_t ip_, uint16_t port_) : ip(ip_), port(port_) {}

    bool operator<(const Peer& other) const {
        return std::tie(ip, port) < std::tie(other.ip, other.port);
    }
};

std::array<uint8_t, PEERID_SIZE> generate_peerid();

#endif