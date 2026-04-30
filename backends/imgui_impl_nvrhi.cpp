/*
* Copyright (c) 2014-2025, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

/*
License for Dear ImGui

Copyright (c) 2014-2025 Omar Cornut

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stddef.h>

#include <imgui.h>

#include <vulkan/vulkan.hpp>

#include <queue>

#include <nvrhi/nvrhi.h>
#include <nvrhi/vulkan.h>
#include "imgui_impl_nvrhi.h"

#include <GLFW/glfw3.h>

#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <spirv_cross/spirv_hlsl.hpp>

#include "nvrhi/imgui_pixel.hlsl.spv.h"
#include "nvrhi/imgui_vertex.hlsl.spv.h"


struct VERTEX_CONSTANT_BUFFER
{
    float        mvp[4][4];
};

struct ImplNvrhi_WindowData {
    VkSurfaceKHR Surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR SurfaceFormat;
    VkSwapchainKHR SwapChain = VK_NULL_HANDLE;
    struct SwapChainImage {
        VkImage Image;
        nvrhi::FramebufferHandle Framebuffer;
    };
    std::vector<SwapChainImage> Backbuffers;
    std::vector<VkSemaphore> AcquireSemaphores;
    std::vector<VkSemaphore> PresentSemaphores;
    uint32_t AcquireSemaphoreIndex = 0;
    uint32_t CurrentBackbufferIndex = 0;
    nvrhi::FramebufferInfo FramebufferInfo;
    std::queue<nvrhi::EventQueryHandle> FramesInFlight;
    std::vector<nvrhi::EventQueryHandle> QueryPool;
    uint32_t MaxFramesInFlight = 2;
};

bool ImGui_NVRHI::updateFontTexture()
{
    ImGuiIO& io = ImGui::GetIO();

    // If the font texture exists and is bound to ImGui, we're done.
    // Note: ImGui_Renderer will reset io.Fonts->TexRef when new fonts are added.
    if (fontTexture && io.Fonts->TexRef.GetTexID())
        return true;

    unsigned char *pixels;
    int width, height;

    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (!pixels)
        return false;

    nvrhi::TextureDesc textureDesc;
    textureDesc.width = width;
    textureDesc.height = height;
    textureDesc.format = nvrhi::Format::RGBA8_UNORM;
    textureDesc.debugName = "ImGui font texture";

    fontTexture = m_device->createTexture(textureDesc);

    if (fontTexture == nullptr)
        return false;

    m_commandList->open();

    m_commandList->beginTrackingTextureState(fontTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);

    m_commandList->writeTexture(fontTexture, 0, 0, pixels, width * 4);

    m_commandList->setPermanentTextureState(fontTexture, nvrhi::ResourceStates::ShaderResource);
    m_commandList->commitBarriers();

    m_commandList->close();
    m_device->executeCommandList(m_commandList);

    io.Fonts->TexRef = ImTextureRef(fontTexture.Get());

    return true;
}

TBuiltInResource GetDefaultResources() {
    TBuiltInResource resources = {};

    resources.maxLights = 32;
    resources.maxClipPlanes = 6;
    resources.maxTextureUnits = 32;
    resources.maxTextureCoords = 32;
    resources.maxVertexAttribs = 64;
    resources.maxVertexUniformComponents = 4096;
    resources.maxVaryingFloats = 64;
    resources.maxVertexTextureImageUnits = 32;
    resources.maxCombinedTextureImageUnits = 80;
    resources.maxTextureImageUnits = 32;
    resources.maxFragmentUniformComponents = 4096;
    resources.maxDrawBuffers = 32;
    resources.maxVertexUniformVectors = 128;
    resources.maxVaryingVectors = 8;
    resources.maxFragmentUniformVectors = 16;
    resources.maxVertexOutputVectors = 16;
    resources.maxFragmentInputVectors = 15;
    resources.minProgramTexelOffset = -8;
    resources.maxProgramTexelOffset = 7;
    resources.maxClipDistances = 8;
    resources.maxComputeWorkGroupCountX = 65535;
    resources.maxComputeWorkGroupCountY = 65535;
    resources.maxComputeWorkGroupCountZ = 65535;
    resources.maxComputeWorkGroupSizeX = 1024;
    resources.maxComputeWorkGroupSizeY = 1024;
    resources.maxComputeWorkGroupSizeZ = 64;
    resources.maxComputeUniformComponents = 1024;
    resources.maxComputeTextureImageUnits = 16;
    resources.maxComputeImageUniforms = 8;
    resources.maxComputeAtomicCounters = 8;
    resources.maxComputeAtomicCounterBuffers = 1;
    resources.maxVaryingComponents = 60;
    resources.maxVertexOutputComponents = 64;
    resources.maxGeometryInputComponents = 64;
    resources.maxGeometryOutputComponents = 128;
    resources.maxFragmentInputComponents = 128;
    resources.maxImageUnits = 8;
    resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
    resources.maxCombinedShaderOutputResources = 8;
    resources.maxImageSamples = 0;
    resources.maxVertexImageUniforms = 0;
    resources.maxTessControlImageUniforms = 0;
    resources.maxTessEvaluationImageUniforms = 0;
    resources.maxGeometryImageUniforms = 0;
    resources.maxFragmentImageUniforms = 8;
    resources.maxCombinedImageUniforms = 8;
    resources.maxGeometryTextureImageUnits = 16;
    resources.maxGeometryOutputVertices = 256;
    resources.maxGeometryTotalOutputComponents = 1024;
    resources.maxGeometryUniformComponents = 1024;
    resources.maxGeometryVaryingComponents = 64;
    resources.maxTessControlInputComponents = 128;
    resources.maxTessControlOutputComponents = 128;
    resources.maxTessControlTextureImageUnits = 16;
    resources.maxTessControlUniformComponents = 1024;
    resources.maxTessControlTotalOutputComponents = 4096;
    resources.maxTessEvaluationInputComponents = 128;
    resources.maxTessEvaluationOutputComponents = 128;
    resources.maxTessEvaluationTextureImageUnits = 16;
    resources.maxTessEvaluationUniformComponents = 1024;
    resources.maxTessPatchComponents = 120;
    resources.maxPatchVertices = 32;
    resources.maxTessGenLevel = 64;
    resources.maxViewports = 16;
    resources.maxVertexAtomicCounters = 0;
    resources.maxTessControlAtomicCounters = 0;
    resources.maxTessEvaluationAtomicCounters = 0;
    resources.maxGeometryAtomicCounters = 0;
    resources.maxFragmentAtomicCounters = 8;
    resources.maxCombinedAtomicCounters = 8;
    resources.maxAtomicCounterBindings = 1;
    resources.maxVertexAtomicCounterBuffers = 0;
    resources.maxTessControlAtomicCounterBuffers = 0;
    resources.maxTessEvaluationAtomicCounterBuffers = 0;
    resources.maxGeometryAtomicCounterBuffers = 0;
    resources.maxFragmentAtomicCounterBuffers = 1;
    resources.maxCombinedAtomicCounterBuffers = 1;
    resources.maxAtomicCounterBufferSize = 16384;
    resources.maxTransformFeedbackBuffers = 4;
    resources.maxTransformFeedbackInterleavedComponents = 64;
    resources.maxCullDistances = 8;
    resources.maxCombinedClipAndCullDistances = 8;
    resources.maxSamples = 4;

    resources.limits.nonInductiveForLoops = 1;
    resources.limits.whileLoops = 1;
    resources.limits.doWhileLoops = 1;
    resources.limits.generalUniformIndexing = 1;
    resources.limits.generalAttributeMatrixVectorIndexing = 1;
    resources.limits.generalVaryingIndexing = 1;
    resources.limits.generalSamplerIndexing = 1;
    resources.limits.generalVariableIndexing = 1;
    resources.limits.generalConstantMatrixVectorIndexing = 1;

    return resources;
}

static std::string ReadFile(const std::filesystem::path& filepath) {
    std::string result;
    std::ifstream in(filepath, std::ios::in | std::ios::binary);
    if (in) {
        in.seekg(0, std::ios::end);
        result.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&result[0], result.size());
        in.close();
    }
    else {
        IMGUI_DEBUG_LOG("Could not open file '{}'", filepath.string());
    }

    return result;
}

static EShLanguage NVRHIShaderStageToGlslang(nvrhi::ShaderType stage) {
    switch (stage) {
    case nvrhi::ShaderType::Vertex: return EShLangVertex;
    case nvrhi::ShaderType::Pixel:  return EShLangFragment;
    }

    IMGUI_DEBUG_LOG("Shader Type not supported!");
    IM_ASSERT(false);
    return (EShLanguage)0;
}

static nvrhi::ShaderHandle makeShader(const std::filesystem::path& filePath, nvrhi::ShaderType type, nvrhi::IDevice* device) {
    std::vector<uint32_t> shaderData;

    std::string source = ReadFile(filePath);
    std::filesystem::path shaderFilePath = filePath;
    // === HLSL -> SPIR-V Compilation ===
    EShLanguage glslangStage = NVRHIShaderStageToGlslang(type);
    glslang::TShader shader(glslangStage);

    std::string preamble = R"(
        #define TARGET_VULKAN 1
        #define SPIRV 1
    )";
    shader.setPreamble(preamble.c_str());

    const char* shaderStrings[1] = { source.c_str() };
    shader.setStrings(shaderStrings, 1);
    shader.setEntryPoint("main");
    shader.setSourceEntryPoint("main");

    shader.setEnvInput(glslang::EShSourceHlsl, glslangStage, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_4);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

    TBuiltInResource resources = GetDefaultResources();
    EShMessages messages = (EShMessages)(EShMsgDefault | EShMsgVulkanRules | EShMsgSpvRules);

    if (!shader.parse(&resources, 100, false, messages)) {
        IMGUI_DEBUG_LOG(shader.getInfoLog());
        IMGUI_DEBUG_LOG("HLSL Parsing failed!");
        IM_ASSERT(false);
    }

    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(messages)) {
        IMGUI_DEBUG_LOG(program.getInfoLog());
        IMGUI_DEBUG_LOG("HLSL Linking failed!");
        IM_ASSERT(false);
    }

    glslang::GlslangToSpv(*program.getIntermediate(glslangStage), shaderData);

    auto name = filePath.filename();

    std::ofstream file(name.string() + ".spv.h", std::ios::binary);

    if (type == nvrhi::ShaderType::Vertex)
        file << "#define IMGUI_PRECOMPILED_VERTEX\n";
    else
        file << "#define IMGUI_PRECOMPILED_PIXEL\n";

    if (type == nvrhi::ShaderType::Vertex)
        file << "constexpr uint32_t IMGUI_VERTEX_SPIRV[] = {\n";
    else
        file << "constexpr uint32_t IMGUI_PIXEL_SPIRV[] = {\n";

    for (size_t i = 0; i < shaderData.size(); i++) {
        file << "0x" << std::hex << shaderData[i];
        if (i < shaderData.size() - 1) file << ",";
        if (i % 8 == 7) file << "\n";
    }

    file << "\n};\n";
    if(type == nvrhi::ShaderType::Vertex)
        file << "constexpr size_t IMGUI_VERTEX_SPIRV_SIZE = " << std::dec << shaderData.size() << ";\n";
    else
        file << "constexpr size_t IMGUI_PIXEL_SPIRV_SIZE = " << std::dec << shaderData.size() << ";\n";

    return device->createShader(
        nvrhi::ShaderDesc().setShaderType(type),
        shaderData.data(), shaderData.size() * sizeof(uint32_t));
}

struct Constants {
    ImVec2 invDisplaySize;
    ImVec2 displayPos;
};

void ImGui_NVRHI::CreateWindowForViewport(ImGuiViewport* viewport) {
    ImplNvrhi_WindowData* windowData = new ImplNvrhi_WindowData();
    windowData->SwapChain = VK_NULL_HANDLE;

    if (glfwCreateWindowSurface(m_Instance, (GLFWwindow*)viewport->PlatformHandle, nullptr, &windowData->Surface) != VK_SUCCESS) {
        delete windowData;
        IM_ASSERT(false);
        return;
    }

    uint32_t surfaceFormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, windowData->Surface, &surfaceFormatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, windowData->Surface, &surfaceFormatCount, surfaceFormats.data());
    windowData->SurfaceFormat = surfaceFormats[0];

    viewport->RendererUserData = windowData;
    RecreateSwapchain(viewport);
}

void ImGui_NVRHI::DestroyWindowForViewport(ImGuiViewport* viewport) {
    if (!viewport->RendererUserData) return;

    ImplNvrhi_WindowData* windowData = (ImplNvrhi_WindowData*)viewport->RendererUserData;

    vkDeviceWaitIdle(m_LogicalDevice);

    // Semaphores zerstören
    for (auto& sem : windowData->AcquireSemaphores)
        vkDestroySemaphore(m_LogicalDevice, sem, nullptr);
    for (auto& sem : windowData->PresentSemaphores)
        vkDestroySemaphore(m_LogicalDevice, sem, nullptr);

    if (windowData->SwapChain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(m_LogicalDevice, windowData->SwapChain, nullptr);

    if (windowData->Surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(m_Instance, windowData->Surface, nullptr);

    delete windowData;
    viewport->RendererUserData = nullptr;
}

bool ImGui_NVRHI::init(nvrhi::vulkan::IDevice* device, VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkQueue presentQueue, std::optional<uint32_t> graphicsFamily, std::optional<uint32_t> presentFamily)
{
    m_device = device;

    m_Instance = instance;
    m_PhysicalDevice = physicalDevice;
    m_LogicalDevice = logicalDevice;

    m_GraphicsFamily = graphicsFamily;
    m_PresentFamily = presentFamily;

    m_PresentQueue = presentQueue;

    m_commandList = m_device->createCommandList();

#ifdef IMGUI_PRECOMPILED_VERTEX
    vertexShader = device->createShader(
        nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Vertex),
        IMGUI_VERTEX_SPIRV, IMGUI_VERTEX_SPIRV_SIZE * sizeof(uint32_t));
#else
    vertexShader = makeShader("nvrhi/imgui_vertex.hlsl", nvrhi::ShaderType::Vertex, device);
#endif
#ifdef IMGUI_PRECOMPILED_PIXEL
    pixelShader = device->createShader(
        nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Pixel),
        IMGUI_PIXEL_SPIRV, IMGUI_PIXEL_SPIRV_SIZE * sizeof(uint32_t));
#else
    pixelShader = makeShader("nvrhi/imgui_pixel.hlsl", nvrhi::ShaderType::Pixel, device);
#endif

    if (!vertexShader || !pixelShader)
    {
        IMGUI_DEBUG_LOG("Failed to create an ImGUI shader");
        return false;
    } 

    // create attribute layout object
    nvrhi::VertexAttributeDesc vertexAttribLayout[] = {
        { "POSITION", nvrhi::Format::RG32_FLOAT,  1, 0, offsetof(ImDrawVert,pos), sizeof(ImDrawVert), false },
        { "TEXCOORD", nvrhi::Format::RG32_FLOAT,  1, 0, offsetof(ImDrawVert,uv),  sizeof(ImDrawVert), false },
        { "COLOR",    nvrhi::Format::RGBA8_UNORM, 1, 0, offsetof(ImDrawVert,col), sizeof(ImDrawVert), false },
    };

    shaderAttribLayout = m_device->createInputLayout(vertexAttribLayout, sizeof(vertexAttribLayout) / sizeof(vertexAttribLayout[0]), vertexShader);

    // create PSO
    {
        nvrhi::BlendState blendState;
        blendState.targets[0].setBlendEnable(true)
            .setSrcBlend(nvrhi::BlendFactor::SrcAlpha)
            .setDestBlend(nvrhi::BlendFactor::InvSrcAlpha)
            .setSrcBlendAlpha(nvrhi::BlendFactor::InvSrcAlpha)
            .setDestBlendAlpha(nvrhi::BlendFactor::Zero);

        auto rasterState = nvrhi::RasterState()
            .setFillSolid()
            .setCullNone()
            .setScissorEnable(true)
            .setDepthClipEnable(true);

        auto depthStencilState = nvrhi::DepthStencilState()
            .disableDepthTest()
            .enableDepthWrite()
            .disableStencil()
            .setDepthFunc(nvrhi::ComparisonFunc::Always);

        nvrhi::RenderState renderState;
        renderState.blendState = blendState;
        renderState.depthStencilState = depthStencilState;
        renderState.rasterState = rasterState;

        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.visibility = nvrhi::ShaderType::All;
        layoutDesc.bindings = {
            nvrhi::BindingLayoutItem::ConstantBuffer(0),   // b0 für Constants
            nvrhi::BindingLayoutItem::Texture_SRV(1),      // t1
            nvrhi::BindingLayoutItem::Sampler(2)           // s2
        };
        layoutDesc.bindingOffsets.constantBuffer = 0;
        layoutDesc.bindingOffsets.sampler = 0;
        layoutDesc.bindingOffsets.shaderResource = 0;
        layoutDesc.registerSpace = 0;
        bindingLayout = m_device->createBindingLayout(layoutDesc);

        basePSODesc.primType = nvrhi::PrimitiveType::TriangleList;
        basePSODesc.inputLayout = shaderAttribLayout;
        basePSODesc.VS = vertexShader;
        basePSODesc.PS = pixelShader;
        basePSODesc.renderState = renderState;
        basePSODesc.bindingLayouts = { bindingLayout };

        nvrhi::BufferDesc cbufferDesc;
        cbufferDesc.byteSize = sizeof(Constants);
        cbufferDesc.isConstantBuffer = true;
        cbufferDesc.debugName = "ImGui Constants";

        m_constantBuffer = m_device->createBuffer(cbufferDesc);
    }

    {
        const auto desc = nvrhi::SamplerDesc()
            .setAllAddressModes(nvrhi::SamplerAddressMode::Wrap)
            .setAllFilters(true);

        fontSampler = m_device->createSampler(desc);

        if (fontSampler == nullptr)
            return false;
    }

    ImGui::GetIO().BackendRendererUserData = this;

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

    platform_io.Renderer_CreateWindow = [](ImGuiViewport* viewport) {
        // 'this' geht nicht in static lambda, daher über UserData
        ImGui_NVRHI* backend = (ImGui_NVRHI*)ImGui::GetIO().BackendRendererUserData;
        backend->CreateWindowForViewport(viewport);
        };

    platform_io.Renderer_DestroyWindow = [](ImGuiViewport* viewport) {
        ImGui_NVRHI* backend = (ImGui_NVRHI*)ImGui::GetIO().BackendRendererUserData;
        backend->DestroyWindowForViewport(viewport);
        };

    platform_io.Renderer_SetWindowSize = [](ImGuiViewport* viewport, ImVec2 size) {
        ImGui_NVRHI* backend = (ImGui_NVRHI*)ImGui::GetIO().BackendRendererUserData;
        backend->RecreateSwapchain(viewport);
        };

    return true;
}

bool ImGui_NVRHI::reallocateBuffer(nvrhi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, const bool indexBuffer)
{
    if (buffer == nullptr || size_t(buffer->getDesc().byteSize) < requiredSize)
    {
        nvrhi::BufferDesc desc;
        desc.byteSize = uint32_t(reallocateSize);
        desc.structStride = 0;
        desc.debugName = indexBuffer ? "ImGui index buffer" : "ImGui vertex buffer";
        desc.canHaveUAVs = false;
        desc.isVertexBuffer = !indexBuffer;
        desc.isIndexBuffer = indexBuffer;
        desc.isDrawIndirectArgs = false;
        desc.isVolatile = false;
        desc.initialState = indexBuffer ? nvrhi::ResourceStates::IndexBuffer : nvrhi::ResourceStates::VertexBuffer;
        desc.keepInitialState = true;

        buffer = m_device->createBuffer(desc);

        if (!buffer)
        {
            return false;
        }
    }

    return true;
}

nvrhi::IGraphicsPipeline* ImGui_NVRHI::getPSO(nvrhi::FramebufferInfo const& framebufferInfo)
{
    auto it = m_psoCache.find(framebufferInfo);
    if (it != m_psoCache.end())
        return it->second;

    nvrhi::GraphicsPipelineHandle newPso = m_device->createGraphicsPipeline(basePSODesc, framebufferInfo);
    IM_ASSERT(newPso);

    m_psoCache[framebufferInfo] = newPso;
    return newPso;
}

nvrhi::BindingSetHandle ImGui_NVRHI::getBindingSet(nvrhi::ITexture* texture)
{
    nvrhi::BindingSetDesc desc;

    desc.bindings = {
        nvrhi::BindingSetItem::ConstantBuffer(0, m_constantBuffer),
        nvrhi::BindingSetItem::Texture_SRV(1, texture),
        nvrhi::BindingSetItem::Sampler(2, fontSampler)
    };

    nvrhi::BindingSetHandle binding;
    binding = m_device->createBindingSet(desc, bindingLayout);
    assert(binding);

    return binding;
}

bool ImGui_NVRHI::updateGeometry(nvrhi::ICommandList* commandList, ImDrawData* drawData)
{
    // create/resize vertex and index buffers if needed
    if (!reallocateBuffer(vertexBuffer, 
        drawData->TotalVtxCount * sizeof(ImDrawVert), 
        (drawData->TotalVtxCount + 5000) * sizeof(ImDrawVert), 
        false))
    {
        return false;
    }

    if (!reallocateBuffer(indexBuffer,
        drawData->TotalIdxCount * sizeof(ImDrawIdx),
        (drawData->TotalIdxCount + 5000) * sizeof(ImDrawIdx),
        true))
    {
        return false;
    }

    vtxBuffer.resize(vertexBuffer->getDesc().byteSize / sizeof(ImDrawVert));
    idxBuffer.resize(indexBuffer->getDesc().byteSize / sizeof(ImDrawIdx));

    // copy and convert all vertices into a single contiguous buffer
    ImDrawVert *vtxDst = &vtxBuffer[0];
    ImDrawIdx *idxDst = &idxBuffer[0];

    for(int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList *cmdList = drawData->CmdLists[n];

        memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

        vtxDst += cmdList->VtxBuffer.Size;
        idxDst += cmdList->IdxBuffer.Size;
    }
    
    commandList->writeBuffer(vertexBuffer, &vtxBuffer[0], vertexBuffer->getDesc().byteSize);
    commandList->writeBuffer(indexBuffer, &idxBuffer[0], indexBuffer->getDesc().byteSize);

    return true;
}

void ImGui_NVRHI::renderViewport(nvrhi::FramebufferHandle framebuffer, ImDrawData* drawData) {
    const auto& io = ImGui::GetIO();

    m_commandList->open();
    m_commandList->beginMarker("ImGUI");
    m_commandList->beginTrackingBufferState(m_constantBuffer, nvrhi::ResourceStates::ConstantBuffer);

    float invDisplaySize[2] = { 1.f / drawData->DisplaySize.x, 1.f / drawData->DisplaySize.y };

    Constants constants;
    constants.invDisplaySize.x = invDisplaySize[0];
    constants.invDisplaySize.y = invDisplaySize[1];
    constants.displayPos.x = drawData->DisplayPos.x;
    constants.displayPos.y = drawData->DisplayPos.y;

    m_commandList->writeBuffer(m_constantBuffer, &constants, sizeof(constants));

    if (!updateGeometry(m_commandList, drawData))
    {
        m_commandList->close();
    }

    // handle DPI scaling
    drawData->ScaleClipRects(io.DisplayFramebufferScale);

    // set up graphics state
    nvrhi::GraphicsState drawState;

    drawState.framebuffer = framebuffer;
    assert(drawState.framebuffer);

    drawState.pipeline = getPSO(framebuffer->getFramebufferInfo());
    assert(drawState.pipeline != nullptr);

    drawState.viewport.viewports.push_back(nvrhi::Viewport(
        0,
        drawData->DisplaySize.x * io.DisplayFramebufferScale.x,
        0,
        drawData->DisplaySize.y * io.DisplayFramebufferScale.y,
        0.0f, 1.0f));
    drawState.viewport.scissorRects.resize(1);  // updated below

    nvrhi::VertexBufferBinding vbufBinding;
    vbufBinding.buffer = vertexBuffer;
    vbufBinding.slot = 0;
    vbufBinding.offset = 0;
    drawState.vertexBuffers.push_back(vbufBinding);

    drawState.indexBuffer.buffer = indexBuffer;
    drawState.indexBuffer.format = (sizeof(ImDrawIdx) == 2 ? nvrhi::Format::R16_UINT : nvrhi::Format::R32_UINT);
    drawState.indexBuffer.offset = 0;

    // render command lists
    int vtxOffset = 0;
    int idxOffset = 0;
    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        for (int i = 0; i < cmdList->CmdBuffer.Size; i++)
        {
            const ImDrawCmd* pCmd = &cmdList->CmdBuffer[i];

            if (pCmd->UserCallback)
            {
                pCmd->UserCallback(cmdList, pCmd);
            }
            else {
                nvrhi::BindingSetHandle bindingSet = getBindingSet((nvrhi::ITexture*)pCmd->TexRef.GetTexID());
                drawState.bindings = { bindingSet };
                IM_ASSERT(drawState.bindings[0]);

                drawState.viewport.scissorRects[0] = nvrhi::Rect(
                    int((pCmd->ClipRect.x - drawData->DisplayPos.x) * drawData->FramebufferScale.x),
                    int((pCmd->ClipRect.z - drawData->DisplayPos.x) * drawData->FramebufferScale.y),
                    int((pCmd->ClipRect.y - drawData->DisplayPos.y) * drawData->FramebufferScale.x),
                    int((pCmd->ClipRect.w - drawData->DisplayPos.y) * drawData->FramebufferScale.y));

                auto& rect = drawState.viewport.scissorRects[0];
                rect.minX = std::max(rect.minX, 0);
                rect.minY = std::max(rect.minY, 0);
                rect.maxX = std::max(rect.maxX, 0);
                rect.maxY = std::max(rect.maxY, 0);

                nvrhi::DrawArguments drawArguments;
                drawArguments.vertexCount = pCmd->ElemCount;
                drawArguments.startIndexLocation = idxOffset;
                drawArguments.startVertexLocation = vtxOffset;

                m_commandList->setGraphicsState(drawState);
                m_commandList->drawIndexed(drawArguments);
            }

            idxOffset += pCmd->ElemCount;
        }

        vtxOffset += cmdList->VtxBuffer.Size;
    }

    m_commandList->endMarker();
    m_commandList->close();
    m_device->executeCommandList(m_commandList);
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {
            (uint32_t)width,
            (uint32_t)height
        };

        actualExtent.width = std::clamp(actualExtent.width,
            capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height,
            capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void ImGui_NVRHI::RecreateSwapchain(ImGuiViewport* viewport) {
    ImplNvrhi_WindowData* windowData = (ImplNvrhi_WindowData*)(viewport->RendererUserData);

    vkDeviceWaitIdle(m_LogicalDevice);

    // Alte Semaphores destroyen
    for (auto& sem : windowData->AcquireSemaphores)
        vkDestroySemaphore(m_LogicalDevice, sem, nullptr);
    for (auto& sem : windowData->PresentSemaphores)
        vkDestroySemaphore(m_LogicalDevice, sem, nullptr);
    windowData->AcquireSemaphores.clear();
    windowData->PresentSemaphores.clear();

    // Alte Backbuffers clearen
    windowData->Backbuffers.clear();

    // FramesInFlight leeren
    while (!windowData->FramesInFlight.empty())
        windowData->FramesInFlight.pop();
    windowData->QueryPool.clear();

    // Format prüfen
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, windowData->Surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, windowData->Surface, &formatCount, formats.data());

    // Present Mode prüfen
    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, windowData->Surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, windowData->Surface, &modeCount, modes.data());

    VkSurfaceCapabilitiesKHR capabilities = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, windowData->Surface, &capabilities);

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = windowData->Surface;

    createInfo.minImageCount = capabilities.minImageCount + 1;
    createInfo.imageFormat = windowData->SurfaceFormat.format;
    createInfo.imageColorSpace = windowData->SurfaceFormat.colorSpace;
    createInfo.imageExtent = chooseSwapExtent(capabilities, (GLFWwindow*)viewport->PlatformHandle);
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; ;

    if (m_GraphicsFamily.value() != m_PresentFamily.value()) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        uint32_t queueFamilyIndices[] = { m_GraphicsFamily.value(), m_PresentFamily.value() };
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = windowData->SwapChain;

    VkSwapchainKHR oldSwapchain = windowData->SwapChain;
    VkResult result = vkCreateSwapchainKHR(m_LogicalDevice, &createInfo, nullptr, &windowData->SwapChain);
    if (result == VK_SUCCESS) {
        if (oldSwapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_LogicalDevice, oldSwapchain, nullptr);
        }
    }
    else {
        IM_ASSERT(false);
    }

    auto textureDesc = nvrhi::TextureDesc()
        .setDimension(nvrhi::TextureDimension::Texture2D)
        .setFormat(nvrhi::Format::RGBA8_UNORM)
        .setWidth(createInfo.imageExtent.width)
        .setHeight(createInfo.imageExtent.height)
        .setIsRenderTarget(true)
        .setDebugName("Swap Chain Image")
        .setInitialState(nvrhi::ResourceStates::Present)
        .setKeepInitialState(true);

    auto depthDesc = nvrhi::TextureDesc()
        .setDimension(nvrhi::TextureDimension::Texture2D)
        .setFormat(nvrhi::Format::D32S8)
        .setWidth(createInfo.imageExtent.width)
        .setHeight(createInfo.imageExtent.height)
        .setIsRenderTarget(true)
        .setDebugName("Swap Chain Depth");

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(m_LogicalDevice, windowData->SwapChain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(m_LogicalDevice, windowData->SwapChain, &imageCount, images.data());

    for (uint32_t i = 0; i < imageCount; i++) {

        ImplNvrhi_WindowData::SwapChainImage sci;
        sci.Image = images[i];

        auto textureHandle = m_device->createHandleForNativeTexture(
            nvrhi::ObjectTypes::VK_Image,
            nvrhi::Object(images[i]),
            textureDesc);
        IM_ASSERT(textureHandle);

        nvrhi::FramebufferDesc createInfo = nvrhi::FramebufferDesc()
            .addColorAttachment(textureHandle);
        sci.Framebuffer = m_device->createFramebuffer(createInfo);
        IM_ASSERT(sci.Framebuffer);
        windowData->Backbuffers.push_back(sci);
    }

    windowData->FramebufferInfo = windowData->Backbuffers[0].Framebuffer->getFramebufferInfo();

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.flags = 0;

    size_t const numPresentSemaphores = windowData->Backbuffers.size();
    windowData->PresentSemaphores.resize(numPresentSemaphores);
    for (uint32_t i = 0; i < numPresentSemaphores; ++i)
    {
        IM_ASSERT(vkCreateSemaphore(m_LogicalDevice, &semaphoreInfo, nullptr, &windowData->PresentSemaphores[i]) == VK_SUCCESS);
    }

    size_t const numAcquireSemaphores = std::max(size_t(windowData->MaxFramesInFlight),
        windowData->Backbuffers.size());
    windowData->AcquireSemaphores.resize(numAcquireSemaphores);
    for (uint32_t i = 0; i < numAcquireSemaphores; ++i)
    {
        IM_ASSERT(vkCreateSemaphore(m_LogicalDevice, &semaphoreInfo, nullptr, &windowData->AcquireSemaphores[i]) == VK_SUCCESS);
    }
    windowData->AcquireSemaphoreIndex = 0;
    viewport->RendererUserData = windowData;
}

nvrhi::FramebufferHandle ImGui_NVRHI::GetFramebufferForViewport(ImGuiViewport* viewport) {
    if (viewport->PlatformWindowCreated && viewport->RendererUserData != nullptr) {
        ImplNvrhi_WindowData* windowData = (ImplNvrhi_WindowData*)(viewport->RendererUserData);

        const auto& semaphore = windowData->AcquireSemaphores[windowData->AcquireSemaphoreIndex];
        VkResult res;
        int const maxAttempts = 3;
        for (int attempt = 0; attempt < maxAttempts; ++attempt) {
            res = vkAcquireNextImageKHR(
                m_LogicalDevice,
                windowData->SwapChain,
                std::numeric_limits<uint64_t>::max(), // timeout
                semaphore,
                VK_NULL_HANDLE,
                &windowData->CurrentBackbufferIndex);

            if ((res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) && attempt < maxAttempts) {
                RecreateSwapchain(viewport);
            }
            else
                break;
        }

        windowData->AcquireSemaphoreIndex = (windowData->AcquireSemaphoreIndex + 1) % windowData->AcquireSemaphores.size();

        if (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR) { // Suboptimal is considered a success
            m_device->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, semaphore, 0);
        }

        return windowData->Backbuffers[windowData->CurrentBackbufferIndex].Framebuffer;
    }
    return nullptr;
}

void ImGui_NVRHI::EndFrame(ImGuiViewport* viewport) {
    ImplNvrhi_WindowData* windowData = (ImplNvrhi_WindowData*)(viewport->RendererUserData);
    VkSemaphore semaphore = windowData->PresentSemaphores[windowData->CurrentBackbufferIndex];

    ((nvrhi::vulkan::IDevice*)m_device)->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, semaphore, 0);

    m_device->executeCommandLists(nullptr, 0);

    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &windowData->SwapChain;
    info.pImageIndices = &windowData->CurrentBackbufferIndex;

    vkQueuePresentKHR(m_PresentQueue, &info);

    while (windowData->FramesInFlight.size() >= windowData->MaxFramesInFlight) {
        auto query = windowData->FramesInFlight.front();
        windowData->FramesInFlight.pop();
        m_device->waitEventQuery(query);
        windowData->QueryPool.push_back(query);
    }

    nvrhi::EventQueryHandle query;
    if (!windowData->QueryPool.empty()) {
        query = windowData->QueryPool.back();
        windowData->QueryPool.pop_back();
    }
    else {
        query = m_device->createEventQuery();
    }

    m_device->resetEventQuery(query);
    m_device->setEventQuery(query, nvrhi::CommandQueue::Graphics);
    windowData->FramesInFlight.push(query);
}

bool ImGui_NVRHI::render(nvrhi::IFramebuffer* mainFramebuffer)
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

    // Für jeden Viewport (Main Window + ausgezogene Windows)
    for (int n = 0; n < platform_io.Viewports.Size; n++)
    {
        ImGuiViewport* viewport = platform_io.Viewports[n];

        if (viewport->Flags & ImGuiViewportFlags_IsMinimized) continue;

        nvrhi::FramebufferHandle framebuffer;

        // Hole den Framebuffer für dieses Viewport
        // Das ist Platform-spezifisch (Windows/Vulkan)
        if (viewport == ImGui::GetMainViewport()) {
            framebuffer = mainFramebuffer;
        }
        else {
            framebuffer = GetFramebufferForViewport(viewport);
        }

        if (!framebuffer) continue;

        ImDrawData* draw_data = viewport->DrawData;
        if (!draw_data) continue;

        // Rendere ImGui für dieses Viewport
        renderViewport(framebuffer, draw_data);

        if (viewport != ImGui::GetMainViewport()) {
            EndFrame(viewport);
        }
    }

    return true;
}

void ImGui_NVRHI::backbufferResizing()
{
    m_psoCache.clear();
}

void ImGui_NVRHI::destroy() {
    if(m_device)
        m_device->waitForIdle();

    fontTexture = nullptr;
    fontSampler = nullptr;
    vertexBuffer = nullptr;
    indexBuffer = nullptr;
    m_constantBuffer = nullptr;
    m_commandList = nullptr;
    basePSODesc.bindingLayouts[0] = nullptr;
    basePSODesc.PS = nullptr;
    basePSODesc.VS = nullptr;
    m_psoCache.clear();
    m_bindingSet = nullptr;
    bindingLayout = nullptr;
    shaderAttribLayout = nullptr;
    pixelShader = nullptr;
    vertexShader = nullptr;

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererUserData = nullptr;

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_CreateWindow = nullptr;
    platform_io.Renderer_DestroyWindow = nullptr;
    platform_io.Renderer_SetWindowSize = nullptr;

    m_device = nullptr;
}
