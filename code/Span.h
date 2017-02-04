#pragma once
#ifndef GG_SPAN_H
#define GG_SPAN_H

#include <cassert>
#include <algorithm>

namespace gg {

template<class T, class T_Allocator> class Array;

template<class T>
class Span {

public:

    Span()
        : first_(nullptr)
        , count_(0) {
    }
    template<class T_Allocator>
    Span(Array<std::remove_const_t<T>, T_Allocator> const& a);
    Span(Span&& source)
        : first_(std::exchange(source.first_, nullptr))
        , count_(std::exchange(source.count_, 0)) {
    }
    Span(Span<T const>& source)
        : first_(source.first_)
        , count_(source.count_) {
    }
    Span(T* first, size_t count)
        : first_(first)
        , count_(count) {
    }
    template<size_t N>
    Span(T (&data)[N])
        : first_(data)
        , count_(N) {
    }

    T* begin() const {
        return first_;
    }

    T* end() const {
        return first_ + count_;
    }

    unsigned count() const {
        assert((unsigned)count_ == count_);
        return (unsigned)count_;
    }

    T& operator[](size_t i) const {
        assert(i < count_);
        return first_[i];
    }

    Span slice(size_t start, size_t end) const {
        assert(start <= end && end <= count_);
        return {first_ + start, end - start};
    }

    operator Span<T const>() const {
        return *(Span<T const>*)this;
    }

private:

    T* first_;
    size_t count_;
};

}

#endif
