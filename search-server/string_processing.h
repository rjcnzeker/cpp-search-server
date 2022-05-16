#pragma once

#include <vector>
#include <string>
#include <set>

using namespace std;

vector<string_view> SplitIntoWords(string_view text);

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings)
{
    std::set<std::string, std::less<>> non_empty_strings;
    for (std::string_view str : strings)
    {
        if (!str.empty())
        {
            non_empty_strings.emplace(str);
        }
    }
    return non_empty_strings;
}