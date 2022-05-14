#include <deque>
#include "search_server.h"

SearchServer::SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor
// from string container
{
}

SearchServer::SearchServer(const string_view& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor
// from string container
{
}

void
SearchServer::AddDocument(int document_id, const string_view & document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        documents_to_words_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(raw_query,
                                          [status](int document_id, DocumentStatus document_status, int rating) {
                                              return document_status == status;
                                          });
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view& raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);

    vector<string_view> matched_words;
    for (basic_string<char> word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word.substr(0));
        }
    }

    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(std::execution::sequenced_policy policy, const string_view& raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);

    vector<string_view> matched_words;
    for (basic_string<char> word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word.substr());
        }
    }
    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
   // vector<string_view> gg (matched_words.begin(), matched_words.end());
    return {matched_words, documents_.at(document_id).status};
}


std::tuple<std::vector<std::string_view>, DocumentStatus>
SearchServer::MatchDocument(std::execution::parallel_policy policy, std::string_view raw_query,
                            int document_id) const {

    if (!document_ids_.count(document_id)) {
        using namespace std::literals::string_literals;
        throw std::out_of_range("incorrect document id"s);
    }
    const auto query = (ParseQuery(raw_query));


    if (std::any_of(policy, query.minus_words.begin(), query.minus_words.end(),
                    [this, document_id](const auto& word) {
                        return documents_to_words_freqs_.at(document_id).count(word);
                    })) {
        return { std::vector<std::string_view>{}, documents_.at(document_id).status };
    }

    std::vector<std::string_view> matched_words(query.plus_words.size());
    auto last = std::copy_if(
            policy,
            std::make_move_iterator(query.plus_words.begin()), std::make_move_iterator(query.plus_words.end()),
            matched_words.begin(),
            [this, document_id, policy](const auto word) {
                // Почему вот так не проходит тест?
                // return document_to_word_freqs_.at(document_id).count(word);
                // А вот так проходит?
                return word_to_document_freqs_.at(word).count(document_id);
            });

    if(!isParallel(policy)) std::sort(policy,matched_words.begin(), last);

    std::sort(policy, matched_words.begin(), last);
    auto it = std::unique(policy, matched_words.begin(), last);
    matched_words.erase(it, matched_words.end());

    return { matched_words, documents_.at(document_id).status };
}

const map<string_view , double>& SearchServer::GetWordFrequencies(int document_id) const {
    static map<basic_string_view<char>, double> word_freq_view = (const map<basic_string_view<char>, double>&) documents_to_words_freqs_.at(
            document_id);
    return word_freq_view;
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

bool SearchServer::IsStopWord(const basic_string<char>& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

vector<string> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string> words;
    for (const string & word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + word + " is invalid"s);
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

SearchServer::QueryWord SearchServer::ParseQueryWord(const string_view& text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    string word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + text + " is invalid");
    }

    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const string_view & text) const {
    Query result;
    for (basic_string_view<char> word : SplitIntoWordsView(text)) {
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

SearchServer::Query_for_par SearchServer::ParseQueryForPar(const string_view& text) const {
    Query_for_par result;
    for (const string& word : SplitIntoWords(text)) {
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
double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

