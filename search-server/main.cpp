#include "search_server.h"
#include "request_queue.h"
#include "paginator.h"
#include "log_duration.h"
#include "remove_duplicates.h"
#include "tests.h"

int main() {
    using namespace std::string_literals;
    TestSearchServer();
    std::cout << "All tests passed successfully"s << std::endl;
    /*SearchServer server("и в на"s);
    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    {
        LOG_DURATION_STREAM("FindTopDocuments"s, std::cout);
        const auto documents = server.FindTopDocuments("пушистый ухоженный кот"s);
    }
    {
        LOG_DURATION_STREAM("MatchDocument"s, std::cout);
        const auto [words, doc_status] = server.MatchDocument("кот"s, 0);
    }*/
    return 0;
}
