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

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <stdint.h>

#include <nvrhi/nvrhi.h>

#include <imgui.h>

struct ImGui_NVRHI
{
    nvrhi::vulkan::IDevice* m_device;
    nvrhi::CommandListHandle m_commandList;

    nvrhi::ShaderHandle vertexShader;
    nvrhi::ShaderHandle pixelShader;
    nvrhi::InputLayoutHandle shaderAttribLayout;

    nvrhi::TextureHandle fontTexture;
    nvrhi::SamplerHandle fontSampler;

    nvrhi::BufferHandle vertexBuffer;
    nvrhi::BufferHandle indexBuffer;

    nvrhi::BufferHandle m_constantBuffer;
    nvrhi::BindingSetHandle m_bindingSet;

    nvrhi::BindingLayoutHandle bindingLayout;
    nvrhi::GraphicsPipelineDesc basePSODesc;

    std::unordered_map<nvrhi::FramebufferInfo, nvrhi::GraphicsPipelineHandle> m_psoCache;

    VkInstance m_Instance;
    VkPhysicalDevice m_PhysicalDevice;
    VkDevice m_LogicalDevice;
    VkQueue m_PresentQueue;

    std::optional<uint32_t> m_GraphicsFamily;
    std::optional<uint32_t> m_PresentFamily;

    std::vector<ImDrawVert> vtxBuffer;
    std::vector<ImDrawIdx> idxBuffer;

    bool init(nvrhi::vulkan::IDevice* device, VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkQueue presentQueue, std::optional<uint32_t> graphicsFamily, std::optional<uint32_t> presentFamily);
    bool updateFontTexture();
    bool render(nvrhi::IFramebuffer* framebuffer);
    void backbufferResizing();
    void destroy();

private:
    bool reallocateBuffer(nvrhi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer);

    void renderViewport(nvrhi::FramebufferHandle framebuffer, ImDrawData* drawData);

    void RecreateSwapchain(ImGuiViewport* viewport);

    void CreateWindowForViewport(ImGuiViewport* viewport);
    void DestroyWindowForViewport(ImGuiViewport* viewport);

    nvrhi::FramebufferHandle GetFramebufferForViewport(ImGuiViewport* viewport);

    void EndFrame(ImGuiViewport* viewport);


    nvrhi::IGraphicsPipeline* getPSO(nvrhi::FramebufferInfo const& framebufferInfo);
    nvrhi::BindingSetHandle getBindingSet(nvrhi::ITexture* texture);
    bool updateGeometry(nvrhi::ICommandList* commandList, ImDrawData* drawData);
};
