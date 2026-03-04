#include "peer.h"

#include <unistd.h>

#include <ctime>

#include "constants.h"
#include <mutex>
#include <set>
#include <thread>

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

void peer_recver(const std::set<Peer> &peers_set, std::mutex &peers_set_mutex) {

  for (;;) {
    std::set<Peer> copy;

    {
      const std::lock_guard<std::mutex> lock(peers_set_mutex);
      copy = peers_set;
    }

    for (auto &peer : copy) {
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
