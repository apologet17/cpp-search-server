#include <iostream>
#include <string>
#include <vector>
#include "search_server.h"

using namespace std;

// -------- Начало модульных тестов поисковой системы ----------

template <typename TestType>
void RunTestImpl(const TestType& testName, const string& testName_str) {
    testName();
    cerr << testName_str << ": OK" << endl;
}

#define RUN_TEST(func)  RunTestImpl(func, #func) 

ostream& operator<<(ostream& os, const vector<string>& inputVect) {
    os << "["s;
    bool first = true;
    for (const auto& str : inputVect) {
        if (first) {
            os << str;
            first = false;
        }
        else
            os << ", "s << str;
    }
    os << "]"s;

    return os;
}

ostream& operator<<(ostream& os, const DocumentStatus& inputStatus) {
    switch (inputStatus) {
    case DocumentStatus::ACTUAL:
        os << "ACTUAL"s;
        break;
    case DocumentStatus::IRRELEVANT:
        os << "IRRELEVANT"s;
        break;
    case DocumentStatus::BANNED:
        os << "BANNED"s;
        break;
    case DocumentStatus::REMOVED:
        os << "REMOVED"s;
        break;
    default:
        os << "UNDEFINED_STATUS"s;
    }
    return os;
}
template <typename LeftType, typename RightType>
void AssertEqualImpl(const LeftType& leftVal, const RightType& rightVal, const string& leftVal_str, const string& rightVal_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (leftVal != rightVal) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << leftVal_str << ", "s << rightVal_str << ") failed: "s;
        cout << leftVal << " != "s << rightVal << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(left, right) AssertEqualImpl((left), (right), #left, #right, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(left, right, hint) AssertEqualImpl((left), (right), #left, #right, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// Тест проверяет, что поисковая система ищет добавленные документы
void TestDocumentAdd() {
    const vector<int> doc_id = { 42, 43, 44 };
    const vector<string> content = { "cat in the city"s, "cat and dog in the small village"s, "cat and dog with rat under the table"s, };
    const vector<int> ratings = { 1, 2, 3 };

    //инициализация поискового сервера
    SearchServer server("in and with");
    server.AddDocument(doc_id[0], content[0], DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[1], content[1], DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[2], content[2], DocumentStatus::ACTUAL, ratings);

    //Должны найтись все 3 документа. ИД не проверяем, так как документов на сервере всего 3
    {
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 3u);
    }
    //Должны найтись 2 документа (ИД = 43,44).
    {
        const auto found_docs = server.FindTopDocuments("dog"s);
        ASSERT_EQUAL(found_docs.size(), 2u);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        ASSERT_EQUAL(doc0.id, doc_id[1]);
        ASSERT_EQUAL(doc1.id, doc_id[2]);
    }
    //Должен найтись 1 документ (ИД = 44)
    {
        const auto found_docs = server.FindTopDocuments("rat"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id[2]);
    }
    {
        ASSERT(server.FindTopDocuments("snake"s).empty());
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server("in and with");
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет, что поисковая система исключает из результата документы с минус-словами
void TestExcludeDocsWithMinusWords() {
    const vector<int> doc_id = { 4, 5 };
    const vector<string> content = { "cat in the city"s, "cat in the city out"s };
    const vector<int> ratings = { 1, 2, 3 };

    //инициализируем поисковый сервер
    SearchServer server("in and with");
    server.AddDocument(doc_id[0], content[0], DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[1], content[1], DocumentStatus::ACTUAL, ratings);

    // Если минус-слов нет, находятся два документа
    {
        const auto found_docs = server.FindTopDocuments("cat in"s);
        ASSERT_EQUAL(found_docs.size(), 2u);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        ASSERT_EQUAL(doc0.id, doc_id[0]);
        ASSERT_EQUAL(doc1.id, doc_id[1]);
    }
    // Если минус-слово в одном документе, находится другой документ
    {
        const auto found_docs = server.FindTopDocuments("cat -out"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id[0]);
    }
    //Если минус-слово в обоих документах, не находится ничего
    ASSERT(server.FindTopDocuments("cat -in"s).empty());
}

//Тест проверяет возвращение слов из поискового запроса, присутствующих в конкретном документе
void TestMatchedWords() {
    const vector<int> doc_id = { 4, 5 };
    const vector<string> content = { "cat in the city"s, "cat in the city out"s };
    const vector<int> ratings = { 1, 2, 3 };

    //инициализируем поисковый сервер
    SearchServer server("in and with");
    server.AddDocument(doc_id[0], content[0], DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[1], content[1], DocumentStatus::ACTUAL, ratings);

    //Поисковый запрос совпадает с содержимым документа - вернуть все слова
    {
        const auto wordsFromQuery = server.MatchDocument(content[0], doc_id[0]);
        vector<string> wordsFromContent = SplitIntoWords(content[0]);
        sort(wordsFromContent.begin(), wordsFromContent.end());
        ASSERT_EQUAL(wordsFromContent, get<0>(wordsFromQuery));
        ASSERT_EQUAL(DocumentStatus::ACTUAL, get<1>(wordsFromQuery));
    }

    //Поисковый запрос содержит больше слов, чем есть в подходящем документе - вернуть только слова из документа
    {
        const auto wordsFromQuery = server.MatchDocument("cat in the city out"s, doc_id[0]);
        vector<string> wordsFromContent = SplitIntoWords(content[0]);
        sort(wordsFromContent.begin(), wordsFromContent.end());
        ASSERT_EQUAL(wordsFromContent, get<0>(wordsFromQuery));
        ASSERT_EQUAL(DocumentStatus::ACTUAL, get<1>(wordsFromQuery));
    }

    //Поисковый запрос содержит меньше слов, чем есть в подходящем документе - вернуть только слова запроса
    {
        const auto wordsFromQuery = server.MatchDocument("cat in the out"s, doc_id[0]);
        vector<string> wordsFromContent = SplitIntoWords("cat in the"s);
        sort(wordsFromContent.begin(), wordsFromContent.end());
        ASSERT_EQUAL(wordsFromContent, get<0>(wordsFromQuery));
        ASSERT_EQUAL(DocumentStatus::ACTUAL, get<1>(wordsFromQuery));
    }

    //Поисковый запрос содержит минус-слова, которые есть в подходящем документе - вернуть пустой вектор
    {
        const auto wordsFromQuery = server.MatchDocument("cat in the -city out", doc_id[0]);
        ASSERT(get<0>(wordsFromQuery).empty());
        ASSERT_EQUAL(DocumentStatus::ACTUAL, get<1>(wordsFromQuery));
    }
}

//Тест проверяет правильность сортировки по релевантности
void TestRelevanceSort() {
    const vector<int> doc_id = { 3, 4, 5, 6 };
    const vector<string> content = { "ui ui ui ui", "cat dog fat rat"s, "cat ty asas hytr"s, "re fd asas hytr"s };
    const vector<int> ratings = { 1, 2, 3 };

    //инициализируем поисковый сервер
    SearchServer server("in and with");
    server.AddDocument(doc_id[0], content[0], DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[1], content[1], DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[2], content[2], DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[3], content[3], DocumentStatus::ACTUAL, ratings);

    //Поисковому запросу наиболее подходит последний документ, менее подходящий - предпоследний, еще менее - первый, совсем не подходит нулевой
    {
        const auto found_docs = server.FindTopDocuments("cat ty re fd asas hytr"s);
        ASSERT_EQUAL(found_docs.size(), 3);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        const Document& doc2 = found_docs[2];
        ASSERT_EQUAL(doc0.id, doc_id[3]);
        ASSERT_EQUAL(doc1.id, doc_id[2]);
        ASSERT_EQUAL(doc2.id, doc_id[1]);
    }

}

//Тест проверяет правильность расчета рейтинга 
void TestRatingCalc() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3, 4, 5 };

    //Если рейтинг посчитан неверно, документ не найдется
    {
        SearchServer server("in and with");
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const int etalonRating = (ratings[0] + ratings[1] + ratings[2] + ratings[3] + ratings[4]) / 5;
        const auto found_docs = server.FindTopDocuments("cat in the city"s, [etalonRating](int document_id, DocumentStatus d_status, int rating) { return rating == etalonRating; });
        ASSERT_EQUAL(found_docs.size(), 1);
    }
}

//Тест проверяет правильность использования предиката 
void TestPredicate() {
    const vector<int> doc_id = { 42, 44 };
    const vector<string> content = { "cat in the city"s, "cat in the city"s };
    const vector<int> ratings = { 1, 2, 3 };

    SearchServer server("in and with");
    server.AddDocument(doc_id[0], content[0], DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[1], content[0], DocumentStatus::IRRELEVANT, ratings);
    //Если предикат по умолчанию не определяет поиск актуальных документов, нулевой документ не найдется
    {
        const auto found_docs = server.FindTopDocuments(content[0]);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id[0]);
    }

    //Если предикат работает по статусу, нулевой документ не найдется, так как документы идентичны во всем, кроме статуса и ИД
    {
        const auto found_docs = server.FindTopDocuments(content[0], DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id[1]);
    }

    //Если предикат не работает по ИД, найдется число документов, отличное от 1 или это будет не тот документ
    {
        int etalonID = 43;
        const auto found_docs = server.FindTopDocuments(content[0], [etalonID](int document_id, DocumentStatus d_status, int rating) { return document_id < etalonID; });
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id[0]);
    }
    //Работу по рейтингу не проверяем - если предикат по рейтингу не работает, откажет TestRatingCalculation()
}

//Тест проверяет поиск документа по заданному статусу
void TestStatusSearch() {
    const vector<int> doc_id = { 42, 44, 45, 46 };
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    SearchServer server("in and with");
    server.AddDocument(doc_id[0], content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[1], content, DocumentStatus::IRRELEVANT, ratings);
    server.AddDocument(doc_id[2], content, DocumentStatus::BANNED, ratings);
    server.AddDocument(doc_id[3], content, DocumentStatus::REMOVED, ratings);

    //По статусу ACTUAL и IRRELEVANT искали в прошлых тестах, сейчас проверяем только BANNED 
    {
        const auto found_docs = server.FindTopDocuments(content, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id[2]);
    }
    //По статусу ACTUAL и IRRELEVANT искали в прошлых тестах, сейчас проверяем только REMOVED 
    {
        const auto found_docs = server.FindTopDocuments(content, DocumentStatus::REMOVED);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id[3]);
    }
}

//Тест проверяет правильность расчета релевантности
void TestRelevancecCalc() {
    const vector<int> doc_id = { 3, 4, 5 };
    const vector<string> content = { "white cat fashion ring"s, "fluffy cat fluffy tail"s, "care dog bright eyes"s };
    const vector<int> ratings = { 1, 2, 3 };
    vector<double> TF_IDF = { 1.0, 1.0, 1.0 };
    double eps = 1e-6;

    TF_IDF[0] = 0.25 * log(3.0 / 2.0);
    TF_IDF[1] = 0.5 * log(3.0 / 1.0) + 0.25 * log(3.0 / 2.0);
    TF_IDF[2] = 0.25 * log(3.0 / 1.0);
    //инициализируем поисковый сервер
    SearchServer server("in and with");
    server.AddDocument(doc_id[0], content[0], DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[1], content[1], DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[2], content[2], DocumentStatus::ACTUAL, ratings);

    {
        const auto found_docs = server.FindTopDocuments("fluffy care cat"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(found_docs.size(), 3);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        const Document& doc2 = found_docs[2];
        ASSERT(abs(doc0.relevance - TF_IDF[1]) < eps);
        ASSERT(abs(doc1.relevance - TF_IDF[2]) < eps);
        ASSERT(abs(doc2.relevance - TF_IDF[0]) < eps);
    }

}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestDocumentAdd);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeDocsWithMinusWords);
    RUN_TEST(TestMatchedWords);
    RUN_TEST(TestRelevanceSort);
    RUN_TEST(TestRatingCalc);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestStatusSearch);
    RUN_TEST(TestRelevancecCalc);
}
