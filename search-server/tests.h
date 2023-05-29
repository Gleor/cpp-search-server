#pragma once
#include "search_server.h"
#include "document.h"
#include <string>

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))
#define RUN_TEST(func) RunTestImpl(func, #func)
using namespace std::string_literals;
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
    const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cerr << std::boolalpha;
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}
void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line, const std::string& hint);

template <typename T>
void RunTestImpl(const T& func, const std::string& f_str) {
    func();
    std::cerr << f_str << " SUCCESS" << std::endl;
}

void TestExcludeStopWordsFromAddedDocumentContent();
void TestMinusWords();
void TestMatchDocuments();
void TestRelevanceSort();
void TestRatingCalculation();
void TestPredicate();
void TestStatusSearch();
void TestRelevanceCalculation();
//void TestRequestQueue();
//void TestRemoveDuplicates();
void TestSearchServer();
