// entry.cpp — bg3se_ext_init: what dllmain.cpp's DLL_PROCESS_ATTACH does on Windows,
// called by the loader (loader/src/preload.cpp) at deferred init on a known build.
#include "platform.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace bg3se {
void SetupScriptExtender();   // Extender/ScriptExtender.cpp (Linux part)
}

namespace {
bg3se_host_api g_host{};
}

const bg3se_host_api& bg3se::platform::host() { return g_host; }

void bg3se::platform::log_line(char const* fmt, ...)
{
    // thread_local and not a local: 4 kB of frame on a game thread is how the loader's
    // own logger killed the Osiris thread -- an 8488-byte frame on a stack that had no
    // room for it (loader/src/loader.h). Every extender log line
    // runs on one of the game's threads, so the same rule applies here.
    static thread_local char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (g_host.log) g_host.log(buf);
    else fprintf(stderr, "bg3se-ext: %s\n", buf);
}

extern "C" __attribute__((visibility("default"))) int bg3se_ext_init(const bg3se_host_api* host)
{
    if (!host || host->version != BG3SE_HOST_API_VERSION || host->size < sizeof(bg3se_host_api)) {
        if (host && host->log) host->log("ext: host api version mismatch");
        return 1;
    }
    g_host = *host;
    bg3se::platform::log_line("ext: init, build %s, base 0x%lx", host->build_id, (unsigned long)host->base);
    bg3se::SetupScriptExtender();
    return 0;
}
