#pragma once
#include <ostream>
#include <vector>

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end)
        : itFirst_(begin)
        , itLast_(end)
    {
        size_ = distance(begin, end);
    }

    Iterator begin() const {
        return itFirst_;
    }

    Iterator end() const {
        return itLast_;
    }

    size_t size() const {
        return size_;
    }

private:
    Iterator itFirst_;
    Iterator itLast_;
    size_t size_;
};

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator beginIt, Iterator endIt, size_t page_size)
    {
        if (distance(beginIt, endIt) == 0)
            pairsOfIterators_ = {};
        else {
            Iterator first = beginIt;
            Iterator last = endIt;
            while (distance(first, endIt) >= static_cast<int>(page_size)) {
                last = next(first, page_size);
                pairsOfIterators_.push_back(IteratorRange<Iterator>(first, last));
                first = last;
            }
            if (distance(first, endIt) > 0)
                pairsOfIterators_.push_back(IteratorRange<Iterator>(first, endIt));
        }
    }
    auto begin() const {
        return pairsOfIterators_.begin();
    }

    auto end() const {
        return pairsOfIterators_.end();
    }

    auto size() const {
        return pairsOfIterators_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> pairsOfIterators_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(c.begin(), c.end(), page_size);
}

template <typename Iterator>
std::ostream& operator<<(std::ostream& output, IteratorRange<Iterator> input) {
    for (auto i = input.begin(); i != input.end(); ++i)
        output << *i;
    return output;
}