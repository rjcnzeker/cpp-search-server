#include "string_processing.h"

vector<string> SplitIntoWords(const string_view &text) {
    vector<string> words;
    string word;
    for (const char c: text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

vector<string> SplitIntoWordsView(basic_string_view<char> str) {
    vector<string_view> result;

    str.remove_prefix(std::min(str.find_first_not_of(" "s), str.size()));
    const int64_t pos_end = str.npos;

    while (str.size()) {
        int64_t space = str.find(' ');
        result.push_back(space == pos_end ? str.substr(0) : str.substr(0, space));
        if (space == pos_end) {
            str.remove_prefix(str.size());
        } else {
            str.remove_prefix(space);
        }
        str.remove_prefix(std::min(str.find_first_not_of(" "), str.size()));
    }

    vector<string> ggg(result.begin(), result.end());
    return ggg;
}