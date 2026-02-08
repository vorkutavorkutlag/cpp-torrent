#ifndef CLIENT_H
#define CLIENT_H

#include <array>

#include "constants.h"

struct Peer;

std::array<uint8_t, PEERID_SIZE> generate_peerid();

#endif