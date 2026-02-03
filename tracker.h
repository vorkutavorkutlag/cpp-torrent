#ifndef TRACKER_H
#define TRACKER_H

#include <set>
#include <string>
#include <variant>

#include "bencode.h"
#include "constants.h"

struct UDP_ConnectRequest {
  int64_t protocol_id;  // magic constant = 0x41727101980
  int32_t action;       // connect = 0
  int32_t transaction_id;
};

struct UDP_ConnectResponse {
  int32_t action;
  int32_t transaction_id;
  int64_t connection_id;
};

struct UDP_IPv4_AnnounceRequest {
  int64_t connection_id;
  int32_t action;
  int32_t transaction_id;
  unsigned char info_hash[INFOHASH_SIZE];
  char peer_id[PEERID_SIZE];
  int64_t downloaded;
  int64_t left;
  int64_t uploaded;
  int32_t event;       // 0: none; 1: completed; 2: started; 3: stopped
  int32_t ip_address;  // 0 default
  int32_t key;
  int32_t num_want;  // -1 default
  int16_t port;
};

struct UDP_IPv4_AnnounceResponse {
  int32_t action;  // 1 announce
  int32_t transaction_id;
  int32_t interval;
  int32_t leechers;
  int32_t seeders;
  std::vector<int32_t> ip_address;  // of size <seeders>
  std::vector<int16_t> tcp_port;    // of size <seeders>
};


std::set<std::string> extract_trackers(BencodeDict torrent_dict);

/*1 - UDP
  2 - HTTP/HTTPS
  -1 - UNKNOWN */
int determine_tracker_type(std::string url);

#endif