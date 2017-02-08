#pragma once
#ifndef GG_MISC_UTIL_H
#define GG_MISC_UTIL_H

#include <cassert>
#include <cstdint>
#include <algorithm>

#ifndef NDEBUG
#define GG_DEBUG
#endif

#define GG_ITER_LAMBDA(what) ([&](auto& it) {return what;})

#define GG_TOKEN_EVAL_CAT(arg1, arg2) GG_TOKEN_CAT(arg1, arg2)
#define GG_TOKEN_CAT(arg1, arg2) arg1 ## arg2
#define GG_SCOPE_EXIT(code) \
    auto GG_TOKEN_EVAL_CAT(scope_exit_, __LINE__) = gg::Internal::MakeScopeExit([&](){code;})

#if defined(_MSC_VER)

#include "MiscUtilWin.inl"

#else

#error Unsupported platform.

#endif

// TODO calling alloca inside argument list is sketchy af; msvc does the right thing, not sure about others...
#define GG_STACK_ARRAY(type, count, ...) gg::ConstructArray<type>(GG_ALLOCA((count)*sizeof(type)), count, __VA_ARGS__)

namespace gg {

template<class T, unsigned N>
constexpr unsigned CountOf(T (&)[N]) {
    return N;
}

unsigned CountLeadingZeroBits(uint32_t bits);
unsigned CountLeadingZeroBits(uint64_t bits);
unsigned CountNonzeroBits(uint32_t bits);
unsigned CountNonzeroBits(uint64_t bits);
uint32_t RotateBitsLeft(uint32_t bits, unsigned shift);
uint64_t RotateBitsLeft(uint64_t bits, unsigned shift);
uint32_t RotateBitsRight(uint32_t bits, unsigned shift);
uint64_t RotateBitsRight(uint64_t bits, unsigned shift);

inline unsigned FloorLog2(uint32_t x) {
    return 31 - CountLeadingZeroBits(x);
}

inline unsigned FloorLog2(uint64_t x) {
    return 63 - CountLeadingZeroBits(x);
}

inline unsigned CeilingLog2(uint32_t x) {
    return 1 + FloorLog2(x ? x - 1 : 0);
}

inline unsigned CeilingLog2(uint64_t x) {
    return 1 + FloorLog2(x ? x - 1 : 0);
}

inline uint8_t NextPow2(uint8_t x) {
    auto result = x - 1;
    for (unsigned sh = 1; sh < 8; sh <<= 1)
        result |= result >> sh;
    return result + 1;
}

inline uint16_t NextPow2(uint16_t x) {
    auto result = x - 1;
    for (unsigned sh = 1; sh < 16; sh <<= 1)
        result |= result >> sh;
    return result + 1;
}

inline uint32_t NextPow2(uint32_t x) {
    auto result = x - 1;
    for (unsigned sh = 1; sh < 32; sh <<= 1)
        result |= result >> sh;
    return result + 1;
}

inline uint64_t NextPow2(uint64_t x) {
    auto result = x - 1;
    for (unsigned sh = 1; sh < 64; sh <<= 1)
        result |= result >> sh;
    return result + 1;
}

template<class T, class... T_Params> T* ConstructInPlace(void* mem, T_Params&&... params);
template<class T, class... T_Params> T* ReconstructInPlace(T& obj, T_Params&&... params);
template<class T, class... T_Params> T* ConstructArray(void* mem, size_t count, T_Params&&... params);
template<class T> T* CopyConstructArray(void* dest, size_t count, T const* source);
template<class T> T* MoveArray(void* dest, size_t count, T* source); // move elements & destruct source
template<class T> void DestroyArray(T* mem, size_t count);

namespace Internal {

template <typename F>
struct ScopeExit {
    ScopeExit(F f)
        : f(f) {
    }
    ~ScopeExit() {
        f();
    }
    F f;
};

template <typename F>
ScopeExit<F> MakeScopeExit(F f) {
    return ScopeExit<F>(f);
}

template<class T, class... T_Params>
GG_FORCE_INLINE T* ConstructInPlaceImpl(void* mem, std::true_type, T_Params&&... params) {
    *(std::remove_const_t<T>*)mem = T(std::forward<T_Params>(params)...);
    return (T*)mem;
}

template<class T, class... T_Params>
GG_FORCE_INLINE T* ConstructInPlaceImpl(void* mem, std::false_type, T_Params&&... params) {
    return new(mem) T(std::forward<T_Params>(params)...);
}

template<class T, class... T_Params>
GG_FORCE_INLINE T* ConstructArrayImpl(void* mem, size_t count, std::true_type, T_Params&&... params) {
    T* const result = (T*)mem;
    T const source(std::forward<T_Params>(params)...);
    for (size_t i = 0; i < count; i++) {
        memcpy(result + i, &source, sizeof(T));
    }
    return result;
}

template<class T, class... T_Params>
GG_FORCE_INLINE T* ConstructArrayImpl(void* mem, size_t count, std::false_type, T_Params&&... params) {
    T* const result = (T*)mem;
    for (size_t i = 0; i < count; i++) {
        new(result + i) T(std::forward<T_Params>(params)...);
    }
    return result;
}

template<class T>
GG_FORCE_INLINE T* CopyConstructArrayImpl(void* dest, size_t count, T const* source, std::true_type) {
    return (T*)memcpy(dest, source, count * sizeof(T));
}

template<class T>
GG_FORCE_INLINE T* CopyConstructArrayImpl(void* dest, size_t count, T const* source, std::false_type) {
    T* const result = (T*)dest;
    for (size_t i = 0; i < count; i++) {
        new(result + i) T(source[i]);
    }
    return result;
}

}

template<class T, class... T_Params>
T* ConstructInPlace(void* mem, T_Params&&... params) {
    return Internal::ConstructInPlaceImpl<T>(mem, std::is_trivially_constructible<T, T_Params&&...>(), std::forward<T_Params>(params)...);
}
template<class T, class... T_Params>
T* ReconstructInPlace(T& obj, T_Params&&... params) {
    obj.~T();
    return ConstructInPlace<T>(&obj, std::forward<T_Params>(params)...);
}
template<class T, class... T_Params>
T* ConstructArray(void* mem, size_t count, T_Params&&... params) {
    return Internal::ConstructArrayImpl<T>(mem, count, std::is_trivially_constructible<T, T_Params&&...>(), std::forward<T_Params>(params)...);
}
template<class T>
T* CopyConstructArray(void* dest, size_t count, T const* source) {
    return Internal::CopyConstructArrayImpl<T>(dest, count, source, std::is_trivially_copy_constructible<T>());
}
template<class T>
T* MoveArray(void* dest, size_t count, T* source) {
    auto* const result = (std::remove_const_t<T>*)dest;
    for (size_t i = 0; i < count; i++) {
        ConstructInPlace<T>(result + i, std::move(source[i]));
    }
    return result;
}
template<class T>
void DestroyArray(T* mem, size_t count) {
    for (size_t i = 0; i < count; i++) {
        mem[i].~T();
    }
}

}

#endif
