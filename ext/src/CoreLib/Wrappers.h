#pragma once

#include <functional>
#include <unordered_set>
#include <detours.h>
#include <hook.h>   // PORT (linux): loader/src/hook.h, the inline hooking engine Detours is replaced by

namespace bg3se {

    template <class T>
    T* ResolveFunctionTrampoline(T* ref)
    {
        auto p = reinterpret_cast<uint8_t *>(ref);
        if (*p == 0xE9) {
            return ResolveFunctionTrampoline(reinterpret_cast<T*>(p + 5 + *(int32_t*)(p + 1)));
        } else {
            return ref;
        }
    }

    void const* ResolveRealFunctionAddress(void const* ptr);

    extern std::unordered_set<void const *> gRegisteredTrampolines;

    template <typename T>
    class WrappedFunction;

    template <typename R, typename... Params>
    class WrappedFunction<R(Params...)>
    {
    public:
        typedef R(*FuncType)(Params...);

        bool IsWrapped() const
        {
            return FuncTrampoline != nullptr;
        }

        // PORT (linux): Module is a dlopen handle and ProcName an Itanium-mangled name
        void Wrap(HMODULE Module, char const * ProcName, FuncType NewFunction)
        {
            void* ExportProc = (void*)GetProcAddress(Module, ProcName);
            if (ExportProc == nullptr) {
                ERR("Could not locate export '%s'", ProcName);
                return;
            }

            Wrap(ExportProc, NewFunction);
        }

        // PORT (linux): hook::install writes the jmp and hands back the trampoline;
        // it refuses (and changes nothing) where the prologue cannot be relocated.
        void Wrap(void * Function, FuncType NewFunction)
        {
            if (IsWrapped()) {
                ERR("[%s] Tried to wrap function multiple times", typeid(*this).name());
                return;
            }

            void* original{ nullptr };
            auto status = hook::install(Function, (void*)NewFunction, &original);
            if (status != hook::Status::ok) {
                ERR("[%s] hook::install failed on %p: %s", typeid(*this).name(), Function, hook::describe(status));
                return;
            }

            OriginalFunc = Function;
            TrampolineFunc = original;
            NewFunc = NewFunction;
            FuncTrampoline = (FuncType)original;
            gRegisteredTrampolines.insert((void const*)NewFunction);
        }

        void Unwrap()
        {
            if (IsWrapped()) {
                auto status = hook::remove(OriginalFunc);
                if (status != hook::Status::ok) {
                    ERR("[%s] hook::remove failed on %p: %s", typeid(*this).name(), OriginalFunc, hook::describe(status));
                }

                OriginalFunc = nullptr;
                TrampolineFunc = nullptr;
                NewFunc = nullptr;
                FuncTrampoline = nullptr;
            }
        }

        __forceinline R operator ()(Params... Args) const
        {
            return FuncTrampoline(std::forward<Params>(Args)...);
        }

        __forceinline FuncType GetTrampoline() const
        {
            return FuncTrampoline;
        }

    private:
        void * OriginalFunc{ nullptr };
        void * TrampolineFunc{ nullptr };
        FuncType NewFunc{ nullptr };
        FuncType FuncTrampoline{ nullptr };
    };

    template <class TObject, class TFunc>
    struct MethodPtrHelpers;

    template <class TObject, class R, class... Params>
    struct MethodPtrHelpers<TObject, R (Params...)>
    {
        using MethodType = R (TObject::*)(Params...);
        using FunctionType = R (*)(TObject*, Params...);
        using BaseFunctionType = R (*)(Params...);
        
#if defined(_MSC_VER)
        static_assert(sizeof(MethodType) == sizeof(FunctionType));

        inline static FunctionType ToFunction(MethodType fun)
        {
            return *reinterpret_cast<FunctionType*>(&fun);
        }

        inline static MethodType ToMethod(FunctionType fun)
        {
            return *reinterpret_cast<MethodType*>(&fun);
        }
#else
        // PATCH (clang): ABI, not syntax. MSVC represents a pointer to a member function of
        // a single-inheritance class as one code pointer; the Itanium ABI uses { ptr, adj },
        // 16 bytes. For a non-virtual member, ptr is the function address and `this`
        // travels as the first argument, so the conversion survives with adj = 0.
        static_assert(sizeof(MethodType) == 2 * sizeof(FunctionType));

        inline static FunctionType ToFunction(MethodType fun)
        {
            FunctionType fn;
            memcpy(&fn, &fun, sizeof(fn));
            return fn;
        }

        inline static MethodType ToMethod(FunctionType fun)
        {
            struct { FunctionType ptr; ptrdiff_t adj; } rep{ fun, 0 };
            MethodType method;
            memcpy(&method, &rep, sizeof(method));
            return method;
        }
#endif
    };

    template <class Tag, class T>
    class WrappableFunction;

    template <class Tag, class R, class... Params>
    class WrappableFunction<Tag, R(Params...)>
    {
    public:
        template <class TReturn, class... TParams>
        struct PostHookSignature
        {
            using Type = void (void*, Params..., R);
        };
        
        template <class... TParams>
        struct PostHookSignature<void, TParams...>
        {
            using Type = void (void*, Params...);
        };
        
        template <class TContext, class TReturn, class... TParams>
        struct PostHookCallbackSignature
        {
            using Type = void (TContext::*)(Params..., TReturn);
        };
        
        template <class TContext, class... TParams>
        struct PostHookCallbackSignature<TContext, void, TParams...>
        {
            using Type = void (TContext::*)(Params...);
        };

        using BaseFuncType = R (Params...);
        using HookFuncType = R (void*, BaseFuncType*, Params...);
        using PreHookFuncType = void (void*, Params...);
        using PostHookFuncType = PostHookSignature<R, Params...>::Type;
        using NoContextHookFuncType = R (BaseFuncType*, Params...);

        void Wrap(HMODULE Module, char const * ProcName)
        {
            if (gHook != nullptr) {
                ERR("[%s] Hook already registered", typeid(*this).name());
                return;
            }

            this->wrapped_.Wrap(Module, ProcName, &CallToTrampoline);
            gHook = this;
        }

        void Wrap(void * Function)
        {
            if (gHook != nullptr) {
                ERR("[%s] Hook already registered", typeid(*this).name());
            }

            this->wrapped_.Wrap(Function, &CallToTrampoline);

            gHook = this;
        }

        // PORT (linux): clang has no implicit function-pointer -> void*
        void Wrap(BaseFuncType* Function)
        {
            Wrap(reinterpret_cast<void*>(Function));
        }

        void Unwrap()
        {
            this->wrapped_.Unwrap();
            gHook = nullptr;
        }

        bool IsWrapped() const
        {
            return wrapped_.IsWrapped();
        }

        void SetWrapper(NoContextHookFuncType* wrapper)
        {
            if (hook_ != nullptr) {
                ERR("[%s] Function already wrapped", typeid(*this).name());
                return;
            }

            gRegisteredTrampolines.insert(ResolveRealFunctionAddress((void const*)&NoContextHook)); // PATCH (clang): no implicit function-pointer -> void*
            gRegisteredTrampolines.insert(ResolveRealFunctionAddress((void const*)wrapper)); // PATCH (clang): no implicit function-pointer -> void*
            hook_ = &NoContextHook;
            func_ = reinterpret_cast<void*>(wrapper);   // PORT (linux): explicit function-pointer -> void*
            func2_ = nullptr;
            context_ = nullptr;
        }

        template <class TContext>
        void SetWrapper(R (* wrapper)(TContext*, BaseFuncType*, Params...), TContext* context)
        {
            if (hook_ != nullptr) {
                ERR("[%s] Function already wrapped", typeid(*this).name());
                return;
            }

            gRegisteredTrampolines.insert(ResolveRealFunctionAddress((void const*)wrapper)); // PATCH (clang): no implicit function-pointer -> void*
            hook_ = reinterpret_cast<HookFuncType*>(wrapper);
            func_ = nullptr;
            func2_ = nullptr;
            context_ = context;
        }

        template <class TContext>
        void SetWrapper(R (TContext::* wrapper)(BaseFuncType*, Params...), TContext* context)
        {
            if (hook_ != nullptr) {
                ERR("[%s] Function already wrapped", typeid(*this).name());
                return;
            }

            auto fun = MethodPtrHelpers<TContext, R (BaseFuncType*, Params...)>::ToFunction(wrapper);
            gRegisteredTrampolines.insert(ResolveRealFunctionAddress((void const*)fun)); // PATCH (clang): no implicit function-pointer -> void*
            hook_ = reinterpret_cast<HookFuncType*>(fun);
            func_ = nullptr;
            func2_ = nullptr;
            context_ = context;
        }

        template <class TContext>
        void SetPreHook(void (TContext::* wrapper)(Params...), TContext* context)
        {
            if (hook_ != nullptr) {
                ERR("[%s] Function already wrapped", typeid(*this).name());
                return;
            }

            gRegisteredTrampolines.insert(ResolveRealFunctionAddress((void const*)&StaticPreHook)); // PATCH (clang): no implicit function-pointer -> void*
            hook_ = &StaticPreHook;
            func_ = reinterpret_cast<void*>(MethodPtrHelpers<TContext, void (Params...)>::ToFunction(wrapper));   // PORT (linux): explicit function-pointer -> void*
            func2_ = nullptr;
            context_ = context;
        }

        template <class TContext>
        void SetPostHook(PostHookCallbackSignature<TContext, R, Params...>::Type wrapper, TContext* context)
        {
            if (hook_ != nullptr) {
                ERR("[%s] Function already wrapped", typeid(*this).name());
                return;
            }

            gRegisteredTrampolines.insert(ResolveRealFunctionAddress((void const*)&StaticPostHook)); // PATCH (clang): no implicit function-pointer -> void*
            hook_ = &StaticPostHook;
            if constexpr (std::is_same_v<R, void>) {
                func_ = reinterpret_cast<void*>(MethodPtrHelpers<TContext, void(Params...)>::ToFunction(wrapper));   // PORT (linux): explicit function-pointer -> void*
            } else {
                func_ = reinterpret_cast<void*>(MethodPtrHelpers<TContext, void(Params..., R)>::ToFunction(wrapper));   // PORT (linux): explicit function-pointer -> void*
            }
            func2_ = nullptr;
            context_ = context;
        }

        template <class TContext>
        void SetPrePostHook(void (TContext::* preHook)(Params...), PostHookCallbackSignature<TContext, R, Params...>::Type postHook, TContext* context)
        {
            if (hook_ != nullptr) {
                ERR("[%s] Function already wrapped", typeid(*this).name());
                return;
            }

            gRegisteredTrampolines.insert(ResolveRealFunctionAddress((void const*)&StaticPrePostHook)); // PATCH (clang): no implicit function-pointer -> void*
            hook_ = &StaticPrePostHook;
            func_ = reinterpret_cast<void*>(MethodPtrHelpers<TContext, void(Params...)>::ToFunction(preHook));   // PORT (linux): explicit function-pointer -> void*
            if constexpr (std::is_same_v<R, void>) {
                func2_ = reinterpret_cast<void*>(MethodPtrHelpers<TContext, void(Params...)>::ToFunction(postHook));   // PORT (linux): explicit function-pointer -> void*
            } else {
                func2_ = reinterpret_cast<void*>(MethodPtrHelpers<TContext, void(Params..., R)>::ToFunction(postHook));   // PORT (linux): explicit function-pointer -> void*
            }
            context_ = context;
        }

        void ClearHook()
        {
            hook_ = nullptr;
            func_ = nullptr;
            func2_ = nullptr;
            context_ = nullptr;
        }

        inline bool IsHooked() const
        {
            return hook_ != nullptr;
        }

        __forceinline R CallOriginal(Params... Args) const
        {
            return wrapped_(std::forward<Params>(Args)...);
        }

        __forceinline R CallWithHooks(Params... Args) const
        {
            if (hook_) {
                return hook_(context_, this->wrapped_.GetTrampoline(), std::forward<Params>(Args)...);
            } else {
                return this->CallOriginal(std::forward<Params>(Args)...);
            }
        }

    private:
        WrappedFunction<R(Params...)> wrapped_;
        HookFuncType* hook_{ nullptr };
        void* func_{ nullptr };
        void* func2_{ nullptr };
        void* context_{ nullptr };

        static R CallToTrampoline(Params... Args)
        {
            return gHook->CallWithHooks(std::forward<Params>(Args)...);
        }

        static R NoContextHook(void* ctx, BaseFuncType* fun, Params... args)
        {
            auto hook = reinterpret_cast<NoContextHookFuncType*>(gHook->func_);
            return hook(fun, std::forward<Params>(args)...);
        }

        static R StaticPreHook(void* ctx, BaseFuncType* fun, Params... args)
        {
            auto hook = reinterpret_cast<PreHookFuncType*>(gHook->func_);
            hook(ctx, std::forward<Params>(args)...);
            return gHook->CallOriginal(std::forward<Params>(args)...);
        }

        static R StaticPostHook(void* ctx, BaseFuncType* fun, Params... args)
        {
            auto hook = reinterpret_cast<PostHookFuncType*>(gHook->func_);
            if constexpr (std::is_same_v<R, void>) {
                gHook->CallOriginal(std::forward<Params>(args)...);
                hook(ctx, std::forward<Params>(args)...);
            } else {
                auto retval = gHook->CallOriginal(std::forward<Params>(args)...);
                hook(ctx, std::forward<Params>(args)..., retval);
                return retval;
            }
        }

        static R StaticPrePostHook(void* ctx, BaseFuncType* fun, Params... args)
        {
            auto preHook = reinterpret_cast<PreHookFuncType*>(gHook->func_);
            preHook(ctx, std::forward<Params>(args)...);

            auto postHook = reinterpret_cast<PostHookFuncType*>(gHook->func2_);
            if constexpr (std::is_same_v<R, void>) {
                gHook->CallOriginal(std::forward<Params>(args)...);
                postHook(ctx, std::forward<Params>(args)...);
            } else {
                auto retval = gHook->CallOriginal(std::forward<Params>(args)...);
                postHook(ctx, std::forward<Params>(args)..., retval);
                return retval;
            }
        }

        // PORT (linux): inline, so no out-of-line `decltype(X)::gHook` definition (which clang rejects) is needed
        inline static WrappableFunction<Tag, R(Params...)> * gHook{ nullptr };
    };
}
