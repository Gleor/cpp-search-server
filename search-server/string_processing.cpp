#include "string_processing.h"
#include <vector>
#include <string>
#include <algorithm>

std::vector<std::string_view> SplitIntoWords(std::string_view str) {
    std::vector<std::string_view> result;
    str.remove_prefix(std::min(str.find_first_not_of(" "), str.size()));
    const int64_t pos_end = str.npos;
    while (str.size()) {
        int64_t space = str.find(' ');
        result.push_back(str.substr(0, space));
        space == pos_end ? str.remove_prefix(str.size()) : str.remove_prefix(space + 1);
        str.remove_prefix(std::min(str.find_first_not_of(" "), str.size()));
    }
    return result;
}
