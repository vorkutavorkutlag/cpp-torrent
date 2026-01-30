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
    BencodeDict,
    std::monostate
> {
    using variant::variant;
};

enum class BencodeType : char {
  INTEGER = 'i',
  LIST = 'l',
  DICT = 'd',
  END = 'e',
  DELIM = ':',
};

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
      std::int64_t int_val = 0;

      while (file.get(c)) {
        if (c == static_cast<char>(BencodeType::END)) break;
        int_val *= 10;
        int_val += c - '0';
      }
      
      return BencodeValue{int_val};
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
      // Would parse dict here
      return BencodeValue{BencodeDict{}};
    }

    // len:bytes
    default: { // BYTE_STRING 
      file.unget(); // put digit back
      
      std::size_t length = 0;
      std::string str;

      // read len
      while (file.get(c)) {
        if (c == static_cast<char>(BencodeType::DELIM)) break;
        length *= 10;
        length += c - '0';
      }

      while(str.size() != length && file.get(c)) {
        str += c;
      }

      return BencodeValue{str};
    }
  }
}

int main() {

  return 0;
}
