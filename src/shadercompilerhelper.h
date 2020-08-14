#ifndef SHADERCOMPILERHELPER_H
#define SHADERCOMPILERHELPER_H

#include "shadercompiler.h"

class ShaderCompilerHelper
{
public:
    ShaderCompilerHelper();

    struct TranspileDesc
    {
        std::string m_name;
        ShaderCompiler::ShaderStage m_stage;
    };

    bool transpileShaders(const TranspileDesc* descriptions, const uint32_t count, const uint32_t rendererApi = 0);

private:
    bool transpileShader(const std::string&, const std::string&, ShaderCompiler::Input&);
};

#endif // SHADERCOMPILERHELPER_H
