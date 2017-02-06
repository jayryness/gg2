#include "Shaders.hxx"

struct Interpolators {
    float4 position : SV_Position;
    float2 uv : layout (location = 0);
};

VERTEX_PROGRAM(
    layout (binding = 3) Buffer<float2> positions;

    struct Vertex {
        uint spriteId : layout (location = 0);
        uint vertexId : SV_VertexId;
    };

    Interpolators main(Vertex input) {
        float2 position = positions[int(input.spriteId)];
        uint corner = input.vertexId % 4;
        position += float2(corner & 1, corner >> 1) * .05;  // todo

        Interpolators result;
        result.position = float4(position, 0., 1.);
        result.uv = float2(corner & 1, corner >> 1);
        return result;
    }
)

FRAGMENT_PROGRAM(
    layout (binding = 0) sampler linearSampler;
    layout (binding = 1) Buffer<float4> clut;
    layout (binding = 2) Texture2D<uint4> texels;

    struct Fragment {
        float4 color : SV_Color0;
        //float depth : SV_DepthLessEqual;
    };

    Fragment main(Interpolators input) {
        Fragment result;
        uint4 indices = texels.GatherRed(linearSampler, input.uv);
        result.color = clut[int(indices.x)];
        //result.depth = result.color.a < 1. ? 0. : input.position.z;
        return result;
    }
)
