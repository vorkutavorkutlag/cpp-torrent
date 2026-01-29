#include <variant>
#include <map>
#include <vector>
#include <string>
#include <cstdint>

#include <fstream>

struct BencodeValue;

using BencodeList = std::vector<BencodeValue>;
using BencodeDict = std::map<std::string, BencodeValue>;

struct BencodeValue : std::variant<
    std::int64_t,
    std::string,
    BencodeList,
    BencodeDict
> {
    using variant::variant;
};

enum class BencodeType : char {
  INTEGER = 'i',
  LIST = 'l',
  DICT = 'd',
  END = 'e',
};

BencodeValue decode(std::ifstream& file) {
  char c;
  file.get(c);
  
  switch(c) {
    
    case static_cast<char>(BencodeType::INTEGER): {
      std::int64_t int_val = 0;
      
      while (file.get(c)) {
        if (c == static_cast<char>(BencodeType::END)) break;
        int_val *= 10;
        int_val += c - '0';
      }
      
      return BencodeValue{int_val};
    }
    
    case static_cast<char>(BencodeType::LIST):
      // Would parse list here
      return BencodeValue{BencodeList{}};
    
    case static_cast<char>(BencodeType::DICT):
      // Would parse dict here
      return BencodeValue{BencodeDict{}};
    
    default: // BYTE_STRING
      file.unget(); // Put digit back
      // Would parse string here
      return BencodeValue{std::string{}};
      
  }
}

int main() {

  return 0;
}
