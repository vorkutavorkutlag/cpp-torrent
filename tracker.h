#ifndef TRACKER_H
#define TRACKER_H

#include <set>
#include <string>

#include "bencode.h"

struct UDPConnectRequest;

std::set<std::string> extract_trackers(BencodeDict torrent_dict);

/*1 - UDP
  2 - HTTP/HTTPS
  -1 - UNKNOWN */
int determine_tracker_type(std::string url);

#endif