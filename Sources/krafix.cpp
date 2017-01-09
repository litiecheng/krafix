//
//Copyright (C) 2002-2005  3Dlabs Inc. Ltd.
//Copyright (C) 2013 LunarG, Inc.
//
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions
//are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//POSSIBILITY OF SUCH DAMAGE.
//

// this only applies to the standalone wrapper, not the front end in general

#include "../glslang/StandAlone/Worklist.h"
#include "./../glslang/Include/ShHandle.h"
#include "./../glslang/Include/revision.h"
#include "./../glslang/Public/ShaderLang.h"
#include "../SPIRV/GlslangToSpv.h"
#include "../SPIRV/GLSL.std.450.h"
#include "../SPIRV/doc.h"
#include "../SPIRV/disassemble.h"

#include "SpirVTranslator.h"
#include "GlslTranslator.h"
#include "GlslTranslator2.h"
#include "HlslTranslator.h"
#include "HlslTranslator2.h"
#include "AgalTranslator.h"
#include "MetalTranslator.h"
#include "MetalTranslator2.h"
#include "VarListTranslator.h"
#include "JavaScriptTranslator.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sstream>

#include "../glslang//OSDependent/osinclude.h"

extern "C" {
	SH_IMPORT_EXPORT void ShOutputHtml();
}

// Command-line options
enum TOptions {
	EOptionNone = 0x0000,
	EOptionIntermediate = 0x0001,
	EOptionSuppressInfolog = 0x0002,
	EOptionMemoryLeakMode = 0x0004,
	EOptionRelaxedErrors = 0x0008,
	EOptionGiveWarnings = 0x0010,
	EOptionLinkProgram = 0x0020,
	EOptionMultiThreaded = 0x0040,
	EOptionDumpConfig = 0x0080,
	EOptionDumpReflection = 0x0100,
	EOptionSuppressWarnings = 0x0200,
	EOptionDumpVersions = 0x0400,
	EOptionSpv = 0x0800,
	EOptionHumanReadableSpv = 0x1000,
	EOptionVulkanRules = 0x2000,
	EOptionDefaultDesktop = 0x4000,
	EOptionOutputPreprocessed = 0x8000,
};

//
// Return codes from main/exit().
//
enum TFailCode {
	ESuccess = 0,
	EFailUsage,
	EFailCompile,
	EFailLink,
	EFailCompilerCreate,
	EFailThreadCreate,
	EFailLinkerCreate
};

//
// Forward declarations.
//
EShLanguage FindLanguage(const std::string& name);
void CompileFile(const char* fileName, ShHandle);
void usage();
void FreeFileData(char** data);
char** ReadFileData(const char* fileName);
void InfoLogMsg(const char* msg, const char* name, const int num);

// Globally track if any compile or link failure.
bool CompileFailed = false;
bool LinkFailed = false;

static bool quiet = false;

// Use to test breaking up a single shader file into multiple strings.
// Set in ReadFileData().
int NumShaderStrings;

TBuiltInResource Resources;
std::string ConfigFile;

//
// These are the default resources for TBuiltInResources, used for both
//  - parsing this string for the case where the user didn't supply one
//  - dumping out a template for user construction of a config file
//
const char* DefaultConfig =
"MaxLights 32\n"
"MaxClipPlanes 6\n"
"MaxTextureUnits 32\n"
"MaxTextureCoords 32\n"
"MaxVertexAttribs 64\n"
"MaxVertexUniformComponents 4096\n"
"MaxVaryingFloats 64\n"
"MaxVertexTextureImageUnits 32\n"
"MaxCombinedTextureImageUnits 80\n"
"MaxTextureImageUnits 32\n"
"MaxFragmentUniformComponents 4096\n"
"MaxDrawBuffers 32\n"
"MaxVertexUniformVectors 128\n"
"MaxVaryingVectors 8\n"
"MaxFragmentUniformVectors 16\n"
"MaxVertexOutputVectors 16\n"
"MaxFragmentInputVectors 15\n"
"MinProgramTexelOffset -8\n"
"MaxProgramTexelOffset 7\n"
"MaxClipDistances 8\n"
"MaxComputeWorkGroupCountX 65535\n"
"MaxComputeWorkGroupCountY 65535\n"
"MaxComputeWorkGroupCountZ 65535\n"
"MaxComputeWorkGroupSizeX 1024\n"
"MaxComputeWorkGroupSizeY 1024\n"
"MaxComputeWorkGroupSizeZ 64\n"
"MaxComputeUniformComponents 1024\n"
"MaxComputeTextureImageUnits 16\n"
"MaxComputeImageUniforms 8\n"
"MaxComputeAtomicCounters 8\n"
"MaxComputeAtomicCounterBuffers 1\n"
"MaxVaryingComponents 60\n"
"MaxVertexOutputComponents 64\n"
"MaxGeometryInputComponents 64\n"
"MaxGeometryOutputComponents 128\n"
"MaxFragmentInputComponents 128\n"
"MaxImageUnits 8\n"
"MaxCombinedImageUnitsAndFragmentOutputs 8\n"
"MaxCombinedShaderOutputResources 8\n"
"MaxImageSamples 0\n"
"MaxVertexImageUniforms 0\n"
"MaxTessControlImageUniforms 0\n"
"MaxTessEvaluationImageUniforms 0\n"
"MaxGeometryImageUniforms 0\n"
"MaxFragmentImageUniforms 8\n"
"MaxCombinedImageUniforms 8\n"
"MaxGeometryTextureImageUnits 16\n"
"MaxGeometryOutputVertices 256\n"
"MaxGeometryTotalOutputComponents 1024\n"
"MaxGeometryUniformComponents 1024\n"
"MaxGeometryVaryingComponents 64\n"
"MaxTessControlInputComponents 128\n"
"MaxTessControlOutputComponents 128\n"
"MaxTessControlTextureImageUnits 16\n"
"MaxTessControlUniformComponents 1024\n"
"MaxTessControlTotalOutputComponents 4096\n"
"MaxTessEvaluationInputComponents 128\n"
"MaxTessEvaluationOutputComponents 128\n"
"MaxTessEvaluationTextureImageUnits 16\n"
"MaxTessEvaluationUniformComponents 1024\n"
"MaxTessPatchComponents 120\n"
"MaxPatchVertices 32\n"
"MaxTessGenLevel 64\n"
"MaxViewports 16\n"
"MaxVertexAtomicCounters 0\n"
"MaxTessControlAtomicCounters 0\n"
"MaxTessEvaluationAtomicCounters 0\n"
"MaxGeometryAtomicCounters 0\n"
"MaxFragmentAtomicCounters 8\n"
"MaxCombinedAtomicCounters 8\n"
"MaxAtomicCounterBindings 1\n"
"MaxVertexAtomicCounterBuffers 0\n"
"MaxTessControlAtomicCounterBuffers 0\n"
"MaxTessEvaluationAtomicCounterBuffers 0\n"
"MaxGeometryAtomicCounterBuffers 0\n"
"MaxFragmentAtomicCounterBuffers 1\n"
"MaxCombinedAtomicCounterBuffers 1\n"
"MaxAtomicCounterBufferSize 16384\n"
"MaxTransformFeedbackBuffers 4\n"
"MaxTransformFeedbackInterleavedComponents 64\n"
"MaxCullDistances 8\n"
"MaxCombinedClipAndCullDistances 8\n"
"MaxSamples 4\n"

"nonInductiveForLoops 1\n"
"whileLoops 1\n"
"doWhileLoops 1\n"
"generalUniformIndexing 1\n"
"generalAttributeMatrixVectorIndexing 1\n"
"generalVaryingIndexing 1\n"
"generalSamplerIndexing 1\n"
"generalVariableIndexing 1\n"
"generalConstantMatrixVectorIndexing 1\n"
;

//
// Parse either a .conf file provided by the user or the default string above.
//
void ProcessConfigFile()
{
    char** configStrings = 0;
    char* config = 0;
    if (ConfigFile.size() > 0) {
        configStrings = ReadFileData(ConfigFile.c_str());
        if (configStrings)
            config = *configStrings;
        else {
            printf("Error opening configuration file; will instead use the default configuration\n");
            usage();
        }
    }

    if (config == 0) {
        config = new char[strlen(DefaultConfig) + 1];
        strcpy(config, DefaultConfig);
    }

    const char* delims = " \t\n\r";
    const char* token = strtok(config, delims);
    while (token) {
        const char* valueStr = strtok(0, delims);
        if (valueStr == 0 || ! (valueStr[0] == '-' || (valueStr[0] >= '0' && valueStr[0] <= '9'))) {
            printf("Error: '%s' bad .conf file.  Each name must be followed by one number.\n", valueStr ? valueStr : "");
            return;
        }
        int value = atoi(valueStr);

        if (strcmp(token, "MaxLights") == 0)
            Resources.maxLights = value;
        else if (strcmp(token, "MaxClipPlanes") == 0)
            Resources.maxClipPlanes = value;
        else if (strcmp(token, "MaxTextureUnits") == 0)
            Resources.maxTextureUnits = value;
        else if (strcmp(token, "MaxTextureCoords") == 0)
            Resources.maxTextureCoords = value;
        else if (strcmp(token, "MaxVertexAttribs") == 0)
            Resources.maxVertexAttribs = value;
        else if (strcmp(token, "MaxVertexUniformComponents") == 0)
            Resources.maxVertexUniformComponents = value;
        else if (strcmp(token, "MaxVaryingFloats") == 0)
            Resources.maxVaryingFloats = value;
        else if (strcmp(token, "MaxVertexTextureImageUnits") == 0)
            Resources.maxVertexTextureImageUnits = value;
        else if (strcmp(token, "MaxCombinedTextureImageUnits") == 0)
            Resources.maxCombinedTextureImageUnits = value;
        else if (strcmp(token, "MaxTextureImageUnits") == 0)
            Resources.maxTextureImageUnits = value;
        else if (strcmp(token, "MaxFragmentUniformComponents") == 0)
            Resources.maxFragmentUniformComponents = value;
        else if (strcmp(token, "MaxDrawBuffers") == 0)
            Resources.maxDrawBuffers = value;
        else if (strcmp(token, "MaxVertexUniformVectors") == 0)
            Resources.maxVertexUniformVectors = value;
        else if (strcmp(token, "MaxVaryingVectors") == 0)
            Resources.maxVaryingVectors = value;
        else if (strcmp(token, "MaxFragmentUniformVectors") == 0)
            Resources.maxFragmentUniformVectors = value;
        else if (strcmp(token, "MaxVertexOutputVectors") == 0)
            Resources.maxVertexOutputVectors = value;
        else if (strcmp(token, "MaxFragmentInputVectors") == 0)
            Resources.maxFragmentInputVectors = value;
        else if (strcmp(token, "MinProgramTexelOffset") == 0)
            Resources.minProgramTexelOffset = value;
        else if (strcmp(token, "MaxProgramTexelOffset") == 0)
            Resources.maxProgramTexelOffset = value;
        else if (strcmp(token, "MaxClipDistances") == 0)
            Resources.maxClipDistances = value;
        else if (strcmp(token, "MaxComputeWorkGroupCountX") == 0)
            Resources.maxComputeWorkGroupCountX = value;
        else if (strcmp(token, "MaxComputeWorkGroupCountY") == 0)
            Resources.maxComputeWorkGroupCountY = value;
        else if (strcmp(token, "MaxComputeWorkGroupCountZ") == 0)
            Resources.maxComputeWorkGroupCountZ = value;
        else if (strcmp(token, "MaxComputeWorkGroupSizeX") == 0)
            Resources.maxComputeWorkGroupSizeX = value;
        else if (strcmp(token, "MaxComputeWorkGroupSizeY") == 0)
            Resources.maxComputeWorkGroupSizeY = value;
        else if (strcmp(token, "MaxComputeWorkGroupSizeZ") == 0)
            Resources.maxComputeWorkGroupSizeZ = value;
        else if (strcmp(token, "MaxComputeUniformComponents") == 0)
            Resources.maxComputeUniformComponents = value;
        else if (strcmp(token, "MaxComputeTextureImageUnits") == 0)
            Resources.maxComputeTextureImageUnits = value;
        else if (strcmp(token, "MaxComputeImageUniforms") == 0)
            Resources.maxComputeImageUniforms = value;
        else if (strcmp(token, "MaxComputeAtomicCounters") == 0)
            Resources.maxComputeAtomicCounters = value;
        else if (strcmp(token, "MaxComputeAtomicCounterBuffers") == 0)
            Resources.maxComputeAtomicCounterBuffers = value;
        else if (strcmp(token, "MaxVaryingComponents") == 0)
            Resources.maxVaryingComponents = value;
        else if (strcmp(token, "MaxVertexOutputComponents") == 0)
            Resources.maxVertexOutputComponents = value;
        else if (strcmp(token, "MaxGeometryInputComponents") == 0)
            Resources.maxGeometryInputComponents = value;
        else if (strcmp(token, "MaxGeometryOutputComponents") == 0)
            Resources.maxGeometryOutputComponents = value;
        else if (strcmp(token, "MaxFragmentInputComponents") == 0)
            Resources.maxFragmentInputComponents = value;
        else if (strcmp(token, "MaxImageUnits") == 0)
            Resources.maxImageUnits = value;
        else if (strcmp(token, "MaxCombinedImageUnitsAndFragmentOutputs") == 0)
            Resources.maxCombinedImageUnitsAndFragmentOutputs = value;
        else if (strcmp(token, "MaxCombinedShaderOutputResources") == 0)
            Resources.maxCombinedShaderOutputResources = value;
        else if (strcmp(token, "MaxImageSamples") == 0)
            Resources.maxImageSamples = value;
        else if (strcmp(token, "MaxVertexImageUniforms") == 0)
            Resources.maxVertexImageUniforms = value;
        else if (strcmp(token, "MaxTessControlImageUniforms") == 0)
            Resources.maxTessControlImageUniforms = value;
        else if (strcmp(token, "MaxTessEvaluationImageUniforms") == 0)
            Resources.maxTessEvaluationImageUniforms = value;
        else if (strcmp(token, "MaxGeometryImageUniforms") == 0)
            Resources.maxGeometryImageUniforms = value;
        else if (strcmp(token, "MaxFragmentImageUniforms") == 0)
            Resources.maxFragmentImageUniforms = value;
        else if (strcmp(token, "MaxCombinedImageUniforms") == 0)
            Resources.maxCombinedImageUniforms = value;
        else if (strcmp(token, "MaxGeometryTextureImageUnits") == 0)
            Resources.maxGeometryTextureImageUnits = value;
        else if (strcmp(token, "MaxGeometryOutputVertices") == 0)
            Resources.maxGeometryOutputVertices = value;
        else if (strcmp(token, "MaxGeometryTotalOutputComponents") == 0)
            Resources.maxGeometryTotalOutputComponents = value;
        else if (strcmp(token, "MaxGeometryUniformComponents") == 0)
            Resources.maxGeometryUniformComponents = value;
        else if (strcmp(token, "MaxGeometryVaryingComponents") == 0)
            Resources.maxGeometryVaryingComponents = value;
        else if (strcmp(token, "MaxTessControlInputComponents") == 0)
            Resources.maxTessControlInputComponents = value;
        else if (strcmp(token, "MaxTessControlOutputComponents") == 0)
            Resources.maxTessControlOutputComponents = value;
        else if (strcmp(token, "MaxTessControlTextureImageUnits") == 0)
            Resources.maxTessControlTextureImageUnits = value;
        else if (strcmp(token, "MaxTessControlUniformComponents") == 0)
            Resources.maxTessControlUniformComponents = value;
        else if (strcmp(token, "MaxTessControlTotalOutputComponents") == 0)
            Resources.maxTessControlTotalOutputComponents = value;
        else if (strcmp(token, "MaxTessEvaluationInputComponents") == 0)
            Resources.maxTessEvaluationInputComponents = value;
        else if (strcmp(token, "MaxTessEvaluationOutputComponents") == 0)
            Resources.maxTessEvaluationOutputComponents = value;
        else if (strcmp(token, "MaxTessEvaluationTextureImageUnits") == 0)
            Resources.maxTessEvaluationTextureImageUnits = value;
        else if (strcmp(token, "MaxTessEvaluationUniformComponents") == 0)
            Resources.maxTessEvaluationUniformComponents = value;
        else if (strcmp(token, "MaxTessPatchComponents") == 0)
            Resources.maxTessPatchComponents = value;
        else if (strcmp(token, "MaxPatchVertices") == 0)
            Resources.maxPatchVertices = value;
        else if (strcmp(token, "MaxTessGenLevel") == 0)
            Resources.maxTessGenLevel = value;
        else if (strcmp(token, "MaxViewports") == 0)
            Resources.maxViewports = value;
        else if (strcmp(token, "MaxVertexAtomicCounters") == 0)
            Resources.maxVertexAtomicCounters = value;
        else if (strcmp(token, "MaxTessControlAtomicCounters") == 0)
            Resources.maxTessControlAtomicCounters = value;
        else if (strcmp(token, "MaxTessEvaluationAtomicCounters") == 0)
            Resources.maxTessEvaluationAtomicCounters = value;
        else if (strcmp(token, "MaxGeometryAtomicCounters") == 0)
            Resources.maxGeometryAtomicCounters = value;
        else if (strcmp(token, "MaxFragmentAtomicCounters") == 0)
            Resources.maxFragmentAtomicCounters = value;
        else if (strcmp(token, "MaxCombinedAtomicCounters") == 0)
            Resources.maxCombinedAtomicCounters = value;
        else if (strcmp(token, "MaxAtomicCounterBindings") == 0)
            Resources.maxAtomicCounterBindings = value;
        else if (strcmp(token, "MaxVertexAtomicCounterBuffers") == 0)
            Resources.maxVertexAtomicCounterBuffers = value;
        else if (strcmp(token, "MaxTessControlAtomicCounterBuffers") == 0)
            Resources.maxTessControlAtomicCounterBuffers = value;
        else if (strcmp(token, "MaxTessEvaluationAtomicCounterBuffers") == 0)
            Resources.maxTessEvaluationAtomicCounterBuffers = value;
        else if (strcmp(token, "MaxGeometryAtomicCounterBuffers") == 0)
            Resources.maxGeometryAtomicCounterBuffers = value;
        else if (strcmp(token, "MaxFragmentAtomicCounterBuffers") == 0)
            Resources.maxFragmentAtomicCounterBuffers = value;
        else if (strcmp(token, "MaxCombinedAtomicCounterBuffers") == 0)
            Resources.maxCombinedAtomicCounterBuffers = value;
        else if (strcmp(token, "MaxAtomicCounterBufferSize") == 0)
            Resources.maxAtomicCounterBufferSize = value;
        else if (strcmp(token, "MaxTransformFeedbackBuffers") == 0)
            Resources.maxTransformFeedbackBuffers = value;
        else if (strcmp(token, "MaxTransformFeedbackInterleavedComponents") == 0)
            Resources.maxTransformFeedbackInterleavedComponents = value;
        else if (strcmp(token, "MaxCullDistances") == 0)
            Resources.maxCullDistances = value;
        else if (strcmp(token, "MaxCombinedClipAndCullDistances") == 0)
            Resources.maxCombinedClipAndCullDistances = value;
        else if (strcmp(token, "MaxSamples") == 0)
            Resources.maxSamples = value;

        else if (strcmp(token, "nonInductiveForLoops") == 0)
            Resources.limits.nonInductiveForLoops = (value != 0);
        else if (strcmp(token, "whileLoops") == 0)
            Resources.limits.whileLoops = (value != 0);
        else if (strcmp(token, "doWhileLoops") == 0)
            Resources.limits.doWhileLoops = (value != 0);
        else if (strcmp(token, "generalUniformIndexing") == 0)
            Resources.limits.generalUniformIndexing = (value != 0);
        else if (strcmp(token, "generalAttributeMatrixVectorIndexing") == 0)
            Resources.limits.generalAttributeMatrixVectorIndexing = (value != 0);
        else if (strcmp(token, "generalVaryingIndexing") == 0)
            Resources.limits.generalVaryingIndexing = (value != 0);
        else if (strcmp(token, "generalSamplerIndexing") == 0)
            Resources.limits.generalSamplerIndexing = (value != 0);
        else if (strcmp(token, "generalVariableIndexing") == 0)
            Resources.limits.generalVariableIndexing = (value != 0);
        else if (strcmp(token, "generalConstantMatrixVectorIndexing") == 0)
            Resources.limits.generalConstantMatrixVectorIndexing = (value != 0);
        else
            printf("Warning: unrecognized limit (%s) in configuration file.\n", token);

        token = strtok(0, delims);
    }
    if (configStrings)
        FreeFileData(configStrings);
}

// thread-safe list of shaders to asynchronously grab and compile
glslang::TWorklist Worklist;

// array of unique places to leave the shader names and infologs for the asynchronous compiles
glslang::TWorkItem** Work = 0;
int NumWorkItems = 0;

int Options = 0;
const char* ExecutableName = nullptr;
const char* binaryFileName = nullptr;

static bool debugMode = false;

//
// Create the default name for saving a binary if -o is not provided.
//
const char* GetBinaryName(EShLanguage stage)
{
    const char* name;
    if (binaryFileName == nullptr) {
        switch (stage) {
        case EShLangVertex:          name = "vert.spv";    break;
        case EShLangTessControl:     name = "tesc.spv";    break;
        case EShLangTessEvaluation:  name = "tese.spv";    break;
        case EShLangGeometry:        name = "geom.spv";    break;
        case EShLangFragment:        name = "frag.spv";    break;
        case EShLangCompute:         name = "comp.spv";    break;
        default:                     name = "unknown";     break;
        }
    } else
        name = binaryFileName;

    return name;
}

//
// *.conf => this is a config file that can set limits/resources
//
bool SetConfigFile(const std::string& name)
{
    if (name.size() < 5)
        return false;

    if (name.compare(name.size() - 5, 5, ".conf") == 0) {
        ConfigFile = name;
        return true;
    }

    return false;
}

//
// Give error and exit with failure code.
//
void Error(const char* message)
{
    printf("%s: Error %s (use -h for usage)\n", ExecutableName, message);
    exit(EFailUsage);
}

//
// Do all command-line argument parsing.  This includes building up the work-items
// to be processed later, and saving all the command-line options.
//
// Does not return (it exits) if command-line is fatally flawed.
//
void ProcessArguments(int argc, char* argv[])
{
    ExecutableName = argv[0];
    NumWorkItems = argc;  // will include some empties where the '-' options were, but it doesn't matter, they'll be 0
    Work = new glslang::TWorkItem*[NumWorkItems];
    for (int w = 0; w < NumWorkItems; ++w)
        Work[w] = 0;

    argc--;
    argv++;    
    for (; argc >= 1; argc--, argv++) {
        if (argv[0][0] == '-') {
            switch (argv[0][1]) {
            case 'H':
                Options |= EOptionHumanReadableSpv;
                // fall through to -V
            case 'V':
                Options |= EOptionSpv;
                Options |= EOptionVulkanRules;
                Options |= EOptionLinkProgram;
                break;
            case 'G':
                Options |= EOptionSpv;
                Options |= EOptionLinkProgram;
                break;
            case 'E':
                Options |= EOptionOutputPreprocessed;
                break;
            case 'c':
                Options |= EOptionDumpConfig;
                break;
            case 'd':
                Options |= EOptionDefaultDesktop;
                break;
            case 'h':
                usage();
                break;
            case 'i':
                Options |= EOptionIntermediate;
                break;
            case 'l':
                Options |= EOptionLinkProgram;
                break;
            case 'm':
                Options |= EOptionMemoryLeakMode;
                break;
            case 'o':
                binaryFileName = argv[1];
                if (argc > 0) {
                    argc--;
                    argv++;
                } else
                    Error("no <file> provided for -o");
                break;
            case 'q':
                Options |= EOptionDumpReflection;
                break;
            case 'r':
                Options |= EOptionRelaxedErrors;
                break;
            case 's':
                Options |= EOptionSuppressInfolog;
                break;
            case 't':
                #ifdef _WIN32
                    Options |= EOptionMultiThreaded;
                #endif
                break;
            case 'v':
                Options |= EOptionDumpVersions;
                break;
            case 'w':
                Options |= EOptionSuppressWarnings;
                break;
            default:
                usage();
                break;
            }
        } else {
            std::string name(argv[0]);
            if (! SetConfigFile(name)) {
                Work[argc] = new glslang::TWorkItem(name);
                Worklist.add(Work[argc]);
            }
        }
    }

    // Make sure that -E is not specified alongside linking (which includes SPV generation)
    if ((Options & EOptionOutputPreprocessed) && (Options & EOptionLinkProgram))
        Error("can't use -E when linking is selected");

    // -o makes no sense if there is no target binary
    if (binaryFileName && (Options & EOptionSpv) == 0)
        Error("no binary generation requested (e.g., -V)");
}

//
// Translate the meaningful subset of command-line options to parser-behavior options.
//
void SetMessageOptions(EShMessages& messages)
{
    if (Options & EOptionRelaxedErrors)
        messages = (EShMessages)(messages | EShMsgRelaxedErrors);
    if (Options & EOptionIntermediate)
        messages = (EShMessages)(messages | EShMsgAST);
    if (Options & EOptionSuppressWarnings)
        messages = (EShMessages)(messages | EShMsgSuppressWarnings);
    //if (Options & EOptionSpv) // Deactivated because this sets spv to 100 which disables lots of glsl functions in Initialize.cpp
    //    messages = (EShMessages)(messages | EShMsgSpvRules);
    if (Options & EOptionVulkanRules)
        messages = (EShMessages)(messages | EShMsgVulkanRules);
    if (Options & EOptionOutputPreprocessed)
        messages = (EShMessages)(messages | EShMsgOnlyPreprocessor);
}

//
// Thread entry point, for non-linking asynchronous mode.
//
// Return 0 for failure, 1 for success.
//
unsigned int CompileShaders(void*)
{
    glslang::TWorkItem* workItem;
    while (Worklist.remove(workItem)) {
        ShHandle compiler = ShConstructCompiler(FindLanguage(workItem->name), Options);
        if (compiler == 0)
            return 0;

        CompileFile(workItem->name.c_str(), compiler);

        if (! (Options & EOptionSuppressInfolog))
            workItem->results = ShGetInfoLog(compiler);

        ShDestruct(compiler);
    }

    return 0;
}

// Outputs the given string, but only if it is non-null and non-empty.
// This prevents erroneous newlines from appearing.
void PutsIfNonEmpty(const char* str)
{
    if (str && str[0]) {
        puts(str);
    }
}

// Outputs the given string to stderr, but only if it is non-null and non-empty.
// This prevents erroneous newlines from appearing.
void StderrIfNonEmpty(const char* str)
{
    if (str && str[0]) {
      fprintf(stderr, "%s\n", str);
    }
}

void executeSync(const char* command);
int compileHLSLToD3D9(const char* from, const char* to, const std::map<std::string, int>& attributes, EShLanguage stage);
int compileHLSLToD3D11(const char* from, const char* to, const std::map<std::string, int>& attributes, EShLanguage stage, bool debug);

std::string extractFilename(std::string path) {
	int i = path.size() - 1;
	for (; i > 0; --i) {
		if (path[i] == '/' || path[i] == '\\') {
			++i;
			break;
		}
	}
	return path.substr(i, std::string::npos);
}

std::string removeExtension(std::string filename) {
	int i = filename.size() - 1;
	for (; i > 0; --i) {
		if (filename[i] == '.') {
			break;
		}
	}
	if (i == 0) return filename;
	else return filename.substr(0, i);
}

class KrafixIncluder : public glslang::TShader::Includer {
public:
	KrafixIncluder(std::string from) {
		dir = from;
		for (int i = from.size() - 1; i >= 0; --i) {
			if (dir[i] == '/' || dir[i] == '\\') {
				dir = dir.substr(0, i + 1);
				break;
			}
		}
	}

	IncludeResult* include(const char* requested_source, IncludeType type, const char* requesting_source, size_t inclusion_depth) override {
		std::string realfilename = dir + requested_source;
		std::stringstream content;
		std::string line;
		std::ifstream file(realfilename);
		if (file.is_open()) {
			while (getline(file, line)) {
				content << line << '\n';
			}
			file.close();
		}
		std::string filecontent = content.str();
		char* heapcontent = new char[filecontent.size() + 1];
		strcpy(heapcontent, filecontent.c_str());
		return new IncludeResult(realfilename, heapcontent, content.str().size(), heapcontent);
	}

	void releaseInclude(IncludeResult* result) override {
		delete (char*)result->user_data;
		delete result;
	}
private:
	std::string dir;
};

krafix::ShaderStage shLanguageToShaderStage(EShLanguage lang) {
	switch (lang) {
	case EShLangVertex: return krafix::StageVertex;
	case EShLangTessControl: return krafix::StageTessControl;
	case EShLangTessEvaluation: return krafix::StageTessEvaluation;
	case EShLangGeometry: return krafix::StageGeometry;
	case EShLangFragment: return krafix::StageFragment;
	case EShLangCompute: return krafix::StageCompute;
	case EShLangCount:
	default:
		return krafix::StageCompute;
	}
}

//
// For linking mode: Will independently parse each item in the worklist, but then put them
// in the same program and link them together.
//
// Uses the new C++ interface instead of the old handle-based interface.
//
void CompileAndLinkShaders(krafix::Target target, const char* sourcefilename, const char* filename, const char* tempdir, glslang::TShader::Includer& includer, const char* defines)
{
    // keep track of what to free
    std::list<glslang::TShader*> shaders;
    
    EShMessages messages = EShMsgDefault;
    SetMessageOptions(messages);

    //
    // Per-shader processing...
    //

    glslang::TProgram& program = *new glslang::TProgram;
    glslang::TWorkItem* workItem;
    while (Worklist.remove(workItem)) {
        EShLanguage stage = FindLanguage(workItem->name);
        glslang::TShader* shader = new glslang::TShader(stage);
		shader->setPreamble(defines);
        shaders.push_back(shader);
    
        char** shaderStrings = ReadFileData(workItem->name.c_str());
        if (! shaderStrings) {
            usage();
            delete &program;

            return;
        }
        const int defaultVersion = Options & EOptionDefaultDesktop? 110: 100;

        shader->setStrings(shaderStrings, 1);
        if (Options & EOptionOutputPreprocessed) {
            std::string str;
            if (shader->preprocess(&Resources, defaultVersion, ENoProfile, false, false,
                                   messages, &str, includer)) {
                PutsIfNonEmpty(str.c_str());
            } else {
                CompileFailed = true;
            }
            StderrIfNonEmpty(shader->getInfoLog());
            StderrIfNonEmpty(shader->getInfoDebugLog());
            FreeFileData(shaderStrings);
            continue;
        }
		if (! shader->parse(&Resources, defaultVersion, ENoProfile, false, false, messages, includer))
            CompileFailed = true;

        program.addShader(shader);

        if (! (Options & EOptionSuppressInfolog)) {
            //PutsIfNonEmpty(workItem->name.c_str());
            PutsIfNonEmpty(shader->getInfoLog());
            PutsIfNonEmpty(shader->getInfoDebugLog());
        }

        FreeFileData(shaderStrings);
    }

    //
    // Program-level processing...
    //

    if (! (Options & EOptionOutputPreprocessed) && ! program.link(messages))
        LinkFailed = true;

    if (! (Options & EOptionSuppressInfolog)) {
        PutsIfNonEmpty(program.getInfoLog());
        PutsIfNonEmpty(program.getInfoDebugLog());
    }

    if (Options & EOptionDumpReflection) {
        program.buildReflection();
        program.dumpReflection();
    }

    if (Options & EOptionSpv) {
        if (CompileFailed || LinkFailed)
            printf("SPIRV is not generated for failed compile or link\n");
        else {
            for (int stage = 0; stage < EShLangCount; ++stage) {
                if (program.getIntermediate((EShLanguage)stage)) {
                    std::vector<unsigned int> spirv;
                    glslang::GlslangToSpv(*program.getIntermediate((EShLanguage)stage), spirv);
					
					if (!quiet) {
						krafix::VarListTranslator* varPrinter = new krafix::VarListTranslator(spirv, shLanguageToShaderStage((EShLanguage)stage));
						varPrinter->print();
					}

					krafix::Translator* translator = NULL;
					std::map<std::string, int> attributes;
					switch (target.lang) {
					case krafix::SpirV:
						translator = new krafix::SpirVTranslator(spirv, shLanguageToShaderStage((EShLanguage)stage));
						break;
					case krafix::GLSL:
						translator = new krafix::GlslTranslator2(spirv, shLanguageToShaderStage((EShLanguage)stage));
						break;
					case krafix::HLSL:
						translator = new krafix::HlslTranslator2(spirv, shLanguageToShaderStage((EShLanguage)stage));
						break;
					case krafix::Metal:
						translator = new krafix::MetalTranslator2(spirv, shLanguageToShaderStage((EShLanguage)stage));
						break;
					case krafix::AGAL:
						translator = new krafix::AgalTranslator(spirv, shLanguageToShaderStage((EShLanguage)stage));
						break;
					case krafix::VarList:
						translator = new krafix::VarListTranslator(spirv, shLanguageToShaderStage((EShLanguage)stage));
						break;
					case krafix::JavaScript:
						translator = new krafix::JavaScriptTranslator(spirv, shLanguageToShaderStage((EShLanguage)stage));
						break;
					}
					
					if (target.lang == krafix::HLSL && target.system != krafix::Unity) {
						std::string temp = std::string(tempdir) + "/" + removeExtension(extractFilename(workItem->name)) + ".hlsl";
						translator->outputCode(target, sourcefilename, temp.c_str(), attributes);
						int returnCode = 0;
						if (target.version == 9) {
							returnCode = compileHLSLToD3D9(temp.c_str(), filename, attributes, (EShLanguage)stage);
						}
						else {
							returnCode = compileHLSLToD3D11(temp.c_str(), filename, attributes, (EShLanguage)stage, debugMode);
						}
						if (returnCode != 0) CompileFailed = true;
					}
					else {
						translator->outputCode(target, sourcefilename, filename, attributes);
					}

					delete translator;
                    
                    //glslang::OutputSpv(spirv, GetBinaryName((EShLanguage)stage));
                    if (Options & EOptionHumanReadableSpv) {
                        spv::Parameterize();
                        spv::Disassemble(std::cout, spirv);
                    }
                }
            }
        }
    }

    // Free everything up, program has to go before the shaders
    // because it might have merged stuff from the shaders, and
    // the stuff from the shaders has to have its destructors called
    // before the pools holding the memory in the shaders is freed.
    delete &program;
    while (shaders.size() > 0) {
        delete shaders.back();
        shaders.pop_back();
    }
}

krafix::TargetSystem getSystem(const char* system) {
	if (strcmp(system, "windows") == 0) return krafix::Windows;
	if (strcmp(system, "windowsapp") == 0) return krafix::WindowsApp;
	if (strcmp(system, "osx") == 0) return krafix::OSX;
	if (strcmp(system, "linux") == 0) return krafix::Linux;
	if (strcmp(system, "ios") == 0) return krafix::iOS;
	if (strcmp(system, "android") == 0) return krafix::Android;
	if (strcmp(system, "html5") == 0) return krafix::HTML5;
    if (strcmp(system, "debug-html5") == 0) return krafix::HTML5;
	if (strcmp(system, "flash") == 0) return krafix::Flash;
	if (strcmp(system, "unity") == 0) return krafix::Unity;
	return krafix::Unknown;
}

void compile(const char* targetlang, const char* from, std::string to, const char* tempdir, const char* system,
			 KrafixIncluder& includer, std::string defines, int version) {
	krafix::Target target;
	target.system = getSystem(system);
	target.es = false;
	if (strcmp(targetlang, "spirv") == 0) {
		target.lang = krafix::SpirV;
		target.version = version > 0 ? version : 1;
		CompileAndLinkShaders(target, from, to.c_str(), tempdir, includer, defines.c_str());
	}
	else if (strcmp(targetlang, "d3d9") == 0) {
		target.lang = krafix::HLSL;
		target.version = version > 0 ? version : 9;
		CompileAndLinkShaders(target, from, to.c_str(), tempdir, includer, defines.c_str());
	}
	else if (strcmp(targetlang, "d3d11") == 0) {
		target.lang = krafix::HLSL;
		target.version = version > 0 ? version : 11;
		CompileAndLinkShaders(target, from, to.c_str(), tempdir, includer, defines.c_str());
	}
	else if (strcmp(targetlang, "glsl") == 0) {
		target.lang = krafix::GLSL;
		if (target.system == krafix::Linux) target.version = version > 0 ? version : 110;
		else target.version = version > 0 ? version : 330;
		CompileAndLinkShaders(target, from, to.c_str(), tempdir, includer, defines.c_str());
	}
	else if (strcmp(targetlang, "essl") == 0) {
		target.lang = krafix::GLSL;
		target.version = version > 0 ? version : 100;
		target.es = true;
		CompileAndLinkShaders(target, from, to.c_str(), tempdir, includer, defines.c_str());
	}
	else if (strcmp(targetlang, "agal") == 0) {
		target.lang = krafix::AGAL;
		target.version = version > 0 ? version : 100;
		target.es = true;
		CompileAndLinkShaders(target, from, to.c_str(), tempdir, includer, defines.c_str());
	}
	else if (strcmp(targetlang, "metal") == 0) {
		target.lang = krafix::Metal;
		target.version = version > 0 ? version : 1;
		CompileAndLinkShaders(target, from, to.c_str(), tempdir, includer, defines.c_str());
	}
	else if (strcmp(targetlang, "varlist") == 0) {
		target.lang = krafix::VarList;
		target.version = version > 0 ? version : 1;
		CompileAndLinkShaders(target, from, to.c_str(), tempdir, includer, defines.c_str());
	}
	else if (strcmp(targetlang, "js") == 0 || strcmp(targetlang, "javascript") == 0) {
		target.lang = krafix::JavaScript;
		target.version = version > 0 ? version : 1;
		CompileAndLinkShaders(target, from, to.c_str(), tempdir, includer, defines.c_str());
	}
	else {
		std::cout << "Unknown profile " << targetlang << std::endl;
		CompileFailed = true;
	}
	if (!CompileFailed) {
		std::cerr << "#file:" << to << std::endl;
	}
}

int C_DECL main(int argc, char* argv[]) {
	if (argc < 6) {
		usage();
		return 1;
	}

	const char* tempdir = argv[4];

	//Options |= EOptionHumanReadableSpv;
	Options |= EOptionSpv;
	Options |= EOptionLinkProgram;
	//Options |= EOptionSuppressInfolog;

	NumWorkItems = 1;
	Work = new glslang::TWorkItem*[NumWorkItems];
	Work[0] = 0;

	std::string name(argv[2]);
	if (!SetConfigFile(name)) {
		Work[0] = new glslang::TWorkItem(name);
		Worklist.add(Work[0]);
	}
	
	std::string defines;
	std::vector<int> textureUnitCounts;
	bool instancedoptional = false;
	int version = -1;
	bool getversion = false;

	for (int i = 6; i < argc; ++i) {
		std::string arg = argv[i];
		if (getversion) {
			version = atoi(argv[i]);
			getversion = false;
		}
		else if (arg.substr(0, 2) == "-D") {
			defines += "#define " + arg.substr(2) + "\n";
		}
		else if (arg.substr(0, 2) == "-T") {
			textureUnitCounts.push_back(atoi(arg.substr(2).c_str()));
		}
		else if (arg == "--instancedoptional") {
			instancedoptional = true;
		}
		else if (arg == "--debug") {
			debugMode = true;
		}
		else if (arg == "--version") {
			getversion = true;
		}
		else if (arg == "--quiet") {
			quiet = true;
		}
	}

	const char* targetlang = argv[1];
	const char* from = argv[2];
	std::string to = argv[3];
	const char* system = argv[5];

	ProcessConfigFile();

	glslang::InitializeProcess();
	
	KrafixIncluder includer(name);
	
	bool usesTextureUnitsCount = false;
	bool usesInstancedoptional = false;
	
	if (textureUnitCounts.size() > 0 || instancedoptional) {
		std::stringstream filecontentstream;
		std::string line;
		std::ifstream file(from);
		if (file.is_open()) {
			while (getline (file, line)) {
				filecontentstream << line << '\n';
			}
			file.close();
		}
		std::string filecontent = filecontentstream.str();
		
		if (filecontent.find("MAX_TEXTURE_UNITS") != std::string::npos) {
			usesTextureUnitsCount = true;
		}
		if (filecontent.find("INSTANCED_RENDERING") != std::string::npos) {
			usesInstancedoptional = true;
		}
	}
	
	std::string towithoutext = to.substr(0, to.find_last_of('.'));
	std::string ext = to.substr(to.find_last_of('.'));
	
	if (textureUnitCounts.size() > 0 && usesTextureUnitsCount) {
		if (instancedoptional && usesInstancedoptional) {
			for (size_t i = 0; i < textureUnitCounts.size(); ++i) {
				int texcount = textureUnitCounts[i];
				std::stringstream toto;
				toto << towithoutext << "-tex" << texcount << ext;
				std::stringstream definesplustex;
				definesplustex << defines << "#define MAX_TEXTURE_UNITS=" << texcount << "\n";
				
				std::stringstream tototo;
				tototo << towithoutext << "-tex" << texcount << "-noinst" << ext;
				compile(targetlang, from, tototo.str(), tempdir, system, includer, definesplustex.str(), version);
				std::stringstream totototo;
				tototo << towithoutext << "-tex" << texcount << "-inst" << ext;
				std::string definesplusinst = definesplustex.str() + "#define INSTANCED_RENDERING\n";
				compile(targetlang, from, totototo.str(), tempdir, system, includer, definesplusinst, version);
			}
		}
		else {
			for (size_t i = 0; i < textureUnitCounts.size(); ++i) {
				int texcount = textureUnitCounts[i];
				std::stringstream toto;
				toto << towithoutext << "-tex" << texcount << ext;
				std::stringstream definesplustex;
				definesplustex << defines << "#define MAX_TEXTURE_UNITS=" << texcount << "\n";
				compile(targetlang, from, toto.str(), tempdir, system, includer, definesplustex.str(), version);
			}
		}
	}
	else {
		if (instancedoptional && usesInstancedoptional) {
			std::string toto = towithoutext + "-noinst" + ext;
			compile(targetlang, from, toto, tempdir, system, includer, defines, version);
			toto = towithoutext + "-inst" + ext;
			std::string definesplusinst = defines + "#define INSTANCED_RENDERING\n";
			compile(targetlang, from, toto, tempdir, system, includer, definesplusinst, version);
		}
		else {
			compile(targetlang, from, to, tempdir, system, includer, defines, version);
		}
	}
	
	glslang::FinalizeProcess();

	if (CompileFailed)
		return EFailCompile;
	if (LinkFailed)
		return EFailLink;

	return 0;
}

int C_DECL main_glslangValidator(int argc, char* argv[])
{
    ProcessArguments(argc, argv);

    if (Options & EOptionDumpConfig) {
        printf("%s", DefaultConfig);
        if (Worklist.empty())
            return ESuccess;
    }

    if (Options & EOptionDumpVersions) {
        printf("Glslang Version: %s %s\n", GLSLANG_REVISION, GLSLANG_DATE);
        printf("ESSL Version: %s\n", glslang::GetEsslVersionString());
        printf("GLSL Version: %s\n", glslang::GetGlslVersionString());
        std::string spirvVersion;
        glslang::GetSpirvVersion(spirvVersion);
        printf("SPIR-V Version %s\n", spirvVersion.c_str());
        printf("GLSL.std.450 Version %d, Revision %d\n", GLSLstd450Version, GLSLstd450Revision);
        if (Worklist.empty())
            return ESuccess;
    }

    if (Worklist.empty()) {
        usage();
    }

    ProcessConfigFile();

    //
    // Two modes:
    // 1) linking all arguments together, single-threaded, new C++ interface
    // 2) independent arguments, can be tackled by multiple asynchronous threads, for testing thread safety, using the old handle interface
    //
    if (Options & EOptionLinkProgram ||
        Options & EOptionOutputPreprocessed) {
        glslang::InitializeProcess();
        //CompileAndLinkShaders();
        glslang::FinalizeProcess();
    } else {
        ShInitialize();

        bool printShaderNames = Worklist.size() > 1;

        if (Options & EOptionMultiThreaded) {
            const int NumThreads = 16;
            void* threads[NumThreads];
            for (int t = 0; t < NumThreads; ++t) {
                threads[t] = glslang::OS_CreateThread(&CompileShaders);
                if (! threads[t]) {
                    printf("Failed to create thread\n");
                    return EFailThreadCreate;
                }
            }
            glslang::OS_WaitForAllThreads(threads, NumThreads);
        } else
            CompileShaders(0);

        // Print out all the resulting infologs
        for (int w = 0; w < NumWorkItems; ++w) {
            if (Work[w]) {
                if (printShaderNames)
                    PutsIfNonEmpty(Work[w]->name.c_str());
                PutsIfNonEmpty(Work[w]->results.c_str());
                delete Work[w];
            }
        }

        ShFinalize();
    }

    if (CompileFailed)
        return EFailCompile;
    if (LinkFailed)
        return EFailLink;

    return 0;
}

//
//   Deduce the language from the filename.  Files must end in one of the
//   following extensions:
//
//   .vert = vertex
//   .tesc = tessellation control
//   .tese = tessellation evaluation
//   .geom = geometry
//   .frag = fragment
//   .comp = compute
//
EShLanguage FindLanguage(const std::string& name)
{
    size_t ext = name.rfind('.');
    if (ext == std::string::npos) {
        usage();
        return EShLangVertex;
    }

    std::string suffix = name.substr(ext + 1, std::string::npos);

	if (suffix == "glsl") {
		size_t ext2 = name.substr(0, ext).rfind('.');
		suffix = name.substr(ext2 + 1, ext - ext2 - 1);
	}

    if (suffix == "vert")
        return EShLangVertex;
    else if (suffix == "tesc")
        return EShLangTessControl;
    else if (suffix == "tese")
        return EShLangTessEvaluation;
    else if (suffix == "geom")
        return EShLangGeometry;
    else if (suffix == "frag")
        return EShLangFragment;
    else if (suffix == "comp")
        return EShLangCompute;

    usage();
    return EShLangVertex;
}

//
// Read a file's data into a string, and compile it using the old interface ShCompile, 
// for non-linkable results.
//
void CompileFile(const char* fileName, ShHandle compiler)
{
    int ret = 0;
    char** shaderStrings = ReadFileData(fileName);
    if (! shaderStrings) {
        usage();
    }

    int* lengths = new int[NumShaderStrings];

    // move to length-based strings, rather than null-terminated strings
    for (int s = 0; s < NumShaderStrings; ++s)
        lengths[s] = (int)strlen(shaderStrings[s]);

    if (! shaderStrings) {
        CompileFailed = true;
        return;
    }

    EShMessages messages = EShMsgDefault;
    SetMessageOptions(messages);
    
    for (int i = 0; i < ((Options & EOptionMemoryLeakMode) ? 100 : 1); ++i) {
        for (int j = 0; j < ((Options & EOptionMemoryLeakMode) ? 100 : 1); ++j) {
            //ret = ShCompile(compiler, shaderStrings, NumShaderStrings, lengths, EShOptNone, &Resources, Options, (Options & EOptionDefaultDesktop) ? 110 : 100, false, messages);
            ret = ShCompile(compiler, shaderStrings, NumShaderStrings, nullptr, EShOptNone, &Resources, Options, (Options & EOptionDefaultDesktop) ? 110 : 100, false, messages);
            //const char* multi[12] = { "# ve", "rsion", " 300 e", "s", "\n#err", 
            //                         "or should be l", "ine 1", "string 5\n", "float glo", "bal", 
            //                         ";\n#error should be line 2\n void main() {", "global = 2.3;}" };
            //const char* multi[7] = { "/", "/", "\\", "\n", "\n", "#", "version 300 es" };
            //ret = ShCompile(compiler, multi, 7, nullptr, EShOptNone, &Resources, Options, (Options & EOptionDefaultDesktop) ? 110 : 100, false, messages);
        }

        if (Options & EOptionMemoryLeakMode)
            glslang::OS_DumpMemoryCounters();
    }

    delete [] lengths;
    FreeFileData(shaderStrings);

    if (ret == 0)
        CompileFailed = true;
}

//
//   print usage to stdout
//
void usage()
{
	printf("Usage: krafix profile in out tempdir system\n");

    /*printf("Usage: glslangValidator [option]... [file]...\n"
           "\n"
           "Where: each 'file' ends in .<stage>, where <stage> is one of\n"
           "    .conf   to provide an optional config file that replaces the default configuration\n"
           "            (see -c option below for generating a template)\n"
           "    .vert   for a vertex shader\n"
           "    .tesc   for a tessellation control shader\n"
           "    .tese   for a tessellation evaluation shader\n"
           "    .geom   for a geometry shader\n"
           "    .frag   for a fragment shader\n"
           "    .comp   for a compute shader\n"
           "\n"
           "Compilation warnings and errors will be printed to stdout.\n"
           "\n"
           "To get other information, use one of the following options:\n"
           "Each option must be specified separately.\n"
           "  -V          create SPIR-V binary, under Vulkan semantics; turns on -l;\n"
           "              default file name is <stage>.spv (-o overrides this)\n"
           "              (unless -o is specified, which overrides the default file name)\n"
           "  -G          create SPIR-V binary, under OpenGL semantics; turns on -l;\n"
           "              default file name is <stage>.spv (-o overrides this)\n"
           "  -H          print human readable form of SPIR-V; turns on -V\n"
           "  -E          print pre-processed GLSL; cannot be used with -l;\n"
           "              errors will appear on stderr.\n"
           "  -c          configuration dump;\n"
           "              creates the default configuration file (redirect to a .conf file)\n"
           "  -d          default to desktop (#version 110) when there is no shader #version\n"
           "              (default is ES version 100)\n"
           "  -h          print this usage message\n"
           "  -i          intermediate tree (glslang AST) is printed out\n"
           "  -l          link all input files together to form a single module\n"
           "  -m          memory leak mode\n"
           "  -o  <file>  save binary into <file>, requires a binary option (e.g., -V)\n"
           "  -q          dump reflection query database\n"
           "  -r          relaxed semantic error-checking mode\n"
           "  -s          silent mode\n"
           "  -t          multi-threaded mode\n"
           "  -v          print version strings\n"
           "  -w          suppress warnings (except as required by #extension : warn)\n"
           );*/

    exit(EFailUsage);
}

#if !defined _MSC_VER && !defined MINGW_HAS_SECURE_API

#include <errno.h>

int fopen_s(
   FILE** pFile,
   const char* filename,
   const char* mode
)
{
   if (!pFile || !filename || !mode) {
      return EINVAL;
   }

   FILE* f = fopen(filename, mode);
   if (! f) {
      if (errno != 0) {
         return errno;
      } else {
         return ENOENT;
      }
   }
   *pFile = f;

   return 0;
}

#endif

//
//   Malloc a string of sufficient size and read a string into it.
//
char** ReadFileData(const char* fileName) 
{
    FILE *in = nullptr;
    int errorCode = fopen_s(&in, fileName, "r");

    int count = 0;
    const int maxSourceStrings = 5;  // for testing splitting shader/tokens across multiple strings
    char** return_data = (char**)malloc(sizeof(char *) * (maxSourceStrings+1)); // freed in FreeFileData()

    if (errorCode || in == nullptr)
        Error("unable to open input file");
    
    while (fgetc(in) != EOF)
        count++;

    fseek(in, 0, SEEK_SET);

    char *fdata = (char*)malloc(count+2); // freed before return of this function
    if (! fdata)
        Error("can't allocate memory");

    if ((int)fread(fdata, 1, count, in) != count) {
        free(fdata);
        Error("can't read input file");
    }

    fdata[count] = '\0';
    fclose(in);

    if (count == 0) {
        // recover from empty file
        return_data[0] = (char*)malloc(count+2);  // freed in FreeFileData()
        return_data[0][0]='\0';
        NumShaderStrings = 0;
        free(fdata);

        return return_data;
    } else
        NumShaderStrings = 1;  // Set to larger than 1 for testing multiple strings

    // compute how to split up the file into multiple strings, for testing multiple strings
    int len = (int)(ceil)((float)count/(float)NumShaderStrings);
    int ptr_len = 0;
    int i = 0;
    while (count > 0) {
        return_data[i] = (char*)malloc(len + 2);  // freed in FreeFileData()
        memcpy(return_data[i], fdata + ptr_len, len);
        return_data[i][len] = '\0';
        count -= len;
        ptr_len += len;
        if (count < len) {
            if (count == 0) {
               NumShaderStrings = i + 1;
               break;
            }
            len = count;
        }  
        ++i;
    }

    free(fdata);

    return return_data;
}

void FreeFileData(char** data)
{
    for(int i = 0; i < NumShaderStrings; i++)
        free(data[i]);

    free(data);
}

void InfoLogMsg(const char* msg, const char* name, const int num)
{
    if (num >= 0 )
        printf("#### %s %s %d INFO LOG ####\n", msg, name, num);
    else
        printf("#### %s %s INFO LOG ####\n", msg, name);
}


#ifdef SYS_WINDOWS
#include <Windows.h>
#endif

void executeSync(const char* command) {
#ifdef SYS_WINDOWS
	STARTUPINFOA startupInfo;
	PROCESS_INFORMATION processInfo;
	memset(&startupInfo, 0, sizeof(startupInfo));
	memset(&processInfo, 0, sizeof(processInfo));
	startupInfo.cb = sizeof(startupInfo);
	CreateProcessA(nullptr, (char*)command, nullptr, nullptr, FALSE, CREATE_DEFAULT_ERROR_MODE, "PATH=%PATH%;.\\cygwin\\bin\0", nullptr, &startupInfo, &processInfo);
	WaitForSingleObject(processInfo.hProcess, INFINITE);
	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);
#endif
	system(command);
}
