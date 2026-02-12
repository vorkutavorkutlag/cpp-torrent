#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

#include "bencode.h"
#include "client.h"
#include "constants.h"
#include "tracker.h"

namespace fs = std::filesystem;

#define ASSERT_ia(condition)                                                   \
  do {                                                                         \
    assert(condition &&                                                        \
           "Invalid arguments. Correct usage: {executable} {torrent} {path}"); \
  } while (0)

#define ASSERT_if(condition)                                      \
  do {                                                            \
    assert(condition && "Failed to properly read torrent file."); \
  } while (0)

int main(int argc, char* argv[]) {
  ASSERT_ia(argc == 3);

  std::string filename = argv[1];
  std::string dest_dir = argv[2];

  ASSERT_ia(fs::is_regular_file(filename));
  ASSERT_ia(fs::is_directory(dest_dir));

  std::ifstream file(filename);
  std::string raw_info;
  bool _IH_parse_condition = false;

  BencodeValue decoded = decode(file, raw_info, _IH_parse_condition);
  std::array<uint8_t, INFOHASH_SIZE> infohash = infohash_bytes(raw_info);
  // std::string hex_ih = infohash_hex(raw_info);

  ASSERT_if(std::holds_alternative<BencodeDict>(decoded));
  BencodeDict torrent_dict = std::get<BencodeDict>(decoded);

  std::set<std::string> trackers = extract_trackers(torrent_dict);

  std::array<uint8_t, PEERID_SIZE> peer_id = generate_peerid();

  uint64_t torrent_size = get_torrent_size(torrent_dict);
  uint64_t total_downloaded = 0;

  std::mutex peers_set_mutex;
  std::mutex download_mutex;
  std::set<Peer> peers_set;
  std::vector<std::thread> tracker_threads;

  // std::cout << hex_ih << std::endl;
  // for (const auto& tracker : trackers) {
  const auto& tracker = *(std::next(trackers.begin(), 1));
  std::cout << "We got " << tracker << std::endl;
  auto params = std::make_shared<TrackerParams>(
      TrackerParams{tracker, infohash, peer_id, torrent_size, peers_set_mutex,
                    download_mutex, total_downloaded, peers_set});

  tracker_threads.emplace_back(tracker_life, params);
  // }

  for (;;) {
    std::set<Peer> copy;

    {
      const std::lock_guard<std::mutex> lock(peers_set_mutex);
      copy = peers_set;
    }

    for (auto& peer : copy) {
      std::cout << peer.ip << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));
  }

  return 0;
}