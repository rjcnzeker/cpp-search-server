#include "process_queries.h"

std::vector<std::vector<Document>>
ProcessQueries(const SearchServer& search_server, const vector<std::string>& queries) {
    std::vector<std::vector<Document>> answer(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), answer.begin(), [&search_server] (const string& query) {
        return search_server.FindTopDocuments(query);
    });
    return answer;
}