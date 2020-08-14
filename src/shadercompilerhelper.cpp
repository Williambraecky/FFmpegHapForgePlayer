#include "shadercompilerhelper.h"

#include <cstdio>
#include <iostream>

#include "Renderer/IResourceLoader.h"
#include "OS/Interfaces/IFileSystem.h"

ShaderCompilerHelper::ShaderCompilerHelper()
{
}

bool ShaderCompilerHelper::transpileShader(const std::string& resourceFile,
                                           const std::string& targetFile,
                                           ShaderCompiler::Input& compilerInput)
{
    std::FILE *fp = std::fopen(resourceFile.c_str(), "rb");
    if (!fp)
    {
        return false;
    }
    std::fseek(fp, 0, SEEK_END);
    compilerInput.sourceCode.resize(std::ftell(fp));
    std::rewind(fp);
    std::fread(&(compilerInput.sourceCode[0]), 1, compilerInput.sourceCode.size(), fp);
    std::fclose(fp);
    fp = nullptr;

    ShaderCompiler compiler;
    ShaderCompiler::Result compileResult;
    compileResult = compiler(compilerInput);
    if (!compileResult.success)
    {
        return false;
    }
    fp = std::fopen(targetFile.c_str(), "wb");
    if (!fp)
    {
        return false;
    }
    std::fwrite(&(compileResult.outputCode[0]), 1, compileResult.outputCode.size(), fp);
    std::fclose(fp);
    return true;
}

static inline char getSeparator()
{
#if defined(_WIN32) || defined(_WINDOWS) ||  defined(XBOX)
    return '\\';
#else
    return '/';
#endif
}

bool ShaderCompilerHelper::transpileShaders(const TranspileDesc* descriptions, const uint32_t count, const uint32_t rendererApi)
{
//    fsGetPathFileName();
    ShaderCompiler::Input input;
    std::string fileSuffix = "";
    fsGetResourceDirectory(RD_SHADER_SOURCES);
    if (rendererApi == RENDERER_API_METAL)
    {
        input.entryPoint = "stageMain";
        input.conversionType = ShaderCompiler::GLSL2MSL;
        fileSuffix = ".metal";
    }
    else if (rendererApi == RENDERER_API_D3D11 || rendererApi == RENDERER_API_D3D12)
    {
        input.conversionType = ShaderCompiler::GLSL2HLSL;
    }
    else if (rendererApi == RENDERER_API_VULKAN)
    {
        input.conversionType = ShaderCompiler::GLSL2GLSL;
    }
    for (uint32_t i = 0; i < count; i++)
    {
        const char *fileName = strrchr(descriptions[i].m_name.c_str(), getSeparator());
        if (!fileName)
        {
            fileName = descriptions[i].m_name.c_str();
        }
        char c_fileOutput[FS_MAX_PATH] = {0};
        fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_SOURCES), fileName, c_fileOutput);
        if (fileSuffix.size() > 0)
        {
            fsAppendPathExtension(c_fileOutput, fileSuffix.c_str(), c_fileOutput);
        }
        std::string fileOutput = c_fileOutput;
        input.stage = descriptions[i].m_stage;
        if (!transpileShader(descriptions[i].m_name,
                              fileOutput,
                              input))
        {
            return false;
        }
    }
    return true;
}
