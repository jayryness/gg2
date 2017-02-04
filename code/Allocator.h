#pragma once
#ifndef GG_ALLOCATOR_H
#define GG_ALLOCATOR_H

#include <memory>

namespace gg {

struct IAllocator {
    virtual void* allocate(size_t bytes, size_t alignment) = 0;
    virtual void deallocate(void const* p) = 0;
    IAllocator* getInterface(){
        return this;
    }
};

class IAllocatorPtr : public IAllocator {
public:
    IAllocatorPtr(IAllocator* iallocator)
        : iallocator_(iallocator) {
    }
    void* allocate(size_t bytes, size_t alignment) {
        return iallocator_->allocate(bytes, alignment);
    }
    void deallocate(void const* p) {
        iallocator_->deallocate(p);
    }
private:
    IAllocator* iallocator_;
};

template<class T_Allocator>
class IAllocatorShim : public IAllocator {
public:
    virtual void* allocate(size_t bytes, size_t alignment) override final {
        return allocator_.allocate(bytes, alignment);
    }
    virtual void deallocate(void const* p) override final {
        allocator_.deallocate(p);
    }
private:
    T_Allocator allocator_;
};

class Mallocator {
public:
    void* allocate(size_t bytes, size_t alignment) const {
        return _aligned_malloc(bytes, alignment);
    }
    void deallocate(void const* p) const {
        _aligned_free((void*)p);
    }
    static IAllocator* getInterface() {
        return &s_iallocatorImpl;
    }
private:
    static IAllocatorShim<Mallocator> s_iallocatorImpl;
};

}

#endif

