// setup.cpp — PORT (linux): what Extender/Shared/dllmain.cpp's SetupScriptExtender()
// does on Windows, from bg3se_ext_init (entry.cpp). Configuration is upstream's
// ScriptExtenderSettings.json, read from the state directory if present; the console
// is always created (it is a socket, not a window) and the Windows-only options are
// forced off.
#include "stdafx.h"
#include <Extender/ScriptExtender.h>
#include <Extender/Shared/Console.h>
#include <CoreLib/JsonLibs.h>
#include <Extender/Shared/ExtenderConfig.inl>
#include "platform.h"

using namespace bg3se;

namespace bg3se {

void SetupScriptExtender()
{
    auto const& host = platform::host();
    std::string state = host.state_dir ? host.state_dir : ".";

    gExtender = std::make_unique<ScriptExtender>();
    auto& config = gExtender->GetConfig();
    LoadConfig(FromStdUTF8(state + "/ScriptExtenderSettings.json"), config);

    config.CreateConsole = true;
    config.SendCrashReports = false;
    config.EnableDebugger = false;
    config.EnableLuaDebugger = false;
    config.Optick = false;
    config.MergeVirtualTextures = false;
    config.DisableLauncher = false;
    if (config.LogDirectory.empty()) {
        config.LogDirectory = FromStdUTF8(state + "/logs");
    }
    if (char const* dir = getenv("BG3SE_LUA_BUILTIN_DIR")) {
        config.LuaBuiltinResourceDirectory = FromStdUTF8(dir);   // read LuaScripts/ from disk instead of the bundle
    }

    auto console = static_cast<DebugConsole*>(gCoreLibPlatformInterface.GlobalConsole);
    console->OpenLogFile(FromStdUTF8(state + "/extender.log"));
    console->SetSocketPath(state + "/console.sock");
    console->Create();

    if (config.DebugFlags == 0) {
        config.DebugFlags = DF_DebugTrace | DF_SuppressInitLog;
    }

    platform::log_line("ext: extender log %s/extender.log, console %s/console.sock", state.c_str(), state.c_str());
    gExtender->Initialize();
    platform::log_line("ext: ScriptExtender::Initialize done");
    // PORT (linux): the ECS name maps. On Windows RegisterLibraries()' pattern scan fills
    // them during Initialize(); here the embedded TypeId table does (typeids.cpp). It has
    // to run AFTER Initialize(), not before: the maps live in StaticSymbols, and
    // gStaticSymbols is null until InitStaticSymbols() runs inside Initialize() -- binding
    // first dereferenced null and took the game down on the first frame.
    // Still early enough: UpdateComponentMappings() first runs when the entity world is
    // set up (EntitySystem.cpp:2006), long after this.
    platform::BindTypeIdTable();
}

}  // namespace bg3se
