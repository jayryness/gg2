#define GG_FORCE_INLINE __forceinline
#define GG_NO_INLINE __declspec(noinline)
#define GG_ALIGN_16 __declspec(align(16))

namespace gg {

inline unsigned CountLeadingZeroBits(uint32_t bits) {
    //return __lzcnt(bits);
    unsigned long count;
    return 31 - (_BitScanReverse(&count, bits) == 0 ? -1 : count);
}

inline unsigned CountLeadingZeroBits(uint64_t bits) {
    //return (unsigned)__lzcnt64(bits);
    unsigned long count;
    return 63 - (_BitScanReverse64(&count, bits) == 0 ? -1 : count);
}

inline unsigned CountNonzeroBits(uint32_t bits) {
    return __popcnt(bits);
}

inline unsigned CountNonzeroBits(uint64_t bits) {
    return (unsigned)__popcnt64(bits);
}

inline uint32_t RotateBitsLeft(uint32_t bits, unsigned shift) {
    return _rotl(bits, shift);
}

inline uint64_t RotateBitsLeft(uint64_t bits, unsigned shift) {
    return _rotl64(bits, shift);
}

inline uint32_t RotateBitsRight(uint32_t bits, unsigned shift) {
    return _rotr(bits, shift);
}

inline uint64_t RotateBitsRight(uint64_t bits, unsigned shift) {
    return _rotr64(bits, shift);
}

}
