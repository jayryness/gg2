#include "RenderTypes.h"
#include "Hash.h"
#include <fstream>

namespace gg {

inline Array<int8_t> LoadEntireFile(char const* filename) {
    std::ifstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }
    Array<int8_t> data;
    data.setCount(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read((char*)data.begin(), data.count());
    return data;
}

uint32_t Hash32(RenderPipelineDescription const& pipelineDescription) {
    uint32_t result = BufferHash32(pipelineDescription.vertexStage.shaderBytecode);
    result = CombineHash32(result, BufferHash32(pipelineDescription.fragmentStage.shaderBytecode));
    return result;
}

RenderPipelineDescription RenderPipelineDescription::MakeFromData(RenderPipelineDescriptionData&& data) {
    RenderPipelineDescription result = {
        {data.vertexBytecode},
        {data.fragmentBytecode},
        std::move(data)
    };
    return result;
}

RenderPipelineDescription RenderPipelineDescription::LoadFromFiles(char const* vertexFilename, char const* fragmentFilename) {
    RenderPipelineDescriptionData data = {LoadEntireFile(vertexFilename), LoadEntireFile(fragmentFilename)};
    return MakeFromData(std::move(data));
}

}
