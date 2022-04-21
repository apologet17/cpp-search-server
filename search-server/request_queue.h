#pragma once
#include <deque>
#include "search_server.h"

class RequestQueue {

public:
    explicit RequestQueue(const SearchServer& search_server)
        : search_server_(search_server)
        , numNoResultsRequests(0)
    {
    }

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;

private:
    struct QueryResult {
        bool queryEmpty;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    int numNoResultsRequests;

    void RequestsControl(bool emptyResults);
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    std::vector<Document> SearchResults = search_server_.FindTopDocuments(raw_query, document_predicate);
    RequestsControl(SearchResults.empty());
    return SearchResults;
}