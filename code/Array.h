#pragma once
#ifndef GG_ARRAY_H
#define GG_ARRAY_H

#include "Allocator.h"
#include "Span.h"
#include "MiscUtil.h"

namespace gg {

template<class T, class T_Allocator = Mallocator>
class Array : private T_Allocator {

public:
    Array() = default;
    explicit Array(size_t initialCapacity)
        : Array() {
        reserve(initialCapacity);
    }
    Array(Array&& src)
        : T_Allocator(std::move(src))
        , data_(std::exchange(src.data_, nullptr))
        , count_(std::exchange(src.count_, 0))
        , capacity_(std::exchange(src.capacity_, 0)) {
    }
    Array(Span<T const> span) {
        addLastSpan(span);
    }

    ~Array() {
        removeAll();
        deallocate(data_);
    }

    Array& operator=(Array&& src) {
        ReconstructInPlace(*this, std::move(src));
        return *this;
    }

    T& addLast(T const& source) {
        return *ConstructInPlace<T>(emplaceLast(1), source);
    }

    T& addLast(T&& source) {
        return *ConstructInPlace<T>(emplaceLast(1), std::move(source));
    }

    template<class... T_Params>
    T& addLast(T_Params&&... params) {
        return *ConstructInPlace<T>(emplaceLast(1), std::forward<T_Params>(params)...);
    }

    T* addLastN(size_t n) {
        return ConstructArray<T>(emplaceLast(n), n);
    }

    T* addLastCopiedSpan(Span<T const> span) {
        return CopyConstructArray<T>(emplaceLast(span.count()), span.count(), span.begin());
    }

    T* addLastMovedSpan(Span<T>&& span) {
        Span<T> temp = std::move(span);
        return MoveArray<T>(emplaceLast(temp.count()), temp.count(), temp.begin());
    }

    void reserve(size_t capacity) {
        if (capacity > capacity_) {
            grow(capacity);
        }
    }

    void* emplaceLast(size_t n) {
        reserve(count_ + n);
        return (std::remove_const<T>::type*)&data_[std::exchange(count_, count_ + (unsigned)n)];
    }

    T removeLast() {
        assert(count_ > 0);
        return std::move(data_[--count_]);
    }

    void removeLastN(size_t n) {
        assert(count_ >= n);
        for (size_t i = 0; i < n; i++) {
            data_[count_ - i - 1].~T();
        }
        count_ -= (unsigned)n;
    }

    void removeAll() {
        removeLastN(count_);
    }

    void setCount(size_t count) {
        if (count < count_) {
            removeLastN(count_ - count);
        } else {
            emplaceLast(count - count_);
        }
    }

    T* begin() const {
        return data_;
    }

    T* end() const {
        return data_ + count_;
    }

    unsigned count() const {
        return count_;
    }

    T& operator[](size_t i) const {
        assert(i < count_);
        return data_[i];
    }

    Span<T> slice(size_t start, size_t end) const {
        assert(start <= end && end <= count_);
        return {data_ + start, end - start};
    }

private:
    GG_NO_INLINE void grow(size_t requestedCapacity) {
        assert((unsigned)requestedCapacity > capacity_);
        capacity_ = std::max(capacity_ * 2, (unsigned)requestedCapacity);
        deallocate(std::exchange(data_, MoveArray<T>(allocate(capacity_ * sizeof(T), alignof(T)), count_, data_)));
    }

    T* data_ = nullptr;
    unsigned count_ = 0;
    unsigned capacity_ = 0;
};

template<class T> template<class T_Allocator>
Span<T>::Span(Array<T, T_Allocator> const& a)
    : first_(a.begin())
    , count_(a.count()) {
}

}

#endif
