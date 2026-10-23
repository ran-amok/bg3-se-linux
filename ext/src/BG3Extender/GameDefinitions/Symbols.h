#pragma once

#include <GameHooks/EngineHooksFwdDecl.h>
#include <GameDefinitions/FileReader.h>

BEGIN_SE()

struct ResourceManager;
struct App;

struct IndexSymbolInfo
{
    char const* name;
    int32_t* context;
};

struct StaticSymbols : Noncopyable<StaticSymbols>
{
    std::unordered_map<int32_t*, char const*> IndexSymbolToContextMaps;
    std::unordered_map<int32_t*, IndexSymbolInfo> IndexSymbolToNameMaps;
    std::unordered_map<void const*, char const*> StaticStringRegistrantMaps;

    struct EoCLibraryInfo
    {
        uint8_t const * initFunc;
        uint8_t const * freeFunc;
        uint32_t refs;
    };

    FixedString::GetStringProc* ls__FixedString__GetString{ nullptr };
    FixedString::IncRefProc* ls__FixedString__IncRef{ nullptr };
    GlobalStringTable::MainTable::CreateFromStringProc* ls__GlobalStringTable__MainTable__CreateFromString{ nullptr };
    GlobalStringTable::MainTable::DecRefProc* ls__GlobalStringTable__MainTable__DecRef{ nullptr };
    GlobalStringTable** ls__gGlobalStringTable{ nullptr };

    ThreadRegistry::RequestThreadIndexProc* ls__ThreadRegistry__RequestThreadIndex{ nullptr };
    FileReader__CtorProc* ls__FileReader__ctor{ nullptr };
    FileReader__DtorProc* ls__FileReader__dtor{ nullptr };
    STDString** ls__PathRoots{ nullptr };
    App__CtorProc* App__Ctor{ nullptr };
    App__UpdatePathsProc* App__UpdatePaths{ nullptr };

    ecl::EoCClient** ecl__EoCClient{ nullptr };
    esv::EoCServer** esv__EoCServer{ nullptr };

    ecl::EoCClient__HandleErrorProc* ecl__EoCClient__HandleError{ nullptr };
    ecl::CursorControl** ecl__gCursorControl{ nullptr };
    ecl::DragDropManager** ls__gDragDropManager{ nullptr };

    TranslatedStringRepository** ls__gTranslatedStringRepository{ nullptr };
    TranslatedStringKeyManager** ls__gTranslatedStringKeyManager{ nullptr };

    GameStateEventManager** ecl__gGameStateEventManager{ nullptr };
    GameStateEventManager** esv__gGameStateEventManager{ nullptr };
    ecl::GameStateThreaded__GameStateWorker__DoWorkProc* ecl__GameStateThreaded__GameStateWorker__DoWork{ nullptr };
    esv::GameStateThreaded__GameStateWorker__DoWorkProc* esv__GameStateThreaded__GameStateWorker__DoWork{ nullptr };
    ecl::GameStateMachine__UpdateProc* ecl__GameStateMachine__Update{ nullptr };
    esv::GameStateMachine__UpdateProc* esv__GameStateMachine__Update{ nullptr };
    App__LoadGraphicSettingsProc* App__LoadGraphicSettings{ nullptr };
    input::InputManager** ls__gInputManager{ nullptr };

    ecs::EntityWorld__UpdateProc* ecs__EntityWorld__Update{ nullptr };
    ecs::EntityWorld__FlushECBsProc* ecs__EntityWorld__FlushECBs{ nullptr };

    stats::SpellPrototypeManager** eoc__SpellPrototypeManager{ nullptr };
    stats::SpellPrototype__InitProc* eoc__SpellPrototype__Init{ nullptr };

    stats::StatusPrototypeManager** eoc__StatusPrototypeManager{ nullptr };
    stats::StatusPrototype__InitProc* eoc__StatusPrototype__Init{ nullptr };

    stats::BoostPrototypeManager** eoc__BoostPrototypeManager{ nullptr };

    stats::PassivePrototypeManager** eoc__PassivePrototypeManager{ nullptr };
    stats::PassivePrototype__InitProc* eoc__PassivePrototype__Init{ nullptr };

    stats::InterruptPrototypeManager** eoc__InterruptPrototypeManager{ nullptr };
    stats::InterruptPrototype__InitProc* eoc__InterruptPrototype__Init{ nullptr };

    DynamicStatsExpressionManager** eoc__DynamicStatsExpressionManager{ nullptr };

    stats::ParseStaticBoostsProc* eoc__ParseStaticBoosts{ nullptr };

    esv::OsirisVariableHelper__SavegameVisitProc* esv__OsirisVariableHelper__SavegameVisit{ nullptr };

    stats::DealDamageFunctor__ApplyDamageProc* stats__DealDamageFunctor__ApplyDamage{ nullptr };
    esv::StatsSystem__ThrowDamageEventProc* esv__StatsSystem__ThrowDamageEvent{ nullptr };

    stats::ExecuteFunctorProc<stats::AttackTargetContextData>* esv__ExecuteStatsFunctor_AttackTargetContext{ nullptr };
    stats::ExecuteFunctorProc<stats::AttackPositionContextData>* esv__ExecuteStatsFunctor_AttackPositionContext{ nullptr };
    stats::ExecuteFunctorProc<stats::MoveContextData>* esv__ExecuteStatsFunctor_MoveContext{ nullptr };
    stats::ExecuteFunctorProc<stats::TargetContextData>* esv__ExecuteStatsFunctor_TargetContext{ nullptr };
    stats::ExecuteFunctorProc<stats::NearbyAttackedContextData>* esv__ExecuteStatsFunctor_NearbyAttackedContext{ nullptr };
    stats::ExecuteFunctorProc<stats::NearbyAttackingContextData>* esv__ExecuteStatsFunctor_NearbyAttackingContext{ nullptr };
    stats::ExecuteFunctorProc<stats::EquipContextData>* esv__ExecuteStatsFunctor_EquipContext{ nullptr };
    stats::ExecuteFunctorProc<stats::SourceContextData>* esv__ExecuteStatsFunctor_SourceContext{ nullptr };
    stats::ExecuteInterruptFunctorProc* esv__ExecuteStatsFunctor_InterruptContext{ nullptr };

    stats::RPGStats** gRPGStats{ nullptr };
    stats::RPGStats__LoadProc* RPGStats__Load{ nullptr };
    stats::RPGStats__ParseDataBuffersProc* RPGStats__ParseDataBuffers{ nullptr };
    stats::Object__SetPropertyStringProc* stats__Object__SetPropertyString{ nullptr };

    esv::LevelManager** esv__LevelManager{ nullptr };
    ecl::LevelManager** ecl__LevelManager{ nullptr };
    GlobalTemplateManager** ls__GlobalTemplateManager{ nullptr };
    LevelCacheTemplateManager** esv__CacheTemplateManager{ nullptr };

    void** esv__gSurfaceActionFactory{ nullptr };
    esv::SurfaceActionFactoryCreateProc* esv__SurfaceActionFactory__DoCreateAction{ nullptr };
    esv::TransformSurfaceAction__InitProc* esv__TransformSurfaceAction__Init{ nullptr };

    AiGrid__FindPathProc* eoc__AiGrid__FindPath{ nullptr };
    AiGrid__FindPathImmediateProc* eoc__AiGrid__FindPathImmediate{ nullptr };

    esv::SavegameManager** esv__SavegameManager{ nullptr };

    App** AppInstance{ nullptr };

    std::map<uint8_t const *, EoCLibraryInfo> Libraries;

    ls__GlobalAllocator__GetProc* ls__GlobalAllocator__Get{ nullptr };
    ls__GlobalAllocator__AllocProc* ls__GlobalAllocator__Alloc{ nullptr };
    ls__GlobalAllocator__FreeProc* ls__GlobalAllocator__Free{ nullptr };

#if defined(BG3SE_LINUX)
    // PORT (linux): functions the Linux binary has under its own shape; the manifest
    // rows carry these names.
    using FixedString__CreateProc = void (uint32_t* result, char const* str, int length);  // sret; length < 0 = strlen
    using FixedString__GetStringLinuxProc = char const* (uint32_t index);
    using Path__CtorRootViewProc = void (Path* self, PathRootType root, LSStringView const* path);
    using FileSystem__DoesFileExistProc = bool (Path const* path, bool unknown);
    FixedString__CreateProc* ls__FixedString__Create{ nullptr };
    FixedString__GetStringLinuxProc* ls__FixedString__GetStringLinux{ nullptr };
    Path__CtorRootViewProc* ls__Path__ctor_root_view{ nullptr };
    FileSystem__DoesFileExistProc* ls__FileSystem__DoesFileExist{ nullptr };
    // The global template bank slot, where Windows reads the TEB: a byte in .data whose
    // value is the slot unless it is 2, and then a thread-local byte is
    // (GameHelpers.cpp's GetTemplateBankSlot). The function is
    // here only so the thread-local displacement can be checked against the instruction
    // that carries it; nothing calls it.
    uint8_t* ls__g_GlobalTemplateBankType{ nullptr };
    void* esv__GetGameObjectTemplate{ nullptr };
    // (anonymous namespace)::LoadTextureHelper(FixedString const&) -- the body upstream's
    // ls::AppliedMaterial::LoadTexture pattern finds on Windows -- is bin/bg3 0x32c8b00, and its
    // Linux shape is not upstream's (self, guid) -> TextureDescriptor*: it returns this 32-byte
    // struct through a hidden pointer in rdi with the FixedString in rsi, and the descriptor
    // (the rf::Texture* InstantiateTexture returned) is its first word. The manifest row
    // ls__AppliedMaterial__LoadTexture binds here and the upstream-typed member stays null
    // (DataLibraries.cpp), so nothing calls the function with the wrong prototype. Field
    // names past Texture describe the stores seen, nothing reads them.
    struct LoadTextureResultLinux
    {
        TextureDescriptor* Texture{ nullptr };   // [0x00] null when the resource is missing or not loadable
        void* Texture2{ nullptr };               // [0x08] the same pointer, or 0 with it
        uint16_t Unk10{ 0 };                     // [0x10] 0xffff
        uint16_t Unk12{ 0 };
        uint8_t Kind{ 0 };                       // [0x14] 4
        uint8_t Pad15[3]{ 0, 0, 0 };
        int32_t ViewIndex{ -1 };                 // [0x18] GetViewIndex 0x2586f00's pair; -1/-1 with a null Texture
        int32_t ViewIndex2{ -1 };                // [0x1c]
    };
    static_assert(sizeof(LoadTextureResultLinux) == 0x20);
    static_assert(offsetof(LoadTextureResultLinux, Kind) == 0x14);
    static_assert(offsetof(LoadTextureResultLinux, ViewIndex) == 0x18);
    using LoadTextureHelperLinuxProc = void (LoadTextureResultLinux* result, FixedString const& textureGuid);
    LoadTextureHelperLinuxProc* ls__AppliedMaterial__LoadTextureLinux{ nullptr };
    // ls::AppliedMaterial::TryOverrideTexture2DParameter is bin/bg3 0x2942580 (the
    // (FixedString const&, FixedString const&, bool) overload of the two the name matches), and
    // LTO did IPA-SRA on its first argument: the param FixedString arrives BY VALUE as its 4-byte
    // index in esi, where upstream's proc declares FixedString const&, and the mangled name's
    // trailing bool is dead (never read). Three call sites read that way; the callee's own
    // `cmp dword [rcx], esi` against a Texture2DParams entry's Name and its
    // `mov edi, esi; call ls__FixedString__IncRef` say it independently. The manifest row binds
    // here and the upstream-typed member stays null (DataLibraries.cpp), so nothing can call the
    // function with the wrong prototype.
    using AppliedMaterial__TryOverrideTexture2DParameterLinuxProc =
        bool (AppliedMaterial* self, uint32_t param, FixedString const& textureGuid);
    AppliedMaterial__TryOverrideTexture2DParameterLinuxProc* ls__AppliedMaterial__TryOverrideTexture2DParameterLinux{ nullptr };
    // ls::AppliedMaterial::LoadVirtualTexture is bin/bg3 0x24f9c20, and LTO's IPA-SRA dropped the
    // AppliedMaterial* this outright -- the body never reads it -- so the sole argument is the
    // texture FixedString by reference in rdi, where upstream's proc declares (self, guid). A call
    // through upstream's type would hand the callee the material where it reads the GUID. Both call
    // sites pass one argument (0x2942b67 with TryOverrideVirtualTextureParameter's third, 0x74189c1
    // with `lea r14,[rsp+0x44]`), and the callee's hash key is `mov ecx, dword ptr [rdi]`. The
    // manifest row binds here and the upstream-typed member stays null (DataLibraries.cpp).
    using AppliedMaterial__LoadVirtualTextureLinuxProc = void* (FixedString const& textureGuid);
    AppliedMaterial__LoadVirtualTextureLinuxProc* ls__AppliedMaterial__LoadVirtualTextureLinux{ nullptr };
    // esv::SurfaceActionFactory::CreateAction is bin/bg3 0x38a5fd0 and takes FIVE arguments where
    // upstream's proc declares three: the 32-bit TStoryActionID and the ClassDescriptions reference
    // sit between the action type and the handle. The body says so on the mirror's own offsets --
    // edx lands at SurfaceAction +0x10 (StoryActionID) and rcx at +0x50 (ClassDescriptionMgr, the
    // member upstream's Surface.inl assigns by hand after the call), and r8 is compared against
    // 0xffc0000000000000, ComponentHandle::NullHandle exactly, before the factory allocates the
    // handle it stores at +0x70. A call through upstream's type would put NullHandle in the story
    // action id and leave the two pointer arguments as whatever the caller's rcx and r8 held. The
    // manifest row binds here and the upstream-typed member stays null (DataLibraries.cpp).
    using SurfaceActionFactoryCreateLinuxProc = esv::SurfaceAction* (void* self,
        SurfaceActionType type, int32_t storyActionId,
        resource::GuidResourceBankBase* classDescriptions, uint64_t actionHandle);
    SurfaceActionFactoryCreateLinuxProc* esv__SurfaceActionFactory__DoCreateActionLinux{ nullptr };
#endif

    resource::GuidResourceManager** eoc__gGuidResourceManager{ nullptr };
    ResourceManager** ls__gGlobalResourceManager{ nullptr };
    TextureAtlasMap** ls__gTextureAtlasMap{ nullptr };

    AppliedMaterial__LoadTextureProc* ls__AppliedMaterial__LoadTexture{ nullptr };
    TextureManager__UnloadTextureProc* ls__TextureManager__UnloadTexture{ nullptr };
    AppliedMaterial__LoadVirtualTextureProc* ls__AppliedMaterial__LoadVirtualTexture{ nullptr };
    AppliedMaterial__TryOverrideTexture2DParameterProc* ls__AppliedMaterial__TryOverrideTexture2DParameter{ nullptr };

    Visual__UpdateBlendshapeWeightsFromSkeletonProc* ls__Visual__UpdateBlendshapeWeightsFromSkeleton{ nullptr };

    // PORT (linux): all seven of these are rowed in the manifest. Both locks are a
    // 4-byte pthread_spinlock_t in bin/bg3, not an 8-byte SRWLOCK -- gMutex 0x7b65ff8 and the
    // SymbolManager's 0x7b66060 on a187af94, both taken with pthread_spin_lock@plt -- so they
    // have no shared mode at all and the declared type is NS_SPINLOCK (ext/linux/shim/
    // windows.h), not SRWLOCK: the latter is the shim's own lazily created pthread_rwlock_t*
    // and taking one of these through it would write a pointer over the lock and the 4 bytes
    // after it. Both of upstream's `Shared` acquires are exclusive spins here
    // (Reflection::GetType in GameDefinitions/UI.h, SymbolManager::FindString in
    // Lua/Libs/ClientUI/Builtins.inl), and GetType follows the game's recursion protocol on
    // top: take the spinlock only when LockedByThreadId (`gThreadID`, a *gettid* tid, not a
    // Win32 thread id and not pthread_self) differs from the caller's, bump NumLocks
    // (`gRecursiveDepth`), and unlock only when the saved depth was 0 -- a plain re-entrant
    // acquire deadlocks a non-recursive spinlock where the Win32 shared acquire merely
    // blocks. Witnesses per row: tools/targets-hand.tsv. This code has not been run in game.
    std::atomic<uint32_t>* Noesis__Reflection__LockedByThreadId{ nullptr };
    NS_SPINLOCK* Noesis__Reflection__Lock{ nullptr };
    std::atomic<uint32_t>* Noesis__Reflection__NumLocks{ nullptr };
    void* Noesis__Reflection__Data{ nullptr };

    NS_SPINLOCK* Noesis__SymbolManager__Lock{ nullptr };
    void* Noesis__SymbolManager__Data{ nullptr };
    void** Noesis__gErrorHandler{ nullptr };
    void* Noesis__GUI__LoadXaml{ nullptr };
    void* Noesis__Visual__AddVisualChild{ nullptr };
    void* Noesis__Visual__RemoveVisualChild{ nullptr };
    void* Noesis__Visual__PointFromScreen{ nullptr };
    void* Noesis__Visual__PointToScreen{ nullptr };
    void* Noesis__VisualTreeHelper__HitTest{ nullptr };

    void* ui__DataContextProvider__ExecuteCommandQueues{ nullptr };

    GlobalSwitches** ls__GlobalSwitches{ nullptr };

    FindFirstFileWProc* Kernel_FindFirstFileW{ nullptr };
    FindNextFileWProc* Kernel_FindNextFileW{ nullptr };
    FindCloseProc* Kernel_FindClose{ nullptr };

    inline StaticSymbols() {}
    StaticSymbols(StaticSymbols const &) = delete;
    StaticSymbols & operator = (StaticSymbols const &) = delete;

    inline GlobalSwitches * GetGlobalSwitches() const
    {
        if (ls__GlobalSwitches && *ls__GlobalSwitches) {
            return *ls__GlobalSwitches;
        } else {
            return nullptr;
        }
    }

    inline ecl::EoCClient * GetEoCClient() const
    {
        if (ecl__EoCClient == nullptr || *ecl__EoCClient == nullptr) {
            return nullptr;
        } else {
            return *ecl__EoCClient;
        }
    }

    inline esv::EoCServer* GetEoCServer() const
    {
        if (esv__EoCServer == nullptr || *esv__EoCServer == nullptr) {
            return nullptr;
        } else {
            return *esv__EoCServer;
        }
    }

    ModManager* GetModManagerClient() const;
    ModManager* GetModManagerServer() const;

    inline stats::RPGStats* GetStats() const
    {
        if (gRPGStats && *gRPGStats) {
            return *gRPGStats;
        } else {
            return nullptr;
        }
    }

    std::optional<ecl::GameState> GetClientState() const;
    std::optional<esv::GameState> GetServerState() const;

    inline TranslatedStringRepository* GetTranslatedStringRepository() const
    {
        if (ls__gTranslatedStringRepository != nullptr) {
            return *ls__gTranslatedStringRepository;
        } else {
            return nullptr;
        }
    }

    inline TranslatedStringKeyManager* GetTranslatedStringKeyManager() const
    {
        if (ls__gTranslatedStringKeyManager != nullptr) {
            return *ls__gTranslatedStringKeyManager;
        } else {
            return nullptr;
        }
    }

    inline esv::LevelManager* GetServerLevelManager() const
    {
        if (esv__LevelManager != nullptr
            && *esv__LevelManager != nullptr) {
            return *esv__LevelManager;
        } else {
            return {};
        }
    }

    esv::Level* GetCurrentServerLevel() const;
    FixedString GetCurrentServerLevelName() const;

    inline GlobalTemplateManager* GetGlobalTemplateManager() const
    {
        if (ls__GlobalTemplateManager) {
            return *ls__GlobalTemplateManager;
        } else {
            return nullptr;
        }
    }

    inline ecl::LevelManager* GetClientLevelManager() const
    {
        if (ecl__LevelManager != nullptr
            && *ecl__LevelManager != nullptr) {
            return *ecl__LevelManager;
        } else {
            return {};
        }
    }

    ecl::Level* GetCurrentClientLevel() const;

    GlobalTemplateBank* GetGlobalTemplateBank() const;
    ResourceBank* GetCurrentResourceBank() const;

    // PORT (linux): a symbol the manifest does not supply stays null, and upstream
    // dereferences these two unconditionally (on Windows the pattern scan always finds
    // them). Callers that can fail gracefully go through these instead; nullptr means
    // "not located in this build".
    inline ResourceManager* GetResourceManager() const
    {
        return (ls__gGlobalResourceManager != nullptr) ? *ls__gGlobalResourceManager : nullptr;
    }

    inline TextureAtlasMap* GetTextureAtlasMap() const
    {
        return (ls__gTextureAtlasMap != nullptr) ? *ls__gTextureAtlasMap : nullptr;
    }

    void CanonicalizePath(STDString & path) const;
    STDString ToPath(StringView path, PathRootType root, bool canonicalize = true) const;
    FileReaderPin MakeFileReader(StringView path, PathRootType root = PathRootType::Data, bool canonicalize = true) const;
    FileReaderPin MakeFileReaderAbsolute(StringView path) const;
    void DestroyFileReader(FileReader* reader);
    bool FileExists(StringView path, PathRootType root = PathRootType::Data, bool canonicalize = true) const;
};

extern StaticSymbols* gStaticSymbols;

void InitStaticSymbols();

inline StaticSymbols& GetStaticSymbols()
{
        return *gStaticSymbols;
}

END_SE()
