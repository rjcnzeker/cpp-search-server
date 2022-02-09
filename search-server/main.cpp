#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cassert>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string &text) {
    vector<string> words;
    string word;
    for (const char c: text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

struct Document {
    int id;
    double relevance;
    int rating;
    DocumentStatus status;
};

class SearchServer {
public:
    void SetStopWords(const string &text) {
        for (const string &word: SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string &document, DocumentStatus status, const vector<int> &ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string &word: words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
                           DocumentData{
                                   ComputeAverageRating(ratings),
                                   status
                           });
    }

    vector<Document> FindTopDocuments(const string &raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus status_doc,
                                                    int rating) {
            return status_doc == status;
        });
    }

    template<typename Predicate>
    vector<Document> FindTopDocuments(const string &raw_query, Predicate predicate) const {
        const Query query = ParseQuery(raw_query);

        vector<Document> matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document &lhs, const Document &rhs) {
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

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string &raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string &word: query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string &word: query.minus_words) {
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

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string &word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string &text) const {
        vector<string> words;
        for (const string &word: SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int> &ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating: ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
                text,
                is_minus,
                IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string &text) const {
        Query query;
        for (const string &word: SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string &word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template<typename Predicate>
    vector<Document> FindAllDocuments(const Query &query, Predicate predicate) const {
        map<int, double> document_to_relevance;

        for (const string &word: query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto[document_id, term_freq]: word_to_document_freqs_.at(word)) {

                if (predicate(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }

            }
        }

        for (const string &word: query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto[document_id, _]: word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto[document_id, relevance]: document_to_relevance) {
            matched_documents.push_back(
                    {document_id, relevance, documents_.at(document_id).rating, documents_.at(document_id).status});
        }

        return matched_documents;
    }
};

void PrintDocument(const Document &document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

//-----------------------Фреймворк----------------------

template <typename T>
void RunTestImpl(T func, const string& func_str) {
    func();
    cerr << func_str << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)

template<typename T>
ostream &operator<<(ostream &out, const set<T> &container) {
    out << "{" ;
    bool is_first = true;
    for (auto c: container) {
        if (!is_first) {
            out << ", "s;
        }
        out << c;
        is_first = false;
    }
    out << "}" ;
    return out;
}

template<typename T>
ostream &operator<<(ostream &out, const vector<T> &container) {
    out << "[";
    bool is_first = true;
    for (auto c: container) {
        if (!is_first) {
            out << ", "s;
        }
        out << c;
        is_first = false;
    }
    out << "]" ;
    return out;
}

template<typename Key, typename Value>
ostream &operator<<(ostream &out, const map<Key, Value> &container) {
    out << "{";
    bool is_first = true;
    for (pair<Key, Value> element: container) {
        if (!is_first) {
            out << ", "s;
        }
        out << element.first << ": " << element.second;
        is_first = false;
    }
    out << "}";
    return out;
}

template<typename T, typename U>
void AssertEqualImpl(const T &t, const U &u, const string &t_str, const string &u_str, const string &file,
                     const string &func, unsigned line, const string &hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string &expr_str, const string &file, const string &func, unsigned line,
                const string &hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))


//------------------Конец фреймворка-----------------------



void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT(found_docs.size() == 1);
        const Document &doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }

    {
        SearchServer server;
        server.SetStopWords("-  -- "s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(!server.FindTopDocuments("in"s).empty());
    }

    {
        SearchServer server;
        server.SetStopWords("in"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }

    {
        SearchServer server;
        server.SetStopWords("  "s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("       "s).empty());
    }
}

void TestMinusWords() {
    const int doc_id = 15;
    const vector<int> ratings = {1, 2, 3};

    {
        const string content = "cat in the city"s;
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("-in"s).empty());
    }
    {
        const string content = "cat in the city"s;
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("-cat -in -the -city"s).empty());
    }

}

void TestMatchedDocuments() {
    const int doc_id = 0;
    const string content = "b a ccc ddd"s;
    const vector<int> ratings = {1, 2, 3};

    {
        tuple<vector<string>, DocumentStatus> founding = {{"a", "b", "ccc", "ddd"}, DocumentStatus::ACTUAL};
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        tuple<vector<string>, DocumentStatus> matched = server.MatchDocument("a b ccc ddd", doc_id);
        auto [matched_vector, matched_Document_Status] = matched;
        auto [founding_vector, founding_Document_Status] = founding;
        ASSERT_EQUAL(matched_vector, founding_vector);
    }

    {
        tuple<vector<string>, DocumentStatus> founding = {{"a", "b", "ccc"}, DocumentStatus::BANNED};
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        tuple<vector<string>, DocumentStatus> matched = server.MatchDocument("a b ccc -ddd", doc_id);
        auto [matched_vector, matched_Document_Status] = matched;
        auto [founding_vector, founding_Document_Status] = founding;
        ASSERT(matched_vector != founding_vector);
    }

    // ASSERT_NON_EQUAL Для несовпадающих векторов
    {
        tuple<vector<string>, DocumentStatus> founding = {{"b"}, DocumentStatus::ACTUAL};
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        tuple<vector<string>, DocumentStatus> matched = server.MatchDocument("g b i m", doc_id);
        auto [matched_vector, matched_Document_Status] = matched;
        auto [founding_vector, founding_Document_Status] = founding;
        ASSERT_EQUAL(matched_vector, founding_vector);
    }
}

void TestSort() {
    const string Hint = "Документы сортируются не правильно";
    {
        SearchServer server;
        server.SetStopWords("и не"s);

        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(2, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, {9});

        /*   vector<Document> searched = {{1, 0.866434, 5},
                                        {0, 0.173287, 2},
                                        {2, 0.173287, -1}};*/

        vector<Document> founded = server.FindTopDocuments("пушистый ухоженный кот", DocumentStatus::ACTUAL);

        double max_rel = 10.0;
        int count = 0;
        for (auto found: founded) {
            if (found.relevance <= max_rel || abs(found.relevance - max_rel) < 1e-6) {
                max_rel = found.relevance;
                ++count;
            }
        }
        if (count == founded.size()) {
            ASSERT_HINT(true, Hint);
        } else {
            ASSERT_HINT(true, Hint);
        }
    }
    {
        string hint = "Документы с одинаковой релевантностью должны быть отсортированы по рейтингу в порядке убывания"s;
        SearchServer server;
        server.SetStopWords("и"s);

        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::IRRELEVANT, {10});
        server.AddDocument(1, "белый кот и модный ошейник"s, DocumentStatus::IRRELEVANT, {20});
        server.AddDocument(3, "белый кот и модный ошейник"s, DocumentStatus::IRRELEVANT, {30});
        server.AddDocument(4, "белый кот и модный ошейник"s, DocumentStatus::IRRELEVANT, {40});
        server.AddDocument(5, ""s, DocumentStatus::ACTUAL, {9});

        /*   vector<Document> searched = {{1, 0.866434, 5},
                                        {0, 0.173287, 2},
                                        {2, 0.173287, -1}};*/

        vector<Document> founded = server.FindTopDocuments("белый кот и модный ошейник", DocumentStatus::IRRELEVANT);

        ASSERT_EQUAL(founded.size() , 4);
        ASSERT_EQUAL_HINT(founded.at(0).rating, 40, hint);
        ASSERT_EQUAL_HINT(founded.at(1).rating, 30, hint);
        ASSERT_EQUAL_HINT(founded.at(2).rating, 20, hint);
        ASSERT_EQUAL_HINT(founded.at(3).rating, 10, hint);

    }

}

void TestRating() {
    const string Hint = "Рейтинг вычисляется не правильно";
    {
        SearchServer server;
        server.SetStopWords("и не"s);

        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {0, 0, 0});
        server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {-50, -10, 9});
        server.AddDocument(2, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, {});

        vector<Document> founded = server.FindTopDocuments("пушистый ухоженный кот", DocumentStatus::ACTUAL);

        ASSERT_EQUAL_HINT(founded[0].rating, -17, Hint);
        ASSERT_EQUAL_HINT(founded[1].rating, 0, Hint);
        ASSERT_EQUAL_HINT(founded[2].rating, 0, Hint);
    }
    {
        SearchServer server;
        server.SetStopWords("и не"s);

        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {0, 200, 200});
        server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {-50, -10, -9, -8, -1000, -9000});
        server.AddDocument(2, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, {90000, 90000, 90000});

        vector<Document> founded = server.FindTopDocuments("пушистый ухоженный кот", DocumentStatus::ACTUAL);

        ASSERT_EQUAL_HINT(founded[0].rating, -1679, Hint);
        ASSERT_EQUAL_HINT(founded[1].rating, 90000, Hint);
        ASSERT_EQUAL_HINT(founded[2].rating, 133, Hint);
    }
}

void TestPredicate() {
    SearchServer search_server;
    search_server.SetStopWords("и не"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});

    {
        auto predicate = [](int document_id, DocumentStatus status,
                            int rating) { return document_id % 2 == 0; };
        vector<Document> founded = search_server.FindTopDocuments("пушистый ухоженный кот"s, predicate);

        for (auto found: founded) {
            if (found.id % 2 != 0) {
                ASSERT(false);
            }
        }
    }
    {
        auto predicate = [](int document_id, DocumentStatus status,
                            int rating) { return rating > 0; };
        vector<Document> founded = search_server.FindTopDocuments("пушистый ухоженный кот"s, predicate);

        for (auto found: founded) {
            if (found.rating <= 0) {
                ASSERT(false);
            }
        }
    }

}

void TestStatus() {
    {
        SearchServer server;
        server.SetStopWords("и не"s);

        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {0, 0, 0});
        server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {-50, -10, 9});
        server.AddDocument(2, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, {});

        vector<Document> founded = server.FindTopDocuments("пушистый ухоженный кот", DocumentStatus::BANNED);

        ASSERT(founded.empty());
    }

    {
        SearchServer server;
        server.SetStopWords("и не"s);

        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::BANNED, {0, 0, 0});
        server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::BANNED, {-50, -10, 9});
        server.AddDocument(2, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {});

        vector<Document> founded = server.FindTopDocuments("пушистый ухоженный кот", DocumentStatus::BANNED);

        ASSERT_EQUAL(founded.size(), 3);
    }

}

void TestIDF_TF() {
    {
        SearchServer server;
        server.SetStopWords("и"s);

        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(2, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {9});

        /*   vector<Document> searched = {{1, 0.866434, 5},
                                        {0, 0.173287, 2},
                                        {2, 0.173287, -1}};*/

        vector<Document> founded = server.FindTopDocuments("белый кот и модный ошейник", DocumentStatus::ACTUAL);

        ASSERT_EQUAL(founded.at(0).relevance , 0.0);
        ASSERT_EQUAL(founded.at(1).relevance, 0.0);
        ASSERT_EQUAL(founded.at(2).relevance, 0.0);

    }
    {
        SearchServer search_server;
        search_server.SetStopWords("и не"s);

        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, {9});

        vector<Document> founded = search_server.FindTopDocuments("пушистый ухоженный кот");

        ASSERT_EQUAL(founded.at(0).relevance, 0.86643397569993164);
        ASSERT_EQUAL(founded.at(1).relevance, 0.23104906018664842);
        ASSERT_EQUAL(founded.at(2).relevance, 0.17328679513998632);
        ASSERT_EQUAL(founded.at(3).relevance, 0.17328679513998632);
    }

    {
        SearchServer search_server;
        search_server.SetStopWords("ухоженный пёс выразительные глаза"s);

        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, {9});

        vector<Document> founded = search_server.FindTopDocuments("-пушистый ухоженный кот");

        ASSERT_EQUAL(founded.at(0).relevance, 0.13862943611198905);
    }

    {
        SearchServer search_server;
        search_server.SetStopWords("и"s);

        search_server.AddDocument(0, "белый и модный ошейник ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза глаза глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        search_server.AddDocument(3, "скворец сковрец"s, DocumentStatus::ACTUAL, {9});

        vector<Document> founded = search_server.FindTopDocuments("ошейник ошейник пёс пёс скворец");

        ASSERT_EQUAL(founded.at(0).relevance, 0.54930614433405489);
        ASSERT_EQUAL(founded.at(1).relevance, 0.54930614433405489);
        ASSERT_EQUAL(founded.at(2).relevance, 0.18310204811135161);

    }
    {
        SearchServer server;
        server.SetStopWords(" "s);

        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::BANNED, {8, -3});
        server.AddDocument(1, "белый кот и модный ошейник"s, DocumentStatus::BANNED, {7, 2, 7});
        server.AddDocument(2, "белый кот и модный ошейник"s, DocumentStatus::BANNED, {9});

        /*   vector<Document> searched = {{1, 0.866434, 5},
                                        {0, 0.173287, 2},
                                        {2, 0.173287, -1}};*/

        vector<Document> founded = server.FindTopDocuments("белый кот", DocumentStatus::BANNED);

        ASSERT_EQUAL(founded.at(0).relevance, 0.0);
        ASSERT_EQUAL(founded.at(1).relevance, 0.0);
        ASSERT_EQUAL(founded.at(2).relevance, 0.0);

    }

}

void TestSearch() {
    {
        SearchServer search_server;
        search_server.SetStopWords("и не"s);

        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, {9});

        vector<Document> founded = search_server.FindTopDocuments("");

        ASSERT(founded.empty());
    }
    {
        SearchServer search_server;
        search_server.SetStopWords("и не"s);

        vector<Document> founded = search_server.FindTopDocuments("пушистый ухоженный кот");

        ASSERT(founded.empty());
    }

    {
        SearchServer search_server;
        search_server.SetStopWords("  "s);

        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        search_server.AddDocument(1, "-пушистый кот -пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(2, "____ -"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        search_server.AddDocument(3, "    "s, DocumentStatus::ACTUAL, {9});

        vector<Document> founded = search_server.FindTopDocuments("    ");

        ASSERT(founded.empty());
    }

}

void TestDocumentCount() {
    {
        SearchServer search_server;
        search_server.SetStopWords("и не"s);

        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, {9});

        ASSERT_EQUAL(search_server.GetDocumentCount(), 4);
    }

    {
        SearchServer search_server;
        search_server.SetStopWords("белый кот и модный ошейник"s);

        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        search_server.AddDocument(1, ""s, DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(2, ""s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        search_server.AddDocument(3, ""s, DocumentStatus::ACTUAL, {9});

        ASSERT_EQUAL(search_server.GetDocumentCount(), 4);
    }

}

void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatchedDocuments);
    RUN_TEST(TestSort);
    RUN_TEST(TestRating);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestStatus);
    RUN_TEST(TestIDF_TF);
    RUN_TEST(TestSearch);
    RUN_TEST(TestDocumentCount);
}

int main() {
    TestSearchServer();

    SearchServer search_server;
    search_server.SetStopWords("и не"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, {9});

    cout << "ACTUAL by default:"s << endl;
    for (const Document &document: search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document &document: search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document &document: search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                                                  [](int document_id, DocumentStatus status,
                                                                     int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    vector<int> hhh = {4, 8, 9};
    float g = -2.0;
    if( hhh.size() > g) {

    }


    return 0;
}