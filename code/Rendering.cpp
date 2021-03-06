#include "Rendering.h"
#include "MiscUtil.h"
#include "Ring.h"
#include "Table.h"
#include "Os.h"
#include <cassert>

#include "VulkanUtil.h"

namespace gg {

static uint32_t const cSpriteVertexBytecode[] = {
    #include "Sprite.vertex.num"
};
static uint32_t const cSpriteFragmentBytecode[] = {
    #include "Sprite.fragment.num"
};
static uint32_t const cPresentVertexBytecode[] = {
    #include "Present.vertex.num"
};
static uint32_t const cPresentFragmentBytecode[] = {
    #include "Present.fragment.num"
};

void Rendering::addImage(float x, float y, Image const& image) {
    imagePrims_.addLast({x, y, image});
}

struct PhysicalDeviceInfo {
    VkPhysicalDevice device = {};
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    unsigned graphicsQueueFamily = ~0;
    unsigned transferQueueFamily = ~0;

    PhysicalDeviceInfo(VkInstance instance, char const**errorMsg) {
        if (!instance) {
            return;
        }
        VkPhysicalDevice& physicalDevice = device;
        uint32_t physicalDeviceCount = 1;
        vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, &physicalDevice);
        assert(physicalDeviceCount);

        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

        {
            VkPhysicalDeviceProperties physicalDeviceProps;
            vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProps);
        }

        {
            VkQueueFamilyProperties queueFamilyProps[8];
            uint32_t queueFamilyCount = gg::CountOf(queueFamilyProps);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProps);
            graphicsQueueFamily = gg::vk::FindQueueFamily(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, queueFamilyProps, queueFamilyCount);
            transferQueueFamily = gg::vk::FindQueueFamily(VK_QUEUE_TRANSFER_BIT, queueFamilyProps, queueFamilyCount);
            assert(errorMsg != nullptr || std::max(graphicsQueueFamily, transferQueueFamily) < queueFamilyCount);
            if (std::max(graphicsQueueFamily, transferQueueFamily) >= queueFamilyCount) {
                *errorMsg = "Failed to find required vulkan queues";
                return;
            }
        }
    }
};

struct Rendering::Hub::Platform {
    static VkInstance MakeInstance(char const** errorMsg) {
        VkApplicationInfo applicationInfo = {};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "gg";
        applicationInfo.engineVersion = 1;
        applicationInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);

        VkInstanceCreateInfo instanceInfo = { };
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &applicationInfo;

    #ifdef GG_DEBUG
        char const* layers[] = { "VK_LAYER_LUNARG_standard_validation" };
        char const* extensions[] = { "VK_KHR_surface", "VK_KHR_win32_surface", "VK_EXT_debug_report" };
        instanceInfo.enabledLayerCount = gg::CountOf(layers);
        instanceInfo.ppEnabledLayerNames = layers;
        instanceInfo.enabledExtensionCount = gg::CountOf(extensions);
        instanceInfo.ppEnabledExtensionNames = extensions;
    #else
        char const* extensions[] = { "VK_KHR_surface", "VK_KHR_win32_surface" };
        instanceInfo.enabledExtensionCount = gg::CountOf(extensions);
        instanceInfo.ppEnabledExtensionNames = extensions;
    #endif

        VkInstance instance = {};
        VkResult result = vkCreateInstance(&instanceInfo, nullptr, &instance);
        assert(errorMsg != nullptr || result == VK_SUCCESS);
        if (result != VK_SUCCESS) {
            *errorMsg = "Failed to create vulkan instance";
        }
        return instance;
    }

    static VkDevice MakeDevice(PhysicalDeviceInfo const& physical, char const** errorMsg) {
        if (!physical.device) {
            return {};
        }
        VkPhysicalDevice physicalDevice = physical.device;

        float const graphicsQueuePriority = 1.f;
        float const transferQueuePriority = 0.5f;

        VkDeviceQueueCreateInfo queueCreateInfos[2] = {};
        queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[0].queueFamilyIndex = physical.graphicsQueueFamily;
        queueCreateInfos[0].queueCount = 1;
        queueCreateInfos[0].pQueuePriorities = &graphicsQueuePriority;
        queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[1].queueFamilyIndex = physical.transferQueueFamily;
        queueCreateInfos[1].queueCount = 1;
        queueCreateInfos[1].pQueuePriorities = &transferQueuePriority;

        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = gg::CountOf(queueCreateInfos);
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
    #ifdef GG_DEBUG
        char const* deviceLayers[] = { "VK_LAYER_LUNARG_standard_validation" };
        deviceCreateInfo.enabledLayerCount = gg::CountOf(deviceLayers);
        deviceCreateInfo.ppEnabledLayerNames = deviceLayers;
    #endif
        char const* deviceExtensions[] = { "VK_KHR_swapchain" };
        deviceCreateInfo.enabledExtensionCount = gg::CountOf(deviceExtensions);
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

        VkPhysicalDeviceFeatures features = {};
        features.shaderStorageImageExtendedFormats = VK_TRUE;
        features.shaderClipDistance = VK_TRUE;
        deviceCreateInfo.pEnabledFeatures = &features;

        VkDevice device = {};
        VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
        assert(errorMsg != nullptr || result == VK_SUCCESS);
        if (result != VK_SUCCESS) {
            *errorMsg = "Failed to create vulkan device";
        }
        return device;
    }

    static VkQueue GetDeviceQueue(VkDevice device, unsigned queueFamily, unsigned queueIndex) {
        if (!device) {
            return {};
        }
        VkQueue queue;
        vkGetDeviceQueue(device, queueFamily, queueIndex, &queue);
        return queue;
    }

    Platform(char const** errorMsg)
        : instance(MakeInstance(errorMsg))
        , physical(instance, errorMsg)
        , device(MakeDevice(physical, errorMsg))
        , graphicsQueue(GetDeviceQueue(device, physical.graphicsQueueFamily, 0))
        , transferQueue(GetDeviceQueue(device, physical.transferQueueFamily, 0))
        , graphicsCommandBufferDispenser(device, physical.graphicsQueueFamily)
        , transferCommandBufferDispenser(device, physical.transferQueueFamily)
    {
        if (errorMsg && *errorMsg) {
            return;
        }
        {
            VkSemaphoreCreateInfo createInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            vkCreateSemaphore(device, &createInfo, nullptr, &renderingFinishedSemaphore);
            vkCreateSemaphore(device, &createInfo, nullptr, &transfersFinishedSemaphore);
        }
        // samplers
        {
            VkSamplerCreateInfo createInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
            createInfo.magFilter = VK_FILTER_LINEAR;
            createInfo.minFilter = VK_FILTER_LINEAR;
            createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            createInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
            VkResult result = vkCreateSampler(device, &createInfo, nullptr, &linearBlackBorderSampler);
            assert(result == VK_SUCCESS);
        }
        // sprite shaders
        {
            VkShaderModuleCreateInfo createInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            createInfo.codeSize = sizeof(cSpriteVertexBytecode);
            createInfo.pCode = cSpriteVertexBytecode;
            VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &spriteVertexShader);
            assert(result == VK_SUCCESS);
            createInfo.codeSize = sizeof(cSpriteFragmentBytecode);
            createInfo.pCode = cSpriteFragmentBytecode;
            result = vkCreateShaderModule(device, &createInfo, nullptr, &spriteFragmentShader);
            assert(result == VK_SUCCESS);
        }
        // present shaders
        {
            VkShaderModuleCreateInfo createInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            createInfo.codeSize = sizeof(cPresentVertexBytecode);
            createInfo.pCode = cPresentVertexBytecode;
            VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &presentVertexShader);
            assert(result == VK_SUCCESS);
            createInfo.codeSize = sizeof(cPresentFragmentBytecode);
            createInfo.pCode = cPresentFragmentBytecode;
            result = vkCreateShaderModule(device, &createInfo, nullptr, &presentFragmentShader);
            assert(result == VK_SUCCESS);
        }
        // sprite descriptor set
        {
            //layout (binding = 0) sampler linearSampler;
            //layout (binding = 1) Buffer<float4> clut;
            //layout (binding = 2) Texture2D<uint> texels;
            //layout (binding = 3) Buffer<float2> positions;
            VkDescriptorSetLayoutBinding bindings[] = {
                {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &linearBlackBorderSampler},
                {1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
                {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
                {3, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
            };
            VkDescriptorSetLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            createInfo.bindingCount = gg::CountOf(bindings);
            createInfo.pBindings = bindings;
            VkResult result = vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &spriteDescriptorSetLayout);
            assert(result == VK_SUCCESS);
        }
        // present descriptor set
        {
            //layout (binding = 0) sampler linearSampler;
            //layout (binding = 1) Texture2D<uint> source;
            VkDescriptorSetLayoutBinding bindings[] = {
                {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &linearBlackBorderSampler},
                {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            VkDescriptorSetLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            createInfo.bindingCount = gg::CountOf(bindings);
            createInfo.pBindings = bindings;
            VkResult result = vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &presentDescriptorSetLayout);
            assert(result == VK_SUCCESS);
        }
        // sprite pipeline layout
        {
            VkPipelineLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            createInfo.setLayoutCount = 1;
            createInfo.pSetLayouts = &spriteDescriptorSetLayout;
            VkResult result = vkCreatePipelineLayout(device, &createInfo, nullptr, &spritePipelineLayout);
            assert(result == VK_SUCCESS);
        }
        // present pipeline layout
        {
            VkPipelineLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            createInfo.setLayoutCount = 1;
            createInfo.pSetLayouts = &presentDescriptorSetLayout;
            VkResult result = vkCreatePipelineLayout(device, &createInfo, nullptr, &presentPipelineLayout);
            assert(result == VK_SUCCESS);
        }
        // sprite render pass
        {
            VkAttachmentDescription attachment = {};
            attachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentReference attachmentRef = {};
            attachmentRef.attachment = 0;
            attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &attachmentRef;

            VkRenderPassCreateInfo createInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            createInfo.attachmentCount = 1;
            createInfo.pAttachments = &attachment;
            createInfo.subpassCount = 1;
            createInfo.pSubpasses = &subpass;
            //createInfo.dependencyCount = blueprint.dependencies.count();
            //createInfo.pDependencies = dependencies;

            VkResult result = vkCreateRenderPass(device, &createInfo, nullptr, &spriteRenderPass);
            assert(result == VK_SUCCESS);
        }
        // sprite pipeline
        {
            VkPipelineShaderStageCreateInfo stages[] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[0].module = spriteVertexShader;
            stages[1].module = spriteFragmentShader;
            stages[0].pName = "main";
            stages[1].pName = "main";
            VkVertexInputBindingDescription vertexBindings[] = {
                {0, 4, VK_VERTEX_INPUT_RATE_INSTANCE},
            };
            VkVertexInputAttributeDescription vertexAttributes[] = {
                {0, 0, VK_FORMAT_R32_UINT, 0},
            };
            VkPipelineVertexInputStateCreateInfo vertexInput = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            vertexInput.vertexBindingDescriptionCount = gg::CountOf(vertexBindings);
            vertexInput.pVertexBindingDescriptions = vertexBindings;
            vertexInput.vertexAttributeDescriptionCount = gg::CountOf(vertexAttributes);
            vertexInput.pVertexAttributeDescriptions = vertexAttributes;
            VkPipelineInputAssemblyStateCreateInfo inputAssembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineRasterizationStateCreateInfo rasterization = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            rasterization.rasterizerDiscardEnable = VK_TRUE;
            rasterization.lineWidth = 1.f;
            VkPipelineColorBlendAttachmentState attachments[] = {
                {VK_TRUE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, 0xf},
            };
            VkPipelineColorBlendStateCreateInfo colorBlend = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            colorBlend.attachmentCount = gg::CountOf(attachments);
            colorBlend.pAttachments = attachments;
            VkGraphicsPipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            createInfo.flags = 0;
            createInfo.stageCount = gg::CountOf(stages);
            createInfo.pStages = stages;
            createInfo.pVertexInputState = &vertexInput;
            createInfo.pInputAssemblyState = &inputAssembly;
            createInfo.pRasterizationState = &rasterization;
            createInfo.pColorBlendState = &colorBlend;
            createInfo.layout = spritePipelineLayout;
            createInfo.renderPass = spriteRenderPass;
            VkResult result = vkCreateGraphicsPipelines(device, nullptr, 1, &createInfo, nullptr, &spritePipeline);
            assert(result == VK_SUCCESS);
        }
    }
    ~Platform() {
        vkDestroyPipeline(device, spritePipeline, nullptr);
        vkDestroyRenderPass(device, spriteRenderPass, nullptr);
        vkDestroyPipelineLayout(device, presentPipelineLayout, nullptr);
        vkDestroyPipelineLayout(device, spritePipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, presentDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, spriteDescriptorSetLayout, nullptr);
        vkDestroyShaderModule(device, presentFragmentShader, nullptr);
        vkDestroyShaderModule(device, spriteFragmentShader, nullptr);
        vkDestroyShaderModule(device, presentVertexShader, nullptr);
        vkDestroyShaderModule(device, spriteVertexShader, nullptr);
        vkDestroySampler(device, linearBlackBorderSampler, nullptr);
        if (presentImageAcquiredFence) {
            vkDestroyFence(device, presentImageAcquiredFence, nullptr);
        }
        if (presentImageAcquiredSemaphore) {
            vkDestroySemaphore(device, presentImageAcquiredSemaphore, nullptr);
        }
        flushWaitAndPurgeTransfers();
        transferCommandBufferDispenser.teardown(device);

        flushWaitAndPurgeGraphics();
        graphicsCommandBufferDispenser.teardown(device);

        vkDestroySemaphore(device, transfersFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, renderingFinishedSemaphore, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
    }
    void flushTransfers() {
        VkCommandBuffer commandBuffer = transferCommandBufferDispenser.finish(transferQueueTracker);
        if (commandBuffer != VK_NULL_HANDLE) {
            VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &transfersFinishedSemaphore;
            transferQueueTracker.doSubmission(device, transferQueue, submitInfo);
            graphicsWaitSemaphores.addLast(transfersFinishedSemaphore);
            graphicsWaitDstStageMasks.addLast(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        }
    }
    void purgeTransferResources() {
        transferBufferDestructionFifo.flushSignalled(device, transferQueueTracker);
        transferDeviceMemoryFreeFifo.flushSignalled(device, transferQueueTracker);
    }
    void flushWaitAndPurgeTransfers() {
        transferCommandBufferDispenser.getOrCreate(device, transferQueueTracker);   // ensure something to submit, so final fence can get signalled
        flushTransfers();
        transferQueueTracker.waitForAll(device);
        purgeTransferResources();
    }
    void flushGraphics(Span<VkSemaphore> signalSemaphores) {
        VkCommandBuffer commandBuffer = graphicsCommandBufferDispenser.finish(graphicsQueueTracker);
        if (commandBuffer != VK_NULL_HANDLE) {
            assert(graphicsWaitSemaphores.count() == graphicsWaitDstStageMasks.count());
            VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submitInfo.waitSemaphoreCount = graphicsWaitSemaphores.count();
            submitInfo.pWaitSemaphores = graphicsWaitSemaphores.begin();
            submitInfo.pWaitDstStageMask = graphicsWaitDstStageMasks.begin();
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            submitInfo.signalSemaphoreCount = signalSemaphores.count();
            submitInfo.pSignalSemaphores = signalSemaphores.begin();

            graphicsQueueTracker.doSubmission(device, graphicsQueue, submitInfo);

            graphicsWaitSemaphores.removeAll();
            graphicsWaitDstStageMasks.removeAll();
        }
    }
    void purgeGraphicsResources() {
        graphicsBufferDestructionFifo.flushSignalled(device, graphicsQueueTracker);
        graphicsImageDestructionFifo.flushSignalled(device, graphicsQueueTracker);
        graphicsDeviceMemoryFreeFifo.flushSignalled(device, graphicsQueueTracker);
        graphicsSwapchainDestructionFifo.flushSignalled(device, graphicsQueueTracker);
        graphicsSurfaceDestructionFifo.flushSignalled(instance, graphicsQueueTracker);
        graphicsRenderPassDestructionFifo.flushSignalled(device, graphicsQueueTracker);
        graphicsPipelineDestructionFifo.flushSignalled(device, graphicsQueueTracker);
    }
    void flushWaitAndPurgeGraphics() {
        graphicsCommandBufferDispenser.getOrCreate(device, graphicsQueueTracker);   // ensure something to submit, so final fence can get signalled
        flushGraphics({});
        graphicsQueueTracker.waitForAll(device);
        purgeGraphicsResources();
    }

    VkInstance instance;
    PhysicalDeviceInfo physical;
    VkDevice device;
    VkQueue graphicsQueue = nullptr;
    VkQueue transferQueue = nullptr;

    VkSemaphore renderingFinishedSemaphore = {};
    VkSemaphore transfersFinishedSemaphore = {};

    gg::vk::CommandBufferDispenser graphicsCommandBufferDispenser;
    gg::vk::QueueTracker graphicsQueueTracker;
    gg::vk::DestructionFifo<VkBuffer> graphicsBufferDestructionFifo;
    gg::vk::DestructionFifo<VkImage> graphicsImageDestructionFifo;
    gg::vk::DestructionFifo<VkDeviceMemory> graphicsDeviceMemoryFreeFifo;
    gg::vk::DestructionFifo<VkSwapchainKHR> graphicsSwapchainDestructionFifo;
    gg::vk::DestructionFifo<VkSurfaceKHR> graphicsSurfaceDestructionFifo;
    gg::vk::DestructionFifo<VkRenderPass> graphicsRenderPassDestructionFifo;
    gg::vk::DestructionFifo<VkPipeline> graphicsPipelineDestructionFifo;
    Array<VkSemaphore> graphicsWaitSemaphores;
    Array<VkPipelineStageFlags> graphicsWaitDstStageMasks;

    gg::vk::CommandBufferDispenser transferCommandBufferDispenser;
    gg::vk::QueueTracker transferQueueTracker;
    gg::vk::DestructionFifo<VkBuffer> transferBufferDestructionFifo;
    gg::vk::DestructionFifo<VkDeviceMemory> transferDeviceMemoryFreeFifo;

    VkSemaphore presentImageAcquiredSemaphore = {};
    VkFence presentImageAcquiredFence = {};
    VkSampler linearBlackBorderSampler = {};
    VkShaderModule spriteVertexShader = {};
    VkShaderModule presentVertexShader = {};
    VkShaderModule spriteFragmentShader = {};
    VkShaderModule presentFragmentShader = {};
    VkDescriptorSetLayout spriteDescriptorSetLayout = {};
    VkDescriptorSetLayout presentDescriptorSetLayout = {};
    VkPipelineLayout spritePipelineLayout = {};
    VkPipelineLayout presentPipelineLayout = {};
    VkRenderPass spriteRenderPass = {};
    VkPipeline spritePipeline = {};

    struct StagingBuffer;
};

struct Rendering::Hub::PresentationSurface {
    VkSurfaceKHR surface;
    VkRenderPass presentRenderPass;
    VkPipeline presentPipeline;
    VkSwapchainKHR swapchain;
    VkImage swapchainImages[2];     // (owned by swapchain, do not destroy)
    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D extent;
    uint32_t acquiredImageIndex;
};

template<class T_Id>
struct Resource {
    Resource()
        : device(nullptr)
        , id({}) {
    }
    Resource(VkDevice device, unsigned id)
        : device(device)
        , id({id}) {
    }
    Resource(Resource&& source)
        : device(source.device)
        , id(std::exchange(source.id, {})) {
    }
    T_Id assignIndex(unsigned index) {
        id = {index + 1};
        return id;
    }

    VkDevice device;
    T_Id id;
};

template<class T_Id>
struct ResourceIdTraits {
    static unsigned GetIndex(T_Id id) {
        return id.value - 1;
    }
};
template<> struct ResourceIdTraitsDefault<Rendering::PipelineId> : public ResourceIdTraits<Rendering::PipelineId> {};
template<> struct ResourceIdTraitsDefault<Rendering::ImageId> : public ResourceIdTraits<Rendering::ImageId> {};
template<> struct ResourceIdTraitsDefault<Rendering::TilesetId> : public ResourceIdTraits<Rendering::TilesetId> {};

struct Rendering::Hub::PipelineResource : public Resource<PipelineId> {
    using Resource::Resource;
    PipelineResource() = default;
    PipelineResource(PipelineResource&&) = default;
    ~PipelineResource() {
        if (fragmentShader) {
            vkDestroyShaderModule(device, fragmentShader, nullptr);
        }
        if (vertexShader) {
            vkDestroyShaderModule(device, vertexShader, nullptr);
        }
    }

    gg::vk::MovableHandle<VkShaderModule> vertexShader;
    gg::vk::MovableHandle<VkShaderModule> fragmentShader;
    WindowHandle displayWindow;
};

struct Rendering::Hub::ImageResource : public Resource<ImageId> {
    using Resource::Resource;
    ImageResource() = default;
    ImageResource(ImageResource&&) = default;
    ~ImageResource() {
        if (imageView) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        if (deviceMemory) {
            vkFreeMemory(device, deviceMemory, nullptr);
        }
        if (image) {
            vkDestroyImage(device, image, nullptr);
        }
    }
    gg::vk::MovableHandle<VkImage> image;
    gg::vk::MovableHandle<VkDeviceMemory> deviceMemory;
    gg::vk::MovableHandle<VkImageView> imageView;
    unsigned width;
    unsigned height;
    VkFormat format;
};

struct Rendering::Hub::TilesetResource : public Resource<TilesetId> {
    using Resource::Resource;
    TilesetResource() = default;
    TilesetResource(TilesetResource&&) = default;
    ~TilesetResource() {
        if (image) {
            vkDestroyImage(device, image, nullptr);
        }
    }
    gg::vk::MovableHandle<VkImage> image;
};

struct Rendering::Hub::Platform::StagingBuffer {
    StagingBuffer(Platform const& platform, VkDeviceSize size)
        : device(platform.device) {
        VkBufferCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.size = size;
        createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        createInfo.queueFamilyIndexCount = 1;
        createInfo.pQueueFamilyIndices = &platform.physical.transferQueueFamily;
        VkResult result = vkCreateBuffer(device, &createInfo, nullptr, &buffer);
        assert(result == VK_SUCCESS);

        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = gg::vk::FindMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memoryRequirements.memoryTypeBits, platform.physical.memoryProperties, nullptr);
        result = vkAllocateMemory(device, &allocInfo, nullptr, &deviceMemory);
        assert(result == VK_SUCCESS);

        vkBindBufferMemory(device, buffer, deviceMemory, 0);
    }
    ~StagingBuffer() {
        if (buffer) {
            vkDestroyBuffer(device, buffer, nullptr);
        }
        if (deviceMemory) {
            vkFreeMemory(device, deviceMemory, nullptr);
        }
    }

    VkDevice device;
    gg::vk::MovableHandle<VkBuffer> buffer;
    gg::vk::MovableHandle<VkDeviceMemory> deviceMemory;
};

Rendering::Hub::Hub(char const** errorMsg) {
    auto platform = std::make_unique<Platform>(errorMsg);
    if (errorMsg == nullptr || *errorMsg == nullptr) {
        platform_ = std::move(platform);
    }
}

Rendering::Hub::~Hub() {
    Platform& platform = *platform_;
    for (PresentationSurface& presentationSurface : presentationSurfaces_) {
        if (presentationSurface.swapchain) {
            platform.graphicsSwapchainDestructionFifo.add(platform.device, presentationSurface.swapchain, platform.graphicsQueueTracker);
        }
        platform.graphicsPipelineDestructionFifo.add(platform.device, presentationSurface.presentPipeline, platform.graphicsQueueTracker);
        platform.graphicsRenderPassDestructionFifo.add(platform.device, presentationSurface.presentRenderPass, platform.graphicsQueueTracker);
        platform.graphicsSurfaceDestructionFifo.add(platform.instance, presentationSurface.surface, platform.graphicsQueueTracker);
    }
}

Rendering::PipelineId Rendering::Hub::createPipeline(WindowHandle displayWindow) {
    Platform& platform = *platform_;
    VkDevice const device = platform.device;
    if (presentationSurfaces_.count() && presentationSurfaces_.find(displayWindow)) {
        assert(false);
        return {};
    }

    PipelineResource pipelineResource = {device, {}};

    {
        VkExtent2D extent;
        gg::Os::GetMaxWindowSize(extent.width, extent.height);

        VkSurfaceKHR surface = {};
    #ifdef VK_USE_PLATFORM_WIN32_KHR
        {
            VkWin32SurfaceCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
            createInfo.hinstance = GetModuleHandle(nullptr);
            createInfo.hwnd = (HWND)displayWindow;
            VkResult result = vkCreateWin32SurfaceKHR(platform.instance, &createInfo, nullptr, &surface);
            assert(result == VK_SUCCESS);

            VkBool32 supported = {};
            result = vkGetPhysicalDeviceSurfaceSupportKHR(platform.physical.device, platform.physical.graphicsQueueFamily, surface, &supported);
            assert(result == VK_SUCCESS);
            assert(supported);
        }
    #else
    #error More work required
    #endif
        if (!platform.presentImageAcquiredSemaphore)
        {
            {
                VkSemaphoreCreateInfo createInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
                vkCreateSemaphore(platform.device, &createInfo, nullptr, &platform.presentImageAcquiredSemaphore);
            }
            {
                VkFenceCreateInfo createInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
                vkCreateFence(platform.device, &createInfo, nullptr, &platform.presentImageAcquiredFence);
            }
        }

        VkSurfaceCapabilitiesKHR surfaceCapabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(platform.physical.device, surface, &surfaceCapabilities);

        VkFormat const expectedFormats[] = {
            VK_FORMAT_R8G8B8_SRGB,
            VK_FORMAT_B8G8R8_SRGB,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_FORMAT_B8G8R8A8_SRGB,
        };

        VkSurfaceFormatKHR formats[16];
        uint32_t formatCount = gg::CountOf(formats);
        vkGetPhysicalDeviceSurfaceFormatsKHR(platform.physical.device, surface, &formatCount, formats);

        unsigned found = 0;
        for (; found < formatCount; found++) {
            VkFormat const* f = std::find(std::begin(expectedFormats), std::end(expectedFormats), formats[found].format);
            if (f < std::end(expectedFormats)) {
                break;
            }
        }
        assert(found < formatCount);

        VkRenderPass presentRenderPass = {};
        {
            VkAttachmentDescription attachment = {};
            attachment.format = formats[found].format;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentReference attachmentRef = {};
            attachmentRef.attachment = 0;
            attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &attachmentRef;

            VkRenderPassCreateInfo createInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            createInfo.attachmentCount = 1;
            createInfo.pAttachments = &attachment;
            createInfo.subpassCount = 1;
            createInfo.pSubpasses = &subpass;
            //createInfo.dependencyCount = blueprint.dependencies.count();
            //createInfo.pDependencies = dependencies;

            VkResult result = vkCreateRenderPass(platform.device, &createInfo, nullptr, &presentRenderPass);
            assert(result == VK_SUCCESS);
        }
        VkPipeline presentPipeline = {};
        {
            VkPipelineShaderStageCreateInfo stages[] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[0].module = platform.presentVertexShader;
            stages[1].module = platform.presentFragmentShader;
            stages[0].pName = "main";
            stages[1].pName = "main";
            VkPipelineVertexInputStateCreateInfo vertexInput = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo inputAssembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineRasterizationStateCreateInfo rasterization = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            rasterization.rasterizerDiscardEnable = VK_TRUE;
            rasterization.lineWidth = 1.f;
            VkPipelineColorBlendAttachmentState attachments[] = {
                {VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0xf},
            };
            VkPipelineColorBlendStateCreateInfo colorBlend = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            colorBlend.attachmentCount = gg::CountOf(attachments);
            colorBlend.pAttachments = attachments;
            VkGraphicsPipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            createInfo.flags = 0;
            createInfo.stageCount = gg::CountOf(stages);
            createInfo.pStages = stages;
            createInfo.pVertexInputState = &vertexInput;
            createInfo.pInputAssemblyState = &inputAssembly;
            createInfo.pRasterizationState = &rasterization;
            createInfo.pColorBlendState = &colorBlend;
            createInfo.layout = platform.presentPipelineLayout;
            createInfo.renderPass = presentRenderPass;
            VkResult result = vkCreateGraphicsPipelines(device, nullptr, 1, &createInfo, nullptr, &presentPipeline);
            assert(result == VK_SUCCESS);
        }

        VkPresentModeKHR presentModes[VK_PRESENT_MODE_RANGE_SIZE_KHR];
        uint32_t presentModeCount = gg::CountOf(presentModes);
        vkGetPhysicalDeviceSurfacePresentModesKHR(platform.physical.device, surface, &presentModeCount, presentModes);

        presentationSurfaces_.add(std::move(displayWindow), {surface, presentRenderPass, presentPipeline, {}, {{}, {}}, formats[found], {}});
        maintainPresentationSurface(displayWindow);
    }

    pipelineResource.displayWindow = displayWindow;

    return pipelineResources_.add(std::move(pipelineResource));
}

Rendering::Hub::PresentationSurface* Rendering::Hub::maintainPresentationSurface(WindowHandle displayWindow) const {
    if (!displayWindow) {
        return nullptr;
    }
    Platform& platform = *platform_;

    PresentationSurface*const presentationSurface = presentationSurfaces_.fetch(displayWindow);

    unsigned w, h;
    Os::GetClientSize(displayWindow, w, h);
    if (w == presentationSurface->extent.width && h == presentationSurface->extent.height) {
        return presentationSurface;
    }

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(platform.physical.device, presentationSurface->surface, &surfaceCapabilities);
    if (result == VK_ERROR_SURFACE_LOST_KHR || surfaceCapabilities.currentExtent.width < surfaceCapabilities.minImageExtent.width || surfaceCapabilities.currentExtent.height < surfaceCapabilities.minImageExtent.height) {
        return presentationSurface;
    }
    assert(result == VK_SUCCESS);
    assert(presentationSurface->extent.width != surfaceCapabilities.currentExtent.width || presentationSurface->extent.height != surfaceCapabilities.currentExtent.height);

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = presentationSurface->surface;
    createInfo.minImageCount = gg::CountOf(presentationSurface->swapchainImages);
    createInfo.imageFormat = presentationSurface->surfaceFormat.format;
    createInfo.imageColorSpace = presentationSurface->surfaceFormat.colorSpace;
    createInfo.imageExtent = surfaceCapabilities.currentExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = presentationSurface->swapchain;

    presentationSurface->swapchain = {};
    result = vkCreateSwapchainKHR(platform.device, &createInfo, nullptr, &presentationSurface->swapchain);
    assert(result == VK_SUCCESS);

    if (createInfo.oldSwapchain) {
        platform.graphicsSwapchainDestructionFifo.add(platform.device, createInfo.oldSwapchain, platform.graphicsQueueTracker);
        platform.flushWaitAndPurgeGraphics();
    }

    uint32_t imageCount = gg::CountOf(presentationSurface->swapchainImages);
    result = vkGetSwapchainImagesKHR(platform_->device, presentationSurface->swapchain, &imageCount, presentationSurface->swapchainImages);
    assert(result == VK_SUCCESS && imageCount == gg::CountOf(presentationSurface->swapchainImages));

    presentationSurface->extent = surfaceCapabilities.currentExtent;
    return presentationSurface;
}

Rendering::ImageId Rendering::Hub::createImage(Span<uint8_t> const& data, RenderFormat const& format, unsigned width, unsigned height) {
    Platform& platform = *platform_;
    ImageResource imageResource = {platform.device, {}};

    imageResource.width = width;
    imageResource.height = height;
    imageResource.format = gg::vk::ConvertFormat(format);

    {
        VkImageCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        createInfo.imageType = VK_IMAGE_TYPE_2D;
        createInfo.format = imageResource.format;
        createInfo.extent = {width, height, 1};
        createInfo.mipLevels = 1;
        createInfo.arrayLayers = 1;
        createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;  // todo xfer_src here is only for temp debug display
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkResult result = vkCreateImage(platform.device, &createInfo, nullptr, &imageResource.image);
        assert(result == VK_SUCCESS);
    }
    if (!imageResource.image)
        return {};

    VkMemoryRequirements memoryRequirements = {};
    {
        // todo proper suballocation from big pages...
        vkGetImageMemoryRequirements(platform.device, imageResource.image, &memoryRequirements);

        VkMemoryAllocateInfo imageAllocateInfo = {};
        imageAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        imageAllocateInfo.allocationSize = memoryRequirements.size;
        imageAllocateInfo.memoryTypeIndex = gg::vk::FindMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryRequirements.memoryTypeBits, platform.physical.memoryProperties, nullptr);

        VkResult result = vkAllocateMemory(platform.device, &imageAllocateInfo, NULL, &imageResource.deviceMemory);
        assert(result == VK_SUCCESS);
    }
    if (!imageResource.deviceMemory)
        return {};

    if (VK_SUCCESS != vkBindImageMemory(platform.device, imageResource.image, imageResource.deviceMemory, 0)) {
        assert(false);
        return {};
    }

    VkCommandBuffer const transferCmdBuffer = platform.transferCommandBufferDispenser.getOrCreate(platform.device, platform.transferQueueTracker);

    {
        VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        imageBarrier.srcAccessMask = 0;
        imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.srcQueueFamilyIndex = platform.physical.transferQueueFamily;
        imageBarrier.dstQueueFamilyIndex = platform.physical.graphicsQueueFamily;
        imageBarrier.image = imageResource.image;
        imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange.levelCount = 1;
        imageBarrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(
            transferCmdBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &imageBarrier);
    }
    {
        Platform::StagingBuffer stagingBuffer(*platform_, data.count());
        uint8_t* mappedData = nullptr;
        if (data.count()) {
            vkMapMemory(platform.device, stagingBuffer.deviceMemory, 0, data.count(), 0, (void**)&mappedData);
            memset(mappedData, -1, data.count());

            unsigned rowPitch = data.count() / height;
            for (unsigned y = 0; y < height; y++) {
                memcpy(mappedData + y*rowPitch, &data[y*rowPitch], rowPitch);
            }

            vkUnmapMemory(platform.device, stagingBuffer.deviceMemory);

            VkBufferImageCopy region = {};
            region.bufferRowLength = width;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1;
            region.imageExtent.width = width;
            region.imageExtent.height = height;
            region.imageExtent.depth = 1;

            vkCmdCopyBufferToImage(transferCmdBuffer, stagingBuffer.buffer, imageResource.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &region);
            platform.transferBufferDestructionFifo.add(platform.device, std::exchange(stagingBuffer.buffer, {}), platform.transferQueueTracker);
            platform.transferDeviceMemoryFreeFifo.add(platform.device, std::exchange(stagingBuffer.deviceMemory, {}), platform.transferQueueTracker);
        }
    }
    {
        VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageBarrier.srcQueueFamilyIndex = platform.physical.transferQueueFamily;
        imageBarrier.dstQueueFamilyIndex = platform.physical.graphicsQueueFamily;
        imageBarrier.image = imageResource.image;
        imageBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(
            platform.graphicsCommandBufferDispenser.getOrCreate(platform.device, platform.graphicsQueueTracker),
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &imageBarrier);
    }
    {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = imageResource.image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = imageResource.format;
        createInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        createInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkResult result = vkCreateImageView(platform.device, &createInfo, nullptr, &imageResource.imageView);
        assert(result == VK_SUCCESS);
    }
    if (!imageResource.imageView)
        return {};

    return imageResources_.add(std::move(imageResource));
}

Rendering::TilesetId Rendering::Hub::createTileset(Span<uint8_t> const& data, RenderFormat const& format, unsigned width, unsigned height, unsigned tileWidth, unsigned tileHeight) {
    TilesetResource tilesetResource = {platform_->device, {}};

    VkImageCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = gg::vk::ConvertFormat(format);
    createInfo.extent = {width, height, 1};
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = vkCreateImage(platform_->device, &createInfo, nullptr, &tilesetResource.image);
    assert(result == VK_SUCCESS);

    return tilesetResources_.add(std::move(tilesetResource));
}

void Rendering::Hub::destroyPipeline(PipelineId id) {
    pipelineResources_.remove(id);
}

void Rendering::Hub::destroyImage(ImageId id) {
    Platform& platform = *platform_;
    ImageResource imageResource = imageResources_.remove(id);
    platform.graphicsImageDestructionFifo.add(platform.device, std::exchange(imageResource.image, {}), platform.graphicsQueueTracker);
    platform.graphicsDeviceMemoryFreeFifo.add(platform.device, std::exchange(imageResource.deviceMemory, {}), platform.graphicsQueueTracker);
    // todo retire imageview as well
}

void Rendering::Hub::destroyTileset(TilesetId id) {
    tilesetResources_.remove(id);
}

void Rendering::Hub::submitQueuedUploads() {
    platform_->flushTransfers();
    platform_->purgeTransferResources();
}

Rendering Rendering::Hub::startRendering(PipelineId pipelineId) {
    if (renderingFreeList_.count()) {
        return renderingFreeList_.removeLast();
    }
    return Rendering(this, pipelineId);
}

void Rendering::Hub::submitRendering(Rendering&& rendering) {
    submitQueuedUploads();

    Platform& platform = *platform_;
    PipelineResource*const pipelineResource = pipelineResources_.fetch(rendering.pipelineId_);
    PresentationSurface*const presentationSurface = maintainPresentationSurface(pipelineResource->displayWindow);
    VkImage presentImage = {};

    VkCommandBuffer graphicsCommandBuffer = platform.graphicsCommandBufferDispenser.getOrCreate(platform.device, platform.graphicsQueueTracker);

    if (presentationSurface && presentationSurface->swapchain) {
        presentationSurface->acquiredImageIndex = ~0;
        VkResult result = vkAcquireNextImageKHR(platform.device, presentationSurface->swapchain, 0, platform.presentImageAcquiredSemaphore, platform.presentImageAcquiredFence, &presentationSurface->acquiredImageIndex);
        if (result == VK_SUCCESS) {
            presentImage = presentationSurface->swapchainImages[presentationSurface->acquiredImageIndex];
        }
    }

    if (presentImage) {
        platform.graphicsWaitSemaphores.addLast(platform.presentImageAcquiredSemaphore);
        platform.graphicsWaitDstStageMasks.addLast(VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkImageSubresourceRange subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        imageBarrier.srcAccessMask = 0;
        imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.srcQueueFamilyIndex = platform_->physical.graphicsQueueFamily;
        imageBarrier.dstQueueFamilyIndex = platform_->physical.graphicsQueueFamily;
        imageBarrier.image = presentationSurface->swapchainImages[presentationSurface->acquiredImageIndex];
        imageBarrier.subresourceRange = subresourceRange;

        VkClearColorValue clearColor = {0.f, 0.f, 0.f, 1.f};
        vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
        vkCmdClearColorImage(graphicsCommandBuffer, presentImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &subresourceRange);
    }

    // todo render these properly
    for (Rendering::ImagePrim imagePrim : rendering.imagePrims_) {
        ImageResource const& imageResource = *imageResources_.fetch(imagePrim.imageId);
        VkImageBlit region = {};
        region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.srcOffsets[0] = {0,0,0};
        region.srcOffsets[1] = {(int)imageResource.width,(int)imageResource.height,1};
        region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.dstOffsets[0] = {(int)imagePrim.x,(int)imagePrim.y,0};
        region.dstOffsets[1] = {(int)(imagePrim.x+imageResource.width),(int)(imagePrim.y+imageResource.height),1};
        vkCmdBlitImage(graphicsCommandBuffer, imageResource.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, presentImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR);
    }

    if (presentImage) {
        VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        imageBarrier.srcQueueFamilyIndex = platform_->physical.graphicsQueueFamily;
        imageBarrier.dstQueueFamilyIndex = platform_->physical.graphicsQueueFamily;
        imageBarrier.image = presentImage;
        imageBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdPipelineBarrier(graphicsCommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
    }

    platform.flushGraphics({});
    platform.purgeGraphicsResources();

    if (presentImage) {
        VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &presentationSurface->swapchain;
        presentInfo.pImageIndices = &presentationSurface->acquiredImageIndex;
        VkResult result = vkQueuePresentKHR(platform.graphicsQueue, &presentInfo);
        assert(result == VK_SUCCESS);

        {
            VkResult result = vkWaitForFences(platform.device, 1, &platform.presentImageAcquiredFence, true, UINT64_MAX);
            assert(result == VK_SUCCESS);
            vkResetFences(platform.device, 1, &platform.presentImageAcquiredFence);
        }
    }

    renderingFreeList_.addLast(std::move(rendering.reset()));
}

}
