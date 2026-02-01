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

void bencode_dump(BencodeValue val);
BencodeValue decode(std::ifstream& file);

#endif