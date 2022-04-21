#pragma once

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