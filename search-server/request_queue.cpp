#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer &search_server) : search_server_(search_server) {
}

vector<Document> RequestQueue::AddFindRequest(const string &raw_query, DocumentStatus status) {
    vector<Document> result = search_server_.FindTopDocuments(raw_query, status);
    if (requests_.size() >= min_in_day_) {
        requests_.pop_front();
    }
    requests_.push_back({!result.empty()});
    return result;
}

vector<Document> RequestQueue::AddFindRequest(const string &raw_query) {
    vector<Document> result = search_server_.FindTopDocuments(raw_query);
    if (requests_.size() >= min_in_day_) {
        requests_.pop_front();
    }
    requests_.push_back({!result.empty()});
    return result;
}

int RequestQueue::GetNoResultRequests() const {
    int answer = 0;
    for (QueryResult ggg : requests_) {
        if (!ggg.result) {
            ++answer;
        }
    }
    return answer;
}
