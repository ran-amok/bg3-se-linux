#pragma once

#include <GameDefinitions/CharacterCreation.h>
#include <GameDefinitions/Animation.h>
#include <GameDefinitions/Resources.h>

BEGIN_SE()

struct CustomIconComponent : public BaseComponent
{
    DEFINE_COMPONENT(CustomIcon, "eoc::CustomIconComponent")

    ScratchBuffer Icon;
    uint8_t Source;
};

struct IconComponent : public BaseComponent
{
    DEFINE_COMPONENT(Icon, "eoc::IconComponent")

    FixedString Icon;
};

struct ActiveCharacterLightComponent : public BaseComponent
{
    DEFINE_COMPONENT(ActiveCharacterLight, "eoc::light::ActiveCharacterLightComponent")

    FixedString Light;
};

struct VisualComponent : public BaseComponent
{
    DEFINE_COMPONENT(Visual, "ls::VisualComponent")

    Visual* Visual;
    uint8_t field_8;
    uint8_t field_9;
    bool NotClustered;
};

struct GameObjectVisualComponent : public BaseComponent
{
    DEFINE_COMPONENT(GameObjectVisual, "eoc::GameObjectVisualComponent")

    FixedString RootTemplateId;
    uint8_t RootTemplateType;
    FixedString Icon;
    float Scale;
    uint8_t Type;
};

struct AppearanceOverrideComponent : public BaseComponent
{
    DEFINE_COMPONENT(AppearanceOverride, "eoc::object_visual::AppearanceOverrideComponent")

    character_creation::Appearance Visual;
};

struct CharacterCreationTemplateOverrideComponent : public BaseComponent
{
    DEFINE_COMPONENT(CharacterCreationTemplateOverride, "eoc::object_visual::CharacterCreationTemplateOverrideComponent")

    FixedString Template;
};

struct DisplayNameComponent : public BaseComponent
{
    DEFINE_COMPONENT(DisplayName, "eoc::DisplayNameComponent")

    [[bg3::legacy(NameKey)]] TranslatedString Name;
    [[bg3::legacy(UnknownKey)]] TranslatedString Title;
};

struct CustomNameComponent : public BaseComponent
{
    DEFINE_COMPONENT(CustomName, "eoc::CustomNameComponent")

    STDString Name;
};

struct GameplayLightComponent : public BaseComponent
{
    DEFINE_COMPONENT(GameplayLight, "eoc::GameplayLightComponent")

    [[bg3::legacy(field_0)]] bool Active;
    [[bg3::legacy(field_4)]] float Radius;
    [[bg3::legacy(field_8)]] float EdgeSharpening;
    [[bg3::legacy(field_C)]] float SpotlightAngle;
    [[bg3::legacy(field_10)]] glm::vec3 DirectionalDimensions;
    [[bg3::legacy(field_1C)]] LightType LightType;
    [[bg3::legacy(field_1D)]] bool IsHalfLit;
    [[bg3::legacy(field_1E)]] bool Sunlight;
    [[bg3::legacy(field_20)]] glm::vec3 AttachAt;
    [[bg3::legacy(field_2C)]] std::optional<float> PointLightVerticalLimit;
    [[bg3::legacy(field_34)]] bool CheckLOS;
    [[bg3::legacy(field_38)]] FixedString LightCookie;
    float field_3C;
    float field_40;
    uint8_t field_44;
};

struct CharacterCreationAppearanceComponent : public BaseComponent
{
    DEFINE_COMPONENT(CharacterCreationAppearance, "eoc::character_creation::AppearanceComponent")

    Array<Guid> Visuals;
    Array<character_creation::AppearanceMaterialSetting> Elements;
    Array<float> AdditionalChoices;
    Guid SkinColor;
    Guid EyeColor;
    Guid SecondEyeColor;
    Guid HairColor;
};

struct AnimationBlueprintComponent : public BaseProxyComponent
{
    DEFINE_COMPONENT(AnimationBlueprint, "ls::AnimationBlueprintComponent")

    gn::GenomeBlueprintInstance* Instance;
    resource::AnimationBlueprintResource* Resource;
    gn::GenomeBlueprint* Blueprint;
    [[bg3::hidden]] void* LoadCallback;
    [[bg3::readonly]] int InstanceId;
    [[bg3::readonly]] uint8_t Flags;
};

struct AnimationWaterfallElement
{
    FixedString Slot;
    FixedString Resource;
    FixedString Type;
};

struct AnimationWaterfallOverride
{
    Guid AnimationTag;
    uint8_t OverrideType;
    Array<AnimationWaterfallElement> Overrides;
};

struct AnimationTag
{
    Guid Tag;
    uint8_t field_10;
};

struct AnimationSetEntry
{
    FixedString Slot;
    FixedString Resource;
    FixedString Type;
    [[bg3::hidden]] AnimationTag* TagsStart;
    [[bg3::hidden]] AnimationTag* TagsEnd;
    bool Dynamic;
};

struct AnimationSetComponent : public BaseComponent
{
    DEFINE_PROXY_COMPONENT(AnimationSet, "ls::AnimationSetComponent")

    MiniCompactSet<AnimationSetEntry> Entries;
    FixedString FallbackSubSet;
};

DEFINE_TAG_COMPONENT(ls, AnimationUpdateComponent, AnimationUpdate)

struct AnimationWaterfallComponent : public BaseComponent
{
    DEFINE_COMPONENT(AnimationWaterfall, "ls::animation::AnimationWaterfallComponent")

    Array<AnimationWaterfallElement> Waterfall;
    Array<AnimationWaterfallOverride> Overrides;
};

struct DynamicAnimationTagsComponent : public BaseComponent
{
    DEFINE_COMPONENT(DynamicAnimationTags, "ls::animation::DynamicAnimationTagsComponent")

    Array<AnimationTag> Tags;
};

struct TemplateAnimationSetOverrideComponent : public BaseComponent
{
    DEFINE_COMPONENT(TemplateAnimationSetOverride, "ls::animation::TemplateAnimationSetOverrideComponent")

    Array<AnimationWaterfallElement> Overrides;
};

struct LoadAnimationFromVisualRequestData
{
    FixedString VisualResource;
    FixedString Type;
    FixedString SkeletonSlot;
    uint8_t field_C;
};

struct RemoveAnimationSetsRequestOneFrameComponent : public BaseComponent
{
    DEFINE_ONEFRAME_COMPONENT(RemoveAnimationSetsRequest, "ls::animation::RemoveAnimationSetsRequestOneFrameComponent")

    HashSet<FixedString> AnimationSets;
};

struct RemoveAnimationSetsGameplayRequestOneFrameComponent : public BaseComponent
{
    DEFINE_ONEFRAME_COMPONENT(RemoveAnimationSetsGameplayRequest, "ls::animation::RemoveAnimationSetsGameplayRequestOneFrameComponent")

    HashSet<FixedString> AnimationSets;
};

struct LoadAnimationSetRequestOneFrameComponent : public BaseComponent
{
    DEFINE_ONEFRAME_COMPONENT(LoadAnimationSetRequest, "ls::animation::LoadAnimationSetRequestOneFrameComponent")

    Array<LoadAnimationFromVisualRequestData> Animations;
};

struct LoadAnimationSetGameplayRequestOneFrameComponent : public BaseComponent
{
    DEFINE_ONEFRAME_COMPONENT(LoadAnimationSetGameplayRequest, "ls::animation::LoadAnimationSetGameplayRequestOneFrameComponent")

    Array<LoadAnimationFromVisualRequestData> Animations;
};

DEFINE_ONEFRAME_TAG_COMPONENT(ls::animation, AnimationSetUpdateRequestComponent, AnimationSetUpdateRequest)

struct AnimationSetSystem : public BaseSystem
{
    DEFINE_SYSTEM(AnimationSet, "ls::AnimationSetSystem")

    [[bg3::hidden]] void* AnimationSetSystemHelper;
    [[bg3::hidden]] void* AnimationBlueprintSystem;
    [[bg3::hidden]] void* VisualSystem;
    bool field_28;
    HashMap<EntityHandle, Array<LoadAnimationFromVisualRequestData>> LoadAnimationSets;
    HashSet<EntityHandle> AnimationSetUpdates;
    HashMap<EntityHandle, Array<FixedString>> RemoveAnimationSets;
    [[bg3::hidden]] UnknownSignalSubscriber DynamicAnimationTagsSignal;
    [[bg3::hidden]] UnknownSignalSubscriber TemplateAnimationSetOverrideSignal;
};

struct MaterialParameterFloat
{
    FixedString Parameter;
    float Value;
    bool Override{ false };
    uint8_t field_9{ 0 };
    bool Preset{ false };
};

struct MaterialParameterVec2
{
    FixedString Parameter;
    glm::vec2 Value;
    bool Override{ false };
    uint8_t field_9{ 0 };
    bool Preset{ false };
};

struct MaterialParameterVec3
{
    FixedString Parameter;
    glm::vec3 Value;
    bool Override{ false };
    uint8_t field_9{ 0 };
    bool Preset{ false };
};

struct MaterialParameterVec4
{
    FixedString Parameter;
    glm::aligned_highp_vec4 Value;
    bool Override{ false };
    uint8_t field_9{ 0 };
    bool Preset{ false };
};

struct MaterialParameterTexture
{
    FixedString Parameter;
    FixedString Value;
    bool Override{ false };
    uint8_t field_9{ 0 };
    bool Preset{ false };
};

struct MaterialParameterPresetSlot
{
    FixedString GroupName;
    FixedString CCPreset;
    uint32_t field_8;
};

struct MaterialParameterPresetsContainer
{
    Array<MaterialParameterFloat> FloatOverrides;
    Array<MaterialParameterVec2> Vec2Overrides;
    Array<MaterialParameterVec3> Vec3Overrides;
    Array<MaterialParameterVec4> Vec4Overrides;
    Array<MaterialParameterTexture> TextureOverrides;
    Array<MaterialParameterTexture> VirtualTextureOverrides;
    FixedString field_60;
    HashMap<FixedString, MaterialParameterPresetSlot> Presets;
};

struct VisualLocatorAttachment
{
    FixedString DisplayName;
    FixedString LocatorName;
};

struct VisualSetSlot
{
    FixedString Slot;
    FixedString Visual;
    FixedString field_8;
};

struct VisualSetSlots
{
    // Editor only
    // FixedString StatsColorPresetResource;
    FixedString BodySetVisual;
    Array<VisualSetSlot> VisualSlots;
    Array<VisualLocatorAttachment> LocatorAttachments;
    MaterialParameterPresetsContainer MaterialParameters;
    LegacyMap<FixedString, MaterialParameterPresetsContainer> Materials;
    HashMap<FixedString, FixedString> MaterialOverrides;
    HashMap<FixedString, FixedString> MaterialRemaps;
    FixedString VisualSet;
    bool ShowEquipmentVisuals;
};

struct VisualSetSlotsWrapper : public ProtectedGameObject<VisualSetSlotsWrapper>
{
    VisualSetSlots* Slots;
    bool Managed;
};

struct VisualChangeRequest
{
    EntityHandle Entity;
    FixedString Visual;
    VisualLoadFlags Flags;
};

struct VisualChangeRequestSystem : public BaseSystem
{
    DEFINE_SYSTEM(VisualChange, "ls::VisualChangeRequestSystem")

    // TODO - Not supported yet!
    [[bg3::hidden]] MPMCQueueBounded<VisualChangeRequest> Requests;
};


struct VisualChangedSystem : public BaseSystem
{
    DEFINE_SYSTEM(VisualChanged, "ls::VisualChangedSystem")

    HashSet<EntityHandle> VisualChanges;
    // PORT (linux): upstream's `uint32_t FrameNo;` is not in the native build, and is deleted
    // rather than fenced because make_property_map.py reads through #if and would still emit its
    // P() (BSMoveCheckpoint::field_34 is the precedent). Its registration (bin/bg3 0x32e89c0) is
    // operator new(0x40) at 0x32e8a03 and zeroes 0x10..0x40; the D1 (0x7427170) destroys the
    // set's Keys at 0x30, NextIds at 0x20 and frees its HashKeys at 0x10, and nothing else. macOS
    // af3f1c71 (new(0x40) at 0x106db600b) is the same. A Lua read of FrameNo was the next heap
    // chunk.
};

#if defined(BG3SE_LINUX)
static_assert(sizeof(VisualChangedSystem) == 0x40,
    "operator new(0x40) at bin/bg3 0x32e8a03; D1 0x7427170 ends at the set's Keys, this+0x30");
#endif

struct VisualLoadComponent : public BaseComponent
{
    DEFINE_COMPONENT(VisualLoad, "ls::VisualLoadComponent")

    aio::Priority Priority;
};

struct VisualStreamComponent : public BaseComponent
{
    DEFINE_COMPONENT(VisualStream, "ls::VisualStreamComponent")

    int field_0{ 0 };
    int field_4{ 0 };
    float field_8{ -1.0 };
    bool IsHLOD{ false };
};

struct VisualStreamLoadComponent : public BaseComponent
{
    DEFINE_COMPONENT(VisualStreamLoad, "ls::VisualStreamLoadComponent")

    aio::Priority Priority;
};

struct VisualLoadDesciptionComponent : public BaseComponent
{
    DEFINE_COMPONENT(VisualLoadDescription, "ls::VisualLoadDesciptionComponent")

    FixedString VisualTemplate;
    VisualLoadFlags Flags;
    RenderChannel RenderChannel;
};

struct VisualLoadRequestsSingletonComponent : public BaseComponent
{
    DEFINE_COMPONENT(VisualLoadRequests, "ls::VisualLoadRequestsSingletonComponent")

    HashMap<EntityHandle, bool> Requests;
};

struct VisualChangeRequestOneFrameComponent : public BaseComponent
{
    DEFINE_ONEFRAME_COMPONENT(VisualChangeRequest, "ls::VisualChangeRequestOneFrameComponent")

    FixedString VisualTemplate;
    VisualLoadFlags Flags;
};

struct VisualAttachRequestOneFrameComponent : public BaseComponent
{
    DEFINE_ONEFRAME_COMPONENT(VisualAttachRequest, "ls::VisualAttachRequestOneFrameComponent")

    EntityHandle Entity;
    int field_8;
};


DEFINE_TAG_COMPONENT(ls, VisualLoadedComponent, VisualLoaded)
DEFINE_ONEFRAME_TAG_COMPONENT(ls, VisualChangedEventOneFrameComponent, VisualChangedEvent)

struct SwapVisualRequest
{
    Visual* From;
    Visual* To;
};

struct VisualSetSlotsPreAllocatedVisual
{
    EntityHandle Entity;
    Visual* Visual;
};

struct DeferredLoadRequest : ProtectedGameObject<DeferredLoadRequest>
{
    [[bg3::hidden]] void* Callback_pInternalCallback{ nullptr };
    Visual* Visual;
    resource::AnimationBlueprintResource* Blueprint;
    gn::GenomeBlueprintInstance* BlueprintInstance;
    EntityHandle Entity;
    VisualSetSlotsWrapper Slots;
    HashMap<FixedString, Array<VisualSetSlotsPreAllocatedVisual>> PreAllocatedEntities;
    uint32_t RequestId;
    VisualLoadFlags LoadFlags;
    uint8_t field_7E;
    FixedString BlueprintOverride;
    uint32_t field_80;
    uint32_t RefCount;
};

struct VisualSystem : public BaseSystem
{
    DEFINE_SYSTEM(Visual, "ls::VisualSystem")

    [[bg3::hidden]] UnknownSignal VisualLoadedSignal;
    [[bg3::hidden]] UnknownSignal VisualUnloadedSignal;
    [[bg3::hidden]] UnknownSignal VisualSwappedSignal;
    HashMap<EntityHandle, SwapVisualRequest> SwapVisuals;
    HashMap<EntityHandle, FixedString> RemoveAnimationSetRequests;
    HashMap<EntityHandle, Array<LoadAnimationFromVisualRequestData>> LoadAnimationRequests;
    [[bg3::hidden]] VisualChangedSystem* VisualChangedSystem;
    HashMap<EntityHandle, DeferredLoadRequest*> LoadRequests;
    [[bg3::hidden]] CRITICAL_SECTION VisualSetSlotUpdatesCS;
    [[bg3::hidden]] Array<void*> VisualSetSlotUpdates; // some kind of hashmap?
#if defined(BG3SE_LINUX)
    // PORT (linux): the game's SRW lock, a bare 56-byte pthread_rwlock_t (ext/linux/shim/windows.h).
    // Each of these lock/queue pairs is really one game object -- ls::MPMCQueueUnbounded<T>, whose
    // lock is its own member 0 -- so the lock widening is absorbed by the alignas(64) before the
    // queue and nothing behind these members moves.  Witness block below.
    [[bg3::hidden]] LS_SRWLOCK DeferredLoadRequestsLock;
#else
    [[bg3::hidden]] SRWLOCK DeferredLoadRequestsLock;
#endif
    // TODO - Not supported yet!
    [[bg3::hidden]] MPMCQueueBounded<DeferredLoadRequest*> DeferredLoadRequests;
#if defined(BG3SE_LINUX)
    [[bg3::hidden]] LS_SRWLOCK UnloadRequestsLock;
#else
    [[bg3::hidden]] SRWLOCK UnloadRequestsLock;
#endif
    // TODO - Not supported yet!
    [[bg3::hidden]] MPMCQueueBounded<Visual*> UnloadRequests;
    [[bg3::hidden]] void* UnloadWorkerThreadBatch;
    [[bg3::hidden]] void* UnloadWorkerThreadJob;
    [[bg3::hidden]] void* GenomeManager;
};

#if defined(BG3SE_LINUX)
// PORT (linux): what the game holds here is not a bare lock next to a bounded queue but one
// object, ls::MPMCQueueUnbounded<T>, with the lock as its own member 0 -- and that object has
// the same 0x100-byte shape as the SynchronizedMPMCQueueBounded measured on gn::GenomeManager
// (EntitySystem.h): lock at +0x00, Capacity +0x40, Slots +0x48,
// WriteIndex +0x80, 64-byte slots with the value at slot+8.  So upstream's lock-then-queue pair
// describes it correctly once the lock is the game's width; only the grow path (absent from the
// bounded mirror, and nothing reads these queues) differs.
//   ls::VisualSystem::UnloadVisual, bin/bg3 0x2f55e00 (matched s2:1.00), reaches the second
//     queue at `add rbx, 0x2c0` / `lea rdi, [rax + 0x2c0]` and calls 0x2f56470, matched
//     void ls::MPMCQueueUnbounded<ls::Visual*>::Emplace<ls::Visual*&>(ls::Visual*&).
//   That Emplace takes the lock at the queue's own base: `mov rdi, rbx; call 0x218f6b0` is the
//     shared pthread_rwlock read-acquire (the same helper TwoStepLoadableResource's lock uses)
//     and every exit releases with pthread_rwlock_unlock@plt (0x77e8310); `[rbx + 0x40]` is the
//     capacity it divides by, `[rbx + 0x48]` the slot array, `[rbx + 0x80]` the write index it
//     cmpxchgs, and the grow path passes `rbx + 0x40` -- the {Capacity, Slots} pair -- to
//     0x4503d10 with twice the old capacity.
//   LoadRequests 0x120 is the anchor in front of it: `lea r14, [rdi + 0x120]` in
//     ls::VisualSystem::CancelLoadRequest (0x2f56550, s5:0.83) and `lea rdi, [rbx + 0x120]` in
//     LoadVisualInternal (0x2ee2c40, r5:2).
// The first queue has no direct witness yet; it follows from the second, since the two are
// adjacent and identically shaped.  sizeof and the three pointers behind them are unasserted
// for the same reason -- nothing in the extender reads any of it.
static_assert(offsetof(VisualSystem, LoadRequests) == 0x120);
static_assert(offsetof(VisualSystem, UnloadRequestsLock) == 0x2c0);
// The queue body sits 0x40 past its own lock, which is what makes the widening free here.
static_assert(offsetof(VisualSystem, UnloadRequests) - offsetof(VisualSystem, UnloadRequestsLock) == 0x40,
              "the game's rw-lock fits inside the alignas(64) padding at the head of the queue");
#endif


struct DecalComponent : public BaseProxyComponent
{
    DEFINE_COMPONENT(Decal, "ls::DecalComponent")

    DecalObject* Decal;
};

struct CullComponent : public BaseComponent
{
    DEFINE_COMPONENT(Cull, "ls::CullComponent")

    uint16_t CullFlags;
};

struct SceneAttachComponent : public BaseComponent
{
    DEFINE_COMPONENT(SceneAttach, "ls::SceneAttachComponent")

    [[bg3::hidden]] Scene* Scene;
};

struct CustomIconsStorageSingletonComponent : public BaseComponent
{
    DEFINE_COMPONENT(CustomIconsStorage, "eoc::CustomIconsStorageSingletonComponent")

    HashMap<Guid, ScratchBuffer> Icons;
};

struct FogVolumeRequestComponent : public BaseComponent
{
    DEFINE_COMPONENT(FogVolumeRequest, "eoc::FogVolumeRequestComponent")

    Guid field_0;
};

struct CombinedLightComponent : public BaseComponent
{
    DEFINE_COMPONENT(CombinedLight, "eoc::CombinedLightComponent")

    EntityHandle Entity;
    BYTE TemplateType;
    FixedString Template;
    FixedString InstanceGuid;
    Transform Transform;
};

struct StandardGameObject : public ProtectedGameObject<StandardGameObject>
{
    [[bg3::hidden]] void* VMT;
    [[bg3::hidden]] void* VMT2;
    ecs::EntityRef GameObjectEntity;
    [[bg3::hidden]] void* field_20;
    FixedString Level;
    uint16_t GameObjectFlags;
};

struct [[bg3::component]] ConstructionTile : public StandardGameObject
{
    DEFINE_PROXY_COMPONENT(ConstructionTile, "ls::ConstructionTile")

    EntityHandle Entity;
    EntityHandle field_48;
    ConstructionTileTemplate* Template;
    FixedString ParentTemplate;
    FixedString TileSetResourceId;
    float Scale;
    ConstructionTileFlags Flags;
};

struct [[bg3::component]] ConstructionFilling : public StandardGameObject
{
    DEFINE_PROXY_COMPONENT(ConstructionFilling, "ls::ConstructionFilling")

    Guid InstanceId;
    EntityHandle Entity;
    // Editor only
    // ConstructionFillingTemplate* Template;
    RenderableObject* Renderable;
    RenderableObject* Renderable2;
    FixedString ParentTemplate;
    FixedString FadeGroup;
    FixedString Material;
    FixedString Physics;
    FixedString Id;
    float Tiling;
    bool WalkOn;
    bool SeeThrough;
    bool Fadeable;
    // Editor only
    // bool HierarchyOnlyFade;
};

struct [[bg3::component]] Construction : public ProtectedGameObject<Construction>
{
    DEFINE_PROXY_COMPONENT(Construction, "ls::Construction")

// PORT (linux): upstream's `[[bg3::hidden]] void* field_0;` is not in the native build, and
// dropping it is what takes this mirror from 56 to the registry's 48 ("'ls::Construction' OOB
// size mismatch: local 56, ECS 48" in both worlds with upstream's layout). The
// macOS oracle -- same Itanium ABI, symbols kept -- settles which 8 bytes: ls::Construction's
// ctor (macOS 0x106be8f80) zeroes exactly [this+0x00], [+0x10] and [+0x20], 48 bytes and no
// vtable store anywhere (there is no _ZTVN2ls12ConstructionE either), and ~Construction
// (0x1042effe0) frees the pointer at [this+0x10] after reading its size at [+0x18] and its
// capacity at [+0x1c] -- an Array at 0x10, which is Filling, not Tiles.
    // Editor only
    // [[bg3::hidden]] UnknownSignal field_8;
    Array<EntityHandle> Tiles;
    Array<EntityHandle> Filling;
    Guid InstanceId;
    // Editor only
    // ConstructionTemplate* Template;
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(Construction, Filling) == 0x10, "the macOS dtor frees the array at +0x10");
static_assert(sizeof(Construction) == 48, "ls::Construction is 48 bytes (the game's registry)");
#endif

struct ConstructionSystem : public BaseSystem
{
    // Editor only
    // HashMap<Guid, EntityHandle> Constructions;
    // HashMap<Guid, EntityHandle> Tiles;
    // HashMap<Guid, EntityHandle> Fillings;
    // HashMap<Guid, EntityHandle> ConstructionUpdateRequests;
    [[bg3::hidden]] void* TransformSystem;
    // Editor only
    // [[bg3::hidden]] void* PhysicsLoaderSystem;
    [[bg3::hidden]] void* PhysicsRequestSystem;
    [[bg3::hidden]] void* VisualChangeRequestSystem;
    [[bg3::hidden]] void* OcclusionSystem;
};

DEFINE_TAG_COMPONENT(ls, IsSeeThroughComponent, IsSeeThrough)


struct HLODInstanceData
{
    Guid UUID;
    int Level;
    // Editor only
    // uint64_t VersionHash;
    Array<Guid> Children;
    Array<Guid> Objects;
};

struct HLODComponent : public BaseComponent
{
    DEFINE_COMPONENT(HLOD, "ls::HLODComponent")

    [[bg3::hidden]] LevelBase* Level;
    HLODInstanceData Instance;
    float CullDistanceBias;
    FixedString UUID;
    Array<EntityHandle> Children;
    // Editor only
    // uint64_t RecursiveHash;
    // uint64_t BaseHash;
    // bool Enabled;
};

struct HLODChildComponent : public BaseComponent
{
    DEFINE_COMPONENT(HLODChild, "ls::HLODChildComponent")

    EntityHandle Parent;
    // Editor only
    // uint64_t Hash;
};

struct ClusterBoundComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClusterBound, "ls::ClusterBoundComponent")

    AABound Bounds;
};

struct ClusterBoundMaxComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClusterBoundMax, "ls::ClusterBoundMaxComponent")

    float BoundMax;
};

struct ClusterChildComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClusterChild, "ls::ClusterChildComponent")

    EntityHandle Parent;
};

struct ClusterChildIndexedComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClusterChildIndexed, "ls::ClusterChildIndexedComponent")

    int X;
    int Y;
    int Z;
    int Radius;
};

struct ClusterContainerComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClusterContainer, "ls::ClusterContainerComponent")

    HashMap<glm::ivec4, EntityHandle> Clusters;
};


struct ClusterDistMinComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClusterDistMin, "ls::ClusterDistMinComponent")

    float DistMin;
};

struct ClusterDistMaxComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClusterDistMax, "ls::ClusterDistMaxComponent")

    float DistMax;
};

struct ClusterPositionXComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClusterPositionX, "ls::ClusterPositionXComponent")

    float X;
};

struct ClusterPositionYComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClusterPositionY, "ls::ClusterPositionYComponent")

    float Y;
};

struct ClusterPositionZComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClusterPositionZ, "ls::ClusterPositionZComponent")

    float Z;
};

struct ClusterRadiusComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClusterRadius, "ls::ClusterRadiusComponent")

    float Radius;
};

DEFINE_TAG_COMPONENT(ls, ClusterComponent, Cluster)
DEFINE_TAG_COMPONENT(ls, ClusterAttachRequestComponent, ClusterAttachRequest)

struct LocalBoundComponent : public BaseComponent
{
    DEFINE_COMPONENT(LocalBound, "ls::LocalBoundComponent")

    AABound Bounds;
};

struct OcclusionComponent : public BaseComponent
{
    DEFINE_COMPONENT(Occlusion, "ls::OcclusionComponent")

    [[bg3::hidden]] void* OcclusionModel;
};

END_SE()

BEGIN_NS(ecl)

struct ConstructionSystem : public bg3se::ConstructionSystem
{
    DEFINE_SYSTEM(ClientConstruction, "ecl::ConstructionSystem")

    // PORT (linux): upstream's `bool RequestSetSeeThrough; bool EnableSeeThrough;` are not in
    // the native build's object, and are deleted rather than fenced (make_property_map.py reads
    // through #if). Its registration (bin/bg3 0x43df2a0) is operator new(0x30) at 0x43df2f7 and
    // stores four pointer arguments at 0x10..0x28 (bg3se::ConstructionSystem's four), the vptr
    // and [+8]; macOS af3f1c71 (new(0x30) at 0x10112ec8c) the same. With upstream's members the
    // bools sit at 0x30/0x31, past the allocation, and a Lua write to them writes the next heap
    // chunk.
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(ConstructionSystem, OcclusionSystem) == 0x28,
    "bin/bg3 0x43df33c stores the fourth pointer at this+0x28");
static_assert(sizeof(ConstructionSystem) == 0x30,
    "operator new(0x30) at bin/bg3 0x43df2f7");
#endif

END_NS()

BEGIN_NS(animation)

struct ReceivedEvent
{
    FixedString Event;
    Array<gn::GenomeVariant> Args;
};


struct TextKeyEventValue
{
    FixedString field_0;
    uint8_t field_4;
};


struct TextKeyEventInfo
{
    FixedString TextKey;
    uint8_t field_4;
    std::optional<TextKeyEventValue> field_8;
};


struct TextKeyEventsSingletonComponent : public BaseComponent
{
    DEFINE_COMPONENT(AnimationTextKeyEventsSingleton, "ls::animation::TextKeyEventsSingletonComponent")

    Array<AnimationReceivedTextKeyEvent> Events;
};

struct GameplayEventsSingletonComponent : public BaseComponent
{
    DEFINE_COMPONENT(AnimationGameplayEventsSingleton, "ls::animation::GameplayEventsSingletonComponent")

    Array<AnimationReceivedGameplayEvent> Events;
};

struct GameplayEventsOneFrameComponent : public BaseComponent
{
    DEFINE_ONEFRAME_COMPONENT(AnimationGameplayEvents, "eoc::animation::GameplayEventsOneFrameComponent")

    HashMap<EntityHandle, Array<ReceivedEvent>> Events;
};


struct TextKeyEventsOneFrameComponent : public BaseComponent
{
    DEFINE_ONEFRAME_COMPONENT(AnimationTextKeyEvents, "eoc::animation::TextKeyEventsOneFrameComponent")

    HashMap<EntityHandle, Array<TextKeyEventInfo>> Events;
};


struct TriggeredEventsOneFrameComponent : public BaseComponent
{
    DEFINE_ONEFRAME_COMPONENT(AnimationTriggeredEvents, "eoc::animation::TriggeredEventsOneFrameComponent")

    HashMap<EntityHandle, Array<FixedString>> Events;
};


END_NS()


BEGIN_NS(esv)

struct DisplayName
{
    TranslatedString NameKey;
    uint8_t field_10;
    STDString Name;
};

struct DisplayTitle
{
    TranslatedString NameKey;
    uint8_t field_10;
};

struct DisplayNameListComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerDisplayNameList, "esv::DisplayNameListComponent")

    Array<DisplayName> Names;
    [[bg3::legacy(TranslatedStrings)]] Array<DisplayTitle> Titles;
    [[bg3::legacy(Unknown)]] uint8_t HideTitle;
};

struct IconInfo
{
    FixedString Icon;
    uint32_t field_4;
};

struct IconListComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerIconList, "esv::IconListComponent")

    Array<IconInfo> Icons;
};

struct VisualSystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerVisual, "esv::VisualSystem")

    [[bg3::hidden]] void* ThothMachine;
    [[bg3::hidden]] void* AnimationBlueprintSystem;
    [[bg3::hidden]] void* ShapeshiftSystem;
    [[bg3::hidden]] void* ResourceManager;
    HashSet<EntityHandle> UpdateVisuals;
    HashSet<EntityHandle> BoostsRemoved;
    [[bg3::hidden]] HashMap<EntityHandle, std::optional<TemplateInfo>> CharacterSetVisualResource;
    HashMap<EntityHandle, FixedString> ItemSetVisualResource;
    // Editor only?
    // [[bg3::hidden]] void* field_110;
    // HashSet<EntityHandle> ScaleChange;
    // HashMap<EntityHandle, FixedString> TemplateIDChange;
    [[bg3::hidden]] void* GlobalTemplateManager;
    [[bg3::hidden]] void* CacheTemplateManager;
    [[bg3::hidden]] void* LevelManager;
    [[bg3::hidden]] UnknownFunction field_1A0;
    [[bg3::hidden]] UnknownFunction field_1E0;
};


END_NS()

BEGIN_NS(ecl)


struct EquipmentVisualRequest
{
    Array<FixedString> VisualTemplates;
    [[bg3::legacy(BoneSheathed)]] FixedString Bone;
    [[bg3::legacy(SourceBoneSheathed)]] FixedString SourceBone;
    FixedString Level;
    VisualAttachmentFlags AttachFlags;
    EntityHandle Parent;
    Array<material::ScalarParameterPreset> ScalarParameters;
    Array<material::Vector3ParameterPreset> Vector3Parameters;
    EntityHandle Item;
    VisualLoadFlags VisualFlags;
    uint8_t HairType;
    VisualRequestFlags RequestFlags;
    ItemSlot Slot;
};


struct EquipmentVisualCallbackLoadDesc : public ProtectedGameObject<EquipmentVisualCallbackLoadDesc>
{
    FixedString VisualTemplate;
    EntityHandle VisualEntity;
    [[bg3::hidden]] void* Callback;
    bool Processed;
};


struct EquipmentVisualSlotRequest
{
    Array<EntityHandle> Item;
    Array<EquipmentVisualCallbackLoadDesc*> Callbacks;
    EquipmentVisualRequest Data;
    EntityHandle field_90;
};

struct EquipmentVisualSlot
{
    EntityHandle Item;
    Array<EntityHandle> SubVisuals;
    EquipmentVisualSlotRequest* VisualRequest{ nullptr };
    std::optional<EquipmentVisualRequest> VisualData;
    [[bg3::legacy(field_20)]] bool Loaded{ false };
};

struct EquipmentVisualsComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClientEquipmentVisuals, "ecl::EquipmentVisualsComponent")

    EntityHandle Entity;
    HashMap<ItemSlot, EquipmentVisualSlot> Equipment;
};

struct PaperdollComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClientPaperdoll, "ecl::PaperdollComponent")

    EntityHandle Entity;
    [[bg3::legacy(field_8)]] bool Combat;
};

struct LoadAnimationFromVisualRequestData
{
    FixedString field_0;
    FixedString field_4;
    FixedString field_8;
    bool field_C;
};

struct EquipmentRemoveMaterialRequest
{
    EntityHandle Entity;
    FixedString Material;
    bool field_C;
    bool RemoveOverlayMaterial;
    uint8_t CheckFlags;
};

struct EquipmentUnloadRequest
{
    EntityHandle Parent;
    EntityHandle Visual;
    EntityHandle Item;
    ItemSlot Slot;
};

struct EquipmentEventSlotData
{
    EntityHandle Item;
    uint32_t field_8{ 0 };
};

struct EquipmentVisualsSystem : public BaseSystem
{
    DEFINE_SYSTEM(ClientEquipmentVisuals, "ecl::EquipmentVisualsSystem")

    [[bg3::hidden]] UnknownSignal field_10;
    [[bg3::hidden]] UnknownSignal field_28;
    [[bg3::hidden]] void* EocClient;
    [[bg3::hidden]] void* LevelManager;
    [[bg3::hidden]] void* MaterialManager;
    [[bg3::hidden]] void* LevelInstanceAttachRequestSystem;
    [[bg3::hidden]] void* EquipmentVisualsUnitTestHelper;
    [[bg3::hidden]] UnknownFunction field_68;
    [[bg3::hidden]] UnknownSignalSubscriber field_A8;
    [[bg3::hidden]] UnknownSignalSubscriber field_F8;
    Array<EquipmentRemoveMaterialRequest> RemoveMaterials;
    HashMap<EntityHandle, uint64_t> PendingSlotLoads;
    HashMap<EntityHandle, HashMap<uint16_t, EquipmentEventSlotData>> InventoryEvents;
    Array<EquipmentUnloadRequest> UnloadRequests;
    HashSet<EntityHandle> DyeUpdates;
    [[bg3::readonly]] int StreamingCount;
    [[bg3::readonly]] int UnloadVisualRequestCount;
    [[bg3::hidden]] void* Dispatcher;
    HashMap<EntityHandle, Array<LoadAnimationFromVisualRequestData>> AnimationSetRequest;
    HashMap<EntityHandle, HashSet<FixedString>> RemoveAnimationSetRequest;
    HashSet<EntityHandle> UpdateSplatter;
    HashMap<EntityHandle, FixedString> InitVisualLevel;
    HashMap<EntityHandle, EntityHandle> VFXTargetRequest;
    HashSet<EntityHandle> DestroyVisuals;
};

struct VisualSystem : public BaseSystem
{
    DEFINE_SYSTEM(ClientVisual, "ecl::VisualSystem")

    [[bg3::hidden]] UnknownSignal field_10;
    [[bg3::hidden]] UnknownSignalSubscriber field_28;
    [[bg3::hidden]] UnknownSignalSubscriber field_78;
    [[bg3::hidden]] UnknownSignalSubscriber field_C8;
    [[bg3::hidden]] UnknownSignalSubscriber field_118;
    [[bg3::hidden]] UnknownFunction field_168;
    HashMap<EntityHandle, bool> ReloadVisuals;
    HashMap<EntityHandle, uint32_t> ChangeSplatterState;
    HashSet<EntityHandle> UpdateScale;
    HashMap<EntityHandle, uint32_t> SplatterType;
    bool ReloadAllVisuals;
    [[bg3::hidden]] void* LSVisualSystem;
    [[bg3::hidden]] void* CacheTemplateManager;
    [[bg3::hidden]] void* LevelManager;
    [[bg3::hidden]] void* GlobalTemplateManager;
    [[bg3::hidden]] void* ResourceManager;
};


struct CharacterIconRequestComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClientCharacterIconRequest, "ecl::CharacterIconRequestComponent")

    FixedString Visual;
    VisualSetSlots VisualSet;
    Array<FixedString> Equipment;
    FixedString Template;
    std::optional<ArmorSetState> ArmorSetState;
    EntityHandle field_190;
    STDString Trigger;
    int field_1B0;
};

struct CharacterIconResultComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClientCharacterIconResult, "ecl::CharacterIconResultComponent")

    ScratchBuffer Icon;
};

struct CharacterIconRenderSystem : public BaseSystem
{
    DEFINE_SYSTEM(ClientCharacterIconRender, "ecl::CharacterIconRenderSystem")

    [[bg3::hidden]] UnknownFunction qword10;
    HashSet<EntityHandle> IconRequests;
    HashSet<EntityHandle> DeletedIconRequests;
    int RenderState;
    float RenderTimeRemaining;
    uint32_t ProcessingStage;
    [[bg3::hidden]] void* IconRenderer;
    uint32_t PollResult;
    int field_CC;
    int field_D0;
    int field_D4;
    bool IconRendererSetUp;
    uint8_t field_D9;
    uint8_t field_DA;
    uint8_t field_DB;
    uint16_t field_DC;
    uint8_t field_DE;
    ScratchBuffer Icon;
    EntityHandle EntityBeingRendered;
    EntityHandle field_140;
    uint32_t field_148;
    EntityHandle RenderCallback;
    uint32_t SessionCount;
    [[bg3::hidden]] void* EquipmentVisualsSystem;
};

struct TradeModeChangeRequest
{
    uint16_t field_0;
    uint8_t field_2;
};

struct CharacterManager : public BaseSystem
{
    DEFINE_SYSTEM(ClientCharacterManager, "ecl::CharacterManager")

    [[bg3::hidden]] void* GameEventListenerVMT;
#if !defined(BG3SE_LINUX)
    [[bg3::hidden]] void* field_18;
#endif
// PORT (linux): upstream's `[[bg3::hidden]] void* field_18;` is not in the native build -- the
// Win32 slot ecl::TimelineSystem and esv::combat::System carried. The macOS oracle's ctor,
// inlined into ecl::RegisterSystems (0x101142f23,
// build af3f1c71), allocates 0x6f0 bytes, writes the vtable pointers at 0, 0x10 and 0x18 only,
// zeroes 0x20..0x80 for the four Signal<> (its dtor frees their Connections at 0x28 ... 0x70)
// and stores the seventeen system pointers at 0x80 ... 0x100. Upstream's offset names
// (`field_28` ... `field_6B0`) are the Win32 layout, where the slot does exist.
    [[bg3::hidden]] void* GameStateEventListenerVMT;
    [[bg3::hidden]] UnknownSignal field_28;
    [[bg3::hidden]] UnknownSignal field_40;
    [[bg3::hidden]] UnknownSignal field_58;
    [[bg3::hidden]] UnknownSignal field_70;
    [[bg3::hidden]] void* EntityManager;
    [[bg3::hidden]] void* AnimationBlueprintSystem;
    [[bg3::hidden]] void* LSVisualSystem;
    [[bg3::hidden]] void* LEDSystem;
    [[bg3::hidden]] void* FactionContainer;
    [[bg3::hidden]] void* EoCGlobalSwitches;
    [[bg3::hidden]] void* ResourceManager;
    [[bg3::hidden]] void* GameAnalyticsSystem;
    [[bg3::hidden]] void* StatusSystem;
    [[bg3::hidden]] void* TransformSystem;
    [[bg3::hidden]] void* StatsSystem;
    [[bg3::hidden]] void* LevelManager;
    [[bg3::hidden]] void* HUB;
    [[bg3::hidden]] void* CharacterSoundSystem;
    [[bg3::hidden]] void* GameActionManager;
    [[bg3::hidden]] void* PlayerManager;
    [[bg3::hidden]] void* SpellPrototypeManager;
    [[bg3::hidden]] Array<void*> field_110;
    [[bg3::hidden]] Array<void*> field_120;
    HashSet<EntityHandle> RefreshLights;
    HashSet<EntityHandle> UpdatePhysicsGroup;
    HashMap<EntityHandle, HashSet<ItemSlot>> RefreshEquipmentSlot;
    HashSet<EntityHandle> UpdateObscurity;
    HashSet<EntityHandle> ReloadVisuals;
    HashSet<EntityHandle> ReloadPhysics;
    HashMap<EntityHandle, TradeModeChangeRequest> TradingModeChanges;
    HashSet<EntityHandle> ChangedTemplates;
    [[bg3::hidden]] UnknownSignalSubscriber field_2D0;
    Array<EntityHandle> UpdateCombatCamera;
    std::array<HashSet<EntityHandle>, 3> UpdateCharacterHighlights;
    std::array<HashSet<EntityHandle>, 3> UpdateCharacterHighlights2;
    HashSet<EntityHandle> UpdateRepose;
    [[bg3::hidden]] UnknownSignalSubscriber field_480;
    [[bg3::hidden]] UnknownSignalSubscriber field_4D0;
    [[bg3::hidden]] UnknownSignalSubscriber field_520;
    [[bg3::hidden]] UnknownSignalSubscriber field_570;
    [[bg3::hidden]] UnknownSignalSubscriber field_5C0;
    [[bg3::hidden]] UnknownSignalSubscriber field_610;
    [[bg3::hidden]] UnknownSignalSubscriber field_660;
    [[bg3::hidden]] UnknownFunction field_6B0;
    bool PauseState;
    bool RestoreCharacters;
    bool ReloadVisualSets;
    uint8_t ColorBlindMode;
};

#if defined(BG3SE_LINUX)
// Lua read this system as nil because 'UpdatePhysicsGroup' failed validation.
static_assert(offsetof(CharacterManager, field_28) == 0x20,
    "the macOS ctor zeroes 0x20..0x80 and its dtor frees the Connections at 0x28, 0x103a60228");
static_assert(offsetof(CharacterManager, EntityManager) == 0x80,
    "the first of seventeen pointer stores, macOS 0x101143005");
static_assert(offsetof(CharacterManager, RefreshLights) == 0x128,
    "macOS dtor ~HashTable on this+0x128, 0x103a6005a");
static_assert(offsetof(CharacterManager, UpdatePhysicsGroup) == 0x158,
    "UpdatePhysicsGroup walks Keys at this+0x178 (+0x20), bin/bg3 0x659d038");
static_assert(offsetof(CharacterManager, UpdateCharacterHighlights2) == 0x3B8,
    "ProcessCharacterHightlightsUpdateRequests: this+i*0x30+0x3b8, bin/bg3 0x659cbf3");
static_assert(offsetof(CharacterManager, ColorBlindMode) == 0x6EB,
    "Global+0x7a8 stored to this+0x6eb, bin/bg3 0x659cc1d");
static_assert(sizeof(CharacterManager) == 0x6F0,
    "operator new(0x6f0) in ecl::RegisterSystems, macOS 0x101142f23");
#endif

struct CharacterLightDesc
{
    EntityHandle Entity;
    TLAutomatedLight* TimelineLight;
};

struct CharacterLightComponent : public BaseComponent
{
    DEFINE_COMPONENT(CharacterLight, "ecl::CharacterLightComponent")

    bool Enabled;
    uint8_t LightChannel;
    uint8_t field_2;
    uint8_t field_3;
    uint8_t field_4;
    uint8_t field_5;
    uint8_t field_6;
    uint8_t field_7;
    Array<HashMap<Guid, Array<CharacterLightDesc>>> Lights;
};

struct CharacterLightViewData
{
    float SwapTime;
    float field_4;
    float Time;
    float field_C;
    Guid TargetAtmosphere;
    Guid PreviousAtmosphere;
};

struct CharacterLightSingletonComponent : public BaseComponent
{
    DEFINE_COMPONENT(CharacterLightSingleton, "ecl::CharacterLightSingletonComponent")

    std::array<CharacterLightViewData, 2> Views;
    [[bg3::hidden]] void* Scene; // TLScene*
};


END_NS()

BEGIN_NS(ecl::equipment)

struct EquipmentSlotVisualState
{
    EntityHandle Item;
    uint8_t field_8;
    uint8_t field_9;
};

struct VisualsVisibilityStateComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClientEquipmentVisibilityState, "ecl::equipment::VisualsVisibilityStateComponent")

    HashMap<ItemSlot, EquipmentSlotVisualState> Equipment;
    uint8_t field_40;
    uint8_t field_41;
};

struct VisualsDesiredStateComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClientVisualsDesiredState, "ecl::equipment::VisualsDesiredStateComponent")

    HashMap<ItemSlot, EquipmentVisualRequest> Slots;
};

struct VisualsVisibilityStateSystem : public BaseSystem
{
    DEFINE_SYSTEM(ClientVisualsVisibilityState, "ecl::equipment::VisualsVisibilityStateSystem")

    HashMap<EntityHandle, bool> SetVisualState;
    HashSet<EntityHandle> UnloadVisuals;
    [[bg3::hidden]] void* CacheTemplateManager;
    [[bg3::hidden]] void* LevelManager;
    [[bg3::hidden]] void* GlobalTemplateManager;
    [[bg3::hidden]] void* ResourceManager;
    [[bg3::hidden]] void* VisualsVisibilityStateUnitTestHelper;
};

END_NS()

BEGIN_NS(esv::splatter)

struct SplatterState
{
    float Blood;
    float Bruises;
    float Dirt;
    float Sweat;
};

struct BaseStateComponent : public BaseComponent
{
    DEFINE_COMPONENT(SplatterBaseState, "esv::splatter::BaseStateComponent")

    SplatterState State;
};

struct StateComponent : public BaseComponent
{
    DEFINE_COMPONENT(SplatterState, "eoc::splatter::StateComponent")

    SplatterState State;
    glm::vec3 Translate;
};

struct SweatChangeComponent : public BaseComponent
{
    DEFINE_COMPONENT(SplatterSweatChange, "eoc::splatter::SweatChangeComponent")

    float Sweat;
};

END_NS()
