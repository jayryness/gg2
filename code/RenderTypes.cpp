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

RenderPipelineDescription RenderPipelineDescription::MakeFromData(RenderPipelineDescriptionData&& data) {
    RenderPipelineDescription result = {
        {data.vertexBytecode},
        {data.fragmentBytecode},
        CombineHash32(
            BufferHash32(data.vertexBytecode),
            BufferHash32(data.fragmentBytecode)),
        std::move(data)
    };
    result.hash = BufferHash32(result.vertexStage.shaderBytecode.begin(), result.vertexStage.shaderBytecode.count());
    result.hash = CombineHash32(result.hash, BufferHash32(result.fragmentStage.shaderBytecode.begin(), result.fragmentStage.shaderBytecode.count()));
    return result;
}

RenderPipelineDescription RenderPipelineDescription::LoadFromFiles(char const* vertexFilename, char const* fragmentFilename) {
    RenderPipelineDescriptionData data = {LoadEntireFile(vertexFilename), LoadEntireFile(fragmentFilename)};
    return MakeFromData(std::move(data));
}

}
