#include "Shaders.hxx"

layout (binding = 0) sampler linearSampler;
layout (binding = 1) Texture2D<uint> source;

struct Interpolators {
    float4 position : SV_Position;
    float2 uv;
};

VERTEX_PROGRAM(
    struct Vertex {
        uint vertexId : SV_VertexId;
    };

    Interpolators main(Vertex input) {
        uint2 position = uint2(input.vertexId << 1, input.vertexId) & 2;

        Interpolators result;
        result.position = float4(position, 0., 1.);
        result.uv = position;
        return result;
    }
)

FRAGMENT_PROGRAM(
    struct Fragment {
        float4 color : SV_Color0;
    };

    Fragment main(Interpolators input) {
        Fragment result;
        result.color = source.SampleLevel(linearSampler, input.uv, 0);
        return result;
    }
)
