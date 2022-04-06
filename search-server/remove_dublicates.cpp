#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer &search_server) {
    map<int, set<string>> documents = search_server.GetDocumentsList();
    set<int> remove_list = {};
    for (auto[id, words]: documents) {
        for (auto[id2, words2]: documents) {
            if (id2 == id) {
                continue;
            }
            if (words == words2 && id2 > id) {
                remove_list.insert(id2);
            }
        }
    }
    for (int id : remove_list) {
        search_server.RemoveDocument(id);
        cout << "Found duplicate document id " << id << endl;
    }
}