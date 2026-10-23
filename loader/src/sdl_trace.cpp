// sdl_trace.cpp — BG3SE_SDL_TRACE=1: log the display-mode calls bin/bg3 makes through its PLT.
//
// Added for the resolution reset: a stored 3840x2160 survives a
// loader-only launch and not a launch with the extender, and the game reads
// graphicSettings.lsx either way. What differs is what it then asks SDL. These interposers sit
// in the loader, not the extender, so a loader-only launch writes the same trace to compare
// against. Passive: each calls the next definition (the game's bin/libSDL2.so) and logs the
// arguments and the result, the first kCap calls per function, and after that every 256th with a count.
//
// Only functions the extender does not reference itself are here: its weak SDL_* references
// (ext/linux/sdl_weak.h) bind through the global scope, where a preloaded object comes before
// libSDL2, so interposing SDL_CreateWindow or SDL_GetWindowSize here would hand the
// extender's inline hooks our function instead of SDL's.
#include "loader.h"

#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <atomic>

namespace {

struct DisplayMode {  // SDL_DisplayMode, SDL 2.30
    uint32_t format;
    int w, h, refresh_rate;
    void* driverdata;
};

constexpr unsigned kCap = 64;

bool tracing()
{
    static int on = -1;
    if (on < 0) {
        const char* v = getenv("BG3SE_SDL_TRACE");
        on = bg3se::g_is_game && v && *v && *v != '0';
    }
    return on;
}

// true if this call is to be logged; *n is its 1-based index
bool sample(std::atomic<unsigned>& count, unsigned* n)
{
    *n = count.fetch_add(1, std::memory_order_relaxed) + 1;
    return *n <= kCap || (*n % 256) == 0;
}

template <class F>
F next(const char* name)
{
    return (F)dlsym(RTLD_NEXT, name);
}

}  // namespace

using bg3se::log_line;

#define REAL(name, type) \
    static auto real = next<type>(#name); \
    if (!real) return -1

extern "C" {

__attribute__((visibility("default"))) int SDL_GetNumVideoDisplays(void)
{
    REAL(SDL_GetNumVideoDisplays, int (*)(void));
    int rc = real();
    static std::atomic<unsigned> c{0}; unsigned n;
    if (tracing() && sample(c, &n)) log_line("sdl #%u SDL_GetNumVideoDisplays() = %d", n, rc);
    return rc;
}

__attribute__((visibility("default"))) int SDL_GetNumDisplayModes(int display)
{
    REAL(SDL_GetNumDisplayModes, int (*)(int));
    int rc = real(display);
    static std::atomic<unsigned> c{0}; unsigned n;
    if (tracing() && sample(c, &n)) log_line("sdl #%u SDL_GetNumDisplayModes(%d) = %d", n, display, rc);
    return rc;
}

__attribute__((visibility("default"))) int SDL_GetDisplayMode(int display, int index, DisplayMode* mode)
{
    REAL(SDL_GetDisplayMode, int (*)(int, int, DisplayMode*));
    int rc = real(display, index, mode);
    static std::atomic<unsigned> c{0}; unsigned n;
    if (tracing() && sample(c, &n))
        log_line("sdl #%u SDL_GetDisplayMode(%d, %d) = %d: %dx%d@%d fmt 0x%x", n, display, index, rc,
                 mode ? mode->w : 0, mode ? mode->h : 0, mode ? mode->refresh_rate : 0, mode ? mode->format : 0);
    return rc;
}

__attribute__((visibility("default"))) int SDL_GetDesktopDisplayMode(int display, DisplayMode* mode)
{
    REAL(SDL_GetDesktopDisplayMode, int (*)(int, DisplayMode*));
    int rc = real(display, mode);
    static std::atomic<unsigned> c{0}; unsigned n;
    if (tracing() && sample(c, &n))
        log_line("sdl #%u SDL_GetDesktopDisplayMode(%d) = %d: %dx%d@%d", n, display, rc,
                 mode ? mode->w : 0, mode ? mode->h : 0, mode ? mode->refresh_rate : 0);
    return rc;
}

__attribute__((visibility("default"))) DisplayMode* SDL_GetClosestDisplayMode(int display, const DisplayMode* want, DisplayMode* got)
{
    static auto real = next<DisplayMode* (*)(int, const DisplayMode*, DisplayMode*)>("SDL_GetClosestDisplayMode");
    if (!real) return nullptr;
    DisplayMode* rc = real(display, want, got);
    static std::atomic<unsigned> c{0}; unsigned n;
    if (tracing() && sample(c, &n))
        log_line("sdl #%u SDL_GetClosestDisplayMode(%d, want %dx%d@%d) = %s %dx%d@%d", n, display,
                 want ? want->w : 0, want ? want->h : 0, want ? want->refresh_rate : 0,
                 rc ? "" : "NULL", rc ? rc->w : 0, rc ? rc->h : 0, rc ? rc->refresh_rate : 0);
    return rc;
}

__attribute__((visibility("default"))) int SDL_GetDisplayBounds(int display, int* rect)
{
    REAL(SDL_GetDisplayBounds, int (*)(int, int*));
    int rc = real(display, rect);
    static std::atomic<unsigned> c{0}; unsigned n;
    if (tracing() && sample(c, &n))
        log_line("sdl #%u SDL_GetDisplayBounds(%d) = %d: %d,%d %dx%d", n, display, rc,
                 rect ? rect[0] : 0, rect ? rect[1] : 0, rect ? rect[2] : 0, rect ? rect[3] : 0);
    return rc;
}

__attribute__((visibility("default"))) int SDL_SetWindowDisplayMode(void* window, const DisplayMode* mode)
{
    REAL(SDL_SetWindowDisplayMode, int (*)(void*, const DisplayMode*));
    int rc = real(window, mode);
    static std::atomic<unsigned> c{0}; unsigned n;
    if (tracing() && sample(c, &n))
        log_line("sdl #%u SDL_SetWindowDisplayMode(%p, %s%dx%d@%d) = %d", n, window, mode ? "" : "NULL ",
                 mode ? mode->w : 0, mode ? mode->h : 0, mode ? mode->refresh_rate : 0, rc);
    return rc;
}

__attribute__((visibility("default"))) int SDL_SetWindowFullscreen(void* window, uint32_t flags)
{
    REAL(SDL_SetWindowFullscreen, int (*)(void*, uint32_t));
    int rc = real(window, flags);
    static std::atomic<unsigned> c{0}; unsigned n;
    if (tracing() && sample(c, &n)) log_line("sdl #%u SDL_SetWindowFullscreen(%p, 0x%x) = %d", n, window, flags, rc);
    return rc;
}

__attribute__((visibility("default"))) void SDL_SetWindowSize(void* window, int w, int h)
{
    static auto real = next<void (*)(void*, int, int)>("SDL_SetWindowSize");
    if (!real) return;
    real(window, w, h);
    static std::atomic<unsigned> c{0}; unsigned n;
    if (tracing() && sample(c, &n)) log_line("sdl #%u SDL_SetWindowSize(%p, %d x %d)", n, window, w, h);
}

}  // extern "C"
