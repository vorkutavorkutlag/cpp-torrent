#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <map>
#include <string>
#include <cstdint>

#define INFOHASH_SIZE 20
#define PEERID_SIZE 20
#define PROTOCOL_ID 0x41727101980ULL

// per bytes
#define UDP_CONREQ_LEN 16

enum TF_Key {
  ANNOUNCE,
  ANNOUNCE_LIST,  // Common, but not guaranteed
  INFO,
  FILES,
  LENGTH,
  PATH,
  FLENGTH,  // Exists only for single-file torrents
  FNAME,    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  PIECE_LENGTH,
  PIECES,  // SHA-1 Hash List.
};

inline std::map<TF_Key, std::string> TF_String = {
    {ANNOUNCE, "announce"},
    {ANNOUNCE_LIST, "announce-list"},
    {INFO, "info"},
    {FILES, "files"},
    {LENGTH, "length"},
    {PATH, "path"},
    {FLENGTH, "length"},
    {FNAME, "name"},
    {PIECE_LENGTH, "piece-length"},
    {PIECES, "pieces"}};

enum class UDP_ACTION : uint16_t {
  CONNECT  = 0,
  ANNOUNCE = 1,
};

enum class UDP_BUFFER : size_t {
  CONNECT_REQUEST        =  16,
  CONNECT_RESPONSE       =  16,
  ANNOUNCE_REQUEST       =  98,
  ANNOUNCE_RESPONSE_HEAD =  20,
};

enum class ANNOUNCE_DEFAULTS : size_t {
  IP_ADDRESS =           0,
  NUM_WANT   =  0xFFFFFFFF,   // == -1
  STARTED    =           1,
  NONE       =           0,
};

#endif