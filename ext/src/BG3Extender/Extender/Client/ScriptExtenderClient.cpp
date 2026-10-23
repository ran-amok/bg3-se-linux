#include <stdafx.h>
#include <platform.h>   // PORT (linux): the BG3SE_NO_IMGUI knob
#include <Extender/Client/ScriptExtenderClient.h>
#include <Extender/ScriptExtender.h>
#include <Extender/Version.h>
#include <shlwapi.h>

#define STATIC_HOOK(name)   /* PORT (linux): gHook is an inline static member now (CoreLib/Wrappers.h) */
STATIC_HOOK(gameStateWorkerStart_)
STATIC_HOOK(gameStateMachineUpdate_)

#include <Extender/Shared/ThreadedExtenderState.inl>

BEGIN_SE()

void InitCrashReporting();

extern char const* BuildDate;

END_SE()

BEGIN_NS(ecl)

char const * GameStateNames[] =
{
    "Unknown",
    "Init",
    "InitMenu",
    "InitNetwork",
    "InitConnection",
    "Idle",
    "LoadMenu",
    "Menu",
    "Exit",
    "SwapLevel",
    "LoadLevel",
    "LoadModule",
    "LoadSession",
    "UnloadLevel",
    "UnloadModule",
    "UnloadSession",
    "Paused",
    "PrepareRunning",
    "Running",
    "Disconnect",
    "Join",
    "Save",
    "StartLoading",
    "StopLoading",
    "StartServer",
    "Movie",
    "Installation",
    "ModReceiving",
    "Lobby",
    "BuildStory",
    "GeneratePsoCache",
    "LoadPsoCache",
    "AnalyticsSessionEnd"
};

ScriptExtender::ScriptExtender(ExtenderConfig& config)
    : ThreadedExtenderState(ContextType::Client),
    config_(config)
{
}

void ScriptExtender::Initialize()
{
    // Wrap state change functions even if extension startup failed, otherwise
    // we won't be able to show any startup errors

    auto& lib = GetStaticSymbols();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    // PORT (linux): a manifest row (bin/bg3 0x507e220), so this wrap fires here too. Register
    // use read off that body: rdi = the GameStateWorker and nothing else incoming, void return
    // -- upstream's void(void* self) is this build's convention, so GameStateWorkerWrapper
    // carries over unchanged.
    if (lib.ecl__GameStateThreaded__GameStateWorker__DoWork != nullptr) {
        gameStateWorkerStart_.Wrap(lib.ecl__GameStateThreaded__GameStateWorker__DoWork);
    }

    if (lib.ecl__GameStateMachine__Update != nullptr) {
        gameStateMachineUpdate_.Wrap(lib.ecl__GameStateMachine__Update);
    }

    DetourTransactionCommit();

    gameStateWorkerStart_.SetWrapper(&ScriptExtender::GameStateWorkerWrapper, this);
    gameStateMachineUpdate_.SetPrePostHook(&ScriptExtender::OnPreUpdate, &ScriptExtender::OnUpdate, this);

    // PORT (linux): the SDL input half of BG3SE_NO_IMGUI.
    if (!platform::NoImgui()) sdl_.EnableHooks();
}

void ScriptExtender::Shutdown()
{
    DEBUG("ecl::ScriptExtender::Shutdown: Exiting");
    sdl_.DisableHooks();
    ContextGuardAnyThread _(ContextType::Client);
    ResetExtensionState();
}

void ScriptExtender::PostStartup()
{
    if (postStartupDone_) return;

    OPTICK_EVENT();
    entityHelpers_.Setup();
    visualHelpers_.Setup();
    gExtender->GetPropertyMapManager().RegisterComponents(entityHelpers_);
    postStartupDone_ = true;
}

bool IsLoadingState(GameState state)
{
    return state == GameState::Init
        || state == GameState::InitMenu
        || state == GameState::InitNetwork
        || state == GameState::InitConnection
        || state == GameState::LoadMenu
        || state == GameState::SwapLevel
        || state == GameState::LoadLevel
        || state == GameState::LoadModule
        || state == GameState::LoadSession
        || state == GameState::UnloadLevel
        || state == GameState::UnloadModule
        || state == GameState::UnloadSession
        || state == GameState::PrepareRunning
        || state == GameState::Installation
        || state == GameState::ModReceiving;
}

// Undo "ModCrashSanityCheck" mechanism that disables mods on an unclean exit
void CleanupSanityCheck()
{
    auto path = GetStaticSymbols().ToPath("ModCrashSanityCheck", PathRootType::UserProfile);
    auto pathw = FromUTF8(path);
    if (PathIsDirectoryW(pathw.c_str())) {
        RemoveDirectoryW(pathw.c_str());
        INFO("Removed ModCrashSanityCheck");
    }
}

void ScriptExtender::OnGameStateChanged(GameState fromState, GameState toState)
{
    if (gExtender->GetConfig().SendCrashReports) {
        // We need to initialize the crash reporter after the game engine has started,
        // otherwise the game will overwrite the top level exception filter
        InitCrashReporting();
    }

    // Check to make sure that startup is done even if the extender was loaded when the game was already in GameState::Init
    if (toState != GameState::Unknown
        && toState != GameState::StartLoading
        && toState != GameState::InitMenu
        && !gExtender->GetLibraryManager().CriticalInitializationFailed()) {
        // We need to initialize the function library here, as GlobalAllocator isn't available in Init().
        gExtender->PostStartup();
    }

    if (toState == GameState::Menu
        && gExtender->GetLibraryManager().InitializationFailed()) {
        gExtender->PostStartup();
    }

    if ((toState == GameState::InitMenu || toState == GameState::StartLoading)
        && !gExtender->GetLibraryManager().CriticalInitializationFailed()
        && gExtender->GetConfig().InsanityCheck) {
        CleanupSanityCheck();
    }

#if defined(DEBUG_SERVER_CLIENT)
    DEBUG("ecl::ScriptExtender::OnGameStateChanged(): %s -> %s", 
        GameStateNames[(unsigned)fromState], GameStateNames[(unsigned)toState]);
#endif

    if (fromState != GameState::Unknown) {
        BindToThreadPersistent();
    }

    if (gExtender->WasInitialized() && !gExtender->GetLibraryManager().CriticalInitializationFailed()) {
        if (IsLoadingState(toState)) {
            UpdateClientProgress(EnumInfo<GameState>::Find(toState).GetString());
        } else {
            UpdateClientProgress("");
        }
    }

    switch (fromState) {
    case GameState::LoadModule:
        INFO("ecl::ScriptExtender::OnGameStateChanged(): Loaded module");
        gExtender->GetVirtualTextureHelpers().Load();
        ShowVersionNumber();
        LoadExtensionState(ExtensionStateContext::Game);
        break;

    // Initialize client state when exiting from a game and returning to menu
    case GameState::LoadMenu:
        LoadExtensionState(ExtensionStateContext::Game);
        break;

    case GameState::LoadSession:
        if (extensionState_) {
            extensionState_->OnGameSessionLoaded();
        }
        break;

    case GameState::InitNetwork:
        network_.ExtendNetworking();
        break;
    }

    switch (toState) {
    case GameState::Init:
        entityHelpers_.Bind();
        ResetExtensionState();
        break;

    case GameState::InitNetwork:
    case GameState::Disconnect:
        network_.Reset();
        break;

    case GameState::UnloadSession:
        INFO("ecl::ScriptExtender::OnGameStateChanged(): Unloading session");
        entityHelpers_.OnDestroy();
        ResetExtensionState();
        break;

    case GameState::Menu:
    {
        auto error = gExtender->GetUpdaterAPI().GetDisplayError();
        if (error) {
            gExtender->GetLibraryManager().ShowStartupError(STDString(*error), false, false);
        } else {
            // PORT (linux): the "experimental version" dialog is compiled out -- every build here
            // is a Devel build, so it would stop every launch at the menu.
            #if defined(SE_IS_DEVELOPER_BUILD) && defined(NDEBUG) && !defined(BG3SE_LINUX)
            if (!gExtender->GetConfig().DeveloperMode) {
                gExtender->GetLibraryManager().ShowStartupError("This is an experimental version of the Script Extender meant for development use; things may frequently break here. It is recommended to switch back to the Release version unless you know what you are doing!", false, false);
            }
            #endif
        }
        break;
    }

    case GameState::LoadModule:
        gExtender->InitRuntimeLogging();
        break;

    case GameState::LoadSession:
        INFO("ecl::ScriptExtender::OnClientGameStateChanged(): Loading game session");
        LoadExtensionState(ExtensionStateContext::Game);
        network_.ExtendNetworking();
        entityHelpers_.OnInit();
        if (extensionState_) {
            extensionState_->OnGameSessionLoading();
        }
        break;

    case GameState::LoadLevel:
        if (extensionState_) {
            LuaClientPin lua(*extensionState_);
            if (lua) {
                lua->OnLevelLoading();
            }
        }
        break;
    }

    if (extensionState_) {
        LuaClientPin lua(*extensionState_);
        if (lua) {
            lua->OnGameStateChanged(fromState, toState);
        }
    }
}

void ScriptExtender::GameStateWorkerWrapper(void (*wrapped)(void*), void* self)
{
    // PORT (linux): DoWork is the client worker thread's whole body here -- 195 bytes around one
    // virtual call -- so this guard spans that thread's lifetime, where OnGameStateChanged's
    // BindToThreadPersistent() would only bind it from the first non-Unknown transition onwards.
    // These two lines are the only positive the wrap can write, and they name the tid the client's
    // game-state transitions run on.
    DEBUG("ecl::ScriptExtender: game state worker tid=%d entered DoWork, context at entry %d",
        GetCurrentThreadId(), (int)GetCurrentContextType());
    {
        ContextGuard _(ContextType::Client);
        wrapped(self);
    }
    DEBUG("ecl::ScriptExtender: game state worker tid=%d left DoWork", GetCurrentThreadId());
}

#if USE_OPTICK
std::unique_ptr< ::Optick::Event> frameEvent;
#endif

void ScriptExtender::OnPreUpdate(void* self, GameTime* time)
{
#if USE_OPTICK
    static ::Optick::ThreadScope mainThreadScope("Client");
    frameEvent.reset();
    ::Optick::EndFrame();
    ::Optick::Update();

    auto frameNumber = ::Optick::BeginFrame();
    frameEvent = std::make_unique<::Optick::Event>(*::Optick::GetFrameDescription());
    OPTICK_TAG("Frame", frameNumber);
#endif
}

void ScriptExtender::OnUpdate(void* self, GameTime* time)
{
    BEGIN_GUARDED()
    // In case we're loaded too late to see LoadModule transition
    BindToThreadPersistent();
    OnUpdateGuarded(self, time);
    END_GUARDED()
}

// PORT (linux): ecl::GameStateMachine::Update is inlined into App::Update on this build
// (tools/targets-hand.tsv, 0x2d1b960), so gameStateMachineUpdate_ never wraps and OnUpdate above
// never runs: no client network_.Update(), no RunPendingTasks(), no ExtensionState::OnUpdate --
// no client Ext.Timer, Tick, Ext.Net replies or Ext.Vars flush. The client world's
// ecs::EntityWorld::Update is hooked, runs on the same main thread once per frame from the same
// App::Update, and carries the frame's GameTime, so OnECSUpdateGuarded drives the tick from there.
void ScriptExtender::OnECSTick(GameTime const& time)
{
    BEGIN_GUARDED()
    static bool first = true;
    if (first) {
        first = false;
        auto state = GetStaticSymbols().GetClientState();
        DEBUG("ecl::ScriptExtender: first client tick from ecs::EntityWorld::Update, client state %d, t=%f",
            state ? (int)*state : -1, time.Time);
    }
    BindToThreadPersistent();
    OnUpdateGuarded(nullptr, const_cast<GameTime*>(&time));
    END_GUARDED()
}

void ScriptExtender::OnUpdateGuarded(void* self, GameTime* time)
{
    network_.Update();
    RunPendingTasks();
    gExtender->IMGUI().Update();
    if (extensionState_) {
        extensionState_->OnUpdate(*time);
        if (gExtender->GetLuaDebugger()) {
            gExtender->GetLuaDebugger()->ClientTick();
        }
    }

}

void ScriptExtender::OnIncLocalProgress(void* self, int progress, char const* state)
{
    if (strcmp(state, "EffectManager") != 0) {
        UpdateClientProgress(state);
    }
    else {
        UpdateClientProgress("");
    }
}

void ScriptExtender::UpdateServerProgress(STDString const& status)
{
    serverStatus_ = status;
    ShowLoadingProgress();
}

void ScriptExtender::UpdateClientProgress(STDString const& status)
{
    clientStatus_ = status;
    ShowLoadingProgress();
}

void ScriptExtender::ShowLoadingProgress()
{
    // FIXME - not supported for now
}

void ScriptExtender::ShowVersionNumber()
{
    // PORT (linux): upstream's code. The write takes the repository's SRWSpinLock, whose mirror
    // has to be the game's (0x18 bytes with a pthread_self owner: Base/Lock.h) -- with a 12-byte
    // DWORD-owner mirror the TextPool write lands and the unlock aborts in
    // SRWSpinLock::WriteUnlock. The repository's own layout is measured to +0x108 off two of
    // its methods.
    RuntimeStringHandle rsh(FixedString("h5b6e4138g2cf0g4d67gb825gee416cf8c54f"));
    auto versionText = GetStaticSymbols().GetTranslatedStringRepository()->GetTranslatedString(rsh);
    if (versionText && !STDString(*versionText).contains("Script Extender")) {
        auto expandedVersion = STDString(*versionText) +
            "\r\nScript Extender v" + STDString(std::to_string(CurrentVersion)) + " loaded, built on " + BuildDate + ".";
        GetStaticSymbols().GetTranslatedStringRepository()->UpdateTranslatedString(rsh, expandedVersion);
    }
}

void ScriptExtender::ResetLuaState()
{
    if (extensionState_ && extensionState_->GetLua()) {
        auto ext = extensionState_.get();

        ext->AddPostResetCallback([&ext]() {
            ext->OnModuleResume();
            auto state = GetStaticSymbols().GetClientState();
            if (state && (state == GameState::Paused || state == GameState::Running)) {
                ext->OnGameSessionLoading();
                ext->OnGameSessionLoaded();
            }
            ext->OnResetCompleted();
        });
        ext->RequestLuaReset(true);
    }
}

void ScriptExtender::ResetExtensionState()
{
    OPTICK_EVENT();
    network_.OnResetExtensionState();
    extensionState_.reset();
    extensionState_ = std::make_unique<ExtensionState>();
    extensionState_->Reset();
    gExtender->ClearPathOverrides();
    extensionLoaded_ = false;
}

void ScriptExtender::LoadExtensionState(ExtensionStateContext ctx)
{
    if (extensionLoaded_ && (!extensionState_ || ctx == extensionState_->Context())) {
        return;
    }

    OPTICK_EVENT(Optick::Category::IO);
    PostStartup();

    if (!extensionState_) {
        ResetExtensionState();
    }

    extensionState_->LoadConfigs();

    if (!gExtender->GetLibraryManager().CriticalInitializationFailed()) {
        OsiMsg("Initializing client with target context " << ContextToString(ctx));
        gExtender->GetLibraryManager().ApplyCodePatches();
        //networkManager_.ExtendNetworkingClient();
        extensionState_->RequestLuaReset(ctx, true);
    }

    extensionLoaded_ = true;
}

END_NS()
