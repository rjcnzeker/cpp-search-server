#include "string_processing.h"

vector<string_view> SplitIntoWords(string_view str) {
    vector<string_view> result;

    str.remove_prefix(std::min(str.find_first_not_of(" "), str.size()));
    const int64_t pos_end = std::string_view::npos;

    while (str.size()) {
        int64_t space = str.find(' ');
        result.push_back(space == pos_end ? str.substr(0) : str.substr(0, space));
        if (space == pos_end) {
            str.remove_prefix(str.size());
        } else {
            str.remove_prefix(space);
        }
        str.remove_prefix(std::min(str.find_first_not_of(" "s), str.size()));
    }

    return result;
}