// host_api.h — the C ABI between libbg3se.so (the loader) and libbg3se-ext.so (the
// ported extender, ext/). The loader dlopens the extender at deferred init on a known
// build and calls bg3se_ext_init() with this table; the extender keeps the pointer.
// Plain C types only: the two objects carry different C++ runtimes (the extender's
// libc++ is ABI v2, the game's; the loader's is zig's v1).
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BG3SE_HOST_API_VERSION 1

typedef struct bg3se_host_api {
    uint32_t version;        // BG3SE_HOST_API_VERSION
    uint32_t size;           // sizeof(bg3se_host_api), for later additions
    const char* build_id;    // hex, of bin/bg3
    uintptr_t base;          // the PIE load base
    const char* state_dir;   // ~/.local/state/bg3-se-linux (logs, console socket)
    void (*log)(const char* line);                   // one line into bg3se.log
    // manifest.h, by name (upstream's symbol names; see manifest/<build-id>.tsv)
    void* (*manifest_get)(const char* symbol);       // runtime address, or NULL
    int (*manifest_hookable)(const char* symbol);    // 1 if hook::install may take it
    int (*manifest_require)(const char* feature, const char* const* symbols, size_t n, int hooks);
} bg3se_host_api;

// The one export of libbg3se-ext.so. Returns 0 on success; anything else means the
// extender is not running and the loader logs it.
typedef int (*bg3se_ext_init_fn)(const bg3se_host_api* host);

#ifdef __cplusplus
}
#endif
