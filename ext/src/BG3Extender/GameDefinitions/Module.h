#pragma once

#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/EntitySystem.h>

BEGIN_SE()

struct ModuleShortDesc
{
    FixedString ModuleUUIDString;
    Guid ModuleUUID;
    STDString Name;
    Version ModVersion;
    Version PublishVersion;
    STDString Hash;
    STDString Folder;
    uint64_t PublishHandle;
};

struct ModuleSettings : public ProtectedGameObject<ModuleSettings>
{
    [[bg3::hidden]]
    void* VMT;
    Array<ModuleShortDesc> Mods;
};

struct UGCModDependency
{
    uint64_t DependencyHandle;
    STDString DependencyName;
};

struct ModuleInfo
{
    FixedString ModuleUUIDString;
    Guid ModuleUUID;
    STDString Name;
    FixedString StartLevelName;
    FixedString MenuLevelName;
    FixedString LobbyLevelName;
    FixedString CharacterCreationLevelName;
    FixedString PhotoBoothLevelName;
    Version ModVersion{ 0 };
    Version PublishVersion{ 0 };
    STDString Hash;
    STDString Directory;
    [[bg3::hidden]] Array<void*> Scripts; // ScriptDataList*
    STDString Author;
    STDString Description;
    uint8_t NumPlayers{ 4 };
    uint64_t FileSize;
    uint64_t PublishHandle;
};

struct Module
{
    using HashProc = bool (Module* self);

    [[bg3::hidden]] void* VMT{ nullptr };
    ModuleInfo Info;
    Array<ModuleShortDesc> Dependencies;
    Array<ModuleShortDesc> ModConflicts;
    Array<ModuleShortDesc> Addons;
};

struct ModManager : public ProtectedGameObject<ModManager>
{
    [[bg3::hidden]] void* VMT;
    // FIXME - swap this to a generic callback list
    [[bg3::hidden]] UnknownSignal LoadCallbacks;
    [[bg3::hidden]] UnknownSignal UnloadCallbacks;
    Module BaseModule;
    uint8_t Flag;
    uint8_t Flag2;
    Array<Module> LoadOrderedModules;
    Array<Module> AvailableMods;
    [[bg3::hidden]] HashMap<uint64_t, void*> field_190;
    [[bg3::hidden]] uint64_t field_218;
    ModuleSettings Settings;

    Module const * FindModByNameGuid(char const * nameGuid) const;
};

#if defined(BG3SE_LINUX)
// PORT (linux): ls::ModManager::GetModFile(StringView const&, Path&) at bin/bg3 0x3b8b0e0
// walks LoadOrderedModules from the last entry down -- buffer at [mgr+0x130], count at
// [mgr+0x13c], stride 0xF0 -- and builds each Path from `module + 0x68`. Both the stride
// and the +0x68 fall out of the 16-byte STDString, so they are the
// second place that size is load-bearing; Ext.Mod and the bootstrap loader read this list.
static_assert(offsetof(ModManager, LoadOrderedModules) == 0x130, "bin/bg3 0x3b8b0e0 reads [mgr+0x130]");
static_assert(sizeof(Module) == 0xF0, "the LoadOrderedModules stride at bin/bg3 0x3b8b0e0");
static_assert(offsetof(Module, Info) + offsetof(ModuleInfo, Directory) == 0x68,
    "GetModFile builds its Path from module + 0x68");
#endif

END_SE()
