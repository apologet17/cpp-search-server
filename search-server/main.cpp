#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <numeric>

using namespace std;
const int MAX_RESULT_DOCUMENT_COUNT = 5;
struct Document {
    int id;
    double relevance;
    int rating;

    Document(int id_input = 0, double relevance_input = 0.0, int rating_input = 0)
        : id(id_input)
        , relevance(relevance_input)
        , rating(rating_input)
    {}
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (word.size())
                words.push_back(word);
            word = ""s;
        }
        else {
            word += c;
        }
    }
    if (word.size())
        words.push_back(word);

    return words;
}

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
        for (const auto& word : stop_words) {
            if (!IsValidWord(word))
                throw invalid_argument("Stop word \""s + word + "\" contents special characters"s);
        }      
    }

    explicit SearchServer(const string& stop_words_text = ""s)
        : SearchServer(SplitIntoWords(stop_words_text))  
    {
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0)
            throw invalid_argument("ID \""s + to_string(document_id) + "\" is negative"s);
        if (documents_.count(document_id) > 0)
            throw invalid_argument("ID \""s + to_string(document_id) + "\" is present in database"s);
        const vector<string> wordsInDoc = SplitIntoWordsNoStop(document);
        if (wordsInDoc.size() != 0) {
            for (const string& word : wordsInDoc)
                if (!IsValidWord(word))
                    throw invalid_argument("Document's word \""s + word + "\" contents special characters"s);
            const double fractFreq = 1.0 / wordsInDoc.size();
            for (const string& word : wordsInDoc) {
                word_to_document_freqs_[word][document_id] += fractFreq;
            }
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        iDbyNumber_.push_back(document_id);
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const double eps = 1e-6;
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(),
            [eps](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < eps) {
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

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus d_status, int rating) { return d_status == status; });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words = {};

        if (documents_.count(document_id) == 0)
            return tuple{ matched_words,  DocumentStatus::ACTUAL };

        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }

        return tuple{ matched_words, documents_.at(document_id).status };
    }

    int GetStopWordsCount() const {
        return stop_words_.size();
    }

    int GetDocumentId(int index) const {
        if ((index < 0) || (index > static_cast<int>(iDbyNumber_.size()) - 1))
          throw  out_of_range("Bad index"s);
        return iDbyNumber_[index];
    }
    inline static constexpr int INVALID_DOCUMENT_ID = -1;
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> iDbyNumber_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& rating_in) {
        int rating_len = rating_in.size();
        if (rating_len != 0)
            return accumulate(rating_in.begin(), rating_in.end(), 0) / rating_len;
        else
            return 0;
    }

    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) { return c >= 0 && c < 32; });
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;

        QueryWord()
            : data(""s)
            , is_minus(false)
            , is_stop(false)
        {}
    };

    QueryWord ParseQueryWord(string text) const {
        QueryWord queryWord;

        if (text.empty()) {
            throw invalid_argument("Query is empty"s);
        }
        if (!IsValidWord(text)) {
            throw invalid_argument("Query \"" + text + "\" contents special characters"s);
        }
        if (text[0] == '-') {
            if (text[1] == '-') {
                throw invalid_argument("Query contents more then 1 minus character"s);
            }  
            if (text.size() == 1) {
                throw invalid_argument("There are no a word after minus character"s);
            }              
            queryWord.is_minus = true;
            text = text.substr(1);
        }
        queryWord.data = text;
        queryWord.is_stop = IsStopWord(text);
        return queryWord;
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;

        Query()
            : plus_words({})
            , minus_words({})
        {}
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);               
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }
    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename Predicate>
    vector<Document> FindAllDocuments(const Query& query, Predicate FilterByStsRating) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                DocumentData temp_doc_data = documents_.at(document_id);
                if (FilterByStsRating(document_id, temp_doc_data.status, temp_doc_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
                });
        }
        return matched_documents;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
    const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const exception& e) {
        cout << "Error document add "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Search results: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    }
    catch (const exception& e) {
        cout << "Search error: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Document match by query: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const exception& e) {
        cout << "Error document match by query "s << query << ": "s << e.what() << endl;
    }
}

int main() {
    try {
        SearchServer search_server("and in on"s);

        AddDocument(search_server, 1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        AddDocument(search_server, 1, "fluffy dog and fashion collar"s, DocumentStatus::ACTUAL, { 1, 2 });
        AddDocument(search_server, -1, "fluffy dog and fashion collar"s, DocumentStatus::ACTUAL, { 1, 2 });
        AddDocument(search_server, 3, "big dog bi\x12rd john"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
        AddDocument(search_server, 4, "big dog bird john"s, DocumentStatus::ACTUAL, { 1, 1, 1 });

        FindTopDocuments(search_server, "fluffy -dog"s);
        FindTopDocuments(search_server, "fluffy --cat"s);
        FindTopDocuments(search_server, "fluffy -"s);

        MatchDocuments(search_server, "fluffy dog"s);
        MatchDocuments(search_server, "fashion -cat"s);
        MatchDocuments(search_server, "fashion --dog"s);
        MatchDocuments(search_server, "fluffy - tail"s);
    }
    catch (const invalid_argument& e) {
        cout << e.what() << endl;
    }
    


}