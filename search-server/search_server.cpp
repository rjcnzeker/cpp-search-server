#include <deque>
#include "search_server.h"


void SearchServer::AddDocument(int document_id, const string_view& document, DocumentStatus status,
                               const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (basic_string_view<char> word : words) {
        word_to_document_freqs_[string(word)][document_id] += inv_word_count;
        documents_to_words_freqs_[document_id][string(word)] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query,
                            [status](int document_id, DocumentStatus document_status, int rating) {
                                return document_status == status;
                            });
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const {
    if (!CorrectUseDashes(raw_query) || !IsValidWord(raw_query)) {
        throw invalid_argument("invalid_argument"s);
    }
    const auto query = ParseQuery(raw_query);
    bool empty_return = false;
    vector<string_view> matched_words;
    for (const string_view word : query.minus_words) {
        string word_str = string(word);
        if (word_to_document_freqs_.count(word_str) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word_str).count(document_id)) {
            empty_return = true;
            break;
        }
    }
    if (empty_return) {
        return {vector<string_view>{}, documents_.at(document_id).status};
    }

    for (basic_string_view<char> word : query.plus_words) {
        string word_str = string(word);
        if (word_to_document_freqs_.count(word_str) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word_str).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return {matched_words, documents_.at(document_id).status};
}

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(std::execution::sequenced_policy policy, string_view raw_query, int document_id) const {
    if (!CorrectUseDashes(raw_query) || !IsValidWord(raw_query)) {
        throw invalid_argument("invalid_argument"s);
    }
    const auto query = ParseQuery(raw_query);
    bool empty_return = false;
    vector<string_view> matched_words;
    for (const string_view word : query.minus_words) {
        string word_str = string(word);
        if (word_to_document_freqs_.count(word_str) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word_str).count(document_id)) {
            empty_return = true;
            break;
        }
    }
    if (empty_return) {
        return {vector<string_view>{}, documents_.at(document_id).status};
    }

    for (basic_string_view<char> word : query.plus_words) {
        string word_str = string(word);
        if (word_to_document_freqs_.count(word_str) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word_str).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return {matched_words, documents_.at(document_id).status};
}

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(std::execution::parallel_policy policy, string_view raw_query, int document_id) const {
    if (!CorrectUseDashes(raw_query) || !IsValidWord(raw_query)) {
        throw invalid_argument("invalid_argument"s);
    }

    const auto query = ParseQueryForPar(raw_query);

    if (any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
               [this, document_id](string_view word) {
                   string word_str = string(word);
                   if (word_to_document_freqs_.count(word_str) == 0) {
                       return false;
                   }
                   if (std::count_if(std::execution::par, word_to_document_freqs_.at(word_str).begin(),
                                     word_to_document_freqs_.at(word_str).end(),
                                     [document_id](const pair<int, double>& ggg) {
                                         return ggg.first == document_id;
                                     })) {
                       return true;
                   }
                   return false;
               })) {

        return {vector<string_view>{}, documents_.at(document_id).status};
    }

    vector<string_view> matched_words(query.plus_words.size());
    matched_words.reserve(10000);

    transform(std::execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(),
              [this, document_id](string_view word) {
                  string word_str = string(word);
                  if (word_to_document_freqs_.count(word_str) == 0) {
                      return string_view{""};
                  }
                  if (std::count_if(std::execution::par, word_to_document_freqs_.at(word_str).begin(),
                                    word_to_document_freqs_.at(word_str).end(),
                                    [document_id](const pair<int, double>& ggg) {
                                        return ggg.first == document_id;
                                    })) {
                      return word;
                  }
                  return string_view{""};
              });

    std::sort(std::execution::par, matched_words.begin(), matched_words.end());
    auto last = std::unique(std::execution::par, matched_words.begin(), matched_words.end());
    matched_words.erase(last, matched_words.end());

    if (*matched_words.begin() == "") {
        matched_words.erase(matched_words.begin());
    }

    return {matched_words, documents_.at(document_id).status};
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    return (const map<basic_string_view<char>, double>&) documents_to_words_freqs_.at(document_id);
}

_Rb_tree_const_iterator<int> SearchServer::begin() {
    //Тут удобнее возвращать итератор на список документов, так я смогу пройтись по списку в RemoveDuplicates()
    return document_ids_.begin();
}

_Rb_tree_const_iterator<int> SearchServer::end() {
    return document_ids_.end();
}

void SearchServer::RemoveDocument(int document_id) {
    SearchServer::RemoveDocument(std::execution::seq, document_id);
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::CorrectUseDashes(string_view query) const {
    bool correct = true;
    if (query.length() > 1) {
        for (int i = 1; static_cast<size_t>(i) < query.length(); i++) {
            if (query[i - 1] == '-' && (query[i] == ' ' || query[i] == '-')) {
                return false;
            } else if (query[i] == '-' && query[i] == query.back()) {
                return false;
            }
        }
    } else {
        if (query == "-") {
            return false;
        }
    }

    return correct;
}

bool SearchServer::IsValidWord(string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view& text) const {
    vector<string_view> words;
    for (string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    return {text, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    Query result;
    for (string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            } else {
                result.plus_words.insert(query_word.data);
            }
        }
    }
    return result;
}

SearchServer::Query_for_par SearchServer::ParseQueryForPar(string_view text) const {
    Query_for_par result;
    for (string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    return result;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(string(word)).size());
}

