#pragma once
#ifndef GG_SIMD_H
#define GG_SIMD_H

namespace gg {

class Float4 {

public:
    template<unsigned N>
    static Float4 Load(void* source) {
        memcpy(r_, source, N*sizeof(float));
    }
    template<unsigned N>
    static Float4 LoadAligned(void* source) {
        memcpy(r_, source, N*sizeof(float));
    }

    Float4(float x, float y, float z = 0.f, float w = 0.f)
        : r_{x, y, z, w} {
    }

    Float4& operator+=(Float4 const& b) {
        for (unsigned i = 0; i < 4; i++)
            r_[i] += b.r_[i];
        return *this;
    }
    Float4& operator-=(Float4 const& b) {
        for (unsigned i = 0; i < 4; i++)
            r_[i] -= b.r_[i];
        return *this;
    }
    Float4& operator*=(Float4 const& b) {
        for (unsigned i = 0; i < 4; i++)
            r_[i] *= b.r_[i];
        return *this;
    }
    Float4& operator/=(Float4 const& b) {
        for (unsigned i = 0; i < 4; i++)
            r_[i] /= b.r_[i];
        return *this;
    }

    float x() const {
        return r_[0];
    }
    float y() const {
        return r_[1];
    }
    float z() const {
        return r_[2];
    }
    float w() const {
        return r_[3];
    }

    template<unsigned N>
    void store(void* dest) const {
        memcpy(dest, r_, N*sizeof(float));
    }
    template<unsigned N>
    void storeAligned(GG_ALIGN_16 void* dest) const {
        memcpy(dest, r_, N*sizeof(float));
    }

private:

    GG_ALIGN_16 float r_[4];
};

Float4 operator+(Float4 const& a, Float4 const& b) {
    Float4 result = a;
    return result += b;
}

Float4 operator-(Float4 const& a, Float4 const& b) {
    Float4 result = a;
    return result -= b;
}

Float4 operator*(Float4 const& a, Float4 const& b) {
    Float4 result = a;
    return result *= b;
}

Float4 operator/(Float4 const& a, Float4 const& b) {
    Float4 result = a;
    return result /= b;
}

}

#endif
