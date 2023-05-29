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

// -------- ������ ��������� ������ ��������� ������� ----------

// ���� ���������, ��� ��������� ������� ��������� ����-����� ��� ���������� ����������
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
        //����� �� ���� ����� �� ������ ������ �����������
        ASSERT_EQUAL(found_docs.size(), 0);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
    // �������� ������ �������������� ����� � ������ ������
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
    // ���������, ���������� ����� ����� �� ���������� � ��������� ������

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
    // ������� ����������
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
    // ���������� �� �������������

    {
        SearchServer server("� � ��"s);
        //server.SetStopWords("� � ��"s);
        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        const auto documents = server.FindTopDocuments("�������� ��������� ���"s);
        //�������� �� ��, ��� ������������ �� ������ ������
        ASSERT_EQUAL_HINT(documents.size(), 3, "Non empty container should be returned"s);
        //��������� ���������� ���������� �� ������������� ���� ��������� ����� relevance
        ASSERT(documents[0].relevance > documents[1].relevance);
        ASSERT(documents[1].relevance > documents[2].relevance);
    }
}

void TestRatingCalculation() {
    // ������� ��������

    {
        SearchServer server("� � ��"s);
        //server.SetStopWords("� � ��"s);
        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        const auto documents = server.FindTopDocuments("�������� ��������� ���"s);
        //�������� �� ��, ��� ������������ �� ������ ������
        ASSERT_EQUAL_HINT(documents.size(), 3, "Non empty container should be returned"s);
        //��������� ��������� ������ ��������
        //���������� ��� �������� ������� � ����� �� ���������� ��������� (������ �������)
        ASSERT_EQUAL(documents[0].rating, ((7 + 2 + 7) / 3));
        ASSERT_EQUAL(documents[1].rating, ((5 + (-12) + 2 + 1) / 4));
        ASSERT_EQUAL(documents[2].rating, ((8 + (-3)) / 2));
    }
}

void TestPredicate() {
    // ���� ���������

    {
        SearchServer server("� � ��"s);
        //server.SetStopWords("� � ��"s);
        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::BANNED, { 9 });
        const auto documents = server.FindTopDocuments("�������� ��������� ���"s);
        //�������� �� ��, ��� ������������ �� ������ ������
        ASSERT_EQUAL_HINT(documents.size(), 3, "Non empty container should be returned"s);
        ASSERT_EQUAL(documents[0].id, 1);
        ASSERT_EQUAL(documents[1].id, 0);
        ASSERT_EQUAL(documents[2].id, 2);
        const auto documents_1 = server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::BANNED);
        //�������� �� ��, ��� ������������ �� ������ ������
        ASSERT_EQUAL_HINT(documents_1.size(), 1, "Non empty container should be returned"s);
        ASSERT_EQUAL(documents_1[0].id, 3);
        const auto documents_2 = server.FindTopDocuments("�������� ��������� ���"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        //�������� �� ��, ��� ������������ �� ������ ������
        ASSERT_EQUAL_HINT(documents_2.size(), 2, "Non empty container should be returned"s);
        ASSERT_EQUAL(documents_2[0].id, 0);
        ASSERT_EQUAL(documents_2[1].id, 2);
    }
}
void TestStatusSearch() {
    // ���� ������ �� �������

    {
        SearchServer server("� � ��"s);
        //server.SetStopWords("� � ��"s);
        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "����� ��� � ������ �������"s, DocumentStatus::IRRELEVANT, { 7, 2, 7 });
        server.AddDocument(2, "����� ��� � ������ �������"s, DocumentStatus::BANNED, { 5, -12, 2, 1 });
        server.AddDocument(3, "����� ��� � ������ �������"s, DocumentStatus::REMOVED, { 9 });
        const auto documents = server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::ACTUAL);
        //�������� �� ��, ��� ������������ �� ������ ������
        ASSERT_EQUAL_HINT(documents.size(), 1, "Non empty container should be returned"s);
        ASSERT_EQUAL(documents[0].id, 0);
        const auto documents_1 = server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::IRRELEVANT);
        //�������� �� ��, ��� ������������ �� ������ ������
        ASSERT_EQUAL_HINT(documents_1.size(), 1, "Non empty container should be returned"s);
        ASSERT_EQUAL(documents_1[0].id, 1);
        const auto documents_2 = server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::BANNED);
        //�������� �� ��, ��� ������������ �� ������ ������
        ASSERT_EQUAL_HINT(documents_2.size(), 1, "Non empty container should be returned"s);
        ASSERT_EQUAL(documents_2[0].id, 2);
        const auto documents_3 = server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::REMOVED);
        //�������� �� ��, ��� ������������ �� ������ ������
        ASSERT_EQUAL_HINT(documents_3.size(), 1, "Non empty container should be returned"s);
        ASSERT_EQUAL(documents_3[0].id, 3);
    }
}
//���������� ��� ��������: ��� ����� ������������ ���������� ���������� THRESHOLD, �� �������� �������� �� ���
void TestRelevanceCalculation() {
    // ������� �������������

    {
        SearchServer server("� � ��"s);
        //server.SetStopWords("� � ��"s);
        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        const auto documents = server.FindTopDocuments("�������� ��������� ���"s);
        //�������� �� ��, ��� ������������ �� ������ ������
        ASSERT_EQUAL_HINT(documents.size(), 3, "Non empty container should be returned"s);
        //������� ��� ���������� IDF: log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
        // ���������� ���������� - 3
        // TF ����� �������� �� ������ ��������� ���������� 2/4=0.5
        // TF ����� ��������� � ������� ��������� ���������� 1/4=0.25
        // TF ����� ��� � ������ � ������ ���������� ���������� 0.25 � 0.25
        // � �������� ���������� ����������� ������ ����� �� ������:
        // �������� - 1; ��������� - 1; ��� - 2
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
    // 1439 �������� � ������� �����������
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }
    // ����� 1440 ��������: 1439 ������ �������� � 1 ��������
    request_queue.AddFindRequest("curly dog"s);
    // ����� �����, ������ ������ ������, 1438 �������� � ������� ����������� � 2 ��������
    request_queue.AddFindRequest("big collar"s);
    // ������ ������ ������, 1437 �������� � ������� ����������� � 3 ��������
    request_queue.AddFindRequest("sparrow"s);
    //���������, ��� ������ �������� ������������� 1437
    ASSERT_EQUAL(request_queue.GetNoResultRequests(), 1437);
    //������� 1444 �������� ��������
    for (int i = 0; i < 1444; ++i) {
        request_queue.AddFindRequest("dog"s);
    }
    //������ �������� ������ ����� 0
    ASSERT_EQUAL(request_queue.GetNoResultRequests(), 0);
}*/
/*void TestRemoveDuplicates() {
    SearchServer search_server("and with"s);

    search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // �������� ��������� 2, ����� �����
    search_server.AddDocument(3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // ������� ������ � ����-������, ������� ����������
    search_server.AddDocument(4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // ��������� ���� ����� ��, ������� ���������� ��������� 1
    search_server.AddDocument(5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // ���������� ����� �����, ���������� �� ��������
    search_server.AddDocument(6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // ��������� ���� ����� ��, ��� � id 6, �������� �� ������ �������, ������� ����������
    search_server.AddDocument(7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, { 1, 2 });

    // ���� �� ��� �����, �� �������� ����������
    search_server.AddDocument(8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // ����� �� ������ ����������, �� �������� ����������
    search_server.AddDocument(9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    //��������� ������� ���������� ���������� �� �������
    ASSERT_EQUAL(search_server.GetDocumentCount(), 9);

    //������� ���������
    RemoveDuplicates(search_server);

    //��������� ������� ���������� ���������� ����� �������
    ASSERT_EQUAL(search_server.GetDocumentCount(), 5);

    //��������, ��� ��������� � �������� ���������� ���������
    auto documents = search_server.FindTopDocuments("nasty rat with curly hair");
    ASSERT_EQUAL(documents.size(), 5);
    ASSERT_EQUAL(documents[0].id, 9);
    ASSERT_EQUAL(documents[1].id, 2);
    ASSERT_EQUAL(documents[2].id, 1);
    ASSERT_EQUAL(documents[3].id, 8);
    ASSERT_EQUAL(documents[4].id, 6);
}*/
// ������� TestSearchServer �������� ������ ����� ��� ������� ������
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

// --------- ��������� ��������� ������ ��������� ������� -----------