#pragma once

#include <GameDefinitions/Render.h>

BEGIN_SE()

struct LevelComponent : public BaseComponent
{
    DEFINE_COMPONENT(Level, "ls::LevelComponent")

    [[bg3::legacy(field_0)]] EntityHandle RootLevel;
    FixedString LevelName;
};

struct LevelRootComponent : public BaseComponent
{
    DEFINE_COMPONENT(LevelRoot, "ls::LevelRootComponent")

    FixedString LevelName;
};

struct LevelInstanceComponent : public BaseComponent
{
    DEFINE_COMPONENT(LevelInstance, "ls::LevelInstanceComponent")

    FixedString LevelInstanceID;
    FixedString SubLevelName;
    FixedString LevelInstanceTemplate;
    uint8_t LevelType{ 0 };
    bool Active{ false };
    bool Platform{ false };
    bool MovingPlatform{ false };
    bool DynamicLayer{ false };
    bool NeedsPhysics{ false };
    bool HasTemplateAfterDestruction{ false };
    bool UseSoundOcclusion{ false };
    bool IsCinematic{ false };
    bool Activated{ false };
    Transform Transform;
};

struct LevelInstanceStateComponent : public BaseComponent
{
    DEFINE_COMPONENT(LevelInstanceState, "ls::LevelInstanceStateComponent")

    HashSet<EntityHandle> Children;
    HashSet<EntityHandle> Children2;
    LevelInstanceState State;
    AABound LocalBound;
    AABound WorldBound;
    FixedString MergedLevelTemplateUUID;
    FixedString LevelInstanceID;
    FixedString LevelName;
    FixedString LevelName2;
    bool Destroyed;
    bool MovingPlatform;
    uint8_t field_A6;
    Transform Transform;
};

struct LevelInstanceTempDestroyedComponent : public BaseComponent
{
    DEFINE_COMPONENT(LevelInstanceTempDestroyed, "ls::level::LevelInstanceTempDestroyedComponent")

    EntityHandle Level;
};

struct LevelUnloadEventComponent : public BaseComponent
{
    DEFINE_ONEFRAME_COMPONENT(LevelUnloadEvent, "ls::LevelUnloadEventComponent")

    FixedString Level;
};

struct LevelPrepareUnloadEventComponent : public BaseComponent
{
    DEFINE_ONEFRAME_COMPONENT(LevelPrepareUnloadEvent, "ls::LevelPrepareUnloadEventComponent")

    FixedString Level;
};

struct LevelUnloadedOneFrameComponent : public BaseComponent
{
    DEFINE_ONEFRAME_COMPONENT(LevelUnloaded, "ls::LevelUnloadedOneFrameComponent")

    FixedString Level;
};

struct [[bg3::component]] SceneComponent : public Scene
{
    DEFINE_PROXY_COMPONENT(Scene, "ls::Scene")
};

DEFINE_TAG_COMPONENT(ls, SceneRootComponent, SceneRoot)
DEFINE_TAG_COMPONENT(ls, LevelIsOwnerComponent, LevelIsOwner)
DEFINE_TAG_COMPONENT(ls, LevelPrepareUnloadBusyComponent, LevelPrepareUnloadBusy)
DEFINE_TAG_COMPONENT(ls, LevelUnloadBusyComponent, LevelUnloadBusy)
DEFINE_TAG_COMPONENT(ls, LevelInstanceLoadComponent, LevelInstanceLoad)
DEFINE_TAG_COMPONENT(ls::level, LevelInstanceUnloadingComponent, LevelInstanceUnloading)
DEFINE_ONEFRAME_TAG_COMPONENT(ls, LevelInstanceUnloadedOneFrameComponent, LevelInstanceUnloaded)
DEFINE_ONEFRAME_TAG_COMPONENT(ls, LevelInstanceLoadedOneFrameComponent, LevelInstanceLoaded)


struct LevelInstanceAttachRequestSystem : public BaseSystem
{
    DEFINE_SYSTEM(LevelInstanceAttachRequest, "ls::LevelInstanceAttachRequestSystem")

    [[bg3::hidden]] CRITICAL_SECTION LevelSwapRequestsCS;
    [[bg3::hidden]] HashMap<EntityHandle, EntityHandle> LevelSwapRequests;
    [[bg3::hidden]] DWORD LastFrame;

    // Lua call helpers
    //# P_FUN(RequestLevelSwap, LevelInstanceAttachRequestSystem::RequestLevelSwap)
    void RequestLevelSwap(EntityHandle entity, EntityHandle level);
};

struct LevelInstanceLoadRequest
{
    EntityHandle Level;
    bool Load{ true };
};

struct LevelInstanceLoadRequestSystem : public BaseSystem
{
    DEFINE_SYSTEM(LevelInstanceLoadRequest, "ls::LevelInstanceLoadRequestSystem")

    [[bg3::hidden]] SynchronizedMPMCQueueBounded<LevelInstanceLoadRequest> Requests;
};

struct LevelInstanceMoveSystem : public BaseSystem
{
    DEFINE_SYSTEM(LevelInstanceMove, "ls::LevelInstanceMoveSystem")

    HashSet<EntityHandle> LastFrameMovedLevels;
    HashSet<EntityHandle> MoveLevels;
    [[bg3::hidden]] UnknownFunction Signals;
};

struct LevelInstanceAttachSystem : public BaseSystem
{
    DEFINE_SYSTEM(LevelInstanceAttach, "ls::level::LevelInstanceAttachSystem")

    HashSet<EntityHandle> RemoveLevelInstance;
    HashMap<EntityHandle, HashSet<EntityHandle>> DetachLevel;
    HashMap<EntityHandle, HashSet<EntityHandle>> AttachLevel;
    [[bg3::hidden]] LevelInstanceAttachRequestSystem* LevelInstanceAttachRequestSystem;
    [[bg3::hidden]] UnknownSignalSubscriber Replication;
    [[bg3::hidden]] void* qword118;
};

struct LevelInstanceLoadSystem : public BaseSystem
{
    DEFINE_SYSTEM(LevelInstanceLoad, "ls::level::LevelInstanceLoadSystem")

#if !defined(BG3SE_LINUX)
    [[bg3::hidden]] UnknownSignal field_10;
#endif
// PORT (linux): there is no signal ahead of LevelManager in the native build. The factory
// (bin/bg3 0x43d60f0, called from the registration 0x43d5f20 that loads this class's TypeId) is
// operator new(0x60): vptr, [+8] = 0, its argument at +0x10, then the queue constructor
// 0x37afa70 on this+0x18, which builds the Queue in 0x18 bytes and initialises the mutex at
// queue+0x18 = this+0x30. macOS af3f1c71 (new(0x48) at 0x1010fbc9d) stores the argument at
// +0x10 and the queue at +0x18 too; its 0x48 is this 0x60 less a lock's 24.
    [[bg3::hidden]] LevelManager* LevelManager;
    [[bg3::hidden]] Queue<EntityHandle> LoadRequests;
    [[bg3::hidden]] CRITICAL_SECTION LoadRequestsCS;
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(LevelInstanceLoadSystem, LevelManager) == 0x10,
    "bin/bg3 factory 0x43d611e stores its argument at this+0x10");
static_assert(offsetof(LevelInstanceLoadSystem, LoadRequestsCS) == 0x30,
    "bin/bg3 queue ctor 0x37afa98 hands this+0x18+0x18 to the mutex init");
static_assert(sizeof(LevelInstanceLoadSystem) == 0x60,
    "operator new(0x60) at bin/bg3 0x43d6100");
#endif


END_SE()
