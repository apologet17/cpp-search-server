#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string_view> queries) {

    std::vector <std::vector<Document>> output(queries.size(), std::vector<Document>());

    std::transform(std::execution::par, queries.begin(), queries.end(),
        output.begin(),
        [&search_server](const auto query)
        { return search_server.FindTopDocuments(query); });
    return output;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string_view>& queries) {
    std::vector <std::vector<Document>> input(queries.size(), std::vector<Document>());
    std::vector<Document> output;

    std::transform(std::execution::par, queries.begin(), queries.end(),
        input.begin(),
        [&search_server](const auto query)
        { return search_server.FindTopDocuments(query); });

    for (size_t i = 0; i < input.size(); ++i) {
        for (size_t j = 0; j < input[i].size(); ++j) {
            output.push_back(input[i][j]);
        }
    }
    return output;
}