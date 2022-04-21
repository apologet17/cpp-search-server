#include "string_processing.h"

using namespace std::string_literals;

std::vector<std::string_view> SplitIntoWordsSV(std::string_view str) {
    std::vector<std::string_view> result;
    const size_t last = str.npos;
    while (true) {
        size_t space = str.find(' ');
        result.push_back(str.substr(0, space));
        if (space == last) {
            break;
        }
        else {
            str.remove_prefix(space + 1);
        }
    }
 
    return result;
}

std::vector<std::string> SplitIntoWords(const std::string& text) {
    std::vector<std::string> words;
    std::string word;
    for (const char c : text) {
        if (c == ' ') {
            if (word.size()) {
                words.push_back(word);
            }
            word = ""s;
        }
        else {
            word += c;
        }
    }
    if (word.size()) {
        words.push_back(word);
    }

    return words;
}