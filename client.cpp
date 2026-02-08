#include "client.h"

#include <unistd.h>

#include <ctime>

#include "constants.h"

struct Peer {
  uint32_t ip;
  uint16_t port;
};

/* Sets new seed for random and
  randomly generates PEERID_SIZE length string
  of alphanumeric characters.*/
std::array<uint8_t, PEERID_SIZE> generate_peerid() {
  std::array<uint8_t, PEERID_SIZE> pid;
  constexpr size_t uint8_ran = 256;

  srand(time(NULL) * getpid());

  for (int i = 0; i < PEERID_SIZE; ++i) {
    pid[i] = rand() % uint8_ran; 
  }

  return pid;
}