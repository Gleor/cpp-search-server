#include "search_server.h"
#include <stdexcept>
#include <utility>
#include <numeric>
#include <algorithm>

using namespace std::string_literals;

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
{
}
SearchServer::SearchServer(const std::string_view stop_words_text_view)
    : SearchServer(SplitIntoWords(stop_words_text_view))
{
}

void SearchServer::AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    using namespace std::string_literals;
    if (document_id < 0) {
        throw std::invalid_argument("Document id must be non-negative"s);
    }
    if (documents_.count(document_id)) {
        throw std::invalid_argument("Document with this id already exists"s);
    }
    if (!IsValidWord(document)) {
        throw std::invalid_argument("Document must not contain special characters"s);
    }

    storage.emplace_back(document);

    const auto words = SplitIntoWordsNoStop(storage.back());

    const double inv_word_count = 1.0 / words.size();
    for (const std::string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        id_to_word_freqs[document_id][word] += inv_word_count;
    }
    document_index_.insert(document_id);
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(std::execution::seq,
        raw_query, [status](int document_id, DocumentStatus document_status, int rating)
        {
            return document_status == status;
        });
}


std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const
{
    return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const
{
    static std::map<std::string_view, double> word_freqs;
    if (!id_to_word_freqs.count(document_id)) {
        return word_freqs;
    }
    return id_to_word_freqs.at(document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
    if (document_index_.count(document_id) == 0) {
        throw std::out_of_range("Not valid document id"s);
    }
    const auto query = ParseQuery(raw_query);

    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return { {}, documents_.at(document_id).status };
        }
    }
    std::vector<std::string_view> matched_words;
    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return { matched_words, documents_.at(document_id).status };
}
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::sequenced_policy&, const std::string_view raw_query, int document_id) const {
    return SearchServer::MatchDocument(raw_query, document_id);
}
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy&, const std::string_view raw_query, int document_id) const {
    if (document_index_.count(document_id) == 0) {
        throw std::out_of_range("Not valid document id"s);
    }
    auto query = ParseQuery(std::execution::par, raw_query);

    if (std::any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [&](const std::string_view word) {
        return (word_to_document_freqs_.count(word) != 0 && word_to_document_freqs_.at(word).count(document_id)); }))
    {
        return { {}, documents_.at(document_id).status };
    }

    std::vector<std::string_view> matched_words(query.plus_words.size());

    auto iter = std::copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), [&](const std::string_view word) {
        return (word_to_document_freqs_.count(word) != 0 && word_to_document_freqs_.at(word).count(document_id));
        });

    matched_words.erase(iter, matched_words.end());

    std::sort(matched_words.begin(), matched_words.end());
    auto it_del = std::unique(matched_words.begin(), matched_words.end());
    matched_words.erase(it_del, matched_words.end());

    return { matched_words, documents_.at(document_id).status };
}
void SearchServer::RemoveDocument(int document_id)
{
    if (!id_to_word_freqs.count(document_id)) { return; }
    auto& word_freq = id_to_word_freqs.at(document_id);
    for (auto iter = word_freq.begin(); iter != word_freq.end(); iter++) {
        auto it_del = word_to_document_freqs_.find(iter->first);
        (it_del->second.size() == 1) ? (void)word_to_document_freqs_.erase(it_del) : (void)it_del->second.erase(document_id);
    }
    id_to_word_freqs.erase(document_id);
    documents_.erase(document_id);
    document_index_.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id)
{
    RemoveDocument(document_id);
}
void SearchServer::RemoveDocument(const std::execution::parallel_policy&, int document_id) {
    if (!id_to_word_freqs.count(document_id)) { return; }
    auto& word_freq = id_to_word_freqs.at(document_id);
    std::vector <std::string_view> v(word_freq.size());
    std::transform(std::execution::par,
        word_freq.begin(),
        word_freq.end(),
        v.begin(),
        [&](const auto& word) {
            return word.first;
        });
    std::mutex mutex_;
    std::for_each(std::execution::par, v.begin(), v.end(), [&](const auto word) {
        const std::lock_guard<std::mutex> lock(mutex_);
        word_to_document_freqs_.at(word).erase(document_id);
        });
    id_to_word_freqs.erase(document_id);
    documents_.erase(document_id);
    document_index_.erase(document_id);
}
bool SearchServer::IsStopWord(const std::string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string_view word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view text) const {
    std::vector<std::string_view> words;
    for (const std::string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word "s + word.data() + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word "s + text.data() + " is invalid");
    }

    return { word, is_minus, IsStopWord(word) };
}
SearchServer::Query SearchServer::ParseQuery(const std::string_view text) const {
    SearchServer::Query result;
    for (const std::string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    std::sort(result.minus_words.begin(), result.minus_words.end());
    auto it_del = std::unique(result.minus_words.begin(), result.minus_words.end());
    result.minus_words.erase(it_del, result.minus_words.end());

    std::sort(result.plus_words.begin(), result.plus_words.end());
    it_del = std::unique(result.plus_words.begin(), result.plus_words.end());
    result.plus_words.erase(it_del, result.plus_words.end());

    return result;
}
SearchServer::Query SearchServer::ParseQuery(const std::execution::sequenced_policy&, const std::string_view text) const {
    return ParseQuery(text);
}
SearchServer::Query SearchServer::ParseQuery(const std::execution::parallel_policy&, const std::string_view text) const {
    SearchServer::Query result;
    for (const std::string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    return result;
}
double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
