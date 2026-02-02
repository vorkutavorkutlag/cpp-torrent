#ifndef BENCODE_H
#define BENCODE_H

#include <fstream>
#include <variant>
#include <map>
#include <vector>
#include <string>
#include <cstdint>

struct BencodeValue;

using BencodeList = std::vector<BencodeValue>;
using BencodeDict = std::map<std::string, BencodeValue>;

struct BencodeValue : std::variant<
    std::int64_t,
    std::string,
    BencodeList,
    BencodeDict,
    std::monostate
> {
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
std::vector<unsigned char> infohash_bytes(const std::string & raw_infohash);
std::string infohash_hex(const std::string & raw_infohash);

BencodeValue decode(std::ifstream& file, std::string & raw_infohash, bool & _IH_parse_condition);
std::vector<unsigned char> infohash(std::ifstream& file);


#endif