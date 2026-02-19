#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <cstdint>
#include <map>
#include <string>

constexpr size_t INFOHASH_SIZE = 20;
constexpr size_t PEERID_SIZE = 20;
constexpr uint64_t PROTOCOL_ID = 0x41727101980ULL;
constexpr size_t MAX_ANNOUNCE_PEERS = 250;  // safe upper bound
constexpr const char* PEERS = "peers";

// per bytes
#define UDP_CONREQ_LEN 16

enum TF_Key {
    ANNOUNCE,
    ANNOUNCE_LIST,  // common, but not guaranteed
    INFO,
    FILES,
    LENGTH,
    PATH,
    FLENGTH,  // exists only for single-file torrents
    FNAME,    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    PIECE_LENGTH,
    PIECES,  // SHA-1 Hash List.
    // PEERS,
};

/* Need to change this to constexpr */
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
    {PIECES, "pieces"},
    // {PEERS, "peers"}
};

enum class UDP_ACTION : uint16_t {
    CONNECT = 0,
    ANNOUNCE = 1,
};

enum class UDP_BUFFER : size_t {
    CONNECT_REQUEST = 16,
    CONNECT_RESPONSE = 16,
    ANNOUNCE_REQUEST = 98,
    ANNOUNCE_HEAD = 20,
    ANNOUNCE_RESPONSE = 20 + MAX_ANNOUNCE_PEERS * 6,
};

enum class ANNOUNCE_DEFAULTS : size_t {
    IP_ADDRESS = 0,
    NUM_WANT = 0xFFFFFFFF,  // == -1
    STARTED = 1,
    NONE = 0,
};

#endif