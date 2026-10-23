#include <stdafx.h>
#include <Extender/ScriptExtender.h>
#include <Extender/Client/SDLManager.h>
#include <CoreLib/Wrappers.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <detours.h>
#if defined(BG3SE_LINUX)
#include <dlfcn.h>
#include <sys/syscall.h>
#include <unistd.h>

// PORT (linux): the [IMGUI-IN] click trace below is compiled out by default, the same way
// IMGUI.cpp compiles its IMGUI_DEBUG out; define both to DEBUG(...) to trace again.
#if !defined(IMGUI_DEBUG)
#define IMGUI_DEBUG(msg, ...)
#endif
#endif

BEGIN_SE()

#if defined(BG3SE_LINUX)
// PORT (linux): SDL_* here are weak references bound to the game's bin/libSDL2.so at dlopen
// (ext/linux/sdl_weak.h); null means no SDL in this process (loader/test/run.sh's stand-in),
// and a refused prologue is reported by hook::install through Wrap()
template <class THook, class TFn>
static void SdlWrap(THook& hook, char const* name, TFn* fn)
{
    if (fn == nullptr) {
        ERR("[SDL] %s: not in this process, not hooked", name);
        return;
    }
    auto target = reinterpret_cast<void*>(ResolveFunctionTrampoline(fn));
    Dl_info info{};
    dladdr(target, &info);
    DEBUG("[SDL] Hooking %s at %p (%s)", name, target, info.dli_fname ? info.dli_fname : "?");
    hook.Wrap(target);
}
#define SDL_WRAP(hook, name) SdlWrap(hook, #name, &name)
#else
#define SDL_WRAP(hook, name) hook.Wrap(ResolveFunctionTrampoline(&name))
#endif

SDLManager::SDLManager()
{}

SDLManager::~SDLManager()
{
    DestroyUI();
    DisableHooks();
}

void SDLManager::EnableHooks()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    SDL_WRAP(CreateWindowHook_, SDL_CreateWindow);
    SDL_WRAP(PollEventHook_, SDL_PollEvent);
    SDL_WRAP(IsTextInputActiveHook_, SDL_IsTextInputActive);
    SDL_WRAP(StartTextInputHook_, SDL_StartTextInput);
    SDL_WRAP(StopTextInputHook_, SDL_StopTextInput);
    DetourTransactionCommit();

    CreateWindowHook_.SetPostHook(&SDLManager::SDLCreateWindowHooked, this);
    PollEventHook_.SetWrapper(&SDLManager::SDLPollEventHooked, this);
    IsTextInputActiveHook_.SetPostHook(&SDLManager::SDLIsTextInputActiveHooked, this);
    StartTextInputHook_.SetWrapper(&SDLManager::SDLStartTextInputHooked, this);
    StopTextInputHook_.SetWrapper(&SDLManager::SDLStopTextInputHooked, this);
}

void SDLManager::DisableHooks()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    CreateWindowHook_.Unwrap();
    PollEventHook_.Unwrap();
    StartTextInputHook_.Unwrap();
    StopTextInputHook_.Unwrap();
    DetourTransactionCommit();
}

void SDLManager::InitializeUI()
{
    if (enableUI_) return;

    ImGui_ImplSDL2_InitForVulkan(window_);
    enableUI_ = true;
}

void SDLManager::DestroyUI()
{
    if (!enableUI_) return;

    enableUI_ = false;
    ImGui_ImplSDL2_Shutdown();
}

void SDLManager::NewFrame()
{
    ImGui_ImplSDL2_NewFrame();
}

void SDLManager::InjectEvent(SDL_Event const& evt)
{
    std::lock_guard _(injectedEventMutex_);
    injectedEvents_.push_back(evt);
}

void SDLManager::SDLCreateWindowHooked(const char* title,
    int x, int y, int w,
    int h, uint32_t flags,
    SDL_Window* window)
{
    window_ = window;
#if defined(BG3SE_LINUX)
    // PORT (linux): the resolution reset follows the extender, not the loader or the
    // wrapper's environment: what the game asked for and what it got.
    int gotW{ 0 }, gotH{ 0 };
    if (window != nullptr) SDL_GetWindowSize(window, &gotW, &gotH);
    INFO("[SDL] SDL_CreateWindow('%s', %d, %d, %d x %d, flags 0x%x) = %p, size %d x %d, flags 0x%x",
        title ? title : "", x, y, w, h, flags, (void*)window, gotW, gotH,
        window != nullptr ? SDL_GetWindowFlags(window) : 0u);
#endif
}

int SDLManager::SDLPollEventHooked(SDLPollEventProc* wrapped, SDL_Event* event)
{
    if (!injectedEvents_.empty()) {
        std::lock_guard _(injectedEventMutex_);
        *event = *injectedEvents_.begin();
        injectedEvents_.ordered_remove_at(0);
        return 1;
    }

#if defined(BG3SE_LINUX)
    // PORT (linux): an event swallowed for IMGUI must not end the game's poll loop. Returning 0
    // tells the caller the queue is empty, so with the cursor over an IMGUI window every motion
    // event ended the game's `while (SDL_PollEvent())` for that frame; the queue drained at about
    // one event per frame and clicks -- the game's own as well -- landed seconds late, at the
    // stale motion position. Poll on until an event survives or the
    // queue really is empty.
    for (;;) {
        int result = wrapped(event);
        if (result == 0) {
            return 0;
        }
        if (SDLPollEventSEH(event, result) != 0) {
            return 1;
        }
    }
#else
    int result = wrapped(event);
    result = SDLPollEventSEH(event, result);

    return result;
#endif
}

int SDLManager::SDLPollEventSEH(SDL_Event* event, int result)
{
    BEGIN_GUARDED()
    return SDLPollEventInternal(event, result);
    END_GUARDED()
    return 0;
}

int SDLManager::SDLPollEventInternal(SDL_Event* event, int result)
{
    gExtender->OnSDLEvent(event);

    if (enableUI_ && result == 1) {
        {
            std::lock_guard _(mutex_);
            ImGui_ImplSDL2_ProcessEvent(event);
        }

        ImGuiIO& io = ImGui::GetIO();
#if defined(BG3SE_LINUX)
        // PORT (linux): click trace for the "spotty clicks" report:
        // which thread delivers each button event, where SDL put it, and where imgui thinks the
        // mouse is. Pairs with the per-frame line in IMGUIManager::Update; both compile out with
        // IMGUI_DEBUG (IMGUI.cpp) now that the poll loop and the queue drain are fixed.
        if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
            IMGUI_DEBUG("[IMGUI-IN] poll tid=%ld %s btn=%d clicks=%d ev=(%d,%d) io=(%.0f,%.0f) capture=%d",
                (long)syscall(SYS_gettid), event->type == SDL_MOUSEBUTTONDOWN ? "down" : "up",
                (int)event->button.button, (int)event->button.clicks, event->button.x, event->button.y,
                io.MousePos.x, io.MousePos.y, io.WantCaptureMouse ? 1 : 0);
        }
#endif
        if (io.WantTextInput
            && (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP || event->type == SDL_TEXTEDITING || event->type == SDL_TEXTINPUT)) {
            result = 0;
        }

        if (io.WantCaptureMouse
            && (event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP || event->type == SDL_MOUSEWHEEL)) {
            result = 0;
        }
    }

    if (result == 1 && gExtender->GetClient().HasExtensionState()) {
        ecl::ExtensionState::Get().OnInputEvent(event, result);
    }

    return result;
}

void SDLManager::SDLIsTextInputActiveHooked(SDL_bool active)
{
    if (enableUI_ && ImGui::GetIO().WantTextInput && !active) {
        SDL_StartTextInput();
    }
}

void SDLManager::SDLStartTextInputHooked(SDLStartTextInputProc* wrapped)
{
    wrapped();
}

void SDLManager::SDLStopTextInputHooked(SDLStartTextInputProc* wrapped)
{
    if (enableUI_ && ImGui::GetIO().WantTextInput) {
        if (!SDL_IsTextInputActive()) {
            SDL_StartTextInput();
        }
    } else {
        wrapped();
    }
}

END_SE()
