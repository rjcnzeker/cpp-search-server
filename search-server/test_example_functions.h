#pragma once

#include "document.h"
#include "search_server.h"

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
                const string &hint);

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

void TestExcludeStopWordsFromAddedDocumentContent();

void TestMinusWords();

void TestMatchedDocuments();

void TestSort();

void TestRating();

void TestPredicate();

void TestStatus();

void TestIDF_TF();

void TestSearch();

void TestDocumentCount();

void TestSearchServer();