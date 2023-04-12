/*
#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <iostream>
//#include "search_server.h"

using namespace std;

const double THRESHOLD = 1e-6;
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

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            words.push_back(word);
            word = "";
        }
        else {
            word += c;
        }
    }
    words.push_back(word);

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < THRESHOLD) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    //Примечание для ревьюера: лишние опрерации присваивания в лямбде нужны были для успешной компиляции в тренажере

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus document_status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [document_status](int document_id, DocumentStatus status, int rating) {
            document_id = document_id;
            rating = rating;
            return status == document_status; });
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
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
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
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

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
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
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
                });
        }
        return matched_documents;
    }
};

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

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "Assert("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(const T& func, const string& f_str) {
    func();
    cerr << f_str << " SUCCESS" << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)
*/

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const int doc_id_1 = 6;
    const string content = "cat in the city"s;
    const string content_1 = "dog on the roof"s;
    const vector<int> ratings = { 1, 2, 3 };
    const vector<int> ratings_1 = { 3, 2, 4 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
    // Проверка поиска отсутствующего слова и пустой строки
    {
        SearchServer server;
        server.SetStopWords("in the on"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        ASSERT_HINT(server.FindTopDocuments("parrot"s).empty(),
            "Nothing should be returned when searching for the missing word"s);
        ASSERT_HINT(server.FindTopDocuments(""s).empty(),
            "Nothing should be returned when searching empty query"s);
    }
}

void TestMinusWords() {
    // Документы, содержащие минус слова не включаются в результат поиска

    {
        SearchServer server;
        server.SetStopWords("in the on"s);
        server.AddDocument(0, "black cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(1, "black dog on the roof"s, DocumentStatus::ACTUAL, { 3, 2, 4 });
        ASSERT_HINT(server.FindTopDocuments("-black"s).empty(),
            "Searching for minus word only"s);
        const auto found_docs = server.FindTopDocuments("black -cat"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 1u);
    }
}

void TestMatchDocuments() {
    // Матчинг документов

    {
        SearchServer server;
        server.SetStopWords("in the on"s);
        server.AddDocument(0, "black cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(1, "black dog on the roof"s, DocumentStatus::ACTUAL, { 3, 2, 4 });
        server.AddDocument(2, "green frog in the big garden"s, DocumentStatus::ACTUAL, { 5, -1, 1 });
        const auto [words, doc_status] = server.MatchDocument("black dog", 0);
        set<string> words_set(words.begin(), words.end());
        ASSERT_EQUAL(words_set.count("black"s), 1);
        const auto [words_1, doc_status_1] = server.MatchDocument("big cat -frog", 2);
        ASSERT_EQUAL(words_1.size(), 0);
    }
}

void TestRelevanceSort() {
    // Сортировка по релевантности

    {
        SearchServer server;
        server.SetStopWords("и в на"s);
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        const auto documents = server.FindTopDocuments("пушистый ухоженный кот"s);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_HINT(!documents.empty(), "Non empty container should be returned"s);
        //Преверяем правильную сортировку по релевантности путём сравнения полей relevance
        ASSERT(documents[0].relevance > documents[1].relevance);
        ASSERT(documents[1].relevance > documents[2].relevance);
    }
}

void TestRatingCalculation() {
    // Подсчёт рейтинга

    {
        SearchServer server;
        server.SetStopWords("и в на"s);
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        const auto documents = server.FindTopDocuments("пушистый ухоженный кот"s);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_HINT(!documents.empty(), "Non empty container should be returned"s);
        //Проверяем правльный расчёт рейтинга
        //Складываем все значения вектора и делим на количество элементов (размер вектора)
        ASSERT_EQUAL(documents[0].rating, ((7 + 2 + 7) / 3));
        ASSERT_EQUAL(documents[1].rating, ((5 + (-12) + 2 + 1) / 4));
        ASSERT_EQUAL(documents[2].rating, ((8 + (-3)) / 2));
    }
}

void TestPredicate() {
    // Тест предиката

    {
        SearchServer server;
        server.SetStopWords("и в на"s);
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
        const auto documents = server.FindTopDocuments("пушистый ухоженный кот"s);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_HINT(!documents.empty(), "Non empty container should be returned"s);
        ASSERT_EQUAL(documents[0].id, 1);
        ASSERT_EQUAL(documents[1].id, 0);
        ASSERT_EQUAL(documents[2].id, 2);
        const auto documents_1 = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_HINT(!documents_1.empty(), "Non empty container should be returned"s);
        ASSERT_EQUAL(documents_1[0].id, 3);
        const auto documents_2 = server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_HINT(!documents_2.empty(), "Non empty container should be returned"s);
        ASSERT_EQUAL(documents_2[0].id, 0);
        ASSERT_EQUAL(documents_2[1].id, 2);
    }
}
void TestStatusSearch() {
    // Тест поиска по статусу

    {
        SearchServer server;
        server.SetStopWords("и в на"s);
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "белый кот и модный ошейник"s, DocumentStatus::IRRELEVANT, { 7, 2, 7 });
        server.AddDocument(2, "белый кот и модный ошейник"s, DocumentStatus::BANNED, { 5, -12, 2, 1 });
        server.AddDocument(3, "белый кот и модный ошейник"s, DocumentStatus::REMOVED, { 9 });
        const auto documents = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::ACTUAL);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_HINT(!documents.empty(), "Non empty container should be returned"s);
        ASSERT_EQUAL(documents[0].id, 0);
        const auto documents_1 = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::IRRELEVANT);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_HINT(!documents_1.empty(), "Non empty container should be returned"s);
        ASSERT_EQUAL(documents_1[0].id, 1);
        const auto documents_2 = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_HINT(!documents_2.empty(), "Non empty container should be returned"s);
        ASSERT_EQUAL(documents_2[0].id, 2);
        const auto documents_3 = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::REMOVED);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_HINT(!documents_3.empty(), "Non empty container should be returned"s);
        ASSERT_EQUAL(documents_3[0].id, 3);
    }
}
//Примечание для ревьюера: тут можно использовать глобальную переменную THRESHOLD, но тренажер ругается на это
void TestRelevanceCalculation() {
    // Подсчёт релевантности

    {
        SearchServer server;
        server.SetStopWords("и в на"s);
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        const auto documents = server.FindTopDocuments("пушистый ухоженный кот"s);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_HINT(!documents.empty(), "Non empty container should be returned"s);
        //Формула для вычисления IDF: log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
        // Количество документов - 3
        // TF слова пушистый во втором документе составляет 2/4=0.5
        // TF слова ухоженный в третьем документе составляет 1/4=0.25
        // TF слова кот в первом и втором документах составляет 0.25 и 0.25
        // В скольких документах встречается каждое слово из поиска:
        // Пушистый - 1; Ухоженный - 1; Кот - 2
        ASSERT(abs(documents[0].relevance - (0.5 * log(3 * 1.0 / 1) + 0.25 * log(3 * 1.0 / 2)) < 1e-6));
        ASSERT(abs(documents[1].relevance - (0.25 * log(3 * 1.0 / 1))) < 1e-6);
        ASSERT(abs(documents[2].relevance - (0.25 * log(3 * 1.0 / 2))) < 1e-6);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatchDocuments);
    RUN_TEST(TestRelevanceSort);
    RUN_TEST(TestRatingCalculation);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestStatusSearch);
    RUN_TEST(TestRelevanceCalculation);

}

// --------- Окончание модульных тестов поисковой системы -----------

/*int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}*/