#include "process_queries.h"

std::vector<std::vector<Document>>
ProcessQueries(const SearchServer& search_server, const vector<std::string>& queries) {
    std::vector<std::vector<Document>> answer(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), answer.begin(),
                   [&search_server](const string& query) {
                       return search_server.FindTopDocuments(query);
                   });
    return answer;
}

vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries) {
    ProcessQueries(search_server, queries);
    std::vector<Document> document_flat;
    for (const std::vector<Document>& documents : ProcessQueries(search_server, queries)) {
        for (const Document& document : documents) {
            document_flat.push_back(document);
        }
    }
    return document_flat;
}