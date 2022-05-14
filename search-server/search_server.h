#pragma once

#include <vector>
#include <string>
#include <tuple>
#include <set>
#include <map>
#include <cmath>
#include <algorithm>
#include <execution>
#include <deque>

#include "string_processing.h"
#include "document.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    template<typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const string& stop_words_text);

    explicit SearchServer(const std::string_view& stop_words_text);

    void AddDocument(int document_id, const string_view& document, DocumentStatus status, const vector<int>& ratings);

    template<typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string_view & raw_query, DocumentPredicate document_predicate) const;

    vector<Document> FindTopDocuments(const string_view& raw_query, DocumentStatus status) const;

    vector<Document> FindTopDocuments(const string_view& raw_query) const;

    int GetDocumentCount() const;

    tuple<vector<string_view>, DocumentStatus> MatchDocument(const string_view& raw_query, int document_id) const;

    tuple<vector<string_view>, DocumentStatus>
    MatchDocument(execution::sequenced_policy policy, const string_view& raw_query, int document_id) const;

    tuple<vector<string_view>, DocumentStatus>
    MatchDocument(execution::parallel_policy policy, const string_view& raw_query, int document_id) const;

    const map<string_view , double>& GetWordFrequencies(int document_id) const;

    _Rb_tree_const_iterator<int> begin();

    _Rb_tree_const_iterator<int> end();

    void RemoveDocument(int document_id);

    template<typename P>
    void RemoveDocument(P policy, int document_id);

private:

    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, map<string , double>> documents_to_words_freqs_;
    map<int, DocumentData> documents_;
    set<int> document_ids_;

    bool IsStopWord(const string& word) const;

    static bool IsValidWord(const string& word);

    vector<string> SplitIntoWordsNoStop(const string_view& text) const;

    static int ComputeAverageRating(const vector<int>& ratings);

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const string& text) const;

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    struct Query_for_par {
        vector<string> plus_words;
        vector<string> minus_words;
    };

    Query ParseQuery(const string_view& text) const;

    Query_for_par ParseQueryForPar(const string_view& text) const;

    double ComputeWordInverseDocumentFreq(const string& word) const;

    template<typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

};

template<typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw invalid_argument("Some of stop words are invalid"s);
    }
}

template<typename DocumentPredicate>
vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query, DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
            return lhs.rating > rhs.rating;
        } else {
            return lhs.relevance > rhs.relevance;
        }
    });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template<typename DocumentPredicate>
vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    map<int, double> document_to_relevance;
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template<typename P>
void SearchServer::RemoveDocument(P policy, int document_id) {
    std::vector<string*> words_to_delete(GetWordFrequencies(document_id).size());

    std::transform(policy, documents_to_words_freqs_.at(document_id).begin(),
                   documents_to_words_freqs_.at(document_id).end(), words_to_delete.begin(),
                   [](const pair<const string, double>& word) {
                       auto* word_ref = const_cast<string*>(&word.first);
                       return word_ref;
                   });

    std::for_each(policy, words_to_delete.begin(), words_to_delete.end(),
                  [&document_id, this](std::string* word) {
                      word_to_document_freqs_.at(*word).erase(document_id);
                  });


    //Удавление из списка документов и их слов
    documents_to_words_freqs_.erase(document_id);
    //Удаление из списка документов
    documents_.erase(document_id);
    //Удаление из списка айди
    document_ids_.erase(document_id);
}