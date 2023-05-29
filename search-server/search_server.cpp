#include "search_server.h"
#include <stdexcept>
#include <utility>
#include <numeric>

using namespace std::string_literals;

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor
        // from string container
{
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
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
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const std::string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        id_to_word_freqs[document_id][word] += inv_word_count;
    }
    document_index_.push_back(document_id);
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus document_status) const {
    return FindTopDocuments(raw_query, [document_status](int document_id, DocumentStatus status, int rating) {
        return status == document_status; });
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const
{
    static std::map<std::string, double> word_freqs;
    if (!id_to_word_freqs.count(document_id)) {
        return word_freqs;
    }
    return id_to_word_freqs.at(document_id);
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    if (document_id < 0) {
        throw std::invalid_argument("Document id must be non-negative"s);
    }
    const auto query = ParseQuery(raw_query);

    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words) {
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

void SearchServer::RemoveDocument(int document_id) {
    /*auto it = word_to_document_freqs_.begin();
    while (it != word_to_document_freqs_.end()) {
        if (it->second.count(document_id)) {
            it->second.erase(document_id);
        }
        else {
            word_to_document_freqs_.erase(it);
        }
    }*/

    auto& word_freq = id_to_word_freqs.at(document_id);
    for (auto iter = word_freq.begin(); iter != word_freq.end(); iter++) {
        auto it_del = word_to_document_freqs_.find(iter->first);
        (it_del->second.size() == 1) ? (void)word_to_document_freqs_.erase(it_del) : (void)it_del->second.erase(document_id);
    }
    id_to_word_freqs.erase(id_to_word_freqs.find(document_id));
    documents_.erase(documents_.find(document_id));
    document_index_.erase(find(document_index_.begin(), document_index_.end(), document_id));
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string& word) {    
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

    std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
        std::vector<std::string> words;
        for (const std::string& word : SplitIntoWords(text)) {
            if (!IsValidWord(word)) {
                throw std::invalid_argument("Word "s + word + " is invalid"s);
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

    SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string& text) const {
        if (text.empty()) {
            throw std::invalid_argument("Query word is empty"s);
        }
        std::string word = text;
        bool is_minus = false;
        if (word[0] == '-') {
            is_minus = true;
            word = word.substr(1);
        }
        if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
            throw std::invalid_argument("Query word "s + text + " is invalid");
        }

        return { word, is_minus, IsStopWord(word) };
    }

    SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
        SearchServer::Query result;
        for (const std::string& word : SplitIntoWords(text)) {
            const auto query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    result.minus_words.insert(query_word.data);
                }
                else {
                    result.plus_words.insert(query_word.data);
                }
            }
        }
        return result;
    }

    double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }
