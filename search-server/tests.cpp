#include "tests.h"
#include "search_server.h"
#include "document.h"
#include "request_queue.h"
#include "remove_duplicates.h"
#include <set>

using namespace std::string_literals;

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
    const std::string& hint) {
    if (!value) {
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "Assert("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {

    const int doc_id = 42;
    const int doc_id_1 = 6;
    const std::string content = "cat in the city"s;
    const std::string content_1 = "dog on the roof"s;
    const std::vector<int> ratings = { 1, 2, 3 };
    const std::vector<int> ratings_1 = { 3, 2, 4 };
    {
        SearchServer server("and in at"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        //Поиск по стоп слову не должен давать результатов
        ASSERT_EQUAL(found_docs.size(), 0);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
    // Проверка поиска отсутствующего слова и пустой строки
    {
        SearchServer server("in the on"s);
        //server.SetStopWords("in the on"s);
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
        SearchServer server("in the on"s);
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
        SearchServer server("in the on"s);
        //server.SetStopWords("in the on"s);
        server.AddDocument(0, "black cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(1, "black dog on the roof"s, DocumentStatus::ACTUAL, { 3, 2, 4 });
        server.AddDocument(2, "green frog in the big garden"s, DocumentStatus::ACTUAL, { 5, -1, 1 });
        const auto [words, doc_status] = server.MatchDocument("black dog", 0);
        std::set<std::string> words_set(words.begin(), words.end());
        ASSERT_EQUAL(words_set.count("black"s), 1);
        const auto [words_1, doc_status_1] = server.MatchDocument("big cat -frog", 2);
        ASSERT_EQUAL(words_1.size(), 0);
    }
}

void TestRelevanceSort() {
    // Сортировка по релевантности

    {
        SearchServer server("и в на"s);
        //server.SetStopWords("и в на"s);
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        const auto documents = server.FindTopDocuments("пушистый ухоженный кот"s);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_EQUAL_HINT(documents.size(), 3, "Non empty container should be returned"s);
        //Преверяем правильную сортировку по релевантности путём сравнения полей relevance
        ASSERT(documents[0].relevance > documents[1].relevance);
        ASSERT(documents[1].relevance > documents[2].relevance);
    }
}

void TestRatingCalculation() {
    // Подсчёт рейтинга

    {
        SearchServer server("и в на"s);
        //server.SetStopWords("и в на"s);
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        const auto documents = server.FindTopDocuments("пушистый ухоженный кот"s);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_EQUAL_HINT(documents.size(), 3, "Non empty container should be returned"s);
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
        SearchServer server("и в на"s);
        //server.SetStopWords("и в на"s);
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
        const auto documents = server.FindTopDocuments("пушистый ухоженный кот"s);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_EQUAL_HINT(documents.size(), 3, "Non empty container should be returned"s);
        ASSERT_EQUAL(documents[0].id, 1);
        ASSERT_EQUAL(documents[1].id, 0);
        ASSERT_EQUAL(documents[2].id, 2);
        const auto documents_1 = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_EQUAL_HINT(documents_1.size(), 1, "Non empty container should be returned"s);
        ASSERT_EQUAL(documents_1[0].id, 3);
        const auto documents_2 = server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_EQUAL_HINT(documents_2.size(), 2, "Non empty container should be returned"s);
        ASSERT_EQUAL(documents_2[0].id, 0);
        ASSERT_EQUAL(documents_2[1].id, 2);
    }
}
void TestStatusSearch() {
    // Тест поиска по статусу

    {
        SearchServer server("и в на"s);
        //server.SetStopWords("и в на"s);
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "белый кот и модный ошейник"s, DocumentStatus::IRRELEVANT, { 7, 2, 7 });
        server.AddDocument(2, "белый кот и модный ошейник"s, DocumentStatus::BANNED, { 5, -12, 2, 1 });
        server.AddDocument(3, "белый кот и модный ошейник"s, DocumentStatus::REMOVED, { 9 });
        const auto documents = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::ACTUAL);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_EQUAL_HINT(documents.size(), 1, "Non empty container should be returned"s);
        ASSERT_EQUAL(documents[0].id, 0);
        const auto documents_1 = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::IRRELEVANT);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_EQUAL_HINT(documents_1.size(), 1, "Non empty container should be returned"s);
        ASSERT_EQUAL(documents_1[0].id, 1);
        const auto documents_2 = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_EQUAL_HINT(documents_2.size(), 1, "Non empty container should be returned"s);
        ASSERT_EQUAL(documents_2[0].id, 2);
        const auto documents_3 = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::REMOVED);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_EQUAL_HINT(documents_3.size(), 1, "Non empty container should be returned"s);
        ASSERT_EQUAL(documents_3[0].id, 3);
    }
}
//Примечание для ревьюера: тут можно использовать глобальную переменную THRESHOLD, но тренажер ругается на это
void TestRelevanceCalculation() {
    // Подсчёт релевантности

    {
        SearchServer server("и в на"s);
        //server.SetStopWords("и в на"s);
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        const auto documents = server.FindTopDocuments("пушистый ухоженный кот"s);
        //Проверка на то, что возвращается не пустой вектор
        ASSERT_EQUAL_HINT(documents.size(), 3, "Non empty container should be returned"s);
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
/*void TestRequestQueue() {
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);
    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, { 1, 2, 8 });
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, { 1, 1, 1 });
    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }
    // Стало 1440 запросов: 1439 пустых запросов и 1 непустой
    request_queue.AddFindRequest("curly dog"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом и 2 непустых
    request_queue.AddFindRequest("big collar"s);
    // первый запрос удален, 1437 запросов с нулевым результатом и 3 непустых
    request_queue.AddFindRequest("sparrow"s);
    //Проверяем, что пустых запросов действительно 1437
    ASSERT_EQUAL(request_queue.GetNoResultRequests(), 1437);
    //Добавим 1444 непустых запросов
    for (int i = 0; i < 1444; ++i) {
        request_queue.AddFindRequest("dog"s);
    }
    //Пустых запросов должно стать 0
    ASSERT_EQUAL(request_queue.GetNoResultRequests(), 0);
}*/
/*void TestRemoveDuplicates() {
    SearchServer search_server("and with"s);

    search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // дубликат документа 2, будет удалён
    search_server.AddDocument(3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // отличие только в стоп-словах, считаем дубликатом
    search_server.AddDocument(4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // множество слов такое же, считаем дубликатом документа 1
    search_server.AddDocument(5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // добавились новые слова, дубликатом не является
    search_server.AddDocument(6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
    search_server.AddDocument(7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, { 1, 2 });

    // есть не все слова, не является дубликатом
    search_server.AddDocument(8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // слова из разных документов, не является дубликатом
    search_server.AddDocument(9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    //Проверяем сколько документов содержится до очистки
    ASSERT_EQUAL(search_server.GetDocumentCount(), 9);

    //Удаляем дубликаты
    RemoveDuplicates(search_server);

    //Проверяем сколько документов содержится после очистки
    ASSERT_EQUAL(search_server.GetDocumentCount(), 5);

    //Проверим, что удалились и остались правильные дакументы
    auto documents = search_server.FindTopDocuments("nasty rat with curly hair");
    ASSERT_EQUAL(documents.size(), 5);
    ASSERT_EQUAL(documents[0].id, 9);
    ASSERT_EQUAL(documents[1].id, 2);
    ASSERT_EQUAL(documents[2].id, 1);
    ASSERT_EQUAL(documents[3].id, 8);
    ASSERT_EQUAL(documents[4].id, 6);
}*/
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
    //RUN_TEST(TestRequestQueue);
    //RUN_TEST(TestRemoveDuplicates);
}

// --------- Окончание модульных тестов поисковой системы -----------