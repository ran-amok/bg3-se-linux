// vulkan.cpp — the pointer table of vulkan_fns.h, filled from the loader the game uses.
#include "vulkan_fns.h"
#include "platform.h"

#include <dlfcn.h>
#include <cstdio>
#include <cstring>

namespace bg3se::vk {

namespace {
void* g_loader = nullptr;
VkInstance g_instance = VK_NULL_HANDLE;
}  // namespace

bool LoadLoader()
{
    if (vkGetInstanceProcAddr != nullptr) return true;
    // No RTLD_NOLOAD: the extender initialises before the game's own dlopen (deferred init
    // runs from COsiris::RegisterDIVFunctions, ahead of SDL_Init), so this is the first
    // load, and the game's dlopen("libvulkan.so.1") then returns this object.
    g_loader = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (g_loader == nullptr) {
        platform::log_line("vulkan: dlopen(libvulkan.so.1) failed: %s", dlerror());
        return false;
    }
    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(g_loader, "vkGetInstanceProcAddr");
    if (vkGetInstanceProcAddr == nullptr) {
        platform::log_line("vulkan: libvulkan.so.1 has no vkGetInstanceProcAddr: %s", dlerror());
        return false;
    }
    Dl_info info{};
    dladdr((void const*)vkGetInstanceProcAddr, &info);
    platform::log_line("vulkan: loader %s, vkGetInstanceProcAddr %p",
                       info.dli_fname ? info.dli_fname : "?", (void*)vkGetInstanceProcAddr);
    bool ok = true;
#define BG3SE_VK_LOAD(name) \
    name = (PFN_##name)vkGetInstanceProcAddr(nullptr, #name); \
    if (name == nullptr) { platform::log_line("vulkan: no %s from the loader", #name); ok = false; }
    BG3SE_VK_GLOBAL_FNS(BG3SE_VK_LOAD)
#undef BG3SE_VK_LOAD
    return ok;
}

bool LoadInstance(VkInstance instance)
{
    if (vkGetInstanceProcAddr == nullptr) return false;
    g_instance = instance;
    bool ok = true;
#define BG3SE_VK_LOAD(name) \
    name = (PFN_##name)vkGetInstanceProcAddr(instance, #name); \
    if (name == nullptr) { platform::log_line("vulkan: no %s on instance %p", #name, (void*)instance); ok = false; }
    BG3SE_VK_INSTANCE_FNS(BG3SE_VK_LOAD)
#undef BG3SE_VK_LOAD
    return ok;
}

bool LoadDevice(VkDevice device)
{
    if (vkGetDeviceProcAddr == nullptr) return false;
    bool ok = true;
#define BG3SE_VK_LOAD(name) \
    name = (PFN_##name)vkGetDeviceProcAddr(device, #name); \
    if (name == nullptr) { platform::log_line("vulkan: no %s on device %p", #name, (void*)device); ok = false; }
    BG3SE_VK_DEVICE_FNS(BG3SE_VK_LOAD)
#undef BG3SE_VK_LOAD
    return ok;
}

PFN_vkVoidFunction Load(char const* name, void*)
{
    if (vkGetInstanceProcAddr == nullptr) return nullptr;
    return vkGetInstanceProcAddr(g_instance, name);
}

char const* Module(void const* p, char* buf, size_t n)
{
    Dl_info info{};
    if (p == nullptr || !dladdr(p, &info) || info.dli_fname == nullptr) return "?";
    char const* base = strrchr(info.dli_fname, '/');
    base = base ? base + 1 : info.dli_fname;
    snprintf(buf, n, "%s+0x%lx", base, (unsigned long)((char const*)p - (char const*)info.dli_fbase));
    return buf;
}

}  // namespace bg3se::vk
