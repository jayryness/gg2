#pragma once
#ifndef GG_RING_H
#define GG_RING_H

#include "Allocator.h"
#include "Span.h"
#include "MiscUtil.h"

namespace gg {

template<class T, class T_Allocator = Mallocator>
class Ring : private T_Allocator {

public:
    class Iterator;

    Ring() = default;
    explicit Ring(size_t initialCapacity)
        : Ring() {
        reserve(initialCapacity);
    }
    Ring(Ring&& src)
        : T_Allocator(std::move(src))
        , data_(std::exchange(src.data_, nullptr))
        , first_(std::exchange(src.first_, 0))
        , count_(std::exchange(src.count_, 0))
        , mask_(std::exchange(src.mask_, -1)) {
    }

    ~Ring() {
        removeAll();
        deallocate(data_);
    }

    T& addLast(T const& source) {
        return *ConstructInPlace<T>(emplaceLast(), source);
    }

    T& addLast(T&& source) {
        return *ConstructInPlace<T>(emplaceLast(), std::move(source));
    }

    template<class... T_Params>
    T& addLast(T_Params&&... params) {
        return *ConstructInPlace<T>(emplaceLast(), std::forward<T_Params>(params)...);
    }

    void reserve(size_t capacity) {
        if (capacity > mask_ + 1) {
            grow(capacity);
        }
    }

    void* emplaceLast() {
        reserve(count_ + 1);
        return (std::remove_const<T>::type*)&data_[(first_ + count_++) & mask_];
    }

    T removeFirst() {
        assert(count_ > 0);
        --count_;
        return std::move(data_[std::exchange(first_, (first_ + 1) & mask_)]);
    }

    T removeLast() {
        assert(count_ > 0);
        return std::move(data_[(first_ + --count_) & mask_]);
    }

    void removeLastN(size_t n) {
        assert(count_ >= n);
        unsigned const end = first_ + count_;
        for (int i = -(int)n; i < 0; i++) {
            data_[(end + i) & mask_].~T();
        }
        count_ -= (unsigned)n;
    }

    void removeAll() {
        removeLastN(count_);
    }

    Iterator begin() const {
        return {*this, 0};
    }

    Iterator end() const {
        return {*this, count_};
    }

    unsigned count() const {
        return count_;
    }

    T& operator[](size_t i) const {
        assert(i < count_);
        return data_[(first_ + i) & mask_];
    }

    Span<T> frontSpan() const {
        return {&data_[first_], std::min(mask_ + 1 - first_, count_)};
    }

    Span<T> backSpan() const {
        return {data_, std::max(first_ + count_, mask_ + 1) - mask_ - 1};
    }

    std::remove_const_t<T>* linearizeCopy(std::remove_const_t<T>* dest) const {
        for (unsigned i = 0; i < count_; i++) {
            ConstructInPlace<T>(dest + i, data_[(first_ + i) & mask_]);
        }
        return dest;
    }

private:
    GG_NO_INLINE void grow(size_t requestedCapacity) {
        assert((unsigned)requestedCapacity > mask_ + 1);
        unsigned capacity = (requestedCapacity > 2*mask_ + 2) ? NextPow2((unsigned)requestedCapacity) : 2*mask_ + 2;
        auto* data = (std::remove_const_t<T>*)allocate(capacity * sizeof(T), alignof(T));
        MoveArray<T>(data, std::min(mask_ + 1 - first_, count_), &data_[first_]);
        if (first_ + count_ > mask_ + 1) {
            MoveArray<T>(data + mask_ + 1 - first_, first_ + count_ - mask_ - 1, data_);
        }
        deallocate(std::exchange(data_, data));
        first_ = 0;
        mask_ = capacity - 1;
    }

    T* data_ = nullptr;
    unsigned first_ = 0;
    unsigned count_ = 0;
    unsigned mask_ = -1;
};

template<class T, class T_Allocator>
class Ring<T, T_Allocator>::Iterator {
public:
    Iterator(Ring const& ring, unsigned i)
        : ring_(ring)
        , i_(i) {
    }
    bool operator!=(Iterator const& rhs) const {
        return i_ != rhs.i_;
    }
    Iterator& operator++() {
        ++i_;
        return *this;
    }
    T& operator*() const {
        return ring_[i_];
    }
private:
    Ring const& ring_;
    unsigned i_;
};

}

#endif
