#include "remove_duplicates.h"
#include <set>
#include <iterator>

void RemoveDuplicates(SearchServer& search_server)
{
    std::set<std::set<std::string_view>> dupl_set;
    std::vector<int> docs_to_del;
    for (const int document_id : search_server) {
        auto word_freq = search_server.GetWordFrequencies(document_id);
        std::set<std::string_view> doc_words;
        std::transform(word_freq.begin(), word_freq.end(),
            std::inserter(doc_words, doc_words.end()),
            [](const auto& pair) { return pair.first; });
        if (!dupl_set.insert(doc_words).second) {
            docs_to_del.push_back(document_id);
        }
    }
    for (const auto id : docs_to_del) {
        search_server.RemoveDocument(id);
        std::cout << "Found duplicate document id " << id << std::endl;
    }
}
