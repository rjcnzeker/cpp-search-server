#pragma once

#include <deque>

#include "search_server.h"
#include "document.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer &search_server);

    template<typename DocumentPredicate>
    vector<Document> AddFindRequest(const string &raw_query, DocumentPredicate document_predicate);

    vector<Document> AddFindRequest(const string &raw_query, DocumentStatus status);

    vector<Document> AddFindRequest(const string &raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        bool result;
    };
    deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;

    void ResultPush(const vector<Document> &result);
};

template<typename DocumentPredicate>
vector<Document> RequestQueue::AddFindRequest(const string &raw_query, DocumentPredicate document_predicate) {
    vector<Document> result = search_server_.FindTopDocuments(raw_query, document_predicate);
    ResultPush(result);
    return result;
}