#ifndef SHADERCOMPILER_H
#define SHADERCOMPILER_H

#include <string>
#include <vector>

class ShaderCompiler
{
public:

    enum ConversionType
    {
        GLSL2GLSL,
        GLSL2MSL,
        GLSL2HLSL,
        GLSL2SPV,
        HLSL2GLSL, //NOTE: HLSL2X is not complete on glsl's part
        HLSL2MSL,
        HLSL2HLSL,
        HLSL2SPV,
    };

    enum ShaderStage //NOTE: adding a new stage here requires to change stageToEShLanguage and stageToExecutionModel
    {
        STAGE_VERTEX,
        STAGE_TESSCONTROL,
        STAGE_TESSEVALUATION,
        STAGE_GEOMETRY,
        STAGE_FRAGMENT,
        STAGE_COMPUTE,
        STAGE_RAYGENNV,
        STAGE_INTERSECTNV,
        STAGE_ANYHITNV,
        STAGE_CLOSESTHITNV,
        STAGE_MISSNV,
        STAGE_CALLABLENV,
        STAGE_TASKNV,
        STAGE_MESHNV
    };

    struct Result
    {
        bool success = false;
        std::string logs;
        std::string errors;
        std::string outputCode;
        std::vector<unsigned int> spirv;
    };

    struct Input
    {
        std::string sourceCode;
        std::string entryPoint;
        ShaderStage stage = STAGE_VERTEX;
        ConversionType conversionType = GLSL2GLSL;
        bool includeSpv = false; //defaults to true when using X2SPV
        bool optimize = true;
        bool optimizeSize = false;
        bool disassemble = false;
        bool validate = true;
        int outputVersion = 350;
        bool useOpenglES = false;
    };

    Result operator()(const Input&);
    Result compile(const Input&);
};




#endif // SHADERCOMPILER_H
