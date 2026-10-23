#pragma once

#include <GameDefinitions/Base/Base.h>

BEGIN_SE()

struct EntityManager;

struct LevelDesc : public ProtectedGameObject<LevelDesc>
{
    [[bg3::hidden]] void* VMT;
    FixedString LevelName;
    FixedString UniqueKey;
    Array<Path> Paths;
    uint8_t Type;
};

struct LevelMetaData : public ProtectedGameObject<LevelMetaData>
{
    [[bg3::hidden]] void* VMT;
    AABound Bounds;
    FixedString LevelTemplateBoundTrigger;
    FixedString Atmosphere;
    FixedString Lighting;
    bool VisualStreamingEnabled;
    bool IsPlatformLevel;
    float CombatCameraFadeDistance;
};

struct [[bg3::hidden]] LevelLoadState
{
    int RefCount;
    int State;
    void* LevelDataLoader;
};

struct LevelDataManager : public ProtectedGameObject<LevelDataManager>
{
    [[bg3::hidden]] void* VMT;
    HashMap<FixedString, LevelData*> Levels;
};

struct LevelData : public ProtectedGameObject<LevelData>
{
    LevelDesc* LevelDesc;
    LevelMetaData* Meta;
    [[bg3::hidden]] LevelDataManager* LevelDataManager;
    // PORT (linux): upstream has HashMap<FixedString, GameObjectTemplate*> here, but on this
    // build the value is 16 bytes, not 8 -- an Array of templates per key, not one template.
    // Measured on a live LevelData (TUT_Avernus_C, peek.py): the HashMap's
    // UninitializedStaticArray at +0x48 holds 16 records of 16 bytes ending exactly at its
    // allocation's end, each {ptr, u32 Capacity, u32 Size} with Capacity == Size (7/7, 0x36/0x36,
    // 0xf4/0xf4, 0x15d/0x15d, ... and one reserved-but-empty 0x814/0), and each buffer is a run
    // of 8-byte pointers to vtable-bearing objects drawn from one uniform-stride pool per key
    // (0x140 for one key, 0xe0 for another) -- one concrete template class per key. Read as
    // 8-byte pointers instead, entry 1 is 0x700000007, which is what makes upstream's validator
    // reject 'TrackedTemplates' and disable proxying of the whole LevelData class in game.
    // The container size is unchanged (a HashMap is four arrays whatever TV is), so no offset
    // or sizeof assert below moves; the witness for this is the validator staying quiet.
    HashMap<FixedString, Array<GameObjectTemplate*>> TrackedTemplates;
    [[bg3::hidden]] CRITICAL_SECTION CriticalSection;
    [[bg3::hidden]] LegacyRefMap<FixedString, LevelLoadState> LoadStates;
    uint32_t State;
    HashSet<FixedString> Dependencies;
};


struct [[bg3::hidden]] LevelBase : public ProtectedGameObject<LevelBase>
{
    void* VMT;
    LevelManager* LevelManager;
    void* LevelAllocator;
    LevelData* LevelData;
    LocalTemplateManager* LocalTemplateManager;
    Scene* Scene;
    phx::PhysicsSceneBase* PhysicsScene;
    FixedString LevelGuid;
};

struct ActivePersistentLevelTemplate
{
    FixedString SubLevelName;
    FixedString LevelInstanceID;
    EntityHandle Entity;
    uint64_t field_10{ 0 };
    AABound Bound;
};

struct TemplateTrace
{
    FixedString field_0;
    FixedString field_4;
};

struct [[bg3::hidden]] LSLevel : public LevelBase
{
    EntityHandle Root;
    uint32_t field_48;
    TrackedCompactSet<ActivePersistentLevelTemplate> ActiveLevelTemplates;
    Array<TemplateTrace> LocalTemplateTraces;
    // void* LayerManager;
    uint32_t LoadState;
};


struct [[bg3::hidden]] EoCLevel : public LSLevel
{
    void* EntityManager;
    AiGrid* AiGrid;
    void* CoverManager;
};

struct [[bg3::hidden]] LevelManagerBase
{
    uint64_t Unknown[0x10];
};

struct [[bg3::hidden]] LevelManager : public ProtectedGameObject<LevelManager>
{
    void* VMT;
    LevelManagerBase b;
    LocalTemplateManager* LocalTemplateManager;
    EoCLevel* CurrentLevel;
    LegacyMap<FixedString, EoCLevel*> Levels;
    void* field_B0;
};

#if defined(BG3SE_LINUX)
// PORT (linux): read off the running game, not the disassembly -- these are pointer fields
// whose values identify themselves, so a live read is the cheaper witness (the object the manifest
// row esv__LevelManager = bin/bg3 0x7c9f7b0 points at, read out of the process in a loaded
// save). The three cross-links each close a loop, so no single
// offset is a fit:
//   esv::LevelManager* = 0x3cd200dbac0; its VMT is in bg3.
//   [+0x88] = [+0x90]+0x20 = 0x3cd200a9f80 -- the manager's LocalTemplateManager and the
//     level's are the same object, and it decodes as one: its LegacyRefMaps at the addresses
//     RootTemplates.h already asserts read {ItemCount 1846, HashSize 15013} at +0x38 and the
//     same pair at +0x58 (Templates and TemplatesByHandle hold every local template), and
//     {20, 37} at +0x48 (TemplatesByType, one per template type). Ext.Template
//     .GetAllLocalTemplates() returned 1846 entries in the same save -- the Lua path and the
//     raw read agree, which is what made this an end-to-end witness rather than a decode.
//   [+0x90]+0x08 = 0x3cd200dbac0 -- CurrentLevel back-points to the manager the global names.
//   [+0x90]+0x18 = 0x3cd9c910860, and its +0x10 = 0x3cd200b2c40 = LocalTemplateManager+0x68:
//     LevelData::LevelDataManager and LocalTemplateManager::LevelDataManager are one object,
//     with a bg3 VMT at +0. So LevelBase::LevelData is at 0x18.
// Not asserted, because the read is consistent with them rather than evidence for them:
// LevelAllocator (0x10), Scene (0x28, null on a server level), PhysicsScene (0x30),
// LevelGuid (0x38), LSLevel::Root (0x40) and LevelManager::Levels (0x98).
static_assert(offsetof(LevelBase, LevelManager) == 0x08);
static_assert(offsetof(LevelBase, LevelData) == 0x18);
static_assert(offsetof(LevelBase, LocalTemplateManager) == 0x20);
static_assert(offsetof(LevelManager, LocalTemplateManager) == 0x88);
static_assert(offsetof(LevelManager, CurrentLevel) == 0x90);

// PORT (linux): LevelData past +0x10, read off the macOS oracle -- and it is the one case so
// far where the oracle's *absolute* offsets do not transfer, because a lock sits in the middle.
// ls::CriticalSection is 48 bytes in bin/bg3 (a pthread_mutex_t plus RecursionCount/SpinCount)
// but only 24 in the macOS build, which implements it as a __ulock
// futex word at +0x00, a lock count at +0x04, a recursion count at +0x08 and an owner tid at
// +0x0c (ls::CriticalSection::Lock 0x107439f70, ~CriticalSection 0x107439f30). So only the
// offsets *relative to the end of the lock* carry over, and the asserts below are written that
// way on purpose: they are meant to stay silent if sizeof(CRITICAL_SECTION) is ever remeasured.
// The three macOS witnesses, all on ls::LevelData methods that take `this` in rsi or rdi:
//   ls::LevelData::AcquireLoader (0x106e7c640) calls ls::CriticalSection::Lock() on this+0x58
//     and inlines the unlock on the same object (dec [this+0x60], lock dec [this+0x5c]), so
//     CriticalSection is at 0x58 -- which is also TrackedTemplates' end, 0x18 + 0x40.
//   ls::LevelData::Unload (0x106e7bf20) divides a FixedString hash by dword [this+0x74] and
//     indexes the bucket array at qword [this+0x78], then walks {next, ?, key} nodes: that is
//     LoadStates, a LegacyRefMap at 0x70 -- immediately after the 24-byte macOS lock.
//   ls::LevelData::ResolveDependencies (0x106e7cb50) opens with
//     `lock cmpxchg dword [this+0x80], 3` against eax = 2 and ends with
//     `mov dword [this+0x80], 4`, so State is at 0x80 = LoadStates + 0x10; and it walks three
//     arrays at 0x88/0x98/0xa8 with their sizes at 0x90/0xa0+0xa4/0xb0+0xb4, dividing a hash
//     by [this+0x90] -- a HashSet<FixedString> at 0x88 = LoadStates + 0x18, ending at 0xb8.
// Every one of those is LoadStates-relative +0x00/+0x10/+0x18/+0x48, which is exactly where
// this mirror puts them once the lock is the game's 48 bytes.
static_assert(offsetof(LevelData, TrackedTemplates) == 0x18);
static_assert(offsetof(LevelData, CriticalSection) == 0x58,
    "ls::LevelData::AcquireLoader locks this+0x58 (macOS 0x106e7c640)");
// The three below are written as a difference from LoadStates, not as an absolute offset, for
// two reasons: it is the difference the oracle actually witnesses, and the literal sits on
// the right of the `==` so a calibration that flips the constant can see it.
// There is deliberately no assert that LoadStates is at CriticalSection + sizeof(CRITICAL_SECTION):
// that is true by construction from the declaration order above, so it would be vacuous.
static_assert(offsetof(LevelData, State) - offsetof(LevelData, LoadStates) == 0x10,
    "ResolveDependencies' cmpxchg 2->3 at LoadStates+0x10 (macOS 0x106e7cb50)");
static_assert(offsetof(LevelData, Dependencies) - offsetof(LevelData, LoadStates) == 0x18,
    "ResolveDependencies' HashSet at LoadStates+0x18 (macOS 0x106e7cb50)");
static_assert(sizeof(LevelData) - offsetof(LevelData, LoadStates) == 0x48,
    "Dependencies' last array ends at LoadStates+0x48 (macOS 0x106e7cb50)");
#endif

END_SE()

BEGIN_NS(esv)

struct [[bg3::hidden]] Level : public EoCLevel
{
    void* SightManager;
    void* CharacterManager;
    void* ItemManager;
    SurfaceManager* SurfaceManager;
    void* GameActionManager;
    void* EffectManager;
    void* ShroudManager;
    void* LightingOverrides;
    void* AtmosphereOverrides;
    void* EnvironmentalStatusManager;
    void* StaticPhysicsManager;
    LevelCacheTemplateManager* CacheTemplateManager;
};

struct [[bg3::hidden]] LevelManager : public bg3se::LevelManager
{
    Array<void*> field_B8;
    ecs::EntityWorld* EntityWorld;
};

END_NS()

BEGIN_NS(ecl)

struct [[bg3::hidden]] Level : public EoCLevel
{
    // TODO
};

struct [[bg3::hidden]] LevelManager : public bg3se::LevelManager
{
};

END_NS()
