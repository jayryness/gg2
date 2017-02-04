#pragma once
#ifndef GG_HASH_H
#define GG_HASH_H

#include "MiscUtil.h"
#include "Span.h"

namespace gg {

template<class T_Key>
struct HashKeyTraitsDefault {
    template<class T_AltKey> static bool IsNull(T_AltKey const& key) {
        return !key;
    }
    template<class T_AltKey> static uint32_t Hash(T_AltKey const& key) {
        return Hash32(key);
    }
    template<class T_AltKey> static bool Equals(T_Key const& a, T_AltKey const& b) {
        return a == b;
    }
};

struct TrivialHashKeyTraits {
    template<class T_Key> static bool IsNull(T_Key key) {
        return !key;
    }
    template<class T_Key> static uint32_t Hash(T_Key const& key) {
        assert(key);
        return key;
    }
    template<class T_Key> static bool Equals(T_Key const& a, T_Key const& b) {
        return a == b;
    }
};

inline uint32_t MixBits32(uint64_t x) {
    uint32_t result = (x * 11400714819323198549LL) >> 32;
    assert(result != 0);
    return result;
}

inline uint32_t Hash32(uint64_t x) {
    return MixBits32(x);
}

template<class T>
inline uint32_t Hash32(T* x) {
    return MixBits32((intptr_t)x);
}

inline uint32_t CombineHash32(uint32_t a, uint32_t b) {
    uint32_t result = a;
    result ^= b + 0x9e3779b9 + (a << 6) + (a >> 2);   // from boost... can we do better? todo
    return result;
}

uint32_t BufferHash32(void const* buffer, size_t size, uint32_t seed = 0xdecafbad);
uint32_t BufferHash32(Span<int8_t const> data, uint32_t seed = 0xdecafbad);
uint32_t StringHash32(char const* str, uint32_t seed = 0xdecafbad);

struct HashedString {
    HashedString()
        : hash(0)
        , chars(nullptr) {
    }
    HashedString(char const* chars)
        : hash(StringHash32(chars))
        , chars(chars) {
    }
    bool operator==(HashedString const& b) const {
        return hash == b.hash && strcmp(chars, b.chars) == 0;
    }
    bool operator!() const {
        return chars == nullptr;
    }
    friend uint32_t Hash32(HashedString const& x) {
        return x.hash;
    }

    uint32_t hash;
    char const* chars;
};

GG_NO_INLINE inline uint32_t BufferHash32(void const* buffer, size_t size, uint32_t seed) {
    // Murmurhash3 with cheaper final mixing

    int8_t const*const data = (int8_t*)buffer;
    unsigned const length = (unsigned)size;
    int const blockCount = length / 4;

    uint32_t h1 = seed;

    uint32_t const c1 = 0xcc9e2d51;
    uint32_t const c2 = 0x1b873593;

    uint32_t const* blocks = (uint32_t const*)(data + blockCount * 4);

    for (int i = -blockCount; i; i++) {
        uint32_t k1 = blocks[i];
        k1 *= c1;
        k1 = RotateBitsLeft(k1, 15);
        k1 *= c2;
        h1 ^= k1;
        h1 = RotateBitsLeft(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

    uint8_t const* tail = (uint8_t const*)(data + blockCount * 4);

    uint32_t k1 = 0;

    switch (length & 3) {
    case 3: k1 ^= tail[2] << 16;
    case 2: k1 ^= tail[1] << 8;
    case 1: k1 ^= tail[0];
        k1 *= c1;
        k1 = RotateBitsLeft(k1, 15);
        k1 *= c2;
        h1 ^= k1;
    };

    return MixBits32(h1 ^ length);
}

inline uint32_t StringHash32(char const* data, uint32_t seed) {
    return BufferHash32(data, strlen(data), seed);
}

inline uint32_t BufferHash32(Span<int8_t const> span, uint32_t seed) {
    return BufferHash32(span.begin(), span.count(), seed);
}

}

#endif
