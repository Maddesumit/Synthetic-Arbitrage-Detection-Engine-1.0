#pragma once

#include <string>
#include <algorithm>
#include <cctype>

namespace arbitrage {
namespace utils {

// Case insensitive string comparison
inline bool equalsIgnoreCase(const std::string& str1, const std::string& str2) {
    if (str1.size() != str2.size()) {
        return false;
    }
    
    return std::equal(str1.begin(), str1.end(), str2.begin(),
        [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) == 
                   std::tolower(static_cast<unsigned char>(b));
        });
}

} // namespace utils
} // namespace arbitrage
