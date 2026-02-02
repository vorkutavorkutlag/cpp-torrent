#include "bencode.h"
#include "constants.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>

#include <thread>
#include <mutex>
#include <set>

namespace fs = std::filesystem;

#define ASSERT_ia(condition) \
   do { \
      assert(condition && "Invalid arguments. Correct usage: {executable} {torrent} {path}"); \
   } while (0)

#define ASSERT_if(condition) \
   do { \
      assert(condition && "Failed to properly read torrent file."); \
   } while (0)

int main(int argc, char *argv[]) {
  ASSERT_ia(argc == 3);

  std::string filename = argv[1];
  std::string dest_dir = argv[2];

  ASSERT_ia(fs::is_regular_file(filename));
  ASSERT_ia(fs::is_directory(dest_dir));

  std::ifstream file(filename);
  std::string raw_infohash;
  bool _IH_parse_condition = false;
  
  BencodeValue decoded = decode(file, raw_infohash, _IH_parse_condition);
  std::vector<unsigned char> infohash = infohash_bytes(raw_infohash);
  std::string hex_ih = infohash_hex(raw_infohash);

  ASSERT_if(std::holds_alternative<BencodeDict>(decoded));

  BencodeDict torrent_dict = std::get<BencodeDict>(decoded);

  // for (const auto & i : infohash) {
  //   std::cout << i;
  // }
  // std::cout << std::endl;

  std::cout << hex_ih << std::endl;

  return 0;
}