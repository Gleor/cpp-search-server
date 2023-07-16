#include "process_queries.h"
#include <vector>
#include <string>
#include <execution>
#include <list>
#include <iterator>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> documents_lists(queries.size());
    std::transform(
        std::execution::par,
        queries.cbegin(),
        queries.cend(),
        documents_lists.begin(),
        [&search_server](const std::string& s)
        { return search_server.FindTopDocuments(s); });
    return documents_lists;
}

std::list<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> documents_lists(queries.size());

    std::transform(std::execution::par,
        queries.cbegin(),
        queries.cend(),
        documents_lists.begin(),
        [&search_server](const std::string& s) {
            return search_server.FindTopDocuments(s);
        });

    std::list<Document> documents;
    for (const auto& documents_list : documents_lists) {
        documents.splice(documents.end(), std::list(documents_list.begin(), documents_list.end()));
    }
    return documents;
}