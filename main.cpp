#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <thread>

#include "bencode.h"
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

void thread_test(std::mutex& mut, std::set<std::string>& s) {
  for (;;) {
    {
      std::this_thread::sleep_for(std::chrono::seconds(rand() % 10));
      std::lock_guard<decltype(mut)> lock(mut);
      s.insert("<<" +
               std::to_string(
                   std::hash<std::thread::id>{}(std::this_thread::get_id())) +
               ">>");
    }
  }
}

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
  std::vector<uint8_t> infohash = infohash_bytes(raw_info);
  // std::string hex_ih = infohash_hex(raw_info);

  ASSERT_if(std::holds_alternative<BencodeDict>(decoded));

  BencodeDict torrent_dict = std::get<BencodeDict>(decoded);

  std::set<std::string> trackers = extract_trackers(torrent_dict);

  std::mutex peers_set_mutex;
  std::set<std::string> peers_set;
  std::vector<std::thread> threads;

  // std::cout << hex_ih << std::endl;
  for (const auto& i : trackers) {
    threads.emplace_back(thread_test, std::ref(peers_set_mutex),
                         std::ref(peers_set));
  }

  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "\nLen: " << peers_set.size() << std::endl;
    for (const auto& i : peers_set) std::cout << i << ' ';
  }

  for (std::thread& t : threads) {
    t.join();
  }

  return 0;
}