#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <execution>
#include <mutex>
#include "string_processing.h"
#include "document.h"
#include "concurrent_map.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPS = 1e-6;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    
    explicit SearchServer(const std::string& stop_words_text = std::string())
        : SearchServer(SplitIntoWordsSV(stop_words_text))
    {
    }
    explicit SearchServer(const std::string_view stop_words_sv = std::string_view())
        : SearchServer(SplitIntoWordsSV(stop_words_sv))
    {
    }
    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const; 
    template<typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query, DocumentPredicate document_predicate) const;
    template<typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, const std::string_view raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, const std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(std::execution::sequenced_policy, int document_id);
    void RemoveDocument(std::execution::parallel_policy, int document_id);
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, const std::string_view raw_query, int document_id) const;
    
    int GetDocumentCount() const;
    int GetStopWordsCount() const;
    const std::map<std::string_view, double, std::less<>>& GetWordFrequencies(int document_id) const;

    std::set<int>::iterator begin();
    std::set<int>::iterator end();
    size_t size();
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    std::set<std::string, std::less<>> stop_words_;
    std::map<int, std::map<std::string_view, double, std::less<>>> id_to_document_freqs_SV_;
    std::map<std::string_view, std::map<int, double>, std::less<>> word_to_document_freqs_SV_;
    std::map<int, DocumentData> documents_;
    std::set<int> id_of_documents_;
    std::map<int, std::list<std::string>::iterator> it_of_documents_;
    std::list<std::string> doc_content_;


   struct QueryWordSV {
        std::string_view data;
        bool is_minus;
        bool is_stop;

        QueryWordSV()
            : data(std::string_view())
            , is_minus(false)
            , is_stop(false)
        {}
    }; 
     
    struct QuerySV {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;

        QuerySV()
            : plus_words({})
            , minus_words({})
        {}
    };
    
    bool IsStopWordSV(const std::string_view word) const;
    static bool IsValidWordSV(const std::string_view word);
   
    std::vector<std::string_view> SplitIntoWordsNoStopSV(const std::string_view text) const;
      
    QueryWordSV ParseQueryWordSV(std::string_view text) const;
    QuerySV ParseQuerySV(const std::string_view text) const;

    double ComputeWordInverseDocumentFreq(const std::string_view word) const;
    static int ComputeAverageRating(const std::vector<int>& rating_in);

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const QuerySV& query, DocumentPredicate document_predicate) const;
    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::sequenced_policy, const QuerySV& query, DocumentPredicate document_predicate) const;
    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy, const QuerySV& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    using namespace std::string_literals;
    for (const auto& word : stop_words_) {
        if (!IsValidWordSV(word)) {
            throw std::invalid_argument("Stop word \""s + std::string(word) + "\" contents special characters"s);
        }
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy, const QuerySV& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;

    for (const auto word : query.plus_words) {
        if (word_to_document_freqs_SV_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

        for (const auto [document_id, term_freq] : word_to_document_freqs_SV_.at(word)) {
            DocumentData temp_doc_data = documents_.at(document_id);
            if (document_predicate(document_id, temp_doc_data.status, temp_doc_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    for (const auto word : query.minus_words) {
        if (word_to_document_freqs_SV_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_SV_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({
            document_id,
            relevance,
            documents_.at(document_id).rating
            });
    }

    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const QuerySV& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy, const QuerySV& query, DocumentPredicate document_predicate) const
{
    ConcurrentMap<int, double> document_to_relevance(500);

    for_each(std::execution::par,
        query.plus_words.begin(), query.plus_words.end(),
        [&](auto& word)
        {
            if (word_to_document_freqs_SV_.count(word)) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                std::for_each(std::execution::par,
                    word_to_document_freqs_SV_.at(word).begin(), word_to_document_freqs_SV_.at(word).end(),
                    [&](const auto& id_freq)
                    {
                        const auto& document = documents_.at(id_freq.first);
                        if (document_predicate(id_freq.first, document.status, document.rating)) {
                            document_to_relevance[id_freq.first].ref_to_value += id_freq.second * inverse_document_freq;
                        }
                    });
            }
        });

    std::map<int, double> result(std::move(document_to_relevance.BuildOrdinaryMap()));

    for_each(std::execution::par,
        query.minus_words.begin(), query.minus_words.end(),
        [&result, this](auto& word)
        {
            if (word_to_document_freqs_SV_.count(word)) {
                for (auto [document_id, _] : word_to_document_freqs_SV_.at(word)) {
                    result.erase(document_id);
                }
            }
        });
  
    std::vector<Document> matched_documents(result.size());
    std::atomic_int length = 0;

    std::for_each(std::execution::par,
        result.begin(), result.end(),
        [&matched_documents, &length, this](const auto& id_relevance)
        {
            matched_documents[length++] = { id_relevance.first, id_relevance.second, documents_.at(id_relevance.first).rating };
        });

    return matched_documents;
}

template<typename DocumentPredicate>
 std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query, DocumentPredicate document_predicate) const
{
     const QuerySV query = ParseQuerySV(raw_query);

     auto matched_documents = FindAllDocuments(query, document_predicate);
     sort(matched_documents.begin(), matched_documents.end(),
         [](const Document& lhs, const Document& rhs) {
             if (std::abs(lhs.relevance - rhs.relevance) < EPS) {
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

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy, const std::string_view raw_query, DocumentPredicate document_predicate) const
{
    QuerySV query = ParseQuerySV(raw_query);

    auto matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);
   
    std::sort(std::execution::par,
        matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs)
        {
            if (std::abs(lhs.relevance - rhs.relevance) < EPS) {
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

template<typename DocumentPredicate>
 std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}