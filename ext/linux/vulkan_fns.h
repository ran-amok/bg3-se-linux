// vulkan_fns.h — Vulkan for the IMGUI render backend (Extender/Client/IMGUI/Vulkan.inl),
// reached the way the game reaches it. bin/bg3 links no libvulkan: it dlopens
// libvulkan.so.1 and starts from vkGetInstanceProcAddr, and the
// extender may add no NEEDED library beyond glibc (tools/check-floor.sh). So every vk*
// the backend calls is a pointer here, filled in three steps: the loader (dlopen and
// vkGetInstanceProcAddr, at EnableHooks), the instance (vkCreateInstance's post-hook)
// and the device (vkCreateDevice's post-hook). Device-level pointers come from
// vkGetDeviceProcAddr, so they are the layer's or the ICD's own -- the same pointers
// the game got, which is where the present hook goes. imgui's backend loads its own
// set through Load() (IMGUI_IMPL_VULKAN_NO_PROTOTYPES).
#pragma once
#define VK_NO_PROTOTYPES 1
#include <vulkan/vulkan.h>
#include <cstddef>

// what Vulkan.inl calls, by the level the pointer is resolved at
#define BG3SE_VK_GLOBAL_FNS(X) \
    X(vkCreateInstance)
#define BG3SE_VK_INSTANCE_FNS(X) \
    X(vkGetDeviceProcAddr) X(vkCreateDevice) X(vkDestroyDevice) \
    X(vkGetPhysicalDeviceQueueFamilyProperties)
#define BG3SE_VK_DEVICE_FNS(X) \
    X(vkGetDeviceQueue) X(vkCreateDescriptorPool) X(vkDestroyDescriptorPool) \
    X(vkCreateSampler) X(vkDestroySampler) X(vkCreateRenderPass) X(vkDestroyRenderPass) \
    X(vkCreateCommandPool) X(vkDestroyCommandPool) X(vkAllocateCommandBuffers) \
    X(vkFreeCommandBuffers) X(vkCreateFence) X(vkDestroyFence) X(vkWaitForFences) \
    X(vkResetFences) X(vkCreateSemaphore) X(vkDestroySemaphore) X(vkCreateImageView) \
    X(vkDestroyImageView) X(vkCreateFramebuffer) X(vkDestroyFramebuffer) \
    X(vkGetSwapchainImagesKHR) X(vkResetCommandBuffer) X(vkBeginCommandBuffer) \
    X(vkEndCommandBuffer) X(vkCmdPipelineBarrier) X(vkCmdBeginRenderPass) \
    X(vkCmdEndRenderPass) X(vkQueueSubmit)

// The pointers carry the API names, so Vulkan.inl's call sites read as upstream wrote them.
#define BG3SE_VK_DECLARE(name) inline PFN_##name name = nullptr;
inline PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
BG3SE_VK_GLOBAL_FNS(BG3SE_VK_DECLARE)
BG3SE_VK_INSTANCE_FNS(BG3SE_VK_DECLARE)
BG3SE_VK_DEVICE_FNS(BG3SE_VK_DECLARE)
#undef BG3SE_VK_DECLARE

namespace bg3se::vk {

// dlopen("libvulkan.so.1") -- the game's later dlopen of the same soname gets this very
// object -- and vkGetInstanceProcAddr from it, then the global functions. Idempotent.
// False (with a line in bg3se.log) when the loader is missing: the backend then stays off.
bool LoadLoader();
// The instance-level set, from vkGetInstanceProcAddr(instance, ...).
bool LoadInstance(VkInstance instance);
// The device-level set, from vkGetDeviceProcAddr(device, ...).
bool LoadDevice(VkDevice device);
// For ImGui_ImplVulkan_LoadFunctions: vkGetInstanceProcAddr on the instance recorded by
// LoadInstance (a loader trampoline for device-level names, which dispatches on the object).
PFN_vkVoidFunction Load(char const* name, void*);
// "libvulkan.so.1+0x1234" for a code address, for the hook log; "?" when dladdr has nothing.
char const* Module(void const* p, char* buf, size_t n);

}  // namespace bg3se::vk
