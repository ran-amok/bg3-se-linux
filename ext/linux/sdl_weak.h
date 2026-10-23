// sdl_weak.h — SDL2 for the input hooks (Extender/Client/SDL.cpp) and imgui's SDL2 backend,
// from the game's own copy. bin/bg3 NEEDs bin/libSDL2.so (SDL 2.30.6, the pin in
// ext/fetch-externals.sh), so it is in the global scope when the loader dlopens this
// object, and an undefined SDL_* here binds to it. The references are weak so that the link
// keeps --no-undefined for everything else, and so that loader/test/run.sh's stand-in --
// which has no SDL -- still loads the extender: there the pointers are null, and
// SDLManager::EnableHooks() checks before it hooks. Force-included (-include) into the
// TUs that call SDL; a name missing from this list fails the link.
#pragma once
#define BG3SE_SDL_WEAK(name) _Pragma(BG3SE_SDL_WEAK_STR(weak name))
#define BG3SE_SDL_WEAK_STR(x) #x

// Extender/Client/SDL.cpp, Lua/Libs/ClientInput.inl
BG3SE_SDL_WEAK(SDL_CreateWindow)
BG3SE_SDL_WEAK(SDL_PollEvent)
BG3SE_SDL_WEAK(SDL_IsTextInputActive)
BG3SE_SDL_WEAK(SDL_StartTextInput)
BG3SE_SDL_WEAK(SDL_StopTextInput)
BG3SE_SDL_WEAK(SDL_GetTicks)
// backends/imgui_impl_sdl2.cpp
BG3SE_SDL_WEAK(SDL_CaptureMouse)
BG3SE_SDL_WEAK(SDL_CreateSystemCursor)
BG3SE_SDL_WEAK(SDL_free)
BG3SE_SDL_WEAK(SDL_FreeCursor)
BG3SE_SDL_WEAK(SDL_GameControllerClose)
BG3SE_SDL_WEAK(SDL_GameControllerGetAxis)
BG3SE_SDL_WEAK(SDL_GameControllerGetButton)
BG3SE_SDL_WEAK(SDL_GameControllerOpen)
BG3SE_SDL_WEAK(SDL_GetClipboardText)
BG3SE_SDL_WEAK(SDL_GetCurrentVideoDriver)
BG3SE_SDL_WEAK(SDL_GetGlobalMouseState)
BG3SE_SDL_WEAK(SDL_GetKeyboardFocus)
BG3SE_SDL_WEAK(SDL_GetKeyName)
BG3SE_SDL_WEAK(SDL_GetMouseFocus)
BG3SE_SDL_WEAK(SDL_GetPerformanceCounter)
BG3SE_SDL_WEAK(SDL_GetPerformanceFrequency)
BG3SE_SDL_WEAK(SDL_GetRelativeMouseMode)
BG3SE_SDL_WEAK(SDL_GetRendererOutputSize)
BG3SE_SDL_WEAK(SDL_GetScancodeName)
BG3SE_SDL_WEAK(SDL_GetWindowFlags)
BG3SE_SDL_WEAK(SDL_GetWindowID)
BG3SE_SDL_WEAK(SDL_GetWindowPosition)
BG3SE_SDL_WEAK(SDL_GetWindowSize)
BG3SE_SDL_WEAK(SDL_GetWindowWMInfo)
BG3SE_SDL_WEAK(SDL_GL_GetDrawableSize)
BG3SE_SDL_WEAK(SDL_IsGameController)
BG3SE_SDL_WEAK(SDL_NumJoysticks)
BG3SE_SDL_WEAK(SDL_OpenURL)
BG3SE_SDL_WEAK(SDL_SetClipboardText)
BG3SE_SDL_WEAK(SDL_SetCursor)
BG3SE_SDL_WEAK(SDL_SetHint)
BG3SE_SDL_WEAK(SDL_SetTextInputRect)
BG3SE_SDL_WEAK(SDL_ShowCursor)
BG3SE_SDL_WEAK(SDL_Vulkan_GetDrawableSize)
BG3SE_SDL_WEAK(SDL_WarpMouseInWindow)
