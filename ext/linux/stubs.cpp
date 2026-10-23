// stubs.cpp — what the parts of upstream left out of the Linux build (crash reporter,
// Noesis UI events) still export to the parts that are in. (Networking's in-process half
// is real now, ext/linux/net.cpp; so are the SDL hooks and the IMGUI Vulkan backend,
// Extender/Client/SDL.cpp and IMGUI/Vulkan.inl on ext/linux/{sdl_weak,vulkan_fns}.h.)
#include "stdafx.h"
#include <Extender/ScriptExtender.h>
#include <Lua/Client/LuaBindingClient.h>
#include "platform.h"

BEGIN_SE()

// CrashReporter.cpp: crash reporting is Windows-only (SEH, minidumps, an uploader)
std::atomic<uint32_t> gDisableCrashReportingCount{ 0 };
void InitCrashReporting() {}
void ShutdownCrashReporting() {}
LONG HandleGuardedException(DWORD, EXCEPTION_POINTERS*) { return 0; }
// Wrappers.h records our hook entry points for the crash reporter's "is this crash
// ours" question; the set is still filled, nobody reads it here.
std::unordered_set<void const*> gRegisteredTrampolines;

END_SE()
