#ifndef BENCODE_H
#define BENCODE_H

#include <openssl/sha.h>

#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "constants.h"

struct BencodeValue;

using BencodeList = std::vector<BencodeValue>;
using BencodeDict = std::map<std::string, BencodeValue>;

struct BencodeValue : std::variant<std::int64_t, std::string, BencodeList,
                                   BencodeDict, std::monostate> {
    using variant::variant;
};

enum class BencodeChar : char {
    INTEGER = 'i',
    LIST = 'l',
    DICT = 'd',
    END = 'e',
    DELIM = ':',
};

void bencode_dump(BencodeValue val);
// std::string trim_raw_info(std::string & raw_infohash);
std::array<uint8_t, SHA_DIGEST_LENGTH> infohash_bytes(
    const std::string& raw_infohash);
uint64_t get_torrent_size(const BencodeDict& dict);
std::string infohash_hex(const std::string& raw_infohash);

BencodeValue bdecode(std::istream& file, std::string& raw_infohash,
                     bool& _IH_parse_condition);

// BencodeValue bdecode(const std::vector<char>& vec, size_t& index);
std::vector<unsigned char> infohash(std::ifstream& file);

#endif