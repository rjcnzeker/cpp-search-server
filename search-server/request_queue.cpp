#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer &search_server) : search_server_(search_server) {
}

vector<Document> RequestQueue::AddFindRequest(const string &raw_query, DocumentStatus status) {
    vector<Document> result = search_server_.FindTopDocuments(raw_query, status);
    ResultPush(result);
    return result;
}

vector<Document> RequestQueue::AddFindRequest(const string &raw_query) {
    vector<Document> result = search_server_.FindTopDocuments(raw_query);
    ResultPush(result);
    return result;
}

void RequestQueue::ResultPush(const vector<Document> &result) {
    if (requests_.size() >= min_in_day_) {
        requests_.pop_front();
    }
    requests_.push_back({!result.empty()});
}

int RequestQueue::GetNoResultRequests() const {
    int answer = 0;
    for (QueryResult result : requests_) {
        if (!result.result) {
            ++answer;
        }
    }
    return answer;
}
