#include <map>

#include <variant>
#include <map>
#include <vector>
#include <string>
#include <cstdint>

using BencodeValue = std::variant<
    std::int64_t,                    
    std::string,                     
    std::vector<BencodeValue>,       
    std::map<std::string, BencodeValue> 
>;

int main() {
  
  return 0;
}
