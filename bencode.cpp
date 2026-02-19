#include "bencode.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "constants.h"

BencodeValue _decode_int(std::istream& file, std::string& raw_info,
                         bool& _IH_parse_condition) {
    std::int64_t int_val = 0;
    char c;

    while (file.get(c)) {
        if (_IH_parse_condition) raw_info += c;
        if (c == static_cast<char>(BencodeChar::END)) break;
        int_val *= 10;
        int_val += c - '0';
    }
    return BencodeValue{int_val};
}

// BencodeValue _decode_int(const std::vector<char>& vec, size_t& index) {
//     std::int64_t int_val = 0;

//     while (index < vec.size()) {
//         if (vec[index] == static_cast<char>(BencodeChar::END)) {
//             ++index;
//             break;
//         }
//         int_val *= 10;
//         int_val += vec[index++] - '0';
//     }
//     return BencodeValue{int_val};
// }

BencodeValue _decode_bytestring(std::istream& file, std::string& raw_info,
                                bool& _IH_parse_condition) {
    char c;
    file.unget();  // put digit back
    if (_IH_parse_condition) raw_info.pop_back();

    std::size_t length = 0;
    std::string str;

    // read len
    while (file.get(c)) {
        if (_IH_parse_condition) raw_info += c;
        if (c == static_cast<char>(BencodeChar::DELIM)) break;
        length *= 10;
        length += c - '0';
    }

    // read str
    while (str.size() != length && file.get(c)) {
        if (_IH_parse_condition) raw_info += c;
        str += c;
    }

    if (str == TF_String[INFO])
        _IH_parse_condition = true;  // Begin reading infohash

    return BencodeValue{str};
}

// BencodeValue _decode_bytestring(const std::vector<char>& vec, size_t& index)
// {
//     --index;  // put digit back

//     size_t length = 0;
//     std::string str;

//     // read len
//     while (index < vec.size()) {
//         if (vec[index] == static_cast<char>(BencodeChar::DELIM)) {
//             ++index;
//             break;
//         }
//         length *= 10;
//         length += vec[index++] - '0';
//     }

//     // read str
//     while (str.size() != length && index < vec.size()) {
//         str += vec[index++];
//     }

//     return BencodeValue{str};
// }

BencodeValue bdecode(std::istream& file, std::string& raw_info,
                     bool& _IH_parse_condition) {
    char c;
    file.get(c);

    if (_IH_parse_condition) raw_info += c;

    switch (c) {
        // e (for recursive purpose)
        case static_cast<char>(BencodeChar::END): {
            return BencodeValue{std::monostate{}};
        }

        // i<integer>e
        case static_cast<char>(BencodeChar::INTEGER): {
            return _decode_int(file, raw_info, _IH_parse_condition);
        }

        // l<items>e
        case static_cast<char>(BencodeChar::LIST): {
            BencodeList list;

            for (;;) {
                BencodeValue val = bdecode(file, raw_info, _IH_parse_condition);
                if (std::holds_alternative<std::monostate>(val)) break;

                list.push_back(val);
            }

            return BencodeValue{list};
        }

        // d<pairs>e
        case static_cast<char>(BencodeChar::DICT): {
            BencodeDict dict;

            for (;;) {
                BencodeValue key = bdecode(file, raw_info, _IH_parse_condition);

                if (std::holds_alternative<std::monostate>(key)) break;
                assert(std::holds_alternative<std::string>(key) &&
                       "Bencode dictionary key must be string!");

                std::string str_key = std::get<std::string>(key);

                BencodeValue val = bdecode(file, raw_info, _IH_parse_condition);

                dict[str_key] = val;
            }

            return BencodeValue{dict};
        }

        // len:bytes
        default: {  // BYTE_STRING
            return _decode_bytestring(file, raw_info, _IH_parse_condition);
        }
    }
}

// BencodeValue bdecode(const std::vector<char>& vec, size_t& index) {
//     std::cout << "Read Char: " << vec[index] << '\n';
//     switch (vec[index++]) {
//         // e (for recursive purpose)
//         case static_cast<char>(BencodeChar::END): {
//             return BencodeValue{std::monostate{}};
//         }

//         // i<integer>e
//         case static_cast<char>(BencodeChar::INTEGER): {
//             return _decode_int(vec, index);
//         }

//         // l<items>e
//         case static_cast<char>(BencodeChar::LIST): {
//             BencodeList list;

//             for (;;) {
//                 BencodeValue val = bdecode(vec, index);
//                 if (std::holds_alternative<std::monostate>(val)) break;

//                 list.push_back(val);
//             }

//             return BencodeValue{list};
//         }

//         // d<pairs>e
//         case static_cast<char>(BencodeChar::DICT): {
//             BencodeDict dict;

//             for (;;) {
//                 BencodeValue key = bdecode(vec, index);

//                 if (std::holds_alternative<std::monostate>(key)) break;
//                 assert(std::holds_alternative<std::string>(key) &&
//                        "Bencode dictionary key must be string!");

//                 std::string str_key = std::get<std::string>(key);

//                 BencodeValue val = bdecode(vec, index);

//                 dict[str_key] = val;
//             }

//             return BencodeValue{dict};
//         }

//         // len:bytes
//         default: {  // BYTE_STRING
//             return _decode_bytestring(vec, index);
//         }
//     }
// }

void bencode_dump(BencodeValue val) {
    if (std::holds_alternative<int64_t>(val)) {
        std::cout << std::get<int64_t>(val) << std::endl;

    } else if (std::holds_alternative<std::string>(val)) {
        std::cout << std::get<std::string>(val) << std::endl;

    } else if (std::holds_alternative<BencodeList>(val)) {
        for (const auto i : std::get<BencodeList>(val)) {
            bencode_dump(i);
        }

    } else if (std::holds_alternative<BencodeDict>(val)) {
        BencodeDict dict = std::get<BencodeDict>(val);
        for (auto iter = dict.begin(); iter != dict.end(); ++iter) {
            std::cout << iter->first << std::endl;
            bencode_dump(iter->second);
        }
    }
}

std::string trim_raw_info(const std::string& raw_info) {
    return raw_info.substr(0, raw_info.size() - 1);
}

std::array<uint8_t, SHA_DIGEST_LENGTH> infohash_bytes(
    const std::string& raw_info) {
    std::string trimmed_info = trim_raw_info(raw_info);
    std::array<uint8_t, SHA_DIGEST_LENGTH> hash;

    SHA1(reinterpret_cast<const unsigned char*>(trimmed_info.data()),
         trimmed_info.size(), hash.data());

    return hash;
}

uint64_t get_torrent_size(const BencodeDict& dict) {
    const BencodeDict info =
        std::get<BencodeDict>(dict.find(TF_String[INFO])->second);

    // flength exists <=> single file torrent
    if (info.count(TF_String[FLENGTH])) {
        const int64_t size_int =
            std::get<int64_t>(info.find(TF_String[FLENGTH])->second);
        return static_cast<uint64_t>(size_int);
    }

    // handle multi-file torrent in <files>
    uint64_t sum_size = 0;
    const BencodeList files =
        std::get<BencodeList>(info.find(TF_String[FILES])->second);
    for (const auto& pair : files) {
        const BencodeDict file = std::get<BencodeDict>(pair);
        const int64_t size_int =
            std::get<int64_t>(file.find(TF_String[LENGTH])->second);
        // const BencodeList pathl =
        // std::get<BencodeList>(file.find(TF_String[PATH])->second); const
        // std::string path = std::get<std::string>(pathl[0]); std::cout << path
        // << std::endl;
        sum_size += static_cast<uint64_t>(size_int);
    }

    return sum_size;
}

std::string infohash_hex(const std::string& raw_info) {
    std::string trimmed_info = trim_raw_info(raw_info);
    unsigned char hash[SHA_DIGEST_LENGTH];

    SHA1(reinterpret_cast<const unsigned char*>(trimmed_info.data()),
         trimmed_info.size(), hash);

    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(hash[i]);
    }

    return ss.str().substr(0, INFOHASH_SIZE);
}