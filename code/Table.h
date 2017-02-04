#pragma once
#ifndef GG_TABLE_H
#define GG_TABLE_H

#include "Allocator.h"
#include "Hash.h"

namespace gg {

template<class T_Key, class T_Value, class T_KeyTraits = HashKeyTraitsDefault<T_Key>, class T_Allocator = Mallocator>
class Table : private T_Allocator {

public:
    class Iterator;

    Table() = default;
    explicit Table(size_t initialCapacity)
        : Table() {
        reserve(initialCapacity);
    }
    Table(Table&& src)
        : T_Allocator(std::move(src))
        , keys_(std::exchange(src.keys_, nullptr))
        , values_(std::exchange(src.values_, nullptr))
        , count_(std::exchange(src.count_, 0))
        , mask_(std::exchange(src.mask_, 0)) {
    }
    ~Table() {
        assert((!values_ & !keys_) | mask_);
        if (mask_) {
            removeAll();
            DestroyArray(keys_, mask_ + 1);
            deallocate(values_);
            deallocate(keys_);
        }
    }

    void reserve(size_t capacity) {
        if (capacity > mask_ + 1) {
            grow(capacity);
        }
    }

    Iterator begin() const {
        unsigned slot;
        for (slot = 0; slot <= mask_ && T_KeyTraits::IsNull(keys_[slot]); ++slot) {
        }
        return {*this, slot};
    }

    Iterator end() const {
        return {*this, mask_ + 1};
    }

    unsigned count() const {
        return count_;
    }

    // Returned pointer is not stable!
    template<class... T_Params>
    T_Value* add(T_Key&& key, T_Params&&... params) {
        reserve(2 * ++count_);
        return store(std::move(key), std::forward<T_Params>(params)...);
    }

    // Returned pointer is not stable!
    T_Value* add(T_Key&& key, T_Value&& src) {
        reserve(2 * ++count_);
        return store(std::move(key), std::move(src));
    }

    template<class... T_Params>
    T_Value* add(T_Key const&, T_Params&&...) {
        static_assert(false, "For lvalue key, use add({key}, ...) or add(std::move(key), ...)");
    }

    T_Value remove(T_Key const& key) {
        return removeSlot(fetchSlot(key));
    }

    T_Value removeFound(T_Value* found) {
        return removeSlot((unsigned)(found - values_));
    }

    // Returned pointer is not stable!
    T_Value* fetch(T_Key const& key) const {
        return values_ + fetchSlot(key);
    }

    // Returned pointer is not stable!
    T_Value* find(T_Key const& key) const {
        unsigned slot = getBaseSlot(key);
        while (!T_KeyTraits::IsNull(keys_[slot])) {
            if (T_KeyTraits::Equals(keys_[slot], key)) {
                return values_ + slot;
            }
            slot = (slot + 1) & mask_;
        }
        return nullptr;
    }

    // Returned pointer is not stable!
    template<class... T_Params>
    T_Value* findOrAdd(T_Key&& key, T_Params&&... params) {
        reserve(2 * (count_ + 1));
        unsigned slot = getBaseSlot(key);
        while (!T_KeyTraits::IsNull(keys_[slot])) {
            if (T_KeyTraits::Equals(keys_[slot], key)) {
                return values_ + slot;
            }
            slot = (slot + 1) & mask_;
        }
        count_++;
        ConstructInPlace<T_Key>(keys_ + slot, std::move(key));
        return ConstructInPlace<T_Value>(values_ + slot, std::forward<T_Params>(params)...);
    }

    void removeAll() {
        if (count_ > 0) {
            count_ = 0;
            for (size_t i = 0; i <= mask_; i++) {
                if (!T_KeyTraits::IsNull(keys_[i])) {
                    T_Key trashKey = std::move(keys_[i]);
                    values_[i].~T_Value();
                }
            }
        }
    }

private:
    GG_NO_INLINE void grow(size_t requestedCapacity) {
        assert((unsigned)requestedCapacity > mask_ + 1);
        unsigned oldMask = std::exchange(mask_, NextPow2((unsigned)requestedCapacity) - 1);
        T_Key* oldKeys = std::exchange(keys_, (T_Key*)allocate((mask_ + 1) * sizeof(T_Key), alignof(T_Key)));
        T_Value* oldValues = std::exchange(values_, (T_Value*)allocate((mask_ + 1) * sizeof(T_Value), alignof(T_Value)));

        ConstructArray<T_Key>(keys_, mask_ + 1);

        if (oldMask) {
            for (size_t i = 0; i <= oldMask; i++) {
                if (!T_KeyTraits::IsNull(oldKeys[i])) {
                    store(std::move(oldKeys[i]), std::move(oldValues[i]));
                }
            }
        }

        deallocate(oldValues);
        deallocate(oldKeys);
    }

    template<class... T_Params>
    T_Value* store(T_Key&& key, T_Params&&... params) const {
        assert(2 * count_ <= mask_ + 1);
        unsigned slot = getBaseSlot(key);
        while (!T_KeyTraits::IsNull(keys_[slot])) {
            assert(!T_KeyTraits::Equals(keys_[slot], key));  // Already in table, use findOrAdd()
            slot = (slot + 1) & mask_;
        }
        ConstructInPlace<T_Key>(keys_ + slot, std::move(key));
        return ConstructInPlace<T_Value>(values_ + slot, std::forward<T_Params>(params)...);
    }

    T_Value removeSlot(unsigned slot) {
        assert(slot <= mask_ && !T_KeyTraits::IsNull(keys_[slot]));
        T_Value removed = std::move(values_[slot]);
        T_Key trashKey = std::move(keys_[slot]);
        for (unsigned moving = (slot+1) & mask_; !T_KeyTraits::IsNull(keys_[moving]); moving = (moving+1) & mask_) {
            unsigned const target = getBaseSlot(keys_[moving]);
            bool const a = target <= slot;
            bool const b = slot < moving;
            if (target <= moving ? (a & b) : (a | b)) {
                ReconstructInPlace(keys_[slot], std::move(keys_[moving]));
                ReconstructInPlace(values_[slot], std::move(values_[moving]));
                slot = moving;
            }
        }
        count_--;
        return removed;
    }

    unsigned getBaseSlot(T_Key const& key) const {
        assert(!T_KeyTraits::IsNull(key));
        uint32_t hash = T_KeyTraits::Hash(key);
        return hash & mask_;
    }

    unsigned fetchSlot(T_Key const& key) const {
        unsigned slot = getBaseSlot(key);
        while (!T_KeyTraits::Equals(keys_[slot], key)) {
            assert(!T_KeyTraits::IsNull(keys_[slot]));
            slot = (slot + 1) & mask_;
        }
        return slot;
    }

    T_Key* keys_ = nullptr;
    T_Value* values_ = nullptr;
    unsigned count_ = 0;
    unsigned mask_ = 0;
};

template<class T_Key, class T_Value, class T_KeyTraits, class T_Allocator>
class Table<T_Key, T_Value, T_KeyTraits, T_Allocator>::Iterator {
public:
    Iterator(Table const& table, unsigned slot)
        : table_(table)
        , slot_(slot) {
    }
    bool operator!=(Iterator const& rhs) const {
        return slot_ != rhs.slot_;
    }
    Iterator& operator++() {
        while (++slot_ <= table_.mask_ && T_KeyTraits::IsNull(table_.keys_[slot_])) {
        }
        return *this;
    }
    T_Value& operator*() const {
        assert(slot_ <= table_.mask_);
        return table_.values_[slot_];
    }
private:
    Table const& table_;
    unsigned slot_;
};

}

#endif
