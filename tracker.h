#include <set>
#include <string>

#include "bencode.h"

std::set<std::string> extract_trackers(BencodeDict torrent_dict);
