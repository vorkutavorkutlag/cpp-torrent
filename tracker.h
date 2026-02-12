#ifndef TRACKER_H
#define TRACKER_H

#include <arpa/inet.h>

#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <variant>

#include "bencode.h"
#include "client.h"
#include "constants.h"

struct UDP_ConnectRequest {
  uint64_t protocol_id;  // magic constant = 0x41727101980
  uint32_t action;       // connect = 0
  uint32_t transaction_id;
};

struct UDP_ConnectResponse {
  uint32_t action;
  uint32_t transaction_id;
  uint64_t connection_id;
};

struct IPv4_AnnounceRequest {
  uint64_t connection_id;
  uint32_t action;
  uint32_t transaction_id;
  const uint8_t* info_hash;
  const uint8_t* peer_id;
  uint64_t downloaded;
  uint64_t left;
  uint64_t uploaded;
  uint32_t event;       // 0: none; 1: completed; 2: started; 3: stopped
  uint32_t ip_address;  // 0 default
  uint32_t key;
  uint32_t num_want;  // default -1 == 0xFFFFFFFF
  uint16_t port;
};

struct IPv4_AnnounceResponse {
  uint32_t action;  // 1 announce
  uint32_t transaction_id;
  uint32_t interval;
  uint32_t leechers;
  uint32_t seeders;
  std::vector<uint32_t> ip_addresses;  // of size <seeders>
  std::vector<uint16_t> ports;         // of size <seeders>
  // both ip and port are in network endian order
};

struct TrackerParams {
  const std::string url;
  const std::array<uint8_t, INFOHASH_SIZE> info_hash; // const could be type-problematic..
  const std::array<uint8_t, PEERID_SIZE> peer_id;     
  const uint64_t& size;
  std::mutex& ps_mut;
  std::mutex& d_mut;
  uint64_t& downloaded;
  std::set<Peer>& peer_set;
};

inline uint64_t htonll(uint64_t x) {
  return ((((uint64_t)htonl(x)) << 32) + htonl((x) >> 32));
}

inline std::uint64_t ntohll(std::uint64_t value) {
  return ((value & 0x00000000000000FFULL) << 56) |
         ((value & 0x000000000000FF00ULL) << 40) |
         ((value & 0x0000000000FF0000ULL) << 24) |
         ((value & 0x00000000FF000000ULL) << 8) |
         ((value & 0x000000FF00000000ULL) >> 8) |
         ((value & 0x0000FF0000000000ULL) >> 24) |
         ((value & 0x00FF000000000000ULL) >> 40) |
         ((value & 0xFF00000000000000ULL) >> 56);
}

inline int determine_tracker_type(const std::string url) {
  if (url.rfind("udp", 0) == 0) return 1;
  if (url.rfind("http", 0) == 0 || url.rfind("https", 0)) return 2;
  return -1;
}

std::set<std::string> extract_trackers(BencodeDict torrent_dict);
void tracker_life(const std::shared_ptr<TrackerParams> & params);
void udp_life(const std::shared_ptr<TrackerParams> & params);
void http_life(const std::shared_ptr<TrackerParams> & params);

inline void tracker_life(const std::shared_ptr<TrackerParams> & params) {
  switch (determine_tracker_type(params.get()->url)) {
    case 1:
      udp_life(params);
      break;
    case 2:
      http_life(params);
      break;
    default:
      return;
  }
}

/*1 == UDP
  2 == HTTP/HTTPS
  -1 == UNKNOWN */
int determine_tracker_type(std::string url);

#endif