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

struct DestroyOverloads {
    static void Destroy(VkDevice device, VkBuffer handle, VkAllocationCallbacks const* allocator) { vkDestroyBuffer(device, handle, allocator); }
    static void Destroy(VkDevice device, VkImage handle, VkAllocationCallbacks const* allocator) { vkDestroyImage(device, handle, allocator); }
    static void Destroy(VkDevice device, VkDeviceMemory handle, VkAllocationCallbacks const* allocator) { vkFreeMemory(device, handle, allocator); }
    static void Destroy(VkDevice device, VkSwapchainKHR handle, VkAllocationCallbacks const* allocator) { vkDestroySwapchainKHR(device, handle, allocator); }
    static void Destroy(VkInstance instance, VkSurfaceKHR handle, VkAllocationCallbacks const* allocator) { vkDestroySurfaceKHR(instance, handle, allocator); }
    static void Destroy(VkDevice device, VkRenderPass handle, VkAllocationCallbacks const* allocator) { vkDestroyRenderPass(device, handle, allocator); }
    static void Destroy(VkDevice device, VkPipeline handle, VkAllocationCallbacks const* allocator) { vkDestroyPipeline(device, handle, allocator); }
};

class QueueTracker {
public:
    using FenceValue = int;
    FenceValue unsubmittedFenceValue() const {
        return unsubmittedFenceValue_;
    }
    FenceValue signalledFenceValue() const {
        return signalledFenceValue_;
    }
    bool fenceValueIsSignalled(FenceValue fenceValue) const {
        return signalledFenceValue_ - fenceValue >= 0;  // (using difference to exploit wrapping)
    }
    VkFence doSubmission(VkDevice device, VkQueue queue, VkSubmitInfo const& submitInfo) {
        VkFence submittingFence;
        if (submittedFences_.count() && vkGetFenceStatus(device, submittedFences_[0]) == VK_SUCCESS) {
            assert(signalledFenceValue_ + 1 == unsubmittedFenceValue_ - submittedFences_.count());
            signalledFenceValue_++;
            submittingFence = submittedFences_.removeFirst();
            vkResetFences(device, 1, &submittingFence);
        } else {
            VkFenceCreateInfo createInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            VkResult result = vkCreateFence(device, &createInfo, nullptr, &submittingFence);
            assert(result == VK_SUCCESS);
        }
        VkResult result = vkQueueSubmit(queue, 1, &submitInfo, submittingFence);
        assert(result == VK_SUCCESS);
        submittedFences_.addLast(submittingFence);
        unsubmittedFenceValue_++;
        return submittingFence;
    }
    void waitForAll(VkDevice device) {
        if (submittedFences_.count()) {
            auto fences = GG_STACK_ARRAY(VkFence, submittedFences_.count());
            submittedFences_.linearizeCopy(fences);
            vkWaitForFences(device, submittedFences_.count(), fences, true, ~0);
            signalledFenceValue_ += submittedFences_.count();
            for (VkFence fence : submittedFences_) {
                vkDestroyFence(device, fence, nullptr);
            }
            submittedFences_.removeAll();
        }
    }
    ~QueueTracker() {
        assert(submittedFences_.count() == 0);
    }

private:
    FenceValue unsubmittedFenceValue_ = 0;
    FenceValue signalledFenceValue_ = -1;
    Ring<VkFence> submittedFences_;
};

class CommandBufferDispenser {
public:
    CommandBufferDispenser(VkDevice device, unsigned queueFamilyIndex) {
        VkCommandPoolCreateInfo createInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        createInfo.queueFamilyIndex = queueFamilyIndex;
        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VkResult result = vkCreateCommandPool(device, &createInfo, nullptr, &commandPool_);
        assert(result == VK_SUCCESS);
    }
    ~CommandBufferDispenser() {
        assert(current_ == VK_NULL_HANDLE && recycling_.count() == 0);
    }
    void teardown(VkDevice device) {
        assert(current_ == VK_NULL_HANDLE);
        if (recycling_.count() == 0) {
            return;
        }
        auto* commandBuffers = GG_STACK_ARRAY(VkCommandBuffer, recycling_.count());
        for (unsigned i = 0; i < recycling_.count(); i++) {
            commandBuffers[i] = recycling_[i].commandBuffer;
        }
        vkFreeCommandBuffers(device, commandPool_, recycling_.count(), commandBuffers);
        recycling_.removeAll();
        vkDestroyCommandPool(device, commandPool_, nullptr);
    }
    VkCommandBuffer getOrCreate(VkDevice device, QueueTracker const& tracker) {
        if (current_ == VK_NULL_HANDLE) {
            if (recycling_.count() && tracker.fenceValueIsSignalled(recycling_[0].fenceValue)) {
                current_ = recycling_.removeFirst().commandBuffer;
                vkResetCommandBuffer(current_, 0);
            } else {
                VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                allocInfo.commandPool = commandPool_;
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocInfo.commandBufferCount = 1;
                VkResult result = vkAllocateCommandBuffers(device, &allocInfo, &current_);
                assert(result == VK_SUCCESS);
            }
            VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VkResult result = vkBeginCommandBuffer(current_, &beginInfo);
            assert(result == VK_SUCCESS);
        }
        return current_;
    }
    VkCommandBuffer finish(QueueTracker const& tracker) {
        if (current_ == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }
        VkResult result = vkEndCommandBuffer(current_);
        assert(result == VK_SUCCESS);
        recycling_.addLast({tracker.unsubmittedFenceValue(), current_});
        return std::exchange(current_, {});
    }
private:
    struct Recycled {
        QueueTracker::FenceValue fenceValue;
        VkCommandBuffer commandBuffer;
    };
    VkCommandPool commandPool_;
    VkCommandBuffer current_ = {};
    Ring<Recycled> recycling_;
};

template<class T>
class DestructionFifo {
public:
    ~DestructionFifo() {
        assert(pending_.count() == 0);
    }
    template<class T_Parent>
    void add(T_Parent parent, T handle, QueueTracker const& tracker) {
        if (handle) {
            pending_.addLast({tracker.unsubmittedFenceValue(), handle});
        }
    }
    template<class T_Parent>
    void flushSignalled(T_Parent parent, QueueTracker const& tracker) {
        while (pending_.count() && tracker.fenceValueIsSignalled(pending_[0].fenceValue)) {
            DestroyOverloads::Destroy(parent, pending_.removeFirst().handle, nullptr);
        }
    }
private:
    struct Pending {
        QueueTracker::FenceValue fenceValue;
        T handle;
    };
    Ring<Pending> pending_;
};

}
}

#endif
