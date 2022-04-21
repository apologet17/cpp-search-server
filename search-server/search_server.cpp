#include <algorithm>
#include <numeric>
#include <functional>
#include <iostream>
#include <string_view>
#include "search_server.h"
#include "read_input_functions.h"
#include <chrono>

void SearchServer::AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
 
    using namespace std::string_literals;
 
    if (document_id < 0)
        throw std::invalid_argument("ID \""s + std::to_string(document_id) + "\" is negative"s);
 
    if (documents_.count(document_id) > 0)
        throw std::invalid_argument("ID \""s + std::to_string(document_id) + "\" is present in database"s);
 
    it_of_documents_[document_id] = doc_content_.emplace(doc_content_.end(), std::move(std::string(document)));

    const std::vector<std::string_view> words_in_doc = SplitIntoWordsNoStopSV(doc_content_.back());
 
    if (words_in_doc.size() != 0) {
        for (const auto word : words_in_doc) {
            if (!IsValidWordSV(word)) {
             //   doc_content_.pop_back();
             //   id_of_documents_.erase(document_id);
                throw std::invalid_argument("Document's word \""s + std::string(word) + "\" contents special characters"s);
            }
 
        }
 
        const double fract_freq = 1.0 / words_in_doc.size();
 
        for (const auto word : words_in_doc) {
            word_to_document_freqs_SV_[word][document_id] += fract_freq;
            //word_to_document_freqs_[std::string(word)][document_id] += fract_freq;
            //id_to_document_freqs_[document_id][std::string(word)] += fract_freq;
            id_to_document_freqs_SV_[document_id][word] += fract_freq;
        }
    }
 
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    id_of_documents_.insert(document_id);
}
 
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus d_status, int rating) { return d_status == status; });
}
 
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}
 
std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(std::execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy, const std::string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(std::execution::par, raw_query, [status]( int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}
 
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy, const std::string_view raw_query, int document_id) const {
    
    if (documents_.count(document_id) == 0) {
        throw std::out_of_range("Invalid document_id");
    }

    if (!IsValidWordSV(raw_query)) {
        throw std::invalid_argument("Invalid query");
    }

    const QuerySV query = ParseQuerySV(raw_query);
    std::vector<std::string_view> matched_words = {};

    for (const auto word : query.minus_words) {
        if (word_to_document_freqs_SV_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_SV_.at(word).count(document_id)) {
            return{ {}, documents_.at(document_id).status };
        }
    }

    for (const auto word : query.plus_words) {
        if (word_to_document_freqs_SV_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_SV_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    
    return { matched_words, documents_.at(document_id).status };
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy, const std::string_view raw_query, int document_id) const {
 
    if (documents_.count(document_id) == 0 ) {
		throw std::out_of_range("Invalid document_id");
	}
	if (!IsValidWordSV(raw_query)) {
		throw std::invalid_argument("Invalid query");
	}
 
    const auto query = ParseQuerySV(raw_query);
 	std::vector<std::string_view> matched_words(query.plus_words.size());

     auto ptr = &id_to_document_freqs_SV_.at(document_id);
     	if (std::any_of(std::execution::par,
                    query.minus_words.begin(),
                    query.minus_words.end(),
                    [ptr](auto word) { return ptr->count(word);})) {
		    return { {}, documents_.at(document_id).status };
	    }
 
	std::copy_if(std::execution::par, 
                    query.plus_words.begin(),
                    query.plus_words.end(),
                    matched_words.begin(),
		            [ptr](auto& word) {	return ptr->count(word);});
 
	std::sort(matched_words.begin(), matched_words.end());
	auto word_end = std::unique(matched_words.begin(), matched_words.end());
    matched_words.erase(word_end, matched_words.end());

	if (matched_words[0].empty()) {
		matched_words.erase(matched_words.begin());
	}

	return { matched_words, documents_.at(document_id).status };
 
}
 
int SearchServer::GetStopWordsCount() const {
    return stop_words_.size();
}
 
std::set<int>::iterator SearchServer::begin() {
    return id_of_documents_.begin();
}
 
std::set<int>::iterator SearchServer::end() {
    return id_of_documents_.end();
}
 
size_t SearchServer::size() {
    return id_of_documents_.size();
}
 
const std::map<std::string_view, double, std::less<>>& SearchServer::GetWordFrequencies(int document_id) const {
    static const std::map<std::string_view, double, std::less<>> document_freqs_if_id_absent_;
 
    if (id_to_document_freqs_SV_.count(document_id)) {
        return id_to_document_freqs_SV_.at(document_id) ;
    }
    else {
        return document_freqs_if_id_absent_;
    }
 
}
 
void SearchServer::RemoveDocument(int document_id) {
    if (id_to_document_freqs_SV_.count(document_id)) {
        for (auto word : id_to_document_freqs_SV_.at(document_id)) {
            word_to_document_freqs_SV_.at(word.first).erase(document_id);
        }
       id_to_document_freqs_SV_.erase(document_id);
 
       documents_.erase(document_id);
 
       it_of_documents_.erase(document_id);
       id_of_documents_.erase(document_id);
    }   
}
 
void SearchServer::RemoveDocument(std::execution::sequenced_policy, int document_id) {
     RemoveDocument(document_id);
}
 
void SearchServer::RemoveDocument(std::execution::parallel_policy, int document_id) {
    auto it = find_if(std::execution::par, it_of_documents_.begin(), it_of_documents_.end(), [document_id](auto p_id) {return p_id.first == document_id; });
    if (it != it_of_documents_.end()) {
 
        it_of_documents_.erase(it);
        id_of_documents_.erase(document_id);
        std::vector<std::string_view> words;
        words.reserve(id_to_document_freqs_SV_.at(document_id).size());
  
        for_each(std::execution::par,
            id_to_document_freqs_SV_.at(document_id).begin(),
            id_to_document_freqs_SV_.at(document_id).end(),
            [&words](auto& word) {words.push_back(word.first); });
 
        for_each(std::execution::par,
            words.begin(),
            words.end(),
            [&](const auto word) {word_to_document_freqs_SV_.at(word).erase(document_id); });
 
        id_to_document_freqs_SV_.erase(document_id);
        documents_.erase(document_id);       
    }
}
 
bool SearchServer::IsStopWordSV(const std::string_view word) const {
    return stop_words_.count(word) > 0;
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStopSV(const std::string_view text) const {
    std::vector<std::string_view> words;
    for (const std::string_view word : SplitIntoWordsSV(text)) {
        if (!IsStopWordSV(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& rating_in) {
    int rating_len = rating_in.size();
    if (rating_len != 0)
        return accumulate(rating_in.begin(), rating_in.end(), 0) / rating_len;
    else
        return 0;
}
 
bool SearchServer::IsValidWordSV(const std::string_view word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) { return c >= 0 && c < 32; });
}

SearchServer::QueryWordSV SearchServer::ParseQueryWordSV(std::string_view text) const {
    QueryWordSV queryWord;
    using namespace std::string_literals;
    if (text.empty()) {
        throw std::invalid_argument("Query is empty"s);
    }
    if (text[0] == '-') {
        if (text[1] == '-') {
            throw std::invalid_argument("Query contents more then 1 minus character"s);
        }
        if (text.size() == 1) {
            throw std::invalid_argument("There are no a word after minus character"s);
        }
        queryWord.is_minus = true;
        text = text.substr(1);
    }
    if (!IsValidWordSV(text)) {
        throw std::invalid_argument("Query \""s + std::string(text) + "\" contents special characters"s);
    }
    queryWord.data = text;
    queryWord.is_stop = SearchServer::IsStopWordSV(text);
    return queryWord;
}

SearchServer::QuerySV SearchServer::ParseQuerySV(const std::string_view text) const {
    QuerySV query;
    for (const std::string_view word : SplitIntoWordsSV(text)) {
        const QueryWordSV query_word = ParseQueryWordSV(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            }
            else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }

    std::sort(query.minus_words.begin(), query.minus_words.end());
    query.minus_words.erase(std::unique(query.minus_words.begin(), query.minus_words.end()), query.minus_words.end());

    std::sort(query.plus_words.begin(), query.plus_words.end());
    query.plus_words.erase(std::unique(query.plus_words.begin(), query.plus_words.end()), query.plus_words.end());
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_SV_.at(word).size());
}
