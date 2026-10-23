// DataLibraries.cpp — PORT (linux): where upstream scans bg3.exe for byte patterns
// (BinaryMappings.xml), the Linux port asks the loader's offset manifest for each of
// StaticSymbols' members by upstream's own symbol name (manifest/<build-id>.tsv). A name
// the manifest has no verified row for stays null, and the features
// that need it stay off. The names are kept in RegisterSymbols() below, the same list
// as upstream's, plus the Linux-only entries StaticSymbols carries under BG3SE_LINUX.
#include "stdafx.h"
#include <GameHooks/DataLibraries.h>
#include <Extender/Shared/ExtensionState.h>
#include <Extender/ScriptExtender.h>
#include <GameDefinitions/Symbols.h>
#include <platform.h>
#include <string>
#include <chrono>

namespace bg3se
{
    // The game's allocator, as LTO left it on Linux: Alloc(size, alignment), Free(ptr),
    // no allocator object.
    _Post_writable_byte_size_(size) void* BG3Alloc(std::size_t size)
    {
        auto const& sym = GetStaticSymbols();
        if (sym.ls__GlobalAllocator__Alloc && sym.ls__GlobalAllocator__Free) {
            return sym.ls__GlobalAllocator__Alloc(size, 8);
        } else {
            return malloc(size);
        }
    }

    void BG3Free(_Post_ptr_invalid_ _Pre_valid_ void* ptr) noexcept
    {
        auto const& sym = GetStaticSymbols();
        if (sym.ls__GlobalAllocator__Alloc && sym.ls__GlobalAllocator__Free) {
            sym.ls__GlobalAllocator__Free(ptr);
        } else {
            return free(ptr);
        }
    }

    // FixedString creation on Linux is ls::FixedString::Create(char const*, int len):
    // MainTable::CreateFromString is inlined into it. The result
    // is returned through a hidden pointer (FixedString has a destructor), and carries
    // the reference the caller owns.
    uint32_t LinuxCreateFromString(LSStringView const& sv)
    {
        auto create = GetStaticSymbols().ls__FixedString__Create;
        if (!create) return FixedStringBase::NullIndex;
        uint32_t index = FixedStringBase::NullIndex;
        create(&index, sv.data(), (int)sv.size());
        return index;
    }

    // ls::FixedString::GetString(uint32_t) returns the pooled characters (entry + 0x18,
    // i.e. right after upstream's Header); upstream's GetStringProc fills a view.
    LSStringView* LinuxGetString(FixedStringBase const* fs, LSStringView& sv)
    {
        auto get = GetStaticSymbols().ls__FixedString__GetStringLinux;
        char const* s = get ? get(fs->Index) : nullptr;
        if (s) {
            auto hdr = reinterpret_cast<FixedStringBase::Header const*>(s - sizeof(FixedStringBase::Header));
            sv = LSStringView(s, hdr->Length);
        } else {
            sv = LSStringView();
        }
        return &sv;
    }

    LibraryManager::LibraryManager()
    {}

    // PORT (linux): nine of RegisterSymbols()' 97 names can never have a manifest row on
    // Linux -- LTO removed the entity the name refers to. Counting them as "missing" made
    // InitializationFailed() true for nothing, so they are left out
    // of missingSymbols_ and logged as absent by design instead. Each witness is a
    // tools/targets-hand.tsv row on build-id c8da62b137ca; a name may only join this list
    // with one. Exactly one of ServedBy and Lost is set: ServedBy where something else in the
    // port does the job, Lost where nothing does and the capability is given up -- upstream's
    // own null guard is then what runs, and the log has to say so rather than imply a
    // substitute exists.
    struct AbsentByDesign
    {
        char const* Name;
        char const* ServedBy;
        char const* Lost{ nullptr };
    };

    static constexpr AbsentByDesign sAbsentOnLinux[] = {
        // Alloc 0x218f1e0 / Free 0x218f0c0 take no `this`; there is no allocator object to
        // return, and BG3Alloc/BG3Free above call them directly
        { "ls__GlobalAllocator__Get", "ls__GlobalAllocator__Alloc / ls__GlobalAllocator__Free, which take no `this`" },
        // inlined into ls::FixedString::Create(char const*, int) 0x2438520, which hashes and
        // uses the table itself; LinuxCreateFromString above is the path
        { "ls__GlobalStringTable__MainTable__CreateFromString", "ls__FixedString__Create" },
        // the thread index is the TLS word fs:[-0x24d28], tested inline wherever it is needed;
        // 0x22f02e0 is a caller that contains it, not the function (targets-hand.tsv)
        { "ls__ThreadRegistry__RequestThreadIndex", "the TLS word fs:[-0x24d28], claimed inline by the game" },
        // inlined into the client's App::Update path (the "CLIENT STATE SWAP" site inside
        // 0x2d1b960, reached only through a vtable slot); the port's client tick runs from
        // ecs::EntityWorld::Update instead (ecl::ScriptExtender::OnECSTick)
        { "ecl__GameStateMachine__Update", "ecs__EntityWorld__Update, which drives ecl::ScriptExtender::OnECSTick" },
        // inlined into eoc::Passives::Init 0x2fc1b00, the manager loop that fills every passive
        // prototype as it parses PassiveData; the decisive witness is that the shared description
        // helper 0x2b85570 has exactly four callers, one per prototype family, and Passive's is
        // that manager where the other three are the prototype Inits themselves
        // (targets-hand.tsv). PassivePrototypeManager::SyncStat's `if (!sync)` guard runs instead.
        { "eoc__PassivePrototype__Init", nullptr,
          "re-deriving a passive prototype from a stat written at runtime: Ext.Stats.Sync() on a "
          "PassiveData entry logs and returns false" },
        // inlined at all four of its call sites here and at the matching four on macOS -- its own
        // lambda's method table (Call 0x5e762a0) is built four times where an out-of-line body
        // would build it once (targets-hand.tsv). StatusPrototypeManager::SyncStat's
        // `parseBoosts != nullptr` guard runs instead.
        { "eoc__ParseStaticBoosts", nullptr,
          "re-parsing a status prototype's Boosts string on Ext.Stats.Sync(): SyncStat clears "
          "StatusPrototype::Boosts and cannot refill it, so a synced status loses its static boosts" },
        // inlined into its only caller, the stats text parser RPGStats::ParseDataBuffer 0x2fcdf90 --
        // upstream's own pattern site for the name. The fork it patterns on is there verbatim
        // (ComboCategory 0x7bbd430 calls 0x350ba00, whose only caller is that parser; the other side
        // is the body), and the decisive witness is the property-type dispatch's own set of
        // RPGEnumeration name globals: exactly one function in bin/bg3 refers to all ten, and it is
        // the parser (targets-hand.tsv). SetRawAttribute's null guard runs instead.
        { "stats__Object__SetPropertyString", nullptr,
          "setting a stats attribute from its text form: Object:SetRawAttribute() logs and returns "
          "false, and Ext.Stats.LoadStatsFile, which is written on top of it, cannot apply a line" },
        // inlined into its only caller, ls::Visual::NotifySkeletonUpdated(bool) 0x24a6af0, on Linux
        // and on macOS alike: a 16-bit read of Visual::BlendShapeBase (+0x102) -- the member the
        // function's own guard tests -- happens in exactly one function in bin/bg3, and it is that
        // caller (guard at 0x24a9280, body at 0x24ac2b6). Windows keeps it out of line at
        // 0x143c6d200, 1426 bytes, with 0x143c6be40 = NotifySkeletonUpdated as its only caller
        // (targets-hand.tsv). Upstream has no guard of its own here -- SetWeight
        // would record an override that nothing ever applies -- so VisualHelpers::SetWeight grew one.
        { "ls__Visual__UpdateBlendshapeWeightsFromSkeleton", nullptr,
          "overriding a blendshape weight from Lua: RenderableObject:SetBlendShapeWeight() logs and "
          "returns false, because the post-hook that would apply the override has no function to "
          "hook -- the game computes the weights from the skeleton inside NotifySkeletonUpdated" },
        // inlined into its only caller, gui::GameUI::Update 0x38197f0, on Linux and on macOS alike.
        // Windows keeps it out of line at 0x1420f1180 (436 bytes by .pdata) with that same single
        // caller, and its body -- a Noesis::BaseCollection built with operator new(0x28), the DC
        // widget list copied into it under the provider's lock, then ExecuteCommandQueue(w + 0x40)
        // for every widget whose [w+0x1a0] byte is set -- sits verbatim in bin/bg3 at
        // 0x38206e0..0x3820849. The decisive witness is that ui::CommandProcessor::ExecuteCommandQueue
        // 0x6a2bc80 has exactly the four callers the macOS oracle's 0x103f73290 has, where Windows'
        // caller set has this function as a fifth (targets-hand.tsv). Nothing in
        // upstream or the port subscribes to the hook, so HookAll()'s own null guard is all that the
        // absence reaches.
        { "ui__DataContextProvider__ExecuteCommandQueues", nullptr,
          "nothing: upstream declares this hook in EngineHooks.inl and never subscribes to it, so the "
          "wrapper it installs on Windows has no handler and HookAll() here simply skips the name" },
    };

    static AbsentByDesign const* AbsentByDesignEntry(std::string const& name)
    {
        for (auto const& absent : sAbsentOnLinux) {
            if (name == absent.Name) return &absent;
        }
        return nullptr;
    }

    bool LibraryManager::FindLibraries(uint32_t gameRevision)
    {
        RegisterSymbols();

        auto const& host = platform::host();
        if (!host.manifest_get) {
            ERR("LibraryManager::FindLibraries(): no manifest lookup from the loader");
            CriticalInitFailed = true;
            return false;
        }

        auto& sym = GetStaticSymbols();
        unsigned found = 0;
        std::vector<std::pair<std::string, AbsentByDesign const*>> absent;
        for (auto const& [name, offset] : symbolOffsets_) {
            void* addr = host.manifest_get(name.c_str());
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(&sym) + offset) = addr;
            auto byDesign = AbsentByDesignEntry(name);
            if (addr) {
                found++;
                if (byDesign) {
                    WARN("LibraryManager::FindLibraries(): '%s' has a manifest row after all; "
                        "drop it from sAbsentOnLinux (recorded there as %s %s)", name.c_str(),
                        byDesign->ServedBy ? "served by" : "losing",
                        byDesign->ServedBy ? byDesign->ServedBy : byDesign->Lost);
                }
            } else if (byDesign) {
                // PORT (linux): not a gap in the manifest -- either the substitute runs, or the
                // member stays null on purpose and upstream's own guard refuses
                absent.push_back({ name, byDesign });
            } else {
                missingSymbols_.push_back(name);
            }
        }
        // ls__FixedString__GetString's row is the Linux GetString(uint32_t) -> char const*;
        // the upstream-typed member stays null so nothing calls it
        // with the wrong prototype, and the adapter above goes through the Linux one
        sym.ls__FixedString__GetStringLinux = (StaticSymbols::FixedString__GetStringLinuxProc*)host.manifest_get("ls__FixedString__GetString");
        sym.ls__FixedString__GetString = nullptr;
        std::erase(missingSymbols_, std::string("ls__FixedString__GetStringLinux"));
        // ls__AppliedMaterial__LoadTexture's row is the Linux LoadTextureHelper: a 32-byte
        // struct returned through rdi with the descriptor in its first word (Symbols.h
        // LoadTextureResultLinux); the same treatment
        sym.ls__AppliedMaterial__LoadTextureLinux = (StaticSymbols::LoadTextureHelperLinuxProc*)host.manifest_get("ls__AppliedMaterial__LoadTexture");
        sym.ls__AppliedMaterial__LoadTexture = nullptr;
        // ls__AppliedMaterial__TryOverrideTexture2DParameter's row is the (FixedString const&,
        // FixedString const&, bool) overload with LTO's IPA-SRA applied to the first argument:
        // the param index arrives by value (Symbols.h); the same treatment
        sym.ls__AppliedMaterial__TryOverrideTexture2DParameterLinux =
            (StaticSymbols::AppliedMaterial__TryOverrideTexture2DParameterLinuxProc*)host.manifest_get("ls__AppliedMaterial__TryOverrideTexture2DParameter");
        sym.ls__AppliedMaterial__TryOverrideTexture2DParameter = nullptr;
        // ls__AppliedMaterial__LoadVirtualTexture's row has no `this` at all -- IPA-SRA dropped it,
        // so the sole argument is the texture FixedString (Symbols.h)
        sym.ls__AppliedMaterial__LoadVirtualTextureLinux =
            (StaticSymbols::AppliedMaterial__LoadVirtualTextureLinuxProc*)host.manifest_get("ls__AppliedMaterial__LoadVirtualTexture");
        sym.ls__AppliedMaterial__LoadVirtualTexture = nullptr;
        // esv__SurfaceActionFactory__DoCreateAction's row takes five arguments here, not
        // upstream's three: the story action id and the ClassDescriptions reference sit between
        // the type and the handle (Symbols.h); the same treatment
        sym.esv__SurfaceActionFactory__DoCreateActionLinux =
            (StaticSymbols::SurfaceActionFactoryCreateLinuxProc*)host.manifest_get("esv__SurfaceActionFactory__DoCreateAction");
        sym.esv__SurfaceActionFactory__DoCreateAction = nullptr;
        std::sort(missingSymbols_.begin(), missingSymbols_.end());
        std::sort(absent.begin(), absent.end());
        INFO("LibraryManager::FindLibraries(): %u of %zu symbols from the manifest (%zu absent by design on Linux, %zu missing)",
            found, symbolOffsets_.size(), absent.size(), missingSymbols_.size());
        // PORT (linux): the ones LTO removed are not manifest gaps; say what runs instead of
        // each, and for the ones nothing serves, say what the port gives up
        for (auto const& [name, byDesign] : absent) {
            if (byDesign->ServedBy) {
                INFO("LibraryManager::FindLibraries(): absent by design: %s, served by %s",
                    name.c_str(), byDesign->ServedBy);
            } else {
                INFO("LibraryManager::FindLibraries(): absent by design: %s, inlined away with no "
                    "substitute -- what is lost: %s", name.c_str(), byDesign->Lost);
            }
        }
        // PORT (linux): name what is absent -- the features that depend on a missing
        // symbol are the ones that go wrong, and the log was silent about which they are
        for (auto const& name : missingSymbols_) {
            DEBUG("LibraryManager::FindLibraries(): not in the manifest: %s", name.c_str());
        }

        // What the extender cannot start without: the allocator and the string table
        bool critical = sym.ls__GlobalAllocator__Alloc && sym.ls__GlobalAllocator__Free
            && sym.ls__gGlobalStringTable && sym.ls__FixedString__Create && sym.ls__FixedString__GetStringLinux
            && sym.ls__FixedString__IncRef && sym.ls__GlobalStringTable__MainTable__DecRef;
        if (!critical) {
            ERR("LibraryManager::FindLibraries(): allocator or string table missing from the manifest; extension features unavailable");
            CriticalInitFailed = true;
        }
        InitFailed = InitFailed || !missingSymbols_.empty();

        gCoreLibPlatformInterface.Alloc = &BG3Alloc;
        gCoreLibPlatformInterface.Free = &BG3Free;
        gCoreLibPlatformInterface.ls__GlobalStringTable__MainTable__CreateFromString = nullptr;   // the fallback below is used
        gCoreLibPlatformInterface.ls__FixedString__CreateFromString = &LinuxCreateFromString;
        gCoreLibPlatformInterface.ls__FixedString__GetString = &LinuxGetString;
        gCoreLibPlatformInterface.ls__FixedString__IncRef = sym.ls__FixedString__IncRef;
        gCoreLibPlatformInterface.ls__GlobalStringTable__MainTable__DecRef = sym.ls__GlobalStringTable__MainTable__DecRef;
        gCoreLibPlatformInterface.ls__gGlobalStringTable = sym.ls__gGlobalStringTable;

        return !CriticalInitFailed;
    }

    bool LibraryManager::PostStartupFindLibraries()
    {
        if (PostLoaded) {
            return !CriticalInitFailed;
        }

        auto initStart = std::chrono::high_resolution_clock::now();

        if (!CriticalInitFailed) {
            GFS.Initialize();
            InitializeEnumerations();
        }

        auto initEnd = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(initEnd - initStart).count();
        DEBUG("LibraryManager::PostStartupFindLibraries() took %lld ms", ms);

        PostLoaded = true;
        return !CriticalInitFailed;
    }

    // Code patches (modded achievements, story patching) are byte patches at pattern
    // positions upstream; none has a Linux position yet, so none is applied.
    void LibraryManager::ApplyCodePatches()
    {
        if (gExtender->GetConfig().DisableStoryPatching && GetStaticSymbols().ls__GlobalSwitches) {
            (*GetStaticSymbols().ls__GlobalSwitches)->DisableStoryPatching = true;
        }
    }

    bool LibraryManager::WasPatchApplied(std::string const&) { return false; }
    bool LibraryManager::ApplyCodePatch(std::string const& mapping)
    {
        WARN("Code patch '%s' is not available on Linux", mapping.c_str());
        return false;
    }
    bool LibraryManager::UndoCodePatch(std::string const&) { return false; }

    // PORT (linux): bg3's ELF carries no VERSIONINFO. The version is the .rodata literal the
    // game builds its version STDString from at every site that needs it ("4.1.1.7398727";
    // manifest row ls__GameVersionString). Upstream reads the PE's
    // VS_FIXEDFILEINFO, and Larian packs that product version into its four 16-bit words as
    // 4.73.98.727 -- Major, then the seven-digit build split 2/2/3 -- measured on this build's
    // bg3.exe and on the Toolkit's Glasses.exe (4.1.1.6931813 -> 4.69.31.813); IsSupported()'s
    // 4.69.95 is the same encoding, so the fields here compare like Windows bg3se's do.
    // Asked through the manifest directly: ScriptExtender::Initialize() wants the version
    // before FindLibraries() has filled StaticSymbols.
    bool LibraryManager::GetGameVersion(GameVersionInfo & version)
    {
        auto const& host = platform::host();
        auto s = host.manifest_get ? (char const*)host.manifest_get("ls__GameVersionString") : nullptr;
        if (s == nullptr) return false;
        unsigned major, minor, revision, build;
        char tail;
        if (sscanf(s, "%u.%u.%u.%u%c", &major, &minor, &revision, &build, &tail) != 4) {
            ERR("GetGameVersion(): version string %.32s is not major.minor.revision.build", s);
            return false;
        }
        version.Major = (uint16_t)major;
        version.Minor = (uint16_t)(build / 100000);
        version.Revision = (uint16_t)(build / 1000 % 100);
        version.Build = (uint16_t)(build % 1000);
        return true;
    }

    HMODULE LibraryManager::GetAppHandle()
    {
        return nullptr;
    }

    // PORT (linux): the dialog can only say "some features might be unavailable"; the log is
    // where a mod author can find out which, so the startup error is followed by the names.
    // Debug()'s buffer is 1024 bytes and truncates silently, so the list is wrapped well
    // under that rather than joined into one line.
    void LibraryManager::LogMissingSymbols()
    {
        if (missingSymbols_.empty()) return;

        ERR("%zu game symbols have no manifest row; every extender feature that needs one of "
            "these is unavailable (manifest/<build-id>.tsv):", missingSymbols_.size());

        constexpr size_t maxLine = 480;
        std::string line;
        for (auto const& name : missingSymbols_) {
            if (!line.empty() && line.size() + 2 + name.size() > maxLine) {
                ERR("  %s", line.c_str());
                line.clear();
            }
            if (!line.empty()) line += ", ";
            line += name;
        }
        if (!line.empty()) ERR("  %s", line.c_str());
    }

    void LibraryManager::ShowStartupError(STDString const & msg, bool wait, bool exitGame)
    {
        ERR("STARTUP ERROR: %s", msg.c_str());

        if (GetStaticSymbols().ecl__EoCClient == nullptr
            || GetStaticSymbols().ecl__EoCClient__HandleError == nullptr
            || GetStaticSymbols().ls__GlobalAllocator__Alloc == nullptr) {
            return;
        }

        if (wait) {
            std::thread messageThread([this, msg, exitGame]() {
                unsigned retries{ 0 };
                while (!CanShowError() && retries < 1200) {
                    Sleep(100);
                    retries++;
                }

                if (retries < 1200) {
                    ShowStartupError(msg, exitGame);
                }
            });
            messageThread.detach();
        } else {
            if (CanShowError()) {
                ShowStartupError(msg, exitGame);
            }
        }
    }

    void LibraryManager::ShowStartupError(STDString const & msg, bool exitGame)
    {
        if (!CanShowMessages()) return;

        // Abuse a deprecated string key for displaying custom text
        TranslatedString ts;
        ts.Handle.Handle = FixedString("h11018635g3003g46c6g8013g4630abe55cad");
        ts.Handle.Version = 1;

        // Create a new entry in the string repository text pool
        auto& texts = (*GetStaticSymbols().ls__gTranslatedStringRepository)->TranslatedStrings[0];
        auto tskRef = texts->Texts.try_get(ts.Handle);
        if (!tskRef) return;

        auto str = GameAlloc<STDString>(msg);
        texts->Strings.push_back(str);

        // Update reference to new string
        *tskRef = LSStringView(str->data(), str->size());

        GetStaticSymbols().ecl__EoCClient__HandleError(*GetStaticSymbols().ecl__EoCClient, ts, exitGame, ts, 0);
    }

    bool LibraryManager::CanShowMessages()
    {
        return GetStaticSymbols().GetClientState()
            && GetStaticSymbols().ecl__EoCClient__HandleError != nullptr
            && GetStaticSymbols().ls__GlobalAllocator__Alloc != nullptr;
    }

    bool LibraryManager::CanShowError()
    {
        if (!CanShowMessages()) return false;

        auto state = GetStaticSymbols().GetClientState();
        return state == ecl::GameState::Running
            || state == ecl::GameState::Paused
            || state == ecl::GameState::Menu
            || state == ecl::GameState::Lobby;
    }

#define SYM_OFF(name) symbolOffsets_.insert(std::make_pair(#name, (int)offsetof(StaticSymbols, name)))

    // upstream's list (DataLibrariesBG3Game.cpp), plus the Linux-only members
    void LibraryManager::RegisterSymbols()
    {
        SYM_OFF(ls__GlobalStringTable__MainTable__CreateFromString);
        SYM_OFF(ls__FixedString__GetString);
        SYM_OFF(ls__FixedString__IncRef);
        SYM_OFF(ls__GlobalStringTable__MainTable__DecRef);
        SYM_OFF(ls__gGlobalStringTable);

        SYM_OFF(ls__ThreadRegistry__RequestThreadIndex);
        SYM_OFF(ls__FileReader__ctor);
        SYM_OFF(ls__FileReader__dtor);
        SYM_OFF(ls__PathRoots);
        SYM_OFF(App__Ctor);
        SYM_OFF(App__UpdatePaths);

        SYM_OFF(ecl__EoCClient);
        SYM_OFF(esv__EoCServer);
        SYM_OFF(ecl__EoCClient__HandleError);
        SYM_OFF(ecl__gCursorControl);
        SYM_OFF(ls__gDragDropManager);
        SYM_OFF(ls__gTranslatedStringRepository);
        SYM_OFF(ls__gTranslatedStringKeyManager);

        SYM_OFF(ecl__gGameStateEventManager);
        SYM_OFF(esv__gGameStateEventManager);
        SYM_OFF(ecl__GameStateThreaded__GameStateWorker__DoWork);
        SYM_OFF(esv__GameStateThreaded__GameStateWorker__DoWork);
        SYM_OFF(ecl__GameStateMachine__Update);
        SYM_OFF(esv__GameStateMachine__Update);
        SYM_OFF(App__LoadGraphicSettings);
        SYM_OFF(ls__gInputManager);

        SYM_OFF(ecs__EntityWorld__Update);
        SYM_OFF(ecs__EntityWorld__FlushECBs);

        SYM_OFF(eoc__SpellPrototypeManager);
        SYM_OFF(eoc__SpellPrototype__Init);
        SYM_OFF(eoc__BoostPrototypeManager);
        SYM_OFF(eoc__StatusPrototypeManager);
        SYM_OFF(eoc__StatusPrototype__Init);
        SYM_OFF(eoc__PassivePrototypeManager);
        SYM_OFF(eoc__PassivePrototype__Init);
        SYM_OFF(eoc__InterruptPrototypeManager);
        SYM_OFF(eoc__InterruptPrototype__Init);
        SYM_OFF(eoc__DynamicStatsExpressionManager);
        SYM_OFF(eoc__ParseStaticBoosts);

        SYM_OFF(esv__OsirisVariableHelper__SavegameVisit);

        SYM_OFF(stats__DealDamageFunctor__ApplyDamage);
        SYM_OFF(esv__StatsSystem__ThrowDamageEvent);

        SYM_OFF(esv__ExecuteStatsFunctor_AttackTargetContext);
        SYM_OFF(esv__ExecuteStatsFunctor_AttackPositionContext);
        SYM_OFF(esv__ExecuteStatsFunctor_MoveContext);
        SYM_OFF(esv__ExecuteStatsFunctor_TargetContext);
        SYM_OFF(esv__ExecuteStatsFunctor_NearbyAttackedContext);
        SYM_OFF(esv__ExecuteStatsFunctor_NearbyAttackingContext);
        SYM_OFF(esv__ExecuteStatsFunctor_EquipContext);
        SYM_OFF(esv__ExecuteStatsFunctor_SourceContext);
        SYM_OFF(esv__ExecuteStatsFunctor_InterruptContext);

        SYM_OFF(gRPGStats);
        SYM_OFF(RPGStats__Load);
        SYM_OFF(RPGStats__ParseDataBuffers);
        SYM_OFF(stats__Object__SetPropertyString);

        SYM_OFF(esv__LevelManager);
        SYM_OFF(ecl__LevelManager);
        SYM_OFF(ls__GlobalTemplateManager);
        SYM_OFF(esv__CacheTemplateManager);

        SYM_OFF(esv__gSurfaceActionFactory);
        SYM_OFF(esv__SurfaceActionFactory__DoCreateAction);
        SYM_OFF(esv__TransformSurfaceAction__Init);

        SYM_OFF(eoc__AiGrid__FindPath);
        SYM_OFF(eoc__AiGrid__FindPathImmediate);

        SYM_OFF(esv__SavegameManager);

        SYM_OFF(Noesis__Reflection__LockedByThreadId);
        SYM_OFF(Noesis__Reflection__Lock);
        SYM_OFF(Noesis__Reflection__NumLocks);
        SYM_OFF(Noesis__Reflection__Data);
        SYM_OFF(Noesis__SymbolManager__Data);
        SYM_OFF(Noesis__SymbolManager__Lock);
        SYM_OFF(Noesis__gErrorHandler);
        SYM_OFF(Noesis__GUI__LoadXaml);
        SYM_OFF(Noesis__Visual__AddVisualChild);
        SYM_OFF(Noesis__Visual__RemoveVisualChild);
        SYM_OFF(Noesis__Visual__PointFromScreen);
        SYM_OFF(Noesis__Visual__PointToScreen);
        SYM_OFF(Noesis__VisualTreeHelper__HitTest);

        SYM_OFF(ui__DataContextProvider__ExecuteCommandQueues);

        SYM_OFF(AppInstance);

        SYM_OFF(ls__GlobalAllocator__Get);
        SYM_OFF(ls__GlobalAllocator__Alloc);
        SYM_OFF(ls__GlobalAllocator__Free);

        SYM_OFF(eoc__gGuidResourceManager);
        SYM_OFF(ls__gGlobalResourceManager);

        SYM_OFF(ls__gTextureAtlasMap);
        SYM_OFF(ls__AppliedMaterial__LoadTexture);
        SYM_OFF(ls__TextureManager__UnloadTexture);
        SYM_OFF(ls__AppliedMaterial__LoadVirtualTexture);
        SYM_OFF(ls__AppliedMaterial__TryOverrideTexture2DParameter);

        SYM_OFF(ls__Visual__UpdateBlendshapeWeightsFromSkeleton);

        SYM_OFF(ls__GlobalSwitches);

        // Linux-only rows (manifest names), see GameDefinitions/Symbols.h
        SYM_OFF(ls__FixedString__Create);
        SYM_OFF(ls__Path__ctor_root_view);
        SYM_OFF(ls__FileSystem__DoesFileExist);
        SYM_OFF(ls__g_GlobalTemplateBankType);
        SYM_OFF(esv__GetGameObjectTemplate);
    }
}
