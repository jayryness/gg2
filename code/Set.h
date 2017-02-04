#pragma once
#ifndef GG_SET_H
#define GG_SET_H

#include "Allocator.h"
#include "Hash.h"

namespace gg {

template<class T_Elem>
struct HashElementTraitsDefault {
    using KeyTraits = HashKeyTraitsDefault<T_Elem>;
    static T_Elem const& GetKey(T_Elem const& elem) { return elem; }
};

template<class T_Key, class T_Value>
struct HashElementTraitsDefault<std::pair<T_Key, T_Value>> {
    using KeyTraits = HashKeyTraitsDefault<T_Key>;
    static T_Key const& GetKey(std::pair<T_Key, T_Value> const& elem) {
        return elem.first;
    }
    static T_Key const& GetKey(T_Key const& key) {
        return key;
    }
};

template<class T_Elem, class T_ElemTraits = HashElementTraitsDefault<T_Elem>, class T_Allocator = Mallocator>
class Set : private T_Allocator {

public:
    Set() = default;
    Set(std::nullptr_t)
        : Set() {
    }
    explicit Set(size_t initialCapacity)
        : Set() {
        reserve(initialCapacity);
    }
    Set(Set&& src)
        : T_Allocator(std::move(src))
        , elements_(std::exchange(src.elements_, nullptr))
        , count_(std::exchange(src.count_, 0))
        , mask_(std::exchange(src.mask_, 0)) {
    }
    Set& operator=(Set&& src) {
        ReconstructInPlace(*this, std::move(src));
        return *this;
    }
    ~Set() {
        assert(mask_ | !elements_);
        if (mask_) {
            removeAll();
            deallocate(elements_);
        }
    }

    void reserve(size_t capacity) {
        if (capacity > mask_ + 1) {
            grow(capacity);
        }
    }

    unsigned count() const {
        return count_;
    }

    // Returned pointer is not stable!
    template<class... T_Params>
    T_Elem* add(T_Params&&... params) {
        reserve(2 * ++count_);
        T_Elem elem(std::forward<T_Params>(params)...);
        return store(std::move(elem));
    }

    // Returned pointer is not stable!
    T_Elem* add(T_Elem&& elem) {
        reserve(2 * ++count_);
        return store(std::move(elem));
    }

    template<class T_Key>
    T_Elem remove(T_Key const& key) {
        return removeSlot(fetchSlot(key));
    }

    T_Elem remove(T_Elem* found) {
        return removeSlot((unsigned)(found - elements_));
    }

    // Returned pointer is not stable!
    template<class T_Key>
    T_Elem* fetch(T_Key const& key) const {
        return elements_ + fetchSlot(key);
    }

    // Returned pointer is not stable!
    template<class T_Key>
    T_Elem* find(T_Key const& key) const {
        unsigned slot = getBaseSlot(elem);
        while (!IsNull(elements_[slot])) {
            if (Equals(elements_[slot], key)) {
                return elements_ + slot;
            }
            slot = (slot + 1) & mask_;
        }
        return nullptr;
    }

    // Returned pointer is not stable!
    template<class... T_Params>
    T_Elem* findOrAdd(T_Elem&& elem) {
        reserve(2 * (count_ + 1));
        unsigned slot = getBaseSlot(elem);
        while (!IsNull(elements_[slot])) {
            if (Equals(elements_[slot], elem)) {
                return elements_ + slot;
            }
            slot = (slot + 1) & mask_;
        }
        count_++;
        return ConstructInPlace<T_Elem>(elements_ + slot, std::move(elem));
    }

    void removeAll() {
        if (count_ > 0) {
            count_ = 0;
            for (size_t i = 0; i <= mask_; i++) {
                if (!IsNull(elements_[i])) {
                    T_Elem trashElem = std::move(elements_[i]);
                }
            }
        }
    }

private:
    GG_NO_INLINE void grow(size_t requestedCapacity) {
        assert((unsigned)requestedCapacity > mask_ + 1);
        unsigned oldMask = std::exchange(mask_, NextPow2((unsigned)requestedCapacity) - 1);
        T_Elem* oldElements = std::exchange(elements_, (T_Elem*)allocate((mask_ + 1) * sizeof(T_Elem), alignof(T_Elem)));

        ConstructArray<T_Elem>(elements_, mask_ + 1);

        if (oldMask) {
            for (size_t i = 0; i <= oldMask; i++) {
                if (!IsNull(oldElements[i])) {
                    store(std::move(oldElements[i]));
                }
            }
        }

        deallocate(oldElements);
    }

    T_Elem* store(T_Elem&& elem) const {
        assert(2 * count_ <= mask_ + 1);
        unsigned slot = getBaseSlot(elem);
        while (!IsNull(elements_[slot])) {
            assert(!Equals(elements_[slot], elem)); // Already in set, use findOrAdd()
            slot = (slot + 1) & mask_;
        }
        return ConstructInPlace<T_Elem>(elements_ + slot, std::move(elem));
    }

    T_Elem removeSlot(unsigned slot) {
        assert(slot <= mask_ && !IsNull(elements_[slot]));
        T_Elem removed = std::move(elements_[slot]);
        for (unsigned moving = (slot+1) & mask_; !IsNull(elements_[moving]); moving = (moving+1) & mask_) {
            unsigned const target = getBaseSlot(elements_[moving]);
            bool const a = target <= slot;
            bool const b = slot < moving;
            if (target <= moving ? (a & b) : (a | b)) {
                ReconstructInPlace(elements_[slot], std::move(elements_[moving]));
                slot = moving;
            }
        }
        count_--;
        return removed;
    }

    template<class T_Key>
    unsigned getBaseSlot(T_Key const& key) const {
        assert(!IsNull(key));
        uint32_t hash = Hash(key);
        return hash & mask_;
    }

    template<class T_Key>
    unsigned fetchSlot(T_Key const& key) const {
        unsigned slot = getBaseSlot(key);
        while (!Equals(elements_[slot], key)) {
            assert(!IsNull(elements_[slot]));
            slot = (slot + 1) & mask_;
        }
        return slot;
    }

    template<class T_Rhs>
    static bool Equals(T_Elem const& a, T_Rhs const& b) {
        return T_ElemTraits::KeyTraits::Equals(T_ElemTraits::GetKey(a), T_ElemTraits::GetKey(b));
    }

    template<class T_Key>
    static bool IsNull(T_Key const& key) {
        return T_ElemTraits::KeyTraits::IsNull(T_ElemTraits::GetKey(key));
    }

    template<class T_Key>
    static uint32_t Hash(T_Key const& key) {
        return T_ElemTraits::KeyTraits::Hash(T_ElemTraits::GetKey(key));
    }

    T_Elem* elements_ = nullptr;
    unsigned count_ = 0;
    unsigned mask_ = 0;

};

}

#endif

