#pragma once
#ifndef GG_VULKANUTIL_H
#define GG_VULKANUTIL_H

#include "Array.h"
#include "Ring.h"

#include "RenderTypes.h"

#define VK_USE_PLATFORM_WIN32_KHR   // todo, someday, probably never, allow for other platforms
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "vulkan/vulkan.h"

namespace gg {
namespace vk {

VkFormat ConvertFormat(RenderFormat const& format);

inline int FindQueueFamily(VkQueueFlags requiredFlags, VkQueueFamilyProperties const props[], unsigned count) {
    // find queue family with all required flags, and fewest irrelevant flags
    unsigned extraneousBitCount = 32;
    int found = -1;
    for (unsigned i = 0; i < count && extraneousBitCount != 0; i++) {
        if ((props[i].queueFlags & requiredFlags) == requiredFlags
            && CountNonzeroBits(props[i].queueFlags & ~requiredFlags) < extraneousBitCount) {
            found = i;
            extraneousBitCount = CountNonzeroBits(props[i].queueFlags & ~requiredFlags);
        }
    }
    return found;
}

inline int FindMemoryType(VkMemoryPropertyFlags requiredFlags, uint32_t typeMask, VkPhysicalDeviceMemoryProperties const& memoryProps, unsigned* heapIndexOut) {
    // find memory type with all required flags, and largest heap
    size_t availableHeapSize = 0;
    int found = -1;
    while (typeMask) {
        unsigned const typeIndex = FloorLog2(typeMask & -(int32_t)typeMask);
        VkMemoryType const& memoryType = memoryProps.memoryTypes[typeIndex];
        VkMemoryHeap const& memoryHeap = memoryProps.memoryHeaps[memoryType.heapIndex];
        if ((memoryType.propertyFlags & requiredFlags) == requiredFlags && memoryHeap.size > availableHeapSize) {
            found = typeIndex;
            availableHeapSize = memoryHeap.size;
        }
        typeMask &= typeMask - 1;
    }
    if (found >= 0 && heapIndexOut) {
        *heapIndexOut = memoryProps.memoryTypes[found].heapIndex;
    }
    return found;
}

template<class T>
struct MovableHandle {
    MovableHandle()
        : handle(nullptr) {
    }
    MovableHandle(T& handle)
        : handle(std::exchange(handle, nullptr)) {
    }
    MovableHandle(MovableHandle const&) = delete;
    MovableHandle(MovableHandle&& src)
        : handle(std::exchange(src.handle, {})) {
    }
    MovableHandle& operator=(MovableHandle&& src) {
        handle = std::exchange(src.handle, {});
        return *this;
    }

    T* operator&() {
        return &handle;
    }
    operator T() const {
        return handle;
    }

    T handle;
};

template<class T, void VKAPI_CALL T_DestroyFunc(T handle, VkAllocationCallbacks const* allocator)>
struct Destructomatic {
    Destructomatic() = default;
    Destructomatic(T handle)
        : handle(handle) {
    }
    ~Destructomatic() {
        if (handle) {
            T_DestroyFunc(handle, nullptr);
        }
    }
    operator T() const {
        return handle;
    }
    T* operator&() {
        return &handle;
    }
    T handle = {};
};

struct DestroyOverloads {
    static void Destroy(VkDevice device, VkBuffer handle, VkAllocationCallbacks const* allocator) { vkDestroyBuffer(device, handle, allocator); }
    static void Destroy(VkDevice device, VkImage handle, VkAllocationCallbacks const* allocator) { vkDestroyImage(device, handle, allocator); }
    static void Destroy(VkDevice device, VkDeviceMemory handle, VkAllocationCallbacks const* allocator) { vkFreeMemory(device, handle, allocator); }
    static void Destroy(VkDevice device, VkSwapchainKHR handle, VkAllocationCallbacks const* allocator) { vkDestroySwapchainKHR(device, handle, allocator); }
    static void Destroy(VkInstance instance, VkSurfaceKHR handle, VkAllocationCallbacks const* allocator) { vkDestroySurfaceKHR(instance, handle, allocator); }
    static void Destroy(VkDevice device, VkRenderPass handle, VkAllocationCallbacks const* allocator) { vkDestroyRenderPass(device, handle, allocator); }
    static void Destroy(VkDevice device, VkPipeline handle, VkAllocationCallbacks const* allocator) { vkDestroyPipeline(device, handle, allocator); }
};

template<class T>
struct DeferredDestructionFifo {
    template<class T_Destroyer>
    void flushAndBeginPhase(T_Destroyer destroyer) {
        retiring = pendingDestroy.removeFirst();
        for (T handle : retiring) {
            DestroyOverloads::Destroy(destroyer, handle, nullptr);
        }
        retiring.removeAll();
    }
    void add(T handle) {
        retiring.addLast(handle);
    }
    void endPhase() {
        pendingDestroy.addLast(std::move(retiring));
    }
    template<class T_Destroyer>
    void flushAll(T_Destroyer destroyer) {
        for (Array<T>& handles : pendingDestroy) {
            for (T handle : handles) {
                DestroyOverloads::Destroy(destroyer, handle, nullptr);
            }
            handles.removeAll();
        }
    }
    Array<T> retiring;
    Ring<Array<T>> pendingDestroy;
};

class Channel {
public:
    bool flush(Span<VkSemaphore const> signalSemaphores = {}) {
        if (currentCommandBuffer_ == nullptr) {
            return false;
        }
        vkEndCommandBuffer(currentCommandBuffer_);
        assert(waitDstStageMasks_.count() == waitSemaphores_.count());
        VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.waitSemaphoreCount = waitSemaphores_.count();
        submitInfo.pWaitSemaphores = waitSemaphores_.begin();
        submitInfo.pWaitDstStageMask = waitDstStageMasks_.begin();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &currentCommandBuffer_;
        submitInfo.signalSemaphoreCount = signalSemaphores.count();
        submitInfo.pSignalSemaphores = signalSemaphores.begin();
        VkResult result = vkQueueSubmit(*queue_, 1, &submitInfo, nextFence_);
        assert(result == VK_SUCCESS);

        commandBuffers_.addLast(std::exchange(currentCommandBuffer_, nullptr));
        fences_.addLast(std::exchange(nextFence_, nullptr));
        bufferDestructionFifo_.endPhase();
        imageDestructionFifo_.endPhase();
        deviceMemoryFreeFifo_.endPhase();
        swapchainDestructionFifo_.endPhase();
        surfaceDestructionFifo_.endPhase();
        renderPassDestructionFifo_.endPhase();
        pipelineDestructionFifo_.endPhase();
        waitSemaphores_.removeAll();
        waitDstStageMasks_.removeAll();

        return true;
    }
    VkCommandBuffer beginOrGetCurrentCommandBuffer() {
        if (currentCommandBuffer_ == nullptr) {
            if (fences_.count() && vkGetFenceStatus(*device_, fences_[0]) == VK_SUCCESS) {
                nextFence_ = fences_.removeFirst();
                vkResetFences(*device_, 1, &nextFence_);
                currentCommandBuffer_ = commandBuffers_.removeFirst();
                vkResetCommandBuffer(currentCommandBuffer_, 0);
                bufferDestructionFifo_.flushAndBeginPhase(*device_);
                imageDestructionFifo_.flushAndBeginPhase(*device_);
                swapchainDestructionFifo_.flushAndBeginPhase(*device_);
                surfaceDestructionFifo_.flushAndBeginPhase(*instance_);
                renderPassDestructionFifo_.flushAndBeginPhase(*device_);
                pipelineDestructionFifo_.flushAndBeginPhase(*device_);
            } else {
                VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                allocInfo.commandPool = commandPool_;
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocInfo.commandBufferCount = 1;
                VkResult result = vkAllocateCommandBuffers(*device_, &allocInfo, &currentCommandBuffer_);
                assert(result == VK_SUCCESS);

                VkFenceCreateInfo createInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
                result = vkCreateFence(*device_, &createInfo, nullptr, &nextFence_);
                assert(result == VK_SUCCESS);
            }

            VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VkResult result = vkBeginCommandBuffer(currentCommandBuffer_, &beginInfo);
            assert(result == VK_SUCCESS);
        }
        return currentCommandBuffer_;
    }

    void retireBuffer(VkBuffer buffer, VkCommandBuffer commandBuffer) {
        assert(commandBuffer == currentCommandBuffer_);
        bufferDestructionFifo_.add(buffer);
    }

    void retireImage(VkImage image, VkCommandBuffer commandBuffer) {
        assert(commandBuffer == currentCommandBuffer_);
        imageDestructionFifo_.add(image);
    }

    void retireDeviceMemory(VkDeviceMemory deviceMemory, VkCommandBuffer commandBuffer) {
        assert(commandBuffer == currentCommandBuffer_);
        deviceMemoryFreeFifo_.add(deviceMemory);
    }

    void retireSwapchain(VkSwapchainKHR swapchain, VkCommandBuffer commandBuffer) {
        assert(commandBuffer == currentCommandBuffer_);
        swapchainDestructionFifo_.add(swapchain);
    }

    void retireSurface(VkSurfaceKHR surface, VkCommandBuffer commandBuffer) {
        assert(commandBuffer == currentCommandBuffer_);
        surfaceDestructionFifo_.add(surface);
    }

    void retireRenderPass(VkRenderPass renderPass, VkCommandBuffer commandBuffer) {
        assert(commandBuffer == currentCommandBuffer_);
        renderPassDestructionFifo_.add(renderPass);
    }

    void retirePipeline(VkPipeline pipeline, VkCommandBuffer commandBuffer) {
        assert(commandBuffer == currentCommandBuffer_);
        pipelineDestructionFifo_.add(pipeline);
    }

    void flushAllSwapchains(VkDevice device) {
        swapchainDestructionFifo_.flushAll(device);
    }

    void waitForAll() const {
        if (fences_.count()) {
            vkWaitForFences(*device_, fences_.frontSpan().count(), fences_.frontSpan().begin(), true, ~0);
            if (fences_.backSpan().count()) {
                vkWaitForFences(*device_, fences_.backSpan().count(), fences_.backSpan().begin(), true, ~0);
            }
        }
    }

    void addWaitSemaphore(VkSemaphore semaphore, VkPipelineStageFlags dstStageMask) {
        waitSemaphores_.addLast(semaphore);
        waitDstStageMasks_.addLast(dstStageMask);
    }

    bool ready() const {
        return *device_ != VK_NULL_HANDLE;
    }

    Channel() = default;
    Channel(VkInstance* instance, VkDevice* device, VkQueue* queue, unsigned queueFamilyIndex)
        : instance_(instance)
        , device_(device)
        , queue_(queue) {
        if (!ready()) {
            return;
        }
        VkCommandPoolCreateInfo createInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        createInfo.queueFamilyIndex = queueFamilyIndex;
        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VkResult result = vkCreateCommandPool(*device_, &createInfo, nullptr, &commandPool_);
        assert(result == VK_SUCCESS);
    }

    ~Channel() {
        if (!ready()) {
            return;
        }
        assert(currentCommandBuffer_ == nullptr);
        waitForAll();
        for (VkFence fence : fences_) {
            vkDestroyFence(*device_, fence, nullptr);
        }
        if (commandBuffers_.count()) {
            vkFreeCommandBuffers(*device_, commandPool_, commandBuffers_.frontSpan().count(), commandBuffers_.frontSpan().begin());
            if (commandBuffers_.backSpan().count()) {
                vkFreeCommandBuffers(*device_, commandPool_, commandBuffers_.backSpan().count(), commandBuffers_.backSpan().begin());
            }
        }
        bufferDestructionFifo_.flushAll(*device_);
        imageDestructionFifo_.flushAll(*device_);
        deviceMemoryFreeFifo_.flushAll(*device_);
        swapchainDestructionFifo_.flushAll(*device_);
        surfaceDestructionFifo_.flushAll(*instance_);
        renderPassDestructionFifo_.flushAll(*device_);
        pipelineDestructionFifo_.flushAll(*device_);
        vkDestroyCommandPool(*device_, commandPool_, nullptr);
    }

private:
    VkInstance*const instance_;
    VkDevice*const device_;
    VkQueue*const queue_;
    VkCommandPool commandPool_ = {};
    VkCommandBuffer currentCommandBuffer_ = {};
    DeferredDestructionFifo<VkBuffer> bufferDestructionFifo_;
    DeferredDestructionFifo<VkImage> imageDestructionFifo_;
    DeferredDestructionFifo<VkDeviceMemory> deviceMemoryFreeFifo_;
    DeferredDestructionFifo<VkSwapchainKHR> swapchainDestructionFifo_;
    DeferredDestructionFifo<VkSurfaceKHR> surfaceDestructionFifo_;
    DeferredDestructionFifo<VkRenderPass> renderPassDestructionFifo_;
    DeferredDestructionFifo<VkPipeline> pipelineDestructionFifo_;
    VkFence nextFence_ = {};
    Ring<VkCommandBuffer> commandBuffers_;
    Ring<VkFence> fences_;
    Array<VkSemaphore> waitSemaphores_; // n.b. we don't own these
    Array<VkPipelineStageFlags> waitDstStageMasks_;
};

}
}

#endif
