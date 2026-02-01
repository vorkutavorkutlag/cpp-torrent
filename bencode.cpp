#include <variant>
#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <cassert>
#include <fstream>
#include <iostream>

#include "bencode.h"

enum class BencodeType : char {
  INTEGER = 'i',
  LIST = 'l',
  DICT = 'd',
  END = 'e',
  DELIM = ':',
};


BencodeValue _decode_int(std::ifstream& file) {
  std::int64_t int_val = 0;
  char c;

  while (file.get(c)) {
    if (c == static_cast<char>(BencodeType::END)) break;
    int_val *= 10;
    int_val += c - '0';
  }
  
  return BencodeValue{int_val};
}

BencodeValue _decode_bytestring(std::ifstream& file) {
  char c;
  file.unget(); // put digit back
  
  std::size_t length = 0;
  std::string str;

  // read len
  while (file.get(c)) {
    if (c == static_cast<char>(BencodeType::DELIM)) break;
    length *= 10;
    length += c - '0';
  }

  // read str
  while(str.size() != length && file.get(c)) {
    str += c;
  }

  return BencodeValue{str};
}

BencodeValue decode(std::ifstream& file) {
  char c;
  file.get(c);
  
  switch(c) {
    
    // e (for recursive purpose)
    case static_cast<char>(BencodeType::END): {
        return BencodeValue{std::monostate{}};
    }

    // i<integer>e
    case static_cast<char>(BencodeType::INTEGER): {
      return _decode_int(file);
    }
    
    // l<items>e
    case static_cast<char>(BencodeType::LIST): {
      BencodeList list;

      for (;;) {
        BencodeValue val = decode(file);
        if (std::holds_alternative<std::monostate>(val)) break;

        list.push_back(val);
      }
      
      return BencodeValue{list};
    }
    
    // d<pairs>e
    case static_cast<char>(BencodeType::DICT): {
      BencodeDict dict;

      for (;;) {
        BencodeValue key = decode(file);
        
        if (std::holds_alternative<std::monostate>(key)) break;

        BencodeValue val = decode(file);

        assert(std::holds_alternative<std::string>(key) && "Bencode dictionary key must be string!");

        std::string str_key = std::get<std::string>(key);
        dict[str_key] = val;
      }
      

      return BencodeValue{dict};
    }

    // len:bytes
    default: { // BYTE_STRING 
      return _decode_bytestring(file);
    }
  }
}

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
    for(auto iter = dict.begin(); iter != dict.end(); ++iter) {
        std::cout << iter->first << std::endl;
        bencode_dump(iter->second);
      }
  }

}