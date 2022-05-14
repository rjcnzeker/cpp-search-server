#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <set>

using namespace std;

vector<string> SplitIntoWords(const string_view &text);

vector<string> SplitIntoWordsView(string_view str);

template<typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer &strings) {
    set<string> non_empty_strings;
    for (const string& str: strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}