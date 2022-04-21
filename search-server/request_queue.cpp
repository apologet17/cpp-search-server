#include "request_queue.h"

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    return RequestQueue::AddFindRequest(raw_query, [status](int document_id, DocumentStatus d_status, int rating) { return d_status == status; });
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    return RequestQueue::AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

int RequestQueue::GetNoResultRequests() const {
    return numNoResultsRequests;
}

void RequestQueue::RequestsControl(bool emptyResults) {
    QueryResult queryResult;
    queryResult.queryEmpty = emptyResults;
    requests_.push_back(queryResult);
    if (emptyResults) {
        ++numNoResultsRequests;
    }

    if (requests_.size() > min_in_day_) {
        if (requests_.front().queryEmpty) {
            --numNoResultsRequests;
        }
        requests_.pop_front();
    }
}