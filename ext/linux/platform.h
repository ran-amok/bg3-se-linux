// platform.h — what the Linux layer shares: the host table the loader handed over
// (loader/src/host_api.h) and a line into bg3se.log.
#pragma once

#include <host_api.h>

namespace bg3se::platform {

const bg3se_host_api& host();
void log_line(char const* fmt, ...) __attribute__((format(printf, 1, 2)));
// typeids.cpp: the embedded ECS TypeId table for this build into GetStaticSymbols()'s
// name/context maps; a build with no table logs and leaves them empty.
void BindTypeIdTable();
// PORT (linux): BG3SE_NO_IMGUI=1 leaves upstream's IMGUI hooks out -- the Vulkan
// backend's and the SDL input ones -- and nothing else, so a launch can be held against
// one without them: a crash one line past `LoadModule -> LoadMenu` is in IMGUI's first
// frame. Read once.
bool NoImgui();

}  // namespace bg3se::platform
