#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer &search_server) {
    set<int> remove_list = {};
    set<set<string>> documents_words = {};
    for (int id : search_server) {
        set<string> words_origin = {};
        for (auto [word, freq] : search_server.GetWordFrequencies(id)) {
            words_origin.insert(word);
        }
        if (std::binary_search(documents_words.begin(), documents_words.end(), words_origin)) {
            remove_list.insert(id);
        } else {
            documents_words.insert(words_origin);
        }
    }
    for (int id: remove_list) {
        search_server.RemoveDocument(execution::par, id);
        cout << "Found duplicate document id " << id << endl;
    }
}