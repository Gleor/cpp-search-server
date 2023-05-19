#pragma once
#include "search_server.h"
#include "document.h"

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))
#define RUN_TEST(func) RunTestImpl(func, #func)

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
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
void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line, const string& hint);

template <typename T>
void RunTestImpl(const T& func, const string& f_str) {
    func();
    cerr << f_str << " SUCCESS" << endl;
}

void TestExcludeStopWordsFromAddedDocumentContent();
void TestMinusWords();
void TestMatchDocuments();
void TestRelevanceSort();
void TestRatingCalculation();
void TestPredicate();
void TestStatusSearch();
void TestRelevanceCalculation();
void TestRequestQueue();
