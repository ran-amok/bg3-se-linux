#pragma once

BEGIN_SE()

struct SoundStateSync
{
    FixedString Group;
    FixedString State;
    int field_8;
};

struct SoundRTPCSync
{
    FixedString Name;
    float Value;
    int field_8;
};

struct ResourceSetTriggerData : public ITriggerData
{
    Array<FixedString> Atmosphere;
    float FadeTime;
};

struct CameraLockTriggerData : public ITriggerData
{
    bool TriggerCameraLockPos;
    uint32_t TriggerGameCameraBehavior;
};

struct EventTriggerData : public ITriggerData
{
    FixedString EnterEvent;
    FixedString LeaveEvent;
};

struct ExplorationTriggerData : public ITriggerData
{
    Guid ExplorationReward;
    int LevelOverride;
};

struct MusicOriginSound
{
    FixedString OriginTheme;
    FixedString OriginOnly;
    bool OriginThemeAddInstrument;
};

struct MusicVolumeEvent
{
    bool TriggerOnEnter;
    bool TriggerOnlyOnce;
    bool IsStinger;
    FixedString MusicEvent;
    FixedString Bansuri;
    FixedString Cello;
    FixedString Tambura;
    FixedString Ud;
    MusicOriginSound OriginSound;
};

struct NavigationCloudTriggerData : public ITriggerData
{
    int GenerateType;
};

struct PointSoundTriggerDummyData : public ITriggerData
{
    FixedString ParentTrigger;
};

struct RegionTriggerData : public ITriggerData
{
    bool RegionCameraLock;
    bool RegionCameraLockPos;
    bool RegionMaskRegionInWorld;
};

struct SpectatorTriggerData : public ITriggerData
{
    float FOV;
    float Speed;
    float DOFFocalDistance;
    float DOFAperature;
    float DOFNearOnly;
    float DOFFarOnly;
    float DOFNearSharpDistance;
    float DOFFarSharpDistance;
    bool UseDOF;
    bool Lerp;
};

struct StartTriggerData : public ITriggerData
{
    uint8_t Player;
    float Angle;
};

struct StatsAreaTriggerData : public ITriggerData
{
    Guid AreaLevelOverrideGuid;
};

struct TeleportTriggerData : public ITriggerData
{
    float Angle;
    bool Zoom;
};

struct CinematicArenaTriggerData : public ITriggerData
{
    HashSet<Guid> Timeline;
    char MainMenuStartEventType;
    char MainMenuResetEventType;
    char MainMenuActivateCameraEventType;
};

struct MusicVolumeTriggerData : public ITriggerData
{
    Array<MusicVolumeEvent> Events;
};

struct PointSoundTriggerData : public ITriggerData
{
    FixedString SoundEvent;
    float AttenuationScale;
    float MixdownDelayTime;
    uint8_t MixdownSpeed;
    int16_t MaxAttenuation;
    int16_t CustomMixRadius;
    bool UseOcclusion;
    Array<glm::vec3> Dummies;
};

struct FloorTriggerData : public ITriggerData
{
    bool IsRoof;
    int Floor;
    FixedString BuildingUUID;
    Array<FixedString> TriggersToHide;
    Array<FixedString> AttachedPortal;
};

struct RoomTriggerData : public ITriggerData
{
    uint8_t RenderMethod;
    Array<FixedString> AttachedPortals;
    bool OneShotTrigger;
    float Border;
};

struct TimelineSceneTriggerData : public ITriggerData
{
    HashSet<Guid> Timelines;
    bool Optional;
    bool IsWorldTimeline;
    bool ShouldPreloadWorldTimelineActors;
    bool TriggerWorldTimelineOnSkillPrepare;
    bool IsUsingCalculatedSceneBounds;
};

struct DefaultSyncedTriggerData : public ITriggerData
{
};

struct AiHintAreaTriggerData : public ITriggerData
{
    bool IsAnchor;
};

struct CrowdCharacterTriggerSpawnTemplate
{
    FixedString CharacterTemplateId;
    float SpawnWeight;
};

struct CrowdCharacterCustomAnimation
{
    FixedString AnimationShortName;
    float Weight;
};

struct CrowdCharacterDynamicAnimationTag
{
    Guid Tag;
    float Weight;
};

struct CrowdCharacterTriggerData : public ITriggerData
{
    bool SpawningEnabled;
    bool ProxiesOnly;
    bool ReversePatrolDirection;
    int16_t MaxCharacters;
    int16_t GroupSizeMin;
    int16_t GroupSizeMax;
    float GroupSpawnTimeMin;
    float GroupSpawnTimeMax;
    float PositionPerturbation;
    float YawPerturbationDegrees;
    float CharacterSpacing;
    float AnimationSelectionTimeMin;
    float AnimationSelectionTimeMax;
    float PatrolWalkingPercentage;
    float DetourChance;
    float DetourMinWaitTime;
    float DetourMaxWaitTime;
    Array<CrowdCharacterTriggerSpawnTemplate> SpawnTemplates;
    Array<FixedString> DetourPatrolSplines;
    Array<FixedString> DispersePatrolSplines;
    Array<FixedString> PatrolSplines;
    Array<FixedString> PointsOfInterest;
    Array<CrowdCharacterCustomAnimation> CustomAnimations;
    Array<CrowdCharacterDynamicAnimationTag> DynamicAnimationTags;
};

struct CrimeAreaTriggerData : public ITriggerData
{
    int16_t CrimeArea;
};

struct CrimeRegionTriggerData : public ITriggerData
{
    FixedString Region;
};

struct ChasmSeederTriggerData : public ITriggerData
{
    int32_t MaxChasmTiles;
    float MaxChasmHeightOffset;
    Array<FixedString> Triggers;
};

struct ChasmRegionTriggerData : public ITriggerData
{
    Array<FixedString> Triggers;
};

struct CampRegionTriggerData : public ITriggerData
{
};

struct CampChestTriggerData : public ITriggerData
{
    uint8_t Index;
};

struct Trigger : public ProtectedGameObject<Trigger>
{
    [[bg3::hidden]] void* VMT;
    [[bg3::hidden]] void* LoggableObjectVMT;
    TriggerTemplate* Template;
    uint16_t Flags;
    uint16_t SyncFlags;
    FixedString TriggerType;
    ecs::EntityRef TriggerEntity;
    FixedString Level;
    FixedString PreviousLevel;
    [[bg3::hidden]] void* Protocol;
    bool HasCustomPoint;
    bool IsGlobal;
    Transform CustomPointTransform;
};

struct AreaTrigger : public Trigger
{
};

struct PointTrigger : public Trigger
{
};

struct FloorTrigger : public AreaTrigger
{
    bool IsRoof;
    int Floor;
    FixedString BuildingUUID;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the Itanium tail-padding placement, measured on bin/bg3 itself rather than on the
// macOS oracle. esv::FloorTrigger's constructor (bin/bg3 0x357a550 -- the one `lea` in .text whose
// target is the vtable run at 0x7a7b038, the run that holds the matched
// esv::FloorTrigger::GetTypeName 0x6f85370) lays the whole object out past the base in three
// stores: `mov byte [r14+0x6c], 0` (IsRoof), `movabs rcx, 0xffffffff00000000` + `mov qword
// [r14+0x70], rcx` (Floor = 0 at 0x70, BuildingUUID = the null FixedString 0xffffffff at 0x74 --
// the same -1 the base writes to Level/PreviousLevel) and `mov qword [r14+0x78],
// 0xffc0000000000000`, which is the *derived* component's Entity, so this record ends at 0x78.
// The base's own last store is `movsd [r14+0x64]`: ls::Trigger's data ends at 0x6c and IsRoof sits
// inside its tail padding. MSVC would give 0x70 / 0x74 / 0x78 and sizeof 0x80.
// ecl::FloorTrigger's ctor (bin/bg3 0x379add0) is the same function instruction for instruction.
static_assert(offsetof(FloorTrigger, IsRoof) == 0x6C, "bin/bg3 0x357a604");
static_assert(offsetof(FloorTrigger, Floor) == 0x70, "bin/bg3 0x357a609");
static_assert(offsetof(FloorTrigger, BuildingUUID) == 0x74, "bin/bg3 0x357a609");
static_assert(sizeof(FloorTrigger) == 0x78, "the derived Entity store at bin/bg3 0x357a60d");
#endif

struct TimelineSceneTrigger : public AreaTrigger
{
    HashSet<Guid> DialogueResourceIds;
    bool Optional;
    bool DestroyAfterTimeline;
    bool IsWorldTimeline;
    bool ShouldPreloadWorldTimelineActors;
    bool UseSceneBounds;
    bool TriggerWorldTimelineOnSkillPrepare;
    bool FloodSuccess;
};

struct LightingTrigger : public AreaTrigger
{
    Array<FixedString> LightingResourceIDs;
    FixedString CurrentLightingResourceID;
    float field_84;
};

struct SoundVolumeTrigger : public AreaTrigger
{
    [[bg3::hidden]] __int64 field_70;
    Array<SoundStateSync> States;
    [[bg3::hidden]] __int64 field_88;
    Array<SoundRTPCSync> RTPCs;
    [[bg3::hidden]] __int64 field_A0;
};

struct AtmosphereTrigger : public AreaTrigger
{
    Array<FixedString> AtmosphereResourceIDs;
    FixedString CurrentAtmosphereResourceID;
    float field_84;
};

struct CullTrigger : public AreaTrigger
{
};

struct FadeTrigger : public AreaTrigger
{
};

struct RoomTrigger : public AreaTrigger
{
};

struct PortalTrigger : public AreaTrigger
{
};

struct CameraBlockerTrigger : public AreaTrigger
{
    Array<FixedString> field_70;
    FixedString field_80;
    float field_84;
};

END_SE()

BEGIN_NS(trigger)

struct PhysicsBoundsInfo
{
    float AreaXZ;
    float AreaExp;
    uint64_t field_8;
    uint32_t field_10;
    glm::vec3 BoundsMin;
    glm::vec3 BoundsMax;
    float Area;
    float AreaXZExp;
    glm::vec3 Position;
    glm::vec3 field_40;
    glm::vec3 field_4C;
};

struct NonePhysics
{
};

struct SpherePhysics
{
    float Radius;
};

struct BoxPhysics
{
    glm::vec3 Extents;
};

struct PolyPhysics
{
    float Height;
    Array<glm::vec2> Points;
};

struct AreaComponent : public BaseComponent
{
    DEFINE_COMPONENT(TriggerArea, "ls::trigger::AreaComponent")

    std::variant<NonePhysics, BoxPhysics, PolyPhysics, SpherePhysics> Physics;
    PhysicsBoundsInfo Bounds;
    glm::quat RotationInv;
};

struct ContainerEvent
{
    bool Left;
    Guid Trigger;
    Guid Object;
};

struct TriggerTypeComponent : public BaseComponent
{
    DEFINE_COMPONENT(TriggerType, "eoc::trigger::TypeComponent")

    uint8_t Type;
};

struct ContainerComponent : public BaseComponent
{
    DEFINE_COMPONENT(TriggerContainer, "ls::trigger::ContainerComponent")

    HashSet<Guid> field_0;
    Array<ContainerEvent> Events;
};

struct IsInsideOfComponent : public BaseComponent
{
    DEFINE_COMPONENT(TriggerIsInsideOf, "ls::trigger::IsInsideOfComponent")

    Array<Guid> InsideOf;
};

struct UpdatedContainerComponent : public BaseComponent
{
    DEFINE_COMPONENT(TriggerUpdatedContainer, "ls::trigger::UpdatedContainerComponent")

    Array<EntityHandle> Updates;
};

struct UpdatedPhysicsComponent : public BaseComponent
{
    DEFINE_COMPONENT(TriggerUpdatedPhysics, "ls::trigger::UpdatedPhysicsComponent")

    Array<EntityHandle> Updates;
};

END_NS()

BEGIN_NS(esv)

struct AreaTriggerBase : public AreaTrigger
{
    EntityHandle Entity;
};

struct PointTriggerBase : public PointTrigger
{
    EntityHandle Entity;
};

struct [[bg3::component]] StatsAreaTrigger : public AreaTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerStatsAreaTrigger, "esv::StatsAreaTrigger")

    Guid AreaLevelOverrideGuid;
};

struct [[bg3::component]] SoundVolumeTrigger : public bg3se::SoundVolumeTrigger
{
    DEFINE_PROXY_COMPONENT(ServerSoundVolumeTrigger, "esv::SoundVolumeTrigger")

// PORT (linux): upstream's `[[bg3::hidden]] __int64 field_A8;` is not in the native build. Witnessed twice
// over: the game's component registry says "'esv::SoundVolumeTrigger' OOB size mismatch: local 184,
// ECS 176" of upstream's layout, and the macOS oracle -- same Itanium ABI,
// and it keeps its symbols -- shows why. esv::SoundVolumeTrigger::SoundVolumeTrigger(ls::GameObjectTemplate const*)
// (macOS 0x105915bd0)
// stores the null EntityHandle at [this+0xa8] as its last write and returns, so Entity is at
// 0xa8 and the object ends at 0xb0: there is no member between the base and it. The base
// itself is right (the same ctor lays ls::Trigger out at 0x00..0x6c and the ls:: trigger's
// last byte at 0xa6, which is clang's 112 and 168 here exactly).
    EntityHandle Entity;
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(SoundVolumeTrigger, Entity) == 0xA8, "the macOS ctor's last store");
static_assert(sizeof(SoundVolumeTrigger) == 176, "esv::SoundVolumeTrigger is 176 bytes (the game's registry)");
#endif

struct [[bg3::component]] AIHintAreaTrigger : public AreaTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerAIHintAreaTrigger, "esv::AIHintAreaTrigger")

    bool IsAnchor;
};

struct [[bg3::component]] CrowdCharacterTrigger : public AreaTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerCrowdCharacterTrigger, "esv::CrowdCharacterTrigger")

    bool ReversePatrolDirection;
    bool SpawningEnabled;
    bool ProxiesOnly;
    int16_t MaxCharacters;
    int16_t GroupSizeMin;
    int16_t GroupSizeMax;
    float GroupSpawnTimeMin;
    float GroupSpawnTimeMax;
    float PositionPerturbation;
    float YawPerturbationDegrees;
    float CharacterSpacing;
    float AnimationSelectionTimeMin;
    float AnimationSelectionTimeMax;
    float PatrolWalkingPercentage;
    float DetourChance;
    float DetourMinWaitTime;
    float DetourMaxWaitTime;
    Array<CrowdCharacterTriggerSpawnTemplate> SpawnTemplates;
    Array<FixedString> DispersePatrolSplines;
    Array<FixedString> DetourPatrolSplines;
    Array<FixedString> PatrolSplines;
    Array<FixedString> PointsOfInterest;
    Array<CrowdCharacterCustomAnimation> CustomAnimations;
    Array<CrowdCharacterDynamicAnimationTag> DynamicAnimationTags;
};

struct [[bg3::component]] ChasmSeederTrigger : public PointTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerChasmSeederTrigger, "esv::ChasmSeederTrigger")

    int32_t MaxChasmTiles;
    float MaxChasmHeightOffset;
    Array<FixedString> Triggers;
};

struct [[bg3::component]] TeleportTrigger : public PointTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerTeleportTrigger, "esv::TeleportTrigger")

    float Angle;
    bool Zoom;
};

struct [[bg3::component]] CampRegionTrigger : public AreaTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerCampRegionTrigger, "esv::CampRegionTrigger")
};

struct [[bg3::component]] EocAreaTrigger : public AreaTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerEocAreaTrigger, "esv::EocAreaTrigger")
};

struct [[bg3::component]] StartTrigger : public PointTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerStartTrigger, "esv::StartTrigger")

    uint8_t Player;
    float Angle;
};

struct [[bg3::component]] AtmosphereTrigger : public bg3se::AtmosphereTrigger
{
    DEFINE_PROXY_COMPONENT(ServerAtmosphereTrigger, "esv::AtmosphereTrigger")

    EntityHandle Entity;
};

struct [[bg3::component]] CrimeAreaTrigger : public AreaTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerCrimeAreaTrigger, "esv::CrimeAreaTrigger")

    int MinimumTension;
    FixedString field_7C;
    __int64 field_80;
};

struct [[bg3::component]] CrimeRegionTrigger : public AreaTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerCrimeRegionTrigger, "esv::CrimeRegionTrigger")

    FixedString Region;
};

struct [[bg3::component]] EventTrigger : public AreaTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerEventTrigger, "esv::EventTrigger")

    FixedString EnterEvent;
    FixedString LeaveEvent;
};

struct [[bg3::component]] FloorTrigger : bg3se::FloorTrigger
{
    DEFINE_PROXY_COMPONENT(ServerFloorTrigger, "esv::FloorTrigger")

    EntityHandle Entity;
};

#if defined(BG3SE_LINUX)
// The pair for a component: the ctor says where, the game's own registry says how much.
// esv::FloorTrigger's ctor (bin/bg3 0x357a60d) writes the null EntityHandle 0xffc0000000000000
// to [this+0x78] as its last member store, and the component registrant (bin/bg3 0x4bb1b28,
// `mov dword [rsp+0x20], 0x880008` into ComponentTypeEntry's InlineSize/TotalSize pair) declares
// InlineSize 8 / TotalSize 0x88 -- a proxy, so the out-of-band object is 0x88 - 8 = 128 bytes,
// which is what ValidateMappedComponentSize() compares sizeof(this) against. MSVC would put
// Entity at 0x80 and make the record 136.
static_assert(offsetof(FloorTrigger, Entity) == 0x78, "bin/bg3 0x357a60d");
static_assert(sizeof(FloorTrigger) == 128, "esv::FloorTrigger is 128 bytes (the game's registry, bin/bg3 0x4bb1b28)");
#endif

struct [[bg3::component]] PortalTrigger : public bg3se::PortalTrigger
{
    DEFINE_PROXY_COMPONENT(ServerPortalTrigger, "esv::PortalTrigger")

    EntityHandle Entity;
};

struct [[bg3::component]] RoomTrigger : public bg3se::RoomTrigger
{
    DEFINE_PROXY_COMPONENT(ServerRoomTrigger, "esv::RoomTrigger")

    EntityHandle Entity;
    uint8_t field_78;
};

struct [[bg3::component]] AvatarContainerTrigger : public PointTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerAvatarContainerTrigger, "esv::AvatarContainerTrigger")
};

struct [[bg3::component]] CampChestTrigger : public PointTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerCampChestTrigger, "esv::CampChestTrigger")

    uint8_t Index;
};

struct [[bg3::component]] ChasmRegionTrigger : public AreaTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerChasmRegionTrigger, "esv::ChasmRegionTrigger")

    Array<FixedString> Triggers;
};

struct [[bg3::component]] ExplorationTrigger : public AreaTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerExplorationTrigger, "esv::ExplorationTrigger")

    Guid ExplorationReward;
    int LevelOverride;
};

struct [[bg3::component]] MusicVolumeTrigger : public AreaTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerMusicVolumeTrigger, "esv::MusicVolumeTrigger")

    Array<MusicVolumeEvent> Events;
};

struct MusicVolumeTriggerState
{
    uint16_t PeerId;
    uint32_t Index;
};

struct MusicVolumeTriggerStateComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerMusicVolumeTriggerState, "esv::MusicVolumeTriggerStateComponent")

    Array<MusicVolumeTriggerState> Triggered;
};

struct [[bg3::component]] TimelineSceneTrigger : public bg3se::TimelineSceneTrigger
{
    DEFINE_PROXY_COMPONENT(ServerTimelineSceneTrigger, "esv::TimelineSceneTrigger")

// PORT (linux): upstream's `[[bg3::hidden]] void* field_A8;` is not in the native build. Witnessed twice
// over: the game's component registry says "'esv::TimelineSceneTrigger' OOB size mismatch: local 184,
// ECS 176" of upstream's layout, and the macOS oracle -- same Itanium ABI,
// and it keeps its symbols -- shows why. esv::TimelineSceneTrigger::TimelineSceneTrigger(ls::GameObjectTemplate const*)
// (macOS 0x1065d93a0)
// stores the null EntityHandle at [this+0xa8] as its last write and returns, so Entity is at
// 0xa8 and the object ends at 0xb0: there is no member between the base and it. The base
// itself is right (the same ctor lays ls::Trigger out at 0x00..0x6c and the ls:: trigger's
// last byte at 0xa6, which is clang's 112 and 168 here exactly).
    EntityHandle Entity;
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(TimelineSceneTrigger, Entity) == 0xA8, "the macOS ctor's last store");
static_assert(sizeof(TimelineSceneTrigger) == 176, "esv::TimelineSceneTrigger is 176 bytes (the game's registry)");
#endif

struct [[bg3::component]] BlockBronzeTimelinePlacementTrigger : public AreaTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerBlockBronzeTimelinePlacementTrigger, "esv::BlockBronzeTimelinePlacementTrigger")
};

struct [[bg3::component]] AiGridAreaTrigger : public AreaTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerAiGridAreaTrigger, "esv::AiGridAreaTrigger")
};

struct [[bg3::component]] RegionTrigger : public AreaTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerRegionTrigger, "esv::RegionTrigger")

    bool RegionMaskRegionInWorld;
};

struct [[bg3::component]] LightingTrigger : public bg3se::LightingTrigger
{
    DEFINE_PROXY_COMPONENT(ServerLightingTrigger, "esv::LightingTrigger")

    EntityHandle Entity;
};

struct [[bg3::component]] EocPointTrigger : public PointTriggerBase
{
    DEFINE_PROXY_COMPONENT(ServerEocPointTrigger, "esv::EocPointTrigger")
};

END_NS()

BEGIN_NS(esv::trigger)

struct EventConfigComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerTriggerEventConfig, "esv::trigger::EventConfigComponent")

    uint8_t Config;
};

struct RegisteredForComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerTriggerRegisteredFor, "esv::trigger::RegisteredForComponent")

    HashSet<Guid> RegisteredFor;
};

struct RegistrationSettingsComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerRegistrationSettings, "esv::trigger::RegistrationSettingsComponent")

    bool Registered;
};

struct UpdatedRegisteredForComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerUpdatedRegisteredFor, "esv::trigger::UpdatedRegisteredForComponent")

    Array<EntityHandle> Updated;
};

struct UpdatedRegisteredForItemsComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerUpdatedRegisteredForItems, "esv::trigger::UpdatedRegisteredForItemsComponent")

    Array<EntityHandle> Updated;
};

struct CachedLeaveEventData
{
    Guid Object;
    FixedString Event;
    uint8_t field_14;
};

struct CachedLeaveEventsComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerTriggerCachedLeaveEvents, "esv::trigger::CachedLeaveEventsComponent")

    Array<CachedLeaveEventData> Updated;
};

DEFINE_TAG_COMPONENT(esv::trigger, LoadedHandledComponent, ServerTriggerLoadedHandled)
DEFINE_TAG_COMPONENT(esv::trigger, TriggerWorldAutoTriggeredComponent, ServerTriggerWorldAutoTriggered)

END_NS()

BEGIN_NS(ecl)

struct [[bg3::component]] SoundVolumeTrigger : public bg3se::SoundVolumeTrigger
{
    DEFINE_PROXY_COMPONENT(ClientSoundVolumeTrigger, "ecl::SoundVolumeTrigger")
    
    [[bg3::hidden]] void* SoundStateSyncChangedListenerVMT;
    [[bg3::hidden]] void* SoundRTPCSyncChangedListenerVMT;
    EntityHandle Entity;
    bool Enabled;
    bool IsInternal;
    bool Loaded;
    [[bg3::hidden]] void* OnSoundActivatedListener;
    [[bg3::hidden]] void* OnSoundDeactivatedListener;
    [[bg3::hidden]] void* OnSoundVolumeEnterListener;
    [[bg3::hidden]] void* OnSoundVolumeLeaveListener;
};

struct [[bg3::component]] AtmosphereTrigger : public bg3se::AtmosphereTrigger
{
    DEFINE_PROXY_COMPONENT(ClientAtmosphereTrigger, "ecl::AtmosphereTrigger")

    EntityHandle Entity;
};

struct [[bg3::component]] FloorTrigger : bg3se::FloorTrigger
{
    DEFINE_PROXY_COMPONENT(ClientFloorTrigger, "ecl::FloorTrigger")

    EntityHandle Entity;
};

#if defined(BG3SE_LINUX)
// The pair for a component: the ctor says where, the game's own registry says how much.
// ecl::FloorTrigger's ctor (bin/bg3 0x379ae8d) writes the null EntityHandle 0xffc0000000000000
// to [this+0x78] as its last member store, and the component registrant (bin/bg3 0x4785308,
// `mov dword [rsp+0x20], 0x880008` into ComponentTypeEntry's InlineSize/TotalSize pair) declares
// InlineSize 8 / TotalSize 0x88 -- a proxy, so the out-of-band object is 0x88 - 8 = 128 bytes,
// which is what ValidateMappedComponentSize() compares sizeof(this) against. MSVC would put
// Entity at 0x80 and make the record 136.
static_assert(offsetof(FloorTrigger, Entity) == 0x78, "bin/bg3 0x379ae8d");
static_assert(sizeof(FloorTrigger) == 128, "ecl::FloorTrigger is 128 bytes (the game's registry, bin/bg3 0x4785308)");
#endif

struct [[bg3::component]] PortalTrigger : public bg3se::PortalTrigger
{
    DEFINE_PROXY_COMPONENT(ClientPortalTrigger, "ecl::PortalTrigger")

    EntityHandle Entity;
};

struct [[bg3::component]] RoomTrigger : public bg3se::RoomTrigger
{
    DEFINE_PROXY_COMPONENT(ClientRoomTrigger, "ecl::RoomTrigger")

    EntityHandle Entity;
    uint8_t RenderMethod;
    uint8_t RenderEffect;
    float Border;
};

struct [[bg3::component]] TimelineSceneTrigger : public bg3se::TimelineSceneTrigger
{
    DEFINE_PROXY_COMPONENT(ClientTimelineSceneTrigger, "ecl::TimelineSceneTrigger")

// PORT (linux): upstream's `uint64_t field_A8;` is not in the native build. Witnessed twice
// over: the game's component registry says "'ecl::TimelineSceneTrigger' OOB size mismatch: local 184,
// ECS 176" of upstream's layout, and the macOS oracle -- same Itanium ABI,
// and it keeps its symbols -- shows why. ecl::TimelineSceneTrigger::TimelineSceneTrigger(ls::GameObjectTemplate const*)
// (macOS 0x10385e0d0)
// stores the null EntityHandle at [this+0xa8] as its last write and returns, so Entity is at
// 0xa8 and the object ends at 0xb0: there is no member between the base and it. The base
// itself is right (the same ctor lays ls::Trigger out at 0x00..0x6c and the ls:: trigger's
// last byte at 0xa6, which is clang's 112 and 168 here exactly).
// Deleted rather than #if'd out because this one is not [[bg3::hidden]]:
// make_property_map.py ignores preprocessor lines, so a member fenced off for Linux would
// still be emitted into PropertyMaps.inl and fail to compile.
    EntityHandle Entity;
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(TimelineSceneTrigger, Entity) == 0xA8, "the macOS ctor's last store");
static_assert(sizeof(TimelineSceneTrigger) == 176, "ecl::TimelineSceneTrigger is 176 bytes (the game's registry)");
#endif

struct [[bg3::component]] RegionTrigger : public bg3se::AreaTrigger
{
    DEFINE_PROXY_COMPONENT(ClientRegionTrigger, "ecl::RegionTrigger")

    EntityHandle Entity;
    uint8_t field_78;
    bool CameraPosLocked;
    bool MaskRegionInWorld;
};

struct [[bg3::component]] LightingTrigger : public bg3se::LightingTrigger
{
    DEFINE_PROXY_COMPONENT(ClientLightingTrigger, "ecl::LightingTrigger")

    EntityHandle Entity;
};

END_NS()

BEGIN_NS(floor)

struct InfoComponent : public BaseComponent
{
    DEFINE_COMPONENT(FloorInfo, "eoc::floor::InfoComponent")

    FixedString BuildingUUID;
    Guid FloorTrigger;
    bool IsRoof;
    int Floor;
};

END_NS()
