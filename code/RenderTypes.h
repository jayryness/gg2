#pragma once
#ifndef GG_RENDERTYPES_H
#define GG_RENDERTYPES_H

#define GG_RENDERFORMAT(layout, bitDepth, type) \
    RenderFormat{gg::RenderFormat::Layout::layout, gg::RenderFormat::BitDepth::bitDepth, gg::RenderFormat::Type::type}

#include "Array.h"

namespace gg {

struct RenderFormat {
    enum class Layout {
        cUnknown = 0,
        cRGBA,
        cBGRA,
        cARGB,
        cABGR,
        cRGB,
        cBGR,
        cRG,
        cR,
        cD,
        cDS,
        cBC1,
        cBC1A,
        cBC2,
        cBC3,
        cBC4,
        cBC5,
        cBC6,
        cBC7,
        cEnumCount
    };

    enum class BitDepth {
        cUnknown = 0,
        c4,
        c8,
        c16,
        c32,
        c24_8,
        c32_8,
        c2_10_10_10,
        c5_5_5_1,
        c1_5_5_5,
        c5_6_5,
        cBlock,
        cEnumCount
    };

    enum class Type {
        cUnknown = 0,
        cSrgb,
        cUnorm,
        cSnorm,
        cUint,
        cInt,
        cFloat,
        cUnorm_Uint,
        cFloat_Uint,
        cEnumCount
    };

    bool isDepth() const {
        return (layout == Layout::cD) | (layout == Layout::cDS);
    }

    Layout layout;
    BitDepth bitDepth;
    Type type;
};

struct RenderBlueprintDescriptionData {
};

enum class RenderLoadOp : unsigned {
    cLoad,
    cClear,
    cDontCare
};

enum class RenderAttachmentUsage : unsigned {
    cInput,
    cColor,
    cDepth
};

struct RenderBlueprintDescription {
    using TargetId = unsigned;
    using PassId = unsigned;

    struct Target {
        RenderFormat format;
        RenderLoadOp loadOp;
        TargetId id;
    };

    struct Pass {
        PassId id;
    };

    struct Attachment {
        PassId passId;
        TargetId targetId;
        RenderAttachmentUsage usage;
    };

    struct Dependency {
        PassId src;
        PassId dst;
    };

    Span<Target const> targets;
    Span<Pass const> passes;
    Span<Attachment const> attachments;
    Span<Dependency const> dependencies;
};

struct RenderPipelineDescriptionData {
    Array<int8_t> vertexBytecode;
    Array<int8_t> fragmentBytecode;
};

struct RenderPipelineDescription {
    static RenderPipelineDescription LoadFromFiles(char const* vertexFilename, char const* fragmentFilename);
    static RenderPipelineDescription MakeFromData(RenderPipelineDescriptionData&& data);

    struct Stage {
        Span<int8_t const> shaderBytecode;
    };

    Stage vertexStage;
    Stage fragmentStage;
    RenderPipelineDescriptionData heldData;
};

uint32_t Hash32(RenderPipelineDescription const& pipelineDescription);

}

#endif
