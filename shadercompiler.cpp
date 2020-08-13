#include "shadercompiler.h"
#include "shadercompilerdefaults.h"

//From glslang
#include "glslang/Public/ShaderLang.h"
#include "glslang/SPIRV/SpvTools.h"
#include "glslang/SPIRV/GlslangToSpv.h"

//From The-Forge
#include "ThirdParty/OpenSource/SPIRV_Cross/spirv_msl.hpp"
#include "ThirdParty/OpenSource/SPIRV_Cross/spirv_glsl.hpp"
#include "ThirdParty/OpenSource/SPIRV_Cross/spirv_hlsl.hpp"

#include <memory>
namespace madam{
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
}

struct GLSLLangProcessRAII
{
    GLSLLangProcessRAII()
    {
        glslang::InitializeProcess();
    }

    ~GLSLLangProcessRAII()
    {
        glslang::FinalizeProcess();
    }
};

//NOTE: all spirv_cross compilers inherit from CompilerGLSL
std::unique_ptr<spirv_cross::CompilerGLSL> createCompiler(ShaderCompiler::ConversionType conversionType, const std::vector<unsigned int>& spirv)
{
    std::unique_ptr<spirv_cross::CompilerGLSL> compiler;

    switch (conversionType)
    {
    case ShaderCompiler::ConversionType::GLSL2GLSL:
    case ShaderCompiler::ConversionType::HLSL2GLSL:
        compiler = madam::make_unique<spirv_cross::CompilerGLSL>(spirv);
        break;
    case ShaderCompiler::ConversionType::GLSL2MSL:
    case ShaderCompiler::ConversionType::HLSL2MSL:
        compiler = madam::make_unique<spirv_cross::CompilerMSL>(spirv);
        break;
    case ShaderCompiler::ConversionType::GLSL2HLSL:
    case ShaderCompiler::ConversionType::HLSL2HLSL:
        compiler = madam::make_unique<spirv_cross::CompilerHLSL>(spirv);
        break;
    }
    return compiler;
}

EShLanguage stageToEShLanguage(ShaderCompiler::ShaderStage stage)
{
    switch (stage)
    {
    case ShaderCompiler::STAGE_VERTEX:
        return EShLangVertex;
    case ShaderCompiler::STAGE_TESSCONTROL:
        return EShLangTessControl;
    case ShaderCompiler::STAGE_TESSEVALUATION:
        return EShLangTessEvaluation;
    case ShaderCompiler::STAGE_GEOMETRY:
        return EShLangGeometry;
    case ShaderCompiler::STAGE_FRAGMENT:
        return EShLangFragment;
    case ShaderCompiler::STAGE_COMPUTE:
        return EShLangCompute;
    case ShaderCompiler::STAGE_RAYGENNV:
        return EShLangRayGenNV;
    case ShaderCompiler::STAGE_INTERSECTNV:
        return EShLangIntersectNV;
    case ShaderCompiler::STAGE_ANYHITNV:
        return EShLangAnyHitNV;
    case ShaderCompiler::STAGE_CLOSESTHITNV:
        return EShLangClosestHitNV;
    case ShaderCompiler::STAGE_MISSNV:
        return EShLangMissNV;
    case ShaderCompiler::STAGE_CALLABLENV:
        return EShLangCallableNV;
    case ShaderCompiler::STAGE_TASKNV:
        return EShLangTaskNV;
    case ShaderCompiler::STAGE_MESHNV:
        return EShLangMeshNV;
    }
    return EShLangVertex; // default stage
}

spv::ExecutionModel stageToExecutionModel(ShaderCompiler::ShaderStage stage)
{
    switch (stage)
    {
    case ShaderCompiler::STAGE_VERTEX:
        return spv::ExecutionModel::ExecutionModelVertex;
    case ShaderCompiler::STAGE_TESSCONTROL:
        return spv::ExecutionModel::ExecutionModelTessellationControl;
    case ShaderCompiler::STAGE_TESSEVALUATION:
        return spv::ExecutionModel::ExecutionModelTessellationEvaluation;
    case ShaderCompiler::STAGE_GEOMETRY:
        return spv::ExecutionModel::ExecutionModelGeometry;
    case ShaderCompiler::STAGE_FRAGMENT:
        return spv::ExecutionModel::ExecutionModelFragment;
    case ShaderCompiler::STAGE_COMPUTE:
        return spv::ExecutionModel::ExecutionModelGLCompute;
    case ShaderCompiler::STAGE_RAYGENNV:
        return spv::ExecutionModel::ExecutionModelRayGenerationNV;
    case ShaderCompiler::STAGE_INTERSECTNV:
        return spv::ExecutionModel::ExecutionModelIntersectionNV;
    case ShaderCompiler::STAGE_ANYHITNV:
        return spv::ExecutionModel::ExecutionModelAnyHitNV;
    case ShaderCompiler::STAGE_CLOSESTHITNV:
        return spv::ExecutionModel::ExecutionModelClosestHitNV;
    case ShaderCompiler::STAGE_MISSNV:
        return spv::ExecutionModel::ExecutionModelMissNV;
    case ShaderCompiler::STAGE_CALLABLENV:
        return spv::ExecutionModel::ExecutionModelCallableNV;
    case ShaderCompiler::STAGE_TASKNV:
        return spv::ExecutionModel::ExecutionModelTaskNV;
    case ShaderCompiler::STAGE_MESHNV:
        return spv::ExecutionModel::ExecutionModelMeshNV;
    }
    return spv::ExecutionModel::ExecutionModelVertex; // default stage
}

glslang::EShSource sourceFromConversionType(ShaderCompiler::ConversionType conversionType)
{
    switch (conversionType)
    {
    case ShaderCompiler::GLSL2GLSL:
    case ShaderCompiler::GLSL2MSL:
    case ShaderCompiler::GLSL2HLSL:
    case ShaderCompiler::GLSL2SPV:
        return glslang::EShSourceGlsl;
    case ShaderCompiler::HLSL2GLSL:
    case ShaderCompiler::HLSL2MSL:
    case ShaderCompiler::HLSL2HLSL:
    case ShaderCompiler::HLSL2SPV:
        return glslang::EShSourceHlsl;
    }
    return glslang::EShSourceNone;
}

bool isGLSLTarget(ShaderCompiler::ConversionType conversionType)
{
    return (conversionType == ShaderCompiler::GLSL2GLSL || conversionType == ShaderCompiler::HLSL2GLSL);
}

bool isHLSLTarget(ShaderCompiler::ConversionType conversionType)
{
    return (conversionType == ShaderCompiler::GLSL2HLSL || conversionType == ShaderCompiler::HLSL2HLSL);
}

bool isMSLTarget(ShaderCompiler::ConversionType conversionType)
{
    return (conversionType == ShaderCompiler::GLSL2MSL || conversionType == ShaderCompiler::HLSL2MSL);
}


ShaderCompiler::Result ShaderCompiler::operator ()(const ShaderCompiler::Input& input)
{
    ShaderCompiler::Result result;
    try
    {
        result = compile(input);
    }
    catch (const std::exception& e)
    {
        result.errors = e.what();
    }
    catch (...)
    {
        result.errors = "Exception while compiling";
    }
    return result;
}

static const char* g_attributes[] = {
    "POSITION",
    "NORMAL",
    "TEXCOORD0",
    "TEXCOORD1",
    "TEXCOORD2",
    "TEXCOORD3",
    "TEXCOORD4",
    "TEXCOORD5",
    "TEXCOORD6",
    "TEXCOORD7",
    "COLOR0",
    "COLOR1",
    "COLOR2",
    "COLOR3",
    "TANGENT",
    "BINORMAL",
    "BLENDINDICES",
    "BLENDWEIGHT"
};

ShaderCompiler::Result ShaderCompiler::compile(const ShaderCompiler::Input& input)
{
    ShaderCompiler::Result result;
    //transform shader to spirv binary
    GLSLLangProcessRAII processRAII;
    glslang::TShader shader(stageToEShLanguage(input.stage));
    const char *source = input.sourceCode.c_str();
    shader.setStrings(&source, 1);
    shader.setAutoMapLocations(true);
    shader.setEnvInput(sourceFromConversionType(input.conversionType), stageToEShLanguage(input.stage), glslang::EShClientVulkan, 100);
    shader.setAutoMapBindings(true);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
    if (!shader.parse(&g_defaultConfiguration, 100, false, EShMsgDefault))
    {
        result.logs = std::string(shader.getInfoLog());
        return result;
    }
    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(EShMsgDefault))
    {
        result.logs = std::string(program.getInfoLog());
        return result;
    }
    //Source code is now validated, we can compile to whatever language we want
    std::vector<unsigned int> spirv;
    spv::SpvBuildLogger logger;
    glslang::SpvOptions spvOptions;
    spvOptions.disableOptimizer = !input.optimize;
    spvOptions.optimizeSize = input.optimize;
    spvOptions.disassemble = input.disassemble;
    spvOptions.validate = input.validate;
    glslang::GlslangToSpv(*program.getIntermediate(stageToEShLanguage(input.stage)), spirv, &logger, &spvOptions);
    if ((input.conversionType == GLSL2SPV || input.conversionType == HLSL2SPV) || input.includeSpv)
    {
        result.spirv = spirv;
        if (input.conversionType == GLSL2SPV || input.conversionType == HLSL2SPV)
        {
            //Conversion stops here
            result.success = 1;
            return result;
        }
    }
    result.logs = logger.getAllMessages();

    auto compiler = createCompiler(input.conversionType, spirv);
    // Set some options.
    spirv_cross::CompilerGLSL::Options options = compiler->get_common_options();
    spirv_cross::ShaderResources ress = compiler->get_shader_resources();
    if (isGLSLTarget(input.conversionType))
    {
        options.enable_420pack_extension = false;
    }
    else if (isHLSLTarget(input.conversionType))
    {
        spirv_cross::CompilerHLSL* hlsl_compiler = (spirv_cross::CompilerHLSL*)compiler.get();
        spirv_cross::CompilerHLSL::Options hlsl_opts = hlsl_compiler->get_hlsl_options();

        hlsl_opts.shader_model = input.outputVersion;
        hlsl_opts.point_size_compat = true;
        hlsl_opts.point_coord_compat = true;

        hlsl_compiler->set_hlsl_options(hlsl_opts);

        uint32_t new_builtin = hlsl_compiler->remap_num_workgroups_builtin();
        if (new_builtin) {
            hlsl_compiler->set_decoration(new_builtin, spv::DecorationDescriptorSet, 0);
            hlsl_compiler->set_decoration(new_builtin, spv::DecorationBinding, 0);
        }
        size_t attr_size = sizeof(g_attributes) / sizeof(g_attributes[0]);/*Number of possible vertex attributes*/
        for (size_t i = 0; i < attr_size; i++) {
            spirv_cross::HLSLVertexAttributeRemap remap = { (uint32_t)i, g_attributes[i] };
            hlsl_compiler->add_vertex_attribute_remap(remap);
        }
    }
    else if (isMSLTarget(input.conversionType) && input.stage == STAGE_VERTEX)
    {
        //Reset vertex input locations
        for (size_t i = 0; i < ress.stage_inputs.size(); i++) {
            spirv_cross::Resource& res = ress.stage_inputs[i];
            spirv_cross::Bitset mask = compiler->get_decoration_bitset(res.id);

            if (mask.get(spv::DecorationLocation)) {
                compiler->set_decoration(res.id, spv::DecorationLocation, (uint32_t)i);
            }
        }
    }
    if (isMSLTarget(input.conversionType))
    {
        //Remap structures and variables using the structure name
        //Required by the forge
        for (auto& resource : ress.uniform_buffers)
        {
            //Set variable name to structure name
            compiler->set_name(resource.id, resource.name);
            //Add a 0 to the structure's name
            compiler->set_name(resource.base_type_id, resource.name + "0");
        }
    }
    options.es = input.useOpenglES;
    compiler->set_common_options(options);
    compiler->rename_entry_point("main", input.entryPoint.c_str(), stageToExecutionModel(input.stage));

    result.outputCode = compiler->compile();
    result.success = true;
    return result;
}
