#include "Shaders.hxx"

sampler linearSampler : register(s0);

Texture2D<float4> clut : register(t0);
Texture2D<uint> texels : register(t1);
Buffer<float2> positions : register(t2);

struct Interpolators {
    float4 position : SV_Position;
    float2 uv;
};

VERTEX_PROGRAM(
    struct Vertex {
        uint spriteId : SpriteId;
        uint vertexId : SV_VertexId;
    };

    Interpolators main(Vertex input) {
        float2 position = positions[input.spriteId];
        uint corner = input.vertexId % 4;

        Interpolators result;
        result.position = float4(position, 0., 1.);
        result.uv = float2(corner & 1, corner >> 1);
        return result;
    }
)

FRAGMENT_PROGRAM(
    struct Fragment {
        float4 color : SV_Color0;
        //float depth : SV_DepthLessEqual;
    };

    Fragment main(Interpolators input) {
        Fragment result;
        uint4 indices = texels.GatherRed(linearSampler, input.uv);
        result.color = clut[indices.x];
        //result.depth = result.color.a < 1. ? 0. : input.position.z;
        return result;
    }
)
