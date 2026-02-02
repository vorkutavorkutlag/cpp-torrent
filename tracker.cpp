#include "tracker.h"

#include <map>
#include <set>
#include <variant>

#include "bencode.h"
#include "constants.h"

std::set<std::string> extract_trackers(BencodeDict torrent_dict) {
  std::set<std::string> trackers;

  trackers.insert(std::get<std::string>(torrent_dict[TF_String[ANNOUNCE]]));

  for (const auto& i :
       std::get<BencodeList>(torrent_dict[TF_String[ANNOUNCE_LIST]])) {
    for (const auto& j : std::get<BencodeList>(i)) {
      trackers.insert(std::get<std::string>(j));
    }
  }

  return trackers;
}
