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
#include "concurrent_map.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const int RELEVANCE_ERROR_RATE = 1e-6;

class SearchServer {
public:
    template<typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text)
            : SearchServer(std::string_view(stop_words_text)) {}

    explicit SearchServer(std::string_view stop_words_text)
            : SearchServer(SplitIntoWords(stop_words_text)) {}

    void AddDocument(int document_id, const string_view& document, DocumentStatus status, const vector<int>& ratings);

    template<typename Policy, typename DocumentPredicate>
    vector<Document> FindTopDocuments(Policy policy, string_view raw_query, DocumentPredicate document_predicate) const;

    template<typename Policy>
    vector<Document> FindTopDocuments(Policy policy, string_view raw_query, DocumentStatus status) const;

    template<typename Policy>
    vector<Document> FindTopDocuments(Policy policy, string_view raw_query) const;

    template<typename DocumentPredicate>
    vector<Document> FindTopDocuments(string_view raw_query, DocumentPredicate document_predicate) const;

    vector<Document> FindTopDocuments(string_view raw_query, DocumentStatus status) const;

    vector<Document> FindTopDocuments(string_view raw_query) const;


    int GetDocumentCount() const;

    tuple<vector<string_view>, DocumentStatus> MatchDocument(string_view raw_query, int document_id) const;

    tuple<vector<string_view>, DocumentStatus>
    MatchDocument(execution::sequenced_policy policy, string_view raw_query, int document_id) const;

    tuple<vector<string_view>, DocumentStatus>
    MatchDocument(execution::parallel_policy policy, string_view raw_query, int document_id) const;

    const map<string_view, double>& GetWordFrequencies(int document_id) const;

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

    const set<string, less<>> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, map<string, double>> documents_to_words_freqs_;
    map<int, DocumentData> documents_;
    set<int, less<>> document_ids_;

    bool IsStopWord(string_view word) const;

    bool CorrectUseDashes(string_view query) const;

    static bool IsValidWord(string_view word);

    vector<string_view> SplitIntoWordsNoStop(const string_view& text) const;

    static int ComputeAverageRating(const vector<int>& ratings);

    struct QueryWord {
        basic_string_view<char> data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string_view text) const;

    struct Query {
        set<string_view, less<>> plus_words;
        set<string_view, less<>> minus_words;
    };

    struct Query_for_par {
        vector<string_view> plus_words;
        vector<string_view> minus_words;
    };

    Query ParseQuery(string_view text) const;

    Query_for_par ParseQueryForPar(string_view text) const;

    double ComputeWordInverseDocumentFreq(const string& word) const;

    template<typename Policy, typename DocumentPredicate>
    vector<Document> FindAllDocuments(Policy policy, const Query_for_par& query, DocumentPredicate document_predicate) const;

    bool IsWordInDocument(int document_id, const string& word) const;
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
vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template<typename Policy, typename DocumentPredicate>
vector<Document>
SearchServer::FindTopDocuments(Policy policy, string_view raw_query, DocumentPredicate document_predicate) const {
    Query_for_par query = ParseQueryForPar(raw_query);

    auto last_plus = std::unique(policy, query.plus_words.begin(), query.plus_words.end());
    query.plus_words.erase(last_plus, query.plus_words.end());
    auto last_minus = std::unique(policy, query.minus_words.begin(), query.minus_words.end());
    query.minus_words.erase(last_minus, query.minus_words.end());

    if (!CorrectUseDashes(raw_query) || !IsValidWord(raw_query)) {
        throw std::invalid_argument("invalid_argument"s);
    }

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(policy, matched_documents.begin(), matched_documents.end(),
         [](const Document& lhs, const Document& rhs) {
             if (abs(lhs.relevance - rhs.relevance) < RELEVANCE_ERROR_RATE) {
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

template<typename Policy>
vector<Document> SearchServer::FindTopDocuments(Policy policy, string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query,
                            [status](int document_id, DocumentStatus document_status, int rating) {
                                return document_status == status;
                            });
}

template<typename Policy>
vector<Document> SearchServer::FindTopDocuments(Policy policy, string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template<typename Policy, typename DocumentPredicate>
vector<Document>
SearchServer::FindAllDocuments(Policy policy, const Query_for_par& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance_concurrent(1000);

    for_each(policy, query.plus_words.begin(), query.plus_words.end(), [this, &document_predicate, &document_to_relevance_concurrent, &query] (string_view word) {
        string word_str = string(word);
        if (word_to_document_freqs_.count(word_str) == 0) {
            return ;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word_str);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word_str)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
               // document_to_relevance_concurrent.BuildOrdinaryMap()[document_id] += term_freq * inverse_document_freq;
                document_to_relevance_concurrent[document_id].ref_to_value += term_freq * inverse_document_freq;
            }
        }
    });

    for_each(policy, query.minus_words.begin(), query.minus_words.end(), [this, &document_to_relevance_concurrent](string_view word) {
        string word_str = string(word);
        if (word_to_document_freqs_.count(word_str) == 0) {
            return ;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word_str)) {
            document_to_relevance_concurrent.erase(document_id);
        }
    });

    map<int, double> document_to_relevance = document_to_relevance_concurrent.BuildOrdinaryMap();

    vector<Document> matched_documents;
    matched_documents.reserve(document_to_relevance.size() + 1);

    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.emplace_back(document_id, relevance, documents_.at(document_id).rating);
    }
    return matched_documents;
}


template<typename P>
void SearchServer::RemoveDocument(P policy, int document_id) {
    std::vector<string_view> words_to_delete(GetWordFrequencies(document_id).size());

    std::transform(policy, documents_to_words_freqs_.at(document_id).begin(),
                   documents_to_words_freqs_.at(document_id).end(), words_to_delete.begin(),
                   [](const pair<const string_view, double>& word) {
                       //auto* word_ref = const_cast<string*>(&word.first);
                       return word.first;
                   });

    std::for_each(policy, words_to_delete.begin(), words_to_delete.end(),
                  [&document_id, this](string_view word) {

                      word_to_document_freqs_.at(string(word)).erase(document_id);
                  });


    //Удавление из списка документов и их слов
    documents_to_words_freqs_.erase(document_id);
    //Удаление из списка документов
    documents_.erase(document_id);
    //Удаление из списка айди
    document_ids_.erase(document_id);
}