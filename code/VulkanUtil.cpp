#include "VulkanUtil.h"

namespace gg {
namespace vk {

constexpr unsigned EnumerateFormat(RenderFormat::Layout layout, RenderFormat::BitDepth bitDepth, RenderFormat::Type type) {
    return (unsigned)layout * (unsigned)RenderFormat::Type::cEnumCount * (unsigned)RenderFormat::BitDepth::cEnumCount
        + (unsigned)bitDepth * (unsigned)RenderFormat::Type::cEnumCount
        + (unsigned)type;
}

VkFormat ConvertFormat(RenderFormat const& format) {

    switch(EnumerateFormat(format.layout, format.bitDepth, format.type)) {

    #define F(layout, bitDepth, type) EnumerateFormat(RenderFormat::Layout::c##layout, RenderFormat::BitDepth::c##bitDepth, RenderFormat::Type::c##type)
    case F(RG,      4,          Unorm):         return VK_FORMAT_R4G4_UNORM_PACK8;
    case F(RGBA,    4,          Unorm):         return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
    case F(BGRA,    4,          Unorm):         return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
    case F(RGB,     5_6_5,      Unorm):         return VK_FORMAT_R5G6B5_UNORM_PACK16;
    case F(BGR,     5_6_5,      Unorm):         return VK_FORMAT_B5G6R5_UNORM_PACK16;
    case F(RGBA,    5_5_5_1,    Unorm):         return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
    case F(BGRA,    5_5_5_1,    Unorm):         return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
    case F(ARGB,    1_5_5_5,    Unorm):         return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
    case F(R,       8,          Unorm):         return VK_FORMAT_R8_UNORM;
    case F(R,       8,          Snorm):         return VK_FORMAT_R8_SNORM;
    case F(R,       8,          Uint):          return VK_FORMAT_R8_USCALED;
    case F(R,       8,          Int):           return VK_FORMAT_R8_SSCALED;
    case F(R,       8,          Srgb):          return VK_FORMAT_R8_SRGB;
    case F(RG,      8,          Unorm):         return VK_FORMAT_R8G8_UNORM;
    case F(RG,      8,          Snorm):         return VK_FORMAT_R8G8_SNORM;
    case F(RG,      8,          Uint):          return VK_FORMAT_R8G8_USCALED;
    case F(RG,      8,          Int):           return VK_FORMAT_R8G8_SSCALED;
    case F(RG,      8,          Srgb):          return VK_FORMAT_R8G8_SRGB;
    case F(RGB,     8,          Unorm):         return VK_FORMAT_R8G8B8_UNORM;
    case F(RGB,     8,          Snorm):         return VK_FORMAT_R8G8B8_SNORM;
    case F(RGB,     8,          Uint):          return VK_FORMAT_R8G8B8_USCALED;
    case F(RGB,     8,          Int):           return VK_FORMAT_R8G8B8_SSCALED;
    case F(RGB,     8,          Srgb):          return VK_FORMAT_R8G8B8_SRGB;
    case F(BGR,     8,          Unorm):         return VK_FORMAT_B8G8R8_UNORM;
    case F(BGR,     8,          Snorm):         return VK_FORMAT_B8G8R8_SNORM;
    case F(BGR,     8,          Uint):          return VK_FORMAT_B8G8R8_USCALED;
    case F(BGR,     8,          Int):           return VK_FORMAT_B8G8R8_SSCALED;
    case F(BGR,     8,          Srgb):          return VK_FORMAT_B8G8R8_SRGB;
    case F(RGBA,    8,          Unorm):         return VK_FORMAT_R8G8B8A8_UNORM;
    case F(RGBA,    8,          Snorm):         return VK_FORMAT_R8G8B8A8_SNORM;
    case F(RGBA,    8,          Uint):          return VK_FORMAT_R8G8B8A8_USCALED;
    case F(RGBA,    8,          Int):           return VK_FORMAT_R8G8B8A8_SSCALED;
    case F(RGBA,    8,          Srgb):          return VK_FORMAT_R8G8B8A8_SRGB;
    case F(BGRA,    8,          Unorm):         return VK_FORMAT_B8G8R8A8_UNORM;
    case F(BGRA,    8,          Snorm):         return VK_FORMAT_B8G8R8A8_SNORM;
    case F(BGRA,    8,          Uint):          return VK_FORMAT_B8G8R8A8_USCALED;
    case F(BGRA,    8,          Int):           return VK_FORMAT_B8G8R8A8_SSCALED;
    case F(BGRA,    8,          Srgb):          return VK_FORMAT_B8G8R8A8_SRGB;
    case F(R,       16,         Unorm):         return VK_FORMAT_R16_UNORM;
    case F(R,       16,         Snorm):         return VK_FORMAT_R16_SNORM;
    case F(R,       16,         Uint):          return VK_FORMAT_R16_USCALED;
    case F(R,       16,         Int):           return VK_FORMAT_R16_SSCALED;
    case F(R,       16,         Float):         return VK_FORMAT_R16_SFLOAT;
    case F(RG,      16,         Unorm):         return VK_FORMAT_R16G16_UNORM;
    case F(RG,      16,         Snorm):         return VK_FORMAT_R16G16_SNORM;
    case F(RG,      16,         Uint):          return VK_FORMAT_R16G16_USCALED;
    case F(RG,      16,         Int):           return VK_FORMAT_R16G16_SSCALED;
    case F(RG,      16,         Float):         return VK_FORMAT_R16G16_SFLOAT;
    case F(RGB,     16,         Unorm):         return VK_FORMAT_R16G16B16_UNORM;
    case F(RGB,     16,         Snorm):         return VK_FORMAT_R16G16B16_SNORM;
    case F(RGB,     16,         Uint):          return VK_FORMAT_R16G16B16_USCALED;
    case F(RGB,     16,         Int):           return VK_FORMAT_R16G16B16_SSCALED;
    case F(RGB,     16,         Float):         return VK_FORMAT_R16G16B16_SFLOAT;
    case F(RGBA,    16,         Unorm):         return VK_FORMAT_R16G16B16A16_UNORM;
    case F(RGBA,    16,         Snorm):         return VK_FORMAT_R16G16B16A16_SNORM;
    case F(RGBA,    16,         Uint):          return VK_FORMAT_R16G16B16A16_USCALED;
    case F(RGBA,    16,         Int):           return VK_FORMAT_R16G16B16A16_SSCALED;
    case F(RGBA,    16,         Float):         return VK_FORMAT_R16G16B16A16_SFLOAT;
    case F(R,       32,         Float):         return VK_FORMAT_R32_SFLOAT;
    case F(RG,      32,         Float):         return VK_FORMAT_R32G32_SFLOAT;
    case F(RGB,     32,         Float):         return VK_FORMAT_R32G32B32_SFLOAT;
    case F(RGBA,    32,         Float):         return VK_FORMAT_R32G32B32A32_SFLOAT;
    case F(D,       16,         Unorm):         return VK_FORMAT_D16_UNORM;
    case F(D,       32,         Float):         return VK_FORMAT_D32_SFLOAT;
    case F(DS,      24_8,       Unorm_Uint):    return VK_FORMAT_D24_UNORM_S8_UINT;
    case F(DS,      32_8,       Float_Uint):    return VK_FORMAT_D32_SFLOAT_S8_UINT;
    case F(BC1,     Block,      Unorm):         return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    case F(BC1,     Block,      Srgb):          return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
    case F(BC1A,    Block,      Unorm):         return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case F(BC1A,    Block,      Srgb):          return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case F(BC2,     Block,      Unorm):         return VK_FORMAT_BC2_UNORM_BLOCK;
    case F(BC2,     Block,      Srgb):          return VK_FORMAT_BC2_SRGB_BLOCK;
    case F(BC3,     Block,      Unorm):         return VK_FORMAT_BC3_UNORM_BLOCK;
    case F(BC3,     Block,      Srgb):          return VK_FORMAT_BC3_SRGB_BLOCK;
    case F(BC4,     Block,      Unorm):         return VK_FORMAT_BC4_UNORM_BLOCK;
    case F(BC4,     Block,      Snorm):         return VK_FORMAT_BC4_SNORM_BLOCK;
    case F(BC5,     Block,      Unorm):         return VK_FORMAT_BC5_UNORM_BLOCK;
    case F(BC5,     Block,      Snorm):         return VK_FORMAT_BC5_SNORM_BLOCK;
    case F(BC6,     Block,      Float):         return VK_FORMAT_BC6H_SFLOAT_BLOCK;
    case F(BC7,     Block,      Unorm):         return VK_FORMAT_BC7_UNORM_BLOCK;
    case F(BC7,     Block,      Srgb):          return VK_FORMAT_BC7_SRGB_BLOCK;
    #undef F

    default: break;
    }

    assert(false);
    return VK_FORMAT_UNDEFINED;
}

}
}
