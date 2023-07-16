#pragma once
#include "string_processing.h"
#include "document.h"
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <utility>
#include <cmath>
#include <stdexcept>
#include <execution>
#include <set>
#include <string_view>
#include <deque>
#include <type_traits>
#include <future>
#include "concurrent_map.h"

#define BORDER 1e-6

const int MAX_RESULT_DOCUMENT_COUNT = 5;

const size_t BUCKETS_N = 8;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string_view stop_words_text);
    explicit SearchServer(const std::string& stop_words_text);

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view,
        DocumentPredicate) const;
    template <class ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&&,
        std::string_view,
        DocumentPredicate) const;

    std::vector<Document> FindTopDocuments(std::string_view, DocumentStatus) const;

    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&&, std::string_view, DocumentStatus) const;

    std::vector<Document> FindTopDocuments(std::string_view) const;

    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&&, std::string_view) const;

    int GetDocumentCount() const;

    auto begin() {
        return document_index_.begin();
    }

    auto end() {
        return document_index_.end();
    }

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy&, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&, const std::string_view raw_query, int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy& exec, int document_id);
    void RemoveDocument(const std::execution::parallel_policy&, int document_id);
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> id_to_word_freqs;
    std::map<int, DocumentData> documents_;
    std::set<int> document_index_;
    std::deque<std::string> storage;

    bool IsStopWord(const std::string_view word) const;

    static bool IsValidWord(const std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view text) const;
    Query ParseQuery(const std::execution::sequenced_policy&, const std::string_view text) const;
    Query ParseQuery(const std::execution::parallel_policy&, const std::string_view text) const;

    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query&,
        DocumentPredicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::sequenced_policy,
        const Query&,
        DocumentPredicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy,
        const Query&,
        DocumentPredicate) const;
};

template <typename ExecutionPolicy, typename ForwardRange, typename Function>
void ForEach(const ExecutionPolicy& policy, ForwardRange& range, Function function)
{
    if constexpr (
        std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>
        || std::is_same_v<typename std::iterator_traits<typename ForwardRange::iterator>::iterator_category,
        std::random_access_iterator_tag>
        )
    {
        std::for_each(policy, range.begin(), range.end(), function);
    }
    else
    {
        static constexpr int PART_COUNT = 16;
        const auto part_length = size(range) / PART_COUNT;
        auto part_begin = range.begin();
        auto part_end = next(part_begin, part_length);

        std::vector<std::future<void>> futures;
        for (int i = 0;
            i < PART_COUNT;
            ++i,
            part_begin = part_end,
            part_end = (i == PART_COUNT - 1
                ? range.end()
                : next(part_begin, part_length))
            )
        {
            futures.push_back(async([function, part_begin, part_end]
                {
                    for_each(part_begin, part_end, function);
                }));
        }
    }
}

template <typename ForwardRange, typename Function>
void ForEach(ForwardRange& range, Function function)
{
    ForEach(std::execution::seq, range, function);
}

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    using namespace std::string_literals;
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
    DocumentPredicate document_predicate) const
{
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <class ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy,
    const std::string_view raw_query,
    DocumentPredicate document_predicate) const
{
    const auto query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    std::sort(policy, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < BORDER) {
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

template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(policy, raw_query,
        [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const
{
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template<typename DocumentPredicate>
inline std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const
{
    return SearchServer::FindAllDocuments(std::execution::seq, query, document_predicate);
}

template<typename DocumentPredicate>
inline std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy,
    const Query& query, DocumentPredicate document_predicate) const
{
    std::map<int, double> document_to_relevance;

    std::vector<Document> matched_documents;

    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    for (const auto& [document_id, relevance] : document_to_relevance) {
        matched_documents.emplace_back(
            Document(document_id, relevance, documents_.at(document_id).rating)
        );
    }
    return matched_documents;
}

template<typename DocumentPredicate>
inline std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy policy,
    const Query& query, DocumentPredicate document_predicate) const
{
    ConcurrentMap<int, double> document_to_relevance(BUCKETS_N);

    std::vector<Document> matched_documents;
    ForEach(policy, query.plus_words, [&](std::string_view word)
        {
            if (!word_to_document_freqs_.count(word) == 0)
            {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word))
                {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating))
                    {
                        document_to_relevance[document_id] += term_freq * inverse_document_freq;
                    }
                }
            }
        }
    );

    ForEach(policy, query.minus_words, [&](std::string_view word)
        {
            if (!word_to_document_freqs_.count(word) == 0)
            {
                for (const auto [document_id, _] : word_to_document_freqs_.at(word))
                {
                    document_to_relevance.Erase(document_id);
                }
            }
        }
    );

    for (const auto& [document_id, relevance] : document_to_relevance.BuildOrdinaryMap())
    {
        matched_documents.emplace_back(
            Document(document_id, relevance, documents_.at(document_id).rating)
        );
    }
    return matched_documents;
}
