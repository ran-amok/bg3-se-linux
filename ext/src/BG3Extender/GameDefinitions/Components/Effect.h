#pragma once

BEGIN_SE()

struct EffectComponent : public BaseProxyComponent
{
    DEFINE_COMPONENT(Effect, "ls::EffectComponent")

    [[bg3::hidden]] void* VMT;
    [[bg3::hidden]] void* WorkerThreadJob;
    EntityHandle Entity;
    resource::EffectResource* EffectResource;
    aspk::EffectTimeline* Timeline;
    bool Initialized;
    bool OverridingFadeOpacity;
    EffectFlags Flags;
    FixedString EffectName;
    FixedString AnimationName;
    bool UpdateQueued;
    EntityHandle SoundEntity;
    [[bg3::legacy(field_48)]] EntityHandle Parent;
    std::array<float, 2> OverrideFadeCapacity;
    std::array<bool, 2> OverrideFadeShadowEnabled;
    [[bg3::hidden]] void* ConstructionJob;
    [[bg3::readonly]] uint16_t ConstructFlagsAndSalt;
};

DEFINE_ONEFRAME_TAG_COMPONENT(ls, EffectCreateOneFrameComponent, EffectCreate)
DEFINE_TAG_COMPONENT(ls, AlwaysUpdateEffectComponent, AlwaysUpdateEffect)

struct GameplayEffectSetTimeFactorRequestsSingletonComponent : public BaseComponent
{
    DEFINE_COMPONENT(GameplayEffectSetTimeFactorRequests, "ls::GameplayEffectSetTimeFactorRequestsSingletonComponent")

    HashMap<EntityHandle, float> SetTimeFactor;
};

struct VFXSetPlayTimeRequest
{
    uint64_t Phase;
    float Time;
};

struct GameplayVFXSetPlayTimeRequestsSingletonComponent : public BaseComponent
{
    DEFINE_COMPONENT(GameplayVFXSetPlayTimeRequests, "ls::GameplayVFXSetPlayTimeRequestsSingletonComponent")

    HashMap<EntityHandle, VFXSetPlayTimeRequest> SetPlayTime;
};

struct GameplayVFXInfo
{
    FixedString VFX;
    glm::vec3 Position;
};

struct GameplayVFXSingletonComponent : public BaseComponent
{
    DEFINE_COMPONENT(GameplayVFX, "ls::GameplayVFXSingletonComponent")

    Array<GameplayVFXInfo> VFX;
};


#pragma pack(push, 4)

struct EffectCreateInvoke {};

struct EffectLoadInvoke {};

struct EffectDestroyInvoke {};

struct EffectReleaseFxInvoke {};

struct EffectStopInvoke {};

struct EffectForceStopInvoke {};

struct EffectRegisterForgetInvoke
{
    float field_0;
    bool field_4;
    bool field_5;
};

struct EffectPlayInvoke
{
    [[bg3::hidden]] Scene* Scene{ nullptr };
};

struct EffectPauseInvoke {};

struct EffectPlayToEndInvoke {};

struct EffectUnregisterForgetInvoke {};

struct EffectRestoreRenderStateInvoke {};

struct EffectSoundActivationInvoke
{
    bool Activate;
};

struct EffectSoundStateInvoke
{
    bool State;
};

struct EffectInputIntInvoke
{
    int32_t InputIndex;
    int32_t Value;
};

struct EffectInputFloatInvoke
{
    int32_t InputIndex;
    float Value;
};

struct EffectInputFloat3Invoke
{
    int32_t InputIndex;
    glm::vec3 Value;
};

struct EffectInputColorInvoke
{
    int32_t InputIndex;
    uint32_t Value;
};

struct EffectTargetVisualInvoke
{
    EntityHandle Visual;
};

struct EffectResetTargetVisualInvoke
{
    EntityHandle Visual;
};

struct EffectUpdateTargetVisualInvoke
{
    EntityHandle Visual;
};

struct EffectInputFixedStringInvoke
{
    int32_t InputIndex;
    FixedString Value;
};

struct EffectInputFastStringInvoke
{
    int32_t InputIndex;
    char const* Value{ nullptr };
    uint32_t Length;
};

struct EffectInputStringInvoke
{
    char const* Value{ nullptr };
    uint32_t Length;
};

struct EffectCallbackNewInvoke
{
    [[bg3::hidden]] void* Callback{ nullptr };
};

struct EffectPhaseInvoke
{
    uint64_t PhaseIndex;
    bool field_8;
};

#pragma pack(pop)

struct EffectInvoke
{
    using InvokeType = std::variant<
        EffectCreateInvoke,
        EffectLoadInvoke,
        EffectDestroyInvoke,
        EffectReleaseFxInvoke,
        EffectStopInvoke,
        EffectForceStopInvoke,
        EffectRegisterForgetInvoke,
        EffectPlayInvoke,
        EffectPauseInvoke,
        EffectPlayToEndInvoke,
        EffectUnregisterForgetInvoke,
        EffectRestoreRenderStateInvoke,
        EffectSoundActivationInvoke,
        EffectSoundStateInvoke,
        EffectInputIntInvoke,
        EffectInputFloatInvoke,
        EffectInputFloat3Invoke,
        EffectInputColorInvoke,
        EffectTargetVisualInvoke,
        EffectResetTargetVisualInvoke,
        EffectUpdateTargetVisualInvoke,
        EffectInputFixedStringInvoke,
        EffectInputFastStringInvoke,
        EffectInputStringInvoke,
        EffectCallbackNewInvoke,
        EffectPhaseInvoke
    >;

    InvokeType Invoke;
    uint32_t Salt;
    EntityHandle Effect;
};

struct MaterialInfo : ProtectedGameObject<MaterialInfo>
{
    [[bg3::hidden]] void* VMT;
    EntityHandle Entity;
    FixedString MaterialID;
    float OverlayOffset;
    OverlayMaterialApplyFlags ApplyFlags;
    ApplyMaterialMapFlags MapFlags;
    MaterialInfoFlags Flags;
};

struct OverlayMaterialRequest
{
    [[bg3::hidden]] MaterialInfo** Material{ nullptr };
    [[bg3::hidden]] bool* CompletionFlag{ nullptr };
    float FadingParam{ .0f };
    EntityHandle Entity;
    FixedString MaterialID;
    float OverlayOffset{ .0f };
    OverlayMaterialApplyFlags ApplyFlags{ 0 };
    ApplyMaterialMapFlags MapFlags{ 0 };
    OverlayMaterialRequestFlags Flags{ 0 };
    [[bg3::hidden]] UnknownFunction Callback;
};

struct DestroyEffectStruct
{
    EntityHandle Effect;
    float field_8;
};

struct EffectSoundObjectInfo
{
    FixedString field_0;
    EntityHandle field_8;
};

struct EffectsManager : public BaseSystem
{
    DEFINE_SYSTEM(Effect, "ls::EffectsManager")

    [[bg3::hidden]] void* VMT_IWorkerThreadListener;
    [[bg3::hidden]] UnknownSignal OverlayMaterialRequestSignal;
    bool CanUpdate;
    // PORT (linux): upstream's `uint32_t` (a Win32 DWORD thread id) is a pthread_t here; changed
    // outright, not under #if, because make_property_map.py reads through #if and would emit the
    // property twice. The ctor (bin/bg3 0x5077430, factory operator new(0x240) at 0x50773f6)
    // stores CanUpdate = 1 at +0x30, calls pthread_self (0x5077476) and stores the qword result at
    // +0x38, and builds DestroyEffectData at +0x40 -- so every member from here on sits 8 above
    // upstream.
    [[bg3::readonly]] uint64_t FxCreateThreadId;
    Array<DestroyEffectStruct> DestroyEffectData;
    [[bg3::hidden]] Queue<EffectInvoke> Invokes;
    [[bg3::hidden]] CRITICAL_SECTION InvokesCS;
    HashMap<EntityHandle, EffectSoundObjectInfo> SoundObjectsOnLevel;
    [[bg3::hidden]] Array<void*> Listeners; // IEffectsManagerListener*
    // Editor only?
    // TrackedCompactSet<Path> UnloadEffects;
    // uint8_t field_E8;
    [[bg3::hidden]] CRITICAL_SECTION RCB_CS;
    [[bg3::hidden]] void* RCB; // rf::RendererCommandBuffer*
    [[bg3::hidden]] std::array<void*, 3> GPUFences;
    [[bg3::hidden]] Array<void*> RCBs; // rf::RendererCommandBuffer*
    [[bg3::hidden]] CRITICAL_SECTION AddOverlayMaterialRequestsCS;
    [[bg3::hidden]] Array<OverlayMaterialRequest> OverlayMaterialRequests;
    [[bg3::hidden]] CRITICAL_SECTION NonConstructedEffectsCS;
    [[bg3::hidden]] HashSet<EntityHandle> NonConstructedEffects;
    [[bg3::hidden]] void* RenderBatchesGlobalVB;
    [[bg3::hidden]] void* qword1E0;
    [[bg3::hidden]] void* RibbonGlobalVB;
    [[bg3::hidden]] void* VisualSystem;
    [[bg3::hidden]] void* VisualChangedSystem;
#if defined(BG3SE_LINUX)
// PORT (linux): three Arrays upstream does not have. The ctor zeroes 0x210..0x240, and the D1
// (bin/bg3 0x74fd430) frees buffers at 0x210, 0x220 and 0x230 (the last cleared as an Array of
// non-trivial elements, size at 0x23c). Element types not read. macOS af3f1c71's 0x1e0 is this
// 0x240 less its four locks' 24 bytes each.
    [[bg3::hidden]] Array<void*> field_210;
    [[bg3::hidden]] Array<void*> field_220;
    [[bg3::hidden]] Array<void*> field_230;
#endif

    //# P_FUN(Invoke, EffectsManager::Invoke)
    //# P_FUN(AddMaterial, EffectsManager::AddMaterial)
    void Invoke(EffectInvoke invoke);
    void AddMaterial(OverlayMaterialRequest material);
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(EffectsManager, CanUpdate) == 0x30,
    "bin/bg3 ctor 0x5077472 stores CanUpdate = 1 at this+0x30");
static_assert(offsetof(EffectsManager, FxCreateThreadId) == 0x38,
    "bin/bg3 ctor 0x5077485 stores pthread_self() at this+0x38");
static_assert(offsetof(EffectsManager, DestroyEffectData) == 0x40,
    "bin/bg3 ctor 0x50774a3 stores the Array buffer at this+0x40; PostSceneUpdate 0x2d1dc60 reads it");
static_assert(offsetof(EffectsManager, Invokes) == 0x50,
    "bin/bg3 ctor 0x50774b3 constructs the queue at this+0x50");
static_assert(offsetof(EffectsManager, InvokesCS) == 0x68,
    "bin/bg3 queue ctor 0x3343bc3 initialises the mutex at queue+0x18 = this+0x68");
static_assert(offsetof(EffectsManager, RCB_CS) == 0xE8,
    "bin/bg3 ctor 0x5077533 initialises the mutex at this+0xe8; OnWorkerThreadStopped decrements +0x110");
static_assert(offsetof(EffectsManager, RCBs) == 0x138,
    "bin/bg3 OnWorkerThreadStopped 0x5012da0 reads the Array at this+0x138");
static_assert(offsetof(EffectsManager, AddOverlayMaterialRequestsCS) == 0x148,
    "bin/bg3 ctor 0x507755f initialises the mutex at this+0x148");
static_assert(offsetof(EffectsManager, OverlayMaterialRequests) == 0x178,
    "bin/bg3 ctor 0x50775e8 builds the Array at this+0x178 (capacity 0x40 at +0x180)");
static_assert(offsetof(EffectsManager, NonConstructedEffectsCS) == 0x188,
    "bin/bg3 ctor 0x5077666 initialises the mutex at this+0x188");
static_assert(offsetof(EffectsManager, NonConstructedEffects) == 0x1B8,
    "bin/bg3 ctor 0x507770f gives the set 0x29 buckets at this+0x1b8");
static_assert(offsetof(EffectsManager, RenderBatchesGlobalVB) == 0x1E8,
    "bin/bg3 D1 0x74fd463 destroys the object at this+0x1e8");
static_assert(sizeof(EffectsManager) == 0x240,
    "operator new(0x240) at bin/bg3 0x50773f6");
#endif

END_SE()

BEGIN_NS(ecl::effect)

struct HandlerComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClientEffectHandler, "ecl::effect::HandlerComponent")

    EffectHandler* Handler{ nullptr };
};

struct [[bg3::hidden]] HandlerComponentView
{
    inline HandlerComponentView() {}
    HandlerComponentView(EntityHandle entity);

    HandlerComponent* EffectHandler{ nullptr };
    uint64_t* MutationData{ nullptr };
    void* field_10{ nullptr };
    EntityHandle Entity;
    uint32_t ComponentIndex{ 0 };
    uint8_t field_24{ 0 };
};

struct [[bg3::hidden]] HandlerSystemHelper
{
    virtual ~HandlerSystemHelper() = 0;
    virtual void InitMultiEffect(HandlerComponentView const& entity, EffectHandlerInitInfo const& info, Scene* scene) const = 0;
    virtual void UpdateMultiEffect(HandlerComponentView const& entity) const = 0;
    virtual void DestroyMultiEffect(HandlerComponentView const& entity) const = 0;
    virtual void HideMultiEffect(HandlerComponentView const& entity) const = 0;
    virtual void UnhideMultiEffect(HandlerComponentView const& entity) const = 0;
};

struct HandlerSystem : public BaseSystem
{
    DEFINE_SYSTEM(ClientEffectHandler, "ecl::effect::HandlerSystem")

    [[bg3::hidden]] HandlerSystemHelper* Helper;

    //# P_FUN(InitMultiEffect, ecl::effect::HandlerSystem::InitMultiEffect)
    //# P_FUN(UpdateMultiEffect, ecl::effect::HandlerSystem::UpdateMultiEffect)
    //# P_FUN(DestroyMultiEffect, ecl::effect::HandlerSystem::DestroyMultiEffect)
    //# P_FUN(HideMultiEffect, ecl::effect::HandlerSystem::HideMultiEffect)
    //# P_FUN(UnhideMultiEffect, ecl::effect::HandlerSystem::UnhideMultiEffect)
    EntityHandle InitMultiEffect(EffectHandlerInitInfo info);
    bool UpdateMultiEffect(EntityHandle effect);
    bool DestroyMultiEffect(EntityHandle effect);
    bool HideMultiEffect(EntityHandle effect);
    bool UnhideMultiEffect(EntityHandle effect);
};

END_NS()
