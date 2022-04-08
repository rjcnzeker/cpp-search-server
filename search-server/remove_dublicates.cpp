#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer &search_server) {
    set<int> remove_list = {};
    for (auto[id, words_frq_origin]: search_server) {
        set<string> words_origin = {};
        for (auto[word, freq]: words_frq_origin) {
            words_origin.insert(word);
        }
        int id_origin = id;
        _Rb_tree_iterator<pair<const int, map<basic_string<char>, double>>> dublicate_id = find_if(
                search_server.begin(), search_server.end(), [words_origin, id_origin](auto document) {
                    auto[id_doc, words_freq_doc] = document;
                    if (id_doc <= id_origin) {
                        return false;
                    }
                    set<string> words_doc = {};
                    for (auto[word, freq]: words_freq_doc) {
                        words_doc.insert(word);
                    }
                    if (words_origin.size() != words_doc.size()) {
                        return false;
                    }
                    if (words_origin == words_doc && id_origin <= id_doc) {
                        return true;
                    }
                    return false;
                });
        if (dublicate_id != search_server.end()) {
            remove_list.insert(dublicate_id->first);
        }
    }


    for (int id: remove_list) {
        search_server.RemoveDocument(id);
        cout << "Found duplicate document id " << id << endl;
    }
}