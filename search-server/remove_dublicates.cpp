#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer &search_server) {
    set<int> remove_list = {};
    for (auto [id, words] : search_server) {
        for (auto [id_2, words_2] : search_server) {
            if (id == id_2) {
                continue;
            }
            if (words.size() != words_2.size()) {
                continue;
            }
            int count = 0;
            for (auto [word, freq] : words) {
                if (words.count(word) == words_2.count(word)) {
                    ++count;
                }
            }
            if (count == words_2.size() && id_2 > id) {
                remove_list.insert(id_2);
            }
        }
    }


    for (int id: remove_list) {
        search_server.RemoveDocument(id);
        cout << "Found duplicate document id " << id << endl;
    }
}

/*map<string, double> words_origin = words;
        int id_origin = id;
        remove_list.insert(
                find_if(search_server.begin(), search_server.end(), [words_origin, id_origin](auto document) {
                    auto [id_doc, words_doc] = document;
                    if (id_doc == id_origin) {
                        return false;
                    }
                    int count = 0;
                    for (auto [word, freq] : words_origin) {
                        if (words_origin.count(word) == words_doc.count(word)) {
                            ++count;
                        }
                    }
                    if (count == words_origin.size() && id_origin < id_doc) {
                        return true;
                    }
                    return false;
                })->first);*/