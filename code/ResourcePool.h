#pragma once
#ifndef GG_RESOURCEPOOL_H
#define GG_RESOURCEPOOL_H

#include "Array.h"

namespace gg {

template<class T_Id>
struct ResourceIdTraitsDefault {
    static unsigned GetIndex(T_Id id) {
        static_assert(false, "Resource traits not defined for this class");
    }
};

template<class T_Id, class T, class T_ResourceIdTraits = ResourceIdTraitsDefault<T_Id>>
class ResourcePool {

public:
    T_Id add(T&& item) {
        T_Id id;
        if (freeList_.count()) {
            unsigned index = freeList_.removeLast();
            id = item.assignIndex(index);
            ConstructInPlace<T>(&items_[index], std::move(item));
        } else {
            id = item.assignIndex(items_.count());
            items_.addLast(std::move(item));
        }
        return id;
    }

    T remove(T_Id id) {
        freeList_.addLast(T_ResourceIdTraits::GetIndex(id));
        return std::move(items_[id.value-1]);
    }

    T* fetch(T_Id id) const {
        return &items_[T_ResourceIdTraits::GetIndex(id)];
    }

    ~ResourcePool() {
        assert(items_.count() == freeList_.count());
    }

private:
    gg::Array<T> items_;
    gg::Array<unsigned> freeList_;
};

}

#endif
