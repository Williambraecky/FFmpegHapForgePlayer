#include "HAPAvFormatForgeRenderer.h"

#include <iostream>

#include "hap/hap.h"
#include "shadercompilerhelper.h"

#include "Renderer/IRenderer.h"
#include "Renderer/IResourceLoader.h"
#include "OS/Logging/Log.h"
#include "OS/Interfaces/IApp.h"

#ifdef __APPLE__
#import <Cocoa/cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#endif

#if defined(__APPLE__) || defined( Linux )
    #include <dispatch/dispatch.h>
#else
    #include <ppl.h>
#endif
void HapMTDecode(HapDecodeWorkFunction function, void *info, unsigned int count, void * /*info*/)
{
    #if defined(__APPLE__) || defined( Linux )
        dispatch_apply(count, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(size_t index) {
            function(info, (unsigned int)index);
        });
    #else
        concurrency::parallel_for((unsigned int)0, count, [&](unsigned int i) {
            function(info, i);
        });
    #endif
}

#ifdef __APPLE__
float2 g_retinaScale = { 1.0f, 1.0f };
#endif

// Number of buffers to swap from
#define IMAGE_COUNT 3

const char* g_error_messages[] =
{
    "No error",
    "Renderer initialize error",
    "Memalloc init error",
    "Swapchain initialize error",
    "Depthbuffer initialize error",

    //Add error messages here
};


const char* HAPAvFormatForgeRenderer::get_error()
{
    if (error_code >= (sizeof(g_error_messages) / sizeof(g_error_messages[0])))
        return "Undefined error";
    return g_error_messages[error_code];
}

uint32_t HAPAvFormatForgeRenderer::get_error_code()
{
    return error_code;
}

struct HAPAvFormatForgeRenderer::Pimpl
{
    // The forge main renderer object
    Renderer*       renderer = nullptr;

    // The forge application settings
    IApp::Settings* settings;
    // The forge gpu command queue
    Queue*          graphicsQueue = nullptr;
    // The forge command pool
    CmdPool*        cmdPool[IMAGE_COUNT] = { nullptr };
    Cmd*            cmds[IMAGE_COUNT] = { nullptr };

    // The forge shader program
    Shader*         videoShader = nullptr;
    // Vertex buffer for a quad
    Buffer*         videoVertexBuffer = nullptr;
    // The forge pipeline for video rendering
    Pipeline*       videoPipeline = nullptr;
    // Image Sampler
    Sampler*        videoTextureSampler = nullptr;
    // Image texture
    Texture*        videoTexture[2] = { nullptr }; //2 for HAP Q alpha case
    DescriptorSet*  videoDescriptorSet = nullptr;

    // The forge root signature, still unsure what it does
    RootSignature*  rootSignature = nullptr;
    // The forge depth buffer
    RenderTarget*   depthBuffer = nullptr;

    // The forge swap chain
    SwapChain*      swapChain = nullptr;
    // The forge rendertargets represents the swapchain buffers
    RenderTarget*   renderTarget = nullptr;

    // Fences and semaphores
    // Fence gets locked and unlocked when starting and stopping rendering
    Fence*          renderCompleteFences[IMAGE_COUNT] = { nullptr };
    // Used for querying current swap buffer
    Semaphore*      imageAcquiredSemaphore = nullptr;
    // Per buffer, unlocked when render complete
    Semaphore*      renderCompleteSemaphore[IMAGE_COUNT] = { nullptr };

};

HAPAvFormatForgeRenderer::HAPAvFormatForgeRenderer()
    :m_pImpl(std::make_unique<struct Pimpl>())
{
}

HAPAvFormatForgeRenderer::~HAPAvFormatForgeRenderer()
{
    //TODO: cleanup resources
}

extern char gResourceMounts[RM_COUNT][FS_MAX_PATH];

int HAPAvFormatForgeRenderer::initRenderer()
{
    //Initialize file system and the forge resources
    extern bool MemAllocInit(const char *name);
    if (!MemAllocInit("The forge"))
    {
        error_code = 2;
        return error_code;
    }
    FileSystemInitDesc fsInitDesc;
    fsInitDesc.pAppName = "QtForge";
    fsInitDesc.pPlatformData = nullptr;
    initFileSystem(&fsInitDesc);
    strcpy(gResourceMounts[RM_CONTENT], "");
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_OTHER_FILES, "shaders");
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "resources");
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "shaders/Platform");
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "shaders/Platform/Compiled");
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_LOG, "logs");
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "resources");
    std::cout << "Current RD_SHADER_SOURCES " << fsGetResourceDirectory(RD_SHADER_SOURCES) << std::endl;
    char* pwd = getcwd(NULL, 512);
    std::cout << "Current pwd " << pwd << std::endl;
    delete pwd;
    Log::Init("FFmpegHAPPLayer");

    RendererDesc settings = { 0 };
    ::initRenderer("FFmpegHAPPlayer", &settings, &(m_pImpl->renderer));
    if (!m_pImpl->renderer)
    {
        error_code = 1;
        return error_code;
    }

    QueueDesc queueDesc = {};
    queueDesc.mType = QUEUE_TYPE_GRAPHICS;
    queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
    addQueue(m_pImpl->renderer, &queueDesc, &(m_pImpl->graphicsQueue));

    for (uint32_t i = 0; i < IMAGE_COUNT; i++)
    {
        CmdPoolDesc cmdPoolDesc = {};
        cmdPoolDesc.pQueue = m_pImpl->graphicsQueue;
        addCmdPool(m_pImpl->renderer, &cmdPoolDesc, &(m_pImpl->cmdPool[i]));
        CmdDesc cmdDesc = {};
        cmdDesc.pPool = m_pImpl->cmdPool[i];
        addCmd(m_pImpl->renderer, &cmdDesc, &(m_pImpl->cmds[i]));

        addFence(m_pImpl->renderer, &(m_pImpl->renderCompleteFences[i]));
        addSemaphore(m_pImpl->renderer, &(m_pImpl->renderCompleteSemaphore[i]));
    }
    addSemaphore(m_pImpl->renderer, &(m_pImpl->imageAcquiredSemaphore));

    initResourceLoaderInterface(m_pImpl->renderer);

    float quadTexturePoints[] =
    {
        //vertex                    //texture coord
        -1.0f, 1.0f, 0.0f,          0.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,         0.0f, 1.0f,
        1.0f, -1.0f, 0.0f,          1.0f, 1.0f,

        -1.0f, 1.0f, 0.0f,          0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,           1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,          1.0f, 1.0f,

    };
    BufferLoadDesc triangleVertexDesc = {};
    triangleVertexDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    triangleVertexDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    triangleVertexDesc.mDesc.mSize = sizeof(quadTexturePoints);
    triangleVertexDesc.pData = quadTexturePoints;
    triangleVertexDesc.ppBuffer = &(m_pImpl->videoVertexBuffer);
    addResource(&triangleVertexDesc, NULL);

    return 0;
}

int HAPAvFormatForgeRenderer::openWindow(const char *title, int width, int height)
{
    (void)title;(void)height;(void)width;
    m_winWidth = width;
    m_winHeight = height;
#ifdef __APPLE__

    id <MTLDevice> mtlDevice = MTLCreateSystemDefaultDevice();

    NSRect ViewRect = {0, 0, (float)width, (float)height};
    std::cout << "OpenWindow width " << (float)width << " height " << height << std::endl;

    NSWindow* Window = [[NSWindow alloc] initWithContentRect:ViewRect
                                         styleMask: (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                                         backing: NSBackingStoreBuffered
                                         defer:NO];

    [Window setAcceptsMouseMovedEvents:YES];
    [Window setMinSize:NSSizeFromCGSize(CGSizeMake(128, 128))];
    [Window setTitle:[NSString stringWithUTF8String:title]];

    [Window setOpaque:YES];
    [Window setRestorable:NO];
    [Window invalidateRestorableState];
    [Window makeKeyAndOrderFront: nil];
    [Window makeMainWindow];

    CGFloat scale = [Window backingScaleFactor];
    g_retinaScale = { (float)scale, (float)scale };

    NSView* nsView = [[NSView alloc] initWithFrame: ViewRect];
    CAMetalLayer* metalLayer = [CAMetalLayer layer];
    metalLayer.device = mtlDevice;
    metalLayer.framebufferOnly = YES;
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    metalLayer.wantsExtendedDynamicRangeContent = NO;
    metalLayer.displaySyncEnabled = NO;
    nsView.layer = metalLayer;
    [Window setContentView: nsView];

    m_winHandle = (void*)CFBridgingRetain(nsView);
    // Adjust window size to match retina scaling.
    [Window center];
#endif

    std::cout << "Passed here" << std::endl;
    return 0;
}

bool HAPAvFormatForgeRenderer::addSwapChain()
{
    SwapChainDesc swapChainDesc = {};
    swapChainDesc.mWindowHandle.window = m_winHandle;
    swapChainDesc.mPresentQueueCount = 1;
    swapChainDesc.ppPresentQueues = &(m_pImpl->graphicsQueue);
    swapChainDesc.mWidth = m_winWidth;
    swapChainDesc.mHeight = m_winHeight;
    swapChainDesc.mImageCount = IMAGE_COUNT; //Represents number of buffers to swap from
    swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
    swapChainDesc.mEnableVsync = false;
    ::addSwapChain(m_pImpl->renderer, &swapChainDesc, &(m_pImpl->swapChain));

    return m_pImpl->swapChain != nullptr;
}

bool HAPAvFormatForgeRenderer::addDepthBuffer()
{
    RenderTargetDesc depthRT = {};
    depthRT.mArraySize = 1;
    depthRT.mClearValue.depth = 0.0f;
    depthRT.mClearValue.stencil = 0;
    depthRT.mDepth = 1;
    depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
    depthRT.mHeight = m_winHeight;
    depthRT.mSampleCount = SAMPLE_COUNT_1;
    depthRT.mSampleQuality = 0;
    depthRT.mWidth = m_winWidth;
    depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE;
    addRenderTarget(m_pImpl->renderer, &depthRT, &m_pImpl->depthBuffer);

    return m_pImpl->depthBuffer != NULL;
}

int HAPAvFormatForgeRenderer::createContext()
{
    if (!addSwapChain())
    {
        error_code = 3;
        return error_code;
    }
    if (!addDepthBuffer())
    {
        error_code = 4;
        return false;
    }
    VertexLayout vertexLayout = {};
    vertexLayout.mAttribCount = 2;
    vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
    vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
    vertexLayout.mAttribs[0].mBinding = 0;
    vertexLayout.mAttribs[0].mLocation = 0;
    vertexLayout.mAttribs[0].mOffset = 0;
    vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
    vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
    vertexLayout.mAttribs[1].mBinding = 0;
    vertexLayout.mAttribs[1].mLocation = 1;
    vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

    RasterizerStateDesc rasterizerStateDesc = {};
    rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

    PipelineDesc pipelineDesc = {};
    pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
    GraphicsPipelineDesc& pipelineSettings = pipelineDesc.mGraphicsDesc;
    pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
    pipelineSettings.mRenderTargetCount = 1;
    pipelineSettings.pColorFormats = &(m_pImpl->swapChain->ppRenderTargets[0]->mFormat);
    pipelineSettings.mSampleCount = m_pImpl->swapChain->ppRenderTargets[0]->mSampleCount;
    pipelineSettings.mSampleQuality = m_pImpl->swapChain->ppRenderTargets[0]->mSampleQuality;
    pipelineSettings.mDepthStencilFormat = m_pImpl->depthBuffer->mFormat;
    pipelineSettings.pRootSignature = m_pImpl->rootSignature;
    pipelineSettings.pShaderProgram = m_pImpl->videoShader;
    pipelineSettings.pVertexLayout = &vertexLayout;
    pipelineSettings.pRasterizerState = &rasterizerStateDesc;
    addPipeline(m_pImpl->renderer, &pipelineDesc, &(m_pImpl->videoPipeline));
    return 0;
}

void HAPAvFormatForgeRenderer::createShaderProgram(unsigned int codecTag)
{
    // Get vertex and fragment shader files to use
    std::string vertexFilePath = "shaders/Default.vert";
    std::string vertexShaderName = "Default.vert";
    std::string fragmentFilePath = "shaders/Default.frag";
    std::string fragmentShaderName = "Default.frag";
    switch (codecTag) {
        case MKTAG('H','a','p','1'): // Hap
        case MKTAG('H','a','p','5'): // Hap Alpha
            break;
        case MKTAG('H','a','p','A'): // Hap Alpha Only
            // This file does only contain alpha (no RGB) so we should
            // handle it with a specific shader, but this a codec to use
            // only in tricky situations and we'll ignore it for this example
            break;
        case MKTAG('H','a','p','Y'): // Hap Q
            // Single texture with HapTextureFormat_YCoCg_DXT5;
            fragmentFilePath = "shaders/ScaledCoCgYToRGBA.frag";
            fragmentShaderName = "ScaledCoCgYToRGBA.frag";
            break;
        case MKTAG('H','a','p','M'):
            // Two textures: HapTextureFormat_YCoCg_DXT5 & HapTextureFormat_A_RGTC1;
            fragmentFilePath = "shaders/ScaledCoCgYPlusAToRGBA.frag";
            fragmentShaderName = "ScaledCoCgYPlusAToRGBA.frag";
            break;
        default:
            assert(false);
            throw std::runtime_error("Unhandled HAP codec tab");
    }
    //Convert from gl to target platform
    ShaderCompilerHelper compileHelper;
    ShaderCompilerHelper::TranspileDesc transpileDesc[] =
    {
        {vertexFilePath, ShaderCompiler::STAGE_VERTEX },
        {fragmentFilePath, ShaderCompiler::STAGE_FRAGMENT }
    };
    if (!compileHelper.transpileShaders(transpileDesc, 2, m_pImpl->renderer->mApi))
    {
        std::cout << "Transpile error" << std::endl;
        return ;
    }
    //Load our shader
    ShaderLoadDesc videoShaderDesc = {};

    videoShaderDesc.mStages[0] = {vertexShaderName.c_str(), NULL, 0};
    videoShaderDesc.mStages[1] = {fragmentShaderName.c_str(), NULL, 0};


    addShader(m_pImpl->renderer, &videoShaderDesc, &(m_pImpl->videoShader));
}

void HAPAvFormatForgeRenderer::readCodecParams(AVCodecParameters *codecParams)
{
    (void)codecParams;
    #define FFALIGN(x, a) (((x)+(a)-1)&~((a)-1))
    #define TEXTURE_BLOCK_W 4
    #define TEXTURE_BLOCK_H 4

    m_textureWidth = codecParams->width;
    m_textureHeight = codecParams->height;
    // Encoded texture is 4 bytes aligned
    m_codedWidth = FFALIGN(m_textureWidth,TEXTURE_BLOCK_W);
    m_codedHeight = FFALIGN(m_textureHeight,TEXTURE_BLOCK_H);
    m_textureCount = 1;
    unsigned int outputBufferTextureFormats[2];
    switch (codecParams->codec_tag) {
    case MKTAG('H','a','p','1'): // Hap
        outputBufferTextureFormats[0] = HapTextureFormat_RGB_DXT1;
//        m_glInputFormat[0] = HapTextureFormat_RGB_DXT1;
        break;
    case MKTAG('H','a','p','5'): // Hap Alpha
        outputBufferTextureFormats[0] = HapTextureFormat_RGBA_DXT5;
//        m_glInputFormat[0] = HapTextureFormat_RGBA_DXT5;
        break;
    case MKTAG('H','a','p','Y'): // Hap Q
        outputBufferTextureFormats[0] = HapTextureFormat_YCoCg_DXT5;
//        m_glInputFormat[0] = HapTextureFormat_RGBA_DXT5;
        break;
    case MKTAG('H','a','p','A'): // Hap Alpha Only
        outputBufferTextureFormats[0] = HapTextureFormat_A_RGTC1;
//        m_glInputFormat[0] = HapTextureFormat_A_RGTC1;
        break;
    case MKTAG('H','a','p','M'):
        m_textureCount = 2;
        outputBufferTextureFormats[0] = HapTextureFormat_YCoCg_DXT5;
        outputBufferTextureFormats[1] = HapTextureFormat_A_RGTC1;
//        m_glInputFormat[0] = HapTextureFormat_RGBA_DXT5;
//        m_glInputFormat[1] = HapTextureFormat_A_RGTC1;
        break;
    default:
        assert(false);
        throw std::runtime_error("Unhandled HAP codec tab");
    }

    for (int textureId = 0; textureId < m_textureCount; textureId++) {
        unsigned int bitsPerPixel = 0;
        bool alphaOnly;
        TinyImageFormat imageFormat;
        switch (outputBufferTextureFormats[textureId]) {
            case HapTextureFormat_RGB_DXT1:
                bitsPerPixel = 4;
                alphaOnly=false;
                imageFormat = TinyImageFormat_DXBC1_RGB_UNORM;
                break;
            case HapTextureFormat_RGBA_DXT5:
            case HapTextureFormat_YCoCg_DXT5:
                bitsPerPixel = 8;
                alphaOnly=false;
                imageFormat = TinyImageFormat_DXBC3_UNORM;
                break;
            case HapTextureFormat_A_RGTC1:
                bitsPerPixel = 4;
                alphaOnly=true;
                imageFormat = TinyImageFormat_A8_UNORM;
                break;
            default:
                throw std::runtime_error("Invalid texture format");
        }

        size_t bytesPerRow = (m_codedWidth * bitsPerPixel) / 8;

        m_outputBufferSize[textureId] = bytesPerRow * m_codedHeight; //required?

        TextureDesc texDesc = {};
        texDesc.pName = textureId == 0 ? "video" : "video_alpha";
        texDesc.mWidth = m_textureWidth;
        texDesc.mHeight = m_textureHeight;
        texDesc.mDepth = 1;
        texDesc.mArraySize = 1;
        texDesc.mSampleCount = SAMPLE_COUNT_1;
        texDesc.mFormat = imageFormat;
        texDesc.mClearValue = { 0 };
        texDesc.pNativeHandle = nullptr;
        texDesc.mMipLevels = 1;
        texDesc.mDescriptors |= DESCRIPTOR_TYPE_RW_TEXTURE;

        TextureLoadDesc textureDesc = {};
        textureDesc.pDesc = &texDesc;
        textureDesc.pFileName = nullptr;
        textureDesc.ppTexture = &(m_pImpl->videoTexture[textureId]);
        addResource(&textureDesc, NULL);
    }

    createShaderProgram(codecParams->codec_tag);

    //Setup texture sampler
    SamplerDesc samplerDesc = { FILTER_LINEAR,
                                FILTER_LINEAR,
                                MIPMAP_MODE_NEAREST,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE };
    addSampler(m_pImpl->renderer, &samplerDesc, &(m_pImpl->videoTextureSampler));

    //Setup rootsignature and descriptorsets
    const char*       pStaticSamplers[] = { "uSampler0" };
    RootSignatureDesc rootDesc = {};
    rootDesc.mStaticSamplerCount = 1;
    rootDesc.ppStaticSamplerNames = pStaticSamplers;
    rootDesc.ppStaticSamplers = &(m_pImpl->videoTextureSampler);
    rootDesc.mShaderCount = 1;
    rootDesc.ppShaders = &(m_pImpl->videoShader);
    addRootSignature(m_pImpl->renderer, &rootDesc, &(m_pImpl->rootSignature));

    DescriptorSetDesc desc = { m_pImpl->rootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
    addDescriptorSet(m_pImpl->renderer, &desc, &(m_pImpl->videoDescriptorSet));

    DescriptorData params[2] = {};
    params[0].pName = "cocgsy_src";
    params[0].ppTextures = &(m_pImpl->videoTexture[0]);
    if (m_textureCount == 2)
    {
        params[1].pName = "alpha_src";
        params[1].ppTextures = &(m_pImpl->videoTexture[1]);
    }
    updateDescriptorSet(m_pImpl->renderer, 0, m_pImpl->videoDescriptorSet, m_textureCount, params);
}

// This function will decode the AVPacket into memory buffers using HapDecode
// and will then upload the binary result as an OpenGL texture of the correct type
// It then renders a quad into the current framebuffer using the appropriate shader program
void HAPAvFormatForgeRenderer::renderFrame(AVPacket* packet, double msTime) {
    #ifdef LOG_RUNTIME_INFO
        m_infoLogger.onNewFrame(msTime,packet->size);
    #endif

    // Update textures
    for (int textureId = 0; textureId < m_textureCount; textureId++) {
        unsigned long outputBufferDecodedSize;
        unsigned int outputBufferTextureFormat;
        //We might be able to directly decode inside of the texture buffer
        TextureUpdateDesc textureUpdateDesc = { m_pImpl->videoTexture[textureId] };
        beginUpdateResource(&textureUpdateDesc);
        void* outputBuffer = (void*)textureUpdateDesc.pMappedData;
        size_t outputBufferSize = textureUpdateDesc.mRowCount * textureUpdateDesc.mSrcRowStride;
        unsigned int res = HapDecode(packet->data, packet->size,
                                     textureId,
                                     HapMTDecode,
                                     nullptr,
                                     outputBuffer, outputBufferSize,
                                     &outputBufferDecodedSize,
                                     &outputBufferTextureFormat);
        endUpdateResource(&textureUpdateDesc, NULL);
        #ifdef LOG_RUNTIME_INFO
            m_infoLogger.onHapDataDecoded(outputBufferDecodedSize);
        #endif

        if (res != HapResult_No_Error) {
            throw std::runtime_error("Failed to decode HAP texture");
        }
    }

    uint32_t swapchainImageIndex;

    acquireNextImage(m_pImpl->renderer, m_pImpl->swapChain,
                     m_pImpl->imageAcquiredSemaphore, NULL,
                     &swapchainImageIndex);

    RenderTarget* pRenderTarget = m_pImpl->swapChain->ppRenderTargets[swapchainImageIndex];
    Semaphore*    pRenderCompleteSemaphore = m_pImpl->renderCompleteSemaphore[m_frameIndex];
    Fence*        pRenderCompleteFence = m_pImpl->renderCompleteFences[m_frameIndex];

    FenceStatus fenceStatus;
    getFenceStatus(m_pImpl->renderer, pRenderCompleteFence, &fenceStatus);
    if (fenceStatus == FENCE_STATUS_INCOMPLETE)
    {
        waitForFences(m_pImpl->renderer, 1, &pRenderCompleteFence);
    }

    resetCmdPool(m_pImpl->renderer, m_pImpl->cmdPool[m_frameIndex]);

    Cmd* cmd = m_pImpl->cmds[m_frameIndex];
    beginCmd(cmd);

    RenderTargetBarrier barriers[] =
    {
        { pRenderTarget, RESOURCE_STATE_RENDER_TARGET },
        { m_pImpl->depthBuffer, RESOURCE_STATE_DEPTH_WRITE },
    };

    TextureBarrier textureBarriers[m_textureCount];
    for (uint32_t i = 0; i < m_textureCount; i++)
    {
        textureBarriers[i] = { m_pImpl->videoTexture[i], RESOURCE_STATE_SHADER_RESOURCE };
    }

    cmdResourceBarrier(cmd, 0, nullptr, m_textureCount, textureBarriers, 2, barriers);


    LoadActionsDesc loadActions = {};
    loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
    loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
    loadActions.mClearDepth.depth = 0.0f;
    loadActions.mClearDepth.stencil = 0;
    cmdBindRenderTargets(cmd, 1, &pRenderTarget, m_pImpl->depthBuffer, &loadActions, NULL, NULL, -1, -1);
    cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
    cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

    const uint32_t vertexStride = sizeof(float) * 3 + sizeof(float) * 2; //vec3 + vec2

    cmdBindPipeline(cmd, m_pImpl->videoPipeline);
    cmdBindDescriptorSet(cmd, 0, m_pImpl->videoDescriptorSet);
    cmdBindVertexBuffer(cmd, 1, &m_pImpl->videoVertexBuffer, &vertexStride, NULL);
    cmdDraw(cmd, 6, 0);

    //Reset render target state
    barriers[0] = { pRenderTarget, RESOURCE_STATE_PRESENT };
    cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

    endCmd(cmd);

    QueueSubmitDesc submitDesc = {};
    submitDesc.mCmdCount = 1;
    submitDesc.mSignalSemaphoreCount = 1;
    submitDesc.mWaitSemaphoreCount = 1;
    submitDesc.ppCmds = &cmd;
    submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphore;
    submitDesc.ppWaitSemaphores = &m_pImpl->imageAcquiredSemaphore;
    submitDesc.pSignalFence = pRenderCompleteFence;
    queueSubmit(m_pImpl->graphicsQueue, &submitDesc);
    QueuePresentDesc presentDesc = {};
    presentDesc.mIndex = swapchainImageIndex;
    presentDesc.mWaitSemaphoreCount = 1;
    presentDesc.pSwapChain = m_pImpl->swapChain;
    presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
    presentDesc.mSubmitDone = true;
    queuePresent(m_pImpl->graphicsQueue, &presentDesc);

    m_frameIndex = (m_frameIndex + 1) % IMAGE_COUNT;
}
