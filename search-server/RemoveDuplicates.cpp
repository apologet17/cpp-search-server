#include "RemoveDuplicates.h"
#include "log_duration.h"
#include <iostream>
#include <set>
using namespace std::string_literals;

void RemoveDuplicates(SearchServer& search_server) {
    LOG_DURATION("REmove");
    std::set<std::set<std::string_view>> word_matrix;
    std::vector<int> ids;
    ids.reserve(search_server.size());
    size_t current_size = 0;

    for (const int id : search_server) {
        std::set<std::string_view> current_words;
        for (auto [word, freq] : search_server.GetWordFrequencies(id)) {
            current_words.insert(word);
        }          
        word_matrix.insert(current_words);
        if (word_matrix.size() == current_size) {
            ids.push_back(id);
            std::cout << "Found duplicate document id "s << id << std::endl;
        }
        else {
            ++current_size;
        }     
    }
    for (auto id_for_del: ids) {
        search_server.RemoveDocument(id_for_del);
    }
}