#pragma once

BEGIN_SE()

// ls::LightingDetailsIO::Visit (macOS 0x106eae540) names every member at its offset as it
// serialises it: Type +8, CastShadow +9, VolumetricShadow +0xa, PreExpose +0xb, Radius +0xc,
// Intensity +0x10, ScatteringScale +0x14, SpotLightAngles +0x18, Colour +0x20, Position +0x2c.
// Upstream's mirror is right; what was wrong was where ecl::TimelineSystem puts it.
struct LightingDetails : public ProtectedGameObject<LightingDetails>
{
    [[bg3::hidden]] void* VMT;
    uint8_t Type;
    bool CastShadow;
    bool VolumetricShadow;
    bool PreExpose;
    float Radius;
    float Intensity;
    float ScatteringScale;
    glm::vec2 SpotLightAngles;
    glm::vec3 Color;
    glm::vec3 Position;
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(LightingDetails, VolumetricShadow) == 0xA,
    "ls::LightingDetailsIO::Visit, macOS 0x106eae5da");
static_assert(offsetof(LightingDetails, Position) == 0x2C,
    "ls::LightingDetailsIO::Visit, macOS 0x106eae6ba");
static_assert(sizeof(LightingDetails) == 0x38,
    "the macOS TimelineSystem ctor's last write into it is Position.z at +0x34 (0x103e88974)");
#endif


struct MaterialReference
{
    FixedString VisualResourceID;
    bool ForceEnabled;
    int SlotIndex;
    int MaterialApplyFlags;
    // Editor only
    // Guid Key;
};


struct MaterialGroup
{
    bool IsReplacement;
    FixedString MaterialResourceID;
    FixedString Name;
    Array<MaterialReference> References;
};


struct MaterialOverlayGroupItem
{
    Guid ID;
    int32_t MaterialOverlayType;
};

struct SoundEventDetails
{
    Guid ID;
    float Threshold;
    uint8_t SoundObjectIndex;
    FixedString EventName;
    bool Force;
    bool TriggerOnIncreasing;
};


struct RTPCDetails
{
    FixedString EventName;
    std::optional<float> SoundDuration;
    std::optional<float> TargetValue;
    uint8_t SoundObjectIndex;
    float Delay;
};


struct MaterialTargetDetails
{
    FixedString ParameterName;
    Guid GroupId;
    bool IsIncreasing;
    float MaterialDuration;
    float MaterialMaxValue;
    float MaterialMinValue;
    uint8_t MaterialBlend;
    Array<SoundEventDetails> SoundEvents;
    Array<RTPCDetails> RTPCs;
};


struct MaterialConfiguration
{
    int32_t Category;
    int32_t Variation;
    Array<MaterialOverlayGroupItem> OverlayGroups;
    Array<MaterialTargetDetails> MaterialDetails;
};


struct TimelineEmotionalMaterials
{
    HashMap<Guid, Array<MaterialConfiguration>> MaterialConfigurations;
    HashMap<Guid, MaterialGroup> MaterialGroups;
};


struct MaterialGroupLookup
{
    HashMap<Guid, MaterialGroup> MaterialGroups;
};


struct CombatCameraParams
{
    bool IsEnabled;
    float HoldTimeAfterEvent;
    float MaxSequenceTime;
    float ObstructionCheckRadius;
    float MinChangeCameraTime;
    float MinAngleDegreesDifferenceToChangeCamera;
    float ObstructionBufferTime;
};


struct TLLookAtAutomationConfig : public ProtectedGameObject<TLLookAtAutomationConfig>
{
    [[bg3::hidden]] void* VMT;
    FixedString Bone;
    uint8_t TurnMode;
    uint8_t TrackingMode;
    float TurnSpeedMultiplier;
    float TorsoTurnSpeedMultiplier;
    float HeadTurnSpeedMultiplier;
    float Weight;
    float SafeZoneAngle;
    float HeadSafeZoneAngle;
    glm::vec3 Offset;
    bool Reset;
    uint8_t LookAtMode;
    uint8_t LookAtInterpMode;
    bool IsEyeLookAtEnabled;
    uint8_t EyeLookAtBone;
    glm::vec3 EyeLookAtOffset;
    float MinDelayTimeSeconds;
    float MaxDelayTimeSeconds;
};


struct TLSystemConfig : public ProtectedGameObject<TLSystemConfig>
{
    [[bg3::hidden]] void* VMT;
    Path FileName;
    float FoVOffset;
    float MinimumComponentLength;
    float FoVOffsetDeviation;
    float PercentageActivationChance;
    [[bg3::hidden]] uint8_t QuestionHoldAutomationParams[32];
    Guid CombatCamerasFallbackSystemicTimeline;
    CombatCameraParams CombatCamera;
    TLLookAtAutomationConfig LookAt;
    float SteppingBlendTime;
};


struct TLAutomatedLight
{
    uint8_t TypeId;
    Guid ID;
    STDString Name;
    FixedString AttachBone;
    bool IsDarkVisionLight;
    LightDesc* LightDesc;
};


struct TLAutomatedLightSetup
{
    uint8_t TypeId;
    Guid ID;
    STDString Name;
    Array<TLAutomatedLight*> Lights;
};


struct TimelineActorDataComponent : public BaseComponent
{
    DEFINE_COMPONENT(TimelineActorData, "eoc::TimelineActorDataComponent")

    Guid Actor;
    Guid Timeline;
    uint8_t TimelineType;
    uint8_t field_21;
    uint8_t Slot;
    bool IsListener;
    uint8_t IKToggle;
    uint8_t IKToggle2;
    uint8_t HandsIKToggle;
    uint8_t HandsIKToggle2;
};


struct TimelineNode
{
    Guid NodeId;
    Guid ContentNodeIdOverride;
    Guid ContentNodeIdOverride1;
    Guid ContentNodeIdOverride2;
    __int64 field_40;
    uint8_t field_48;
    uint8_t field_49;
    uint8_t field_4A;
    uint8_t field_4B;
    uint8_t field_4C;
    uint8_t field_4D;
    uint8_t field_4E;
    uint8_t field_4F;
};


struct TimelineDataComponent : public BaseComponent
{
    DEFINE_COMPONENT(TimelineData, "eoc::TimelineDataComponent")

    EntityHandle Entity;
    Guid InstanceId;
    Guid InitialTimeline;
    Array<Guid> TimelineResources;
    Transform CurrentTransform;
    int DialogInstanceId;
    int field_64;
    Array<TimelineNode> Nodes;
    uint8_t TimelineType;
    uint8_t PlaybackState;
    uint8_t CreationState;
    uint8_t field_7B;
    int CurrentPlayCount;
    int PlayCount;
    float TimeRemaining;
    float SteppingBlendTime;
    uint8_t field_8C;
    uint8_t field_8D;
    uint8_t field_8E;
    uint8_t field_8F;
    Guid CurrentCameraActor;
    EntityHandle Sound;
    // Editor only
    // Transform PreviewTransform;
};


struct SyncedTimelineActorControlComponent : public BaseComponent
{
    DEFINE_COMPONENT(SyncedTimelineActorControl, "eoc::SyncedTimelineActorControlComponent")

    Guid Actor;
    Guid Timeline;
    uint8_t Slot;
    bool IsListener;
    bool IsAdditional;
    uint8_t field_23;
    int field_24;
};


struct SyncedTimelineControlComponent : public BaseComponent
{
    DEFINE_COMPONENT(SyncedTimelineControl, "eoc::SyncedTimelineControlComponent")

    Guid Timeline;
    Array<Guid> TimelineResources;
    Guid CurrentTimeline;
    uint32_t DialogInstanceId;
    uint8_t TimelineType;
    Guid SceneTrigger;
    Guid InitialTimeline;
    uint32_t CharacterCreationUser;
    uint8_t CreationState;
    [[bg3::hidden]] HashMap<Guid, void*> SceneTriggers; // HashMap<Guid, SceneTrigger>
    HashMap<Guid, Guid> Triggers;
    Array<TimelineNode> Nodes;
    uint8_t PlaybackState;
    bool SmoothExitRequested;
};


struct TimelineContent;
struct TLBaseComponent;
struct TLVoice;


struct PeanutSlotData
{
    int Index;
    uint8_t Slot;
};


struct TimelinePlayerActor
{
    PlayerId Player;
    EntityHandle Actor;
};


struct TimelineNodeData
{
    Guid field_0;
    Guid field_10;
    Guid field_20;
    uint64_t field_30;
    uint16_t field_38;
    uint32_t field_3C;
};


struct Timeline : public aspk::Timeline
{
    bool field_58;
    bool field_59;
    bool field_5A;
    [[bg3::hidden]] HashMap<uint64_t, void*> ActorOverridePhases; // TimelineActorOverrideCache
    FixedString field_a0;
    Guid Id;
    uint8_t VOState;
    uint8_t InitiatorAnimState;
    Transform InitialTransform;
    Transform CurrentTransform;
    glm::vec3 InitialExtents;
    glm::vec3 Extents;
    bool PositionDirty;
    Guid StartingTimelineResource;
    [[bg3::hidden]] HashMap<Guid, TimelineContent*> CustomContent;
    [[bg3::hidden]] TimelineContent* Content;
    [[bg3::hidden]] TimelineContent* ContentOverride;
    HashSet<Guid> LoadedContent;
    [[bg3::hidden]] void* DialogManager;
    uint8_t ChangeFlags;
    [[bg3::hidden]] Array<TLBaseComponent*> UnprocessedComponentActivations;
    HashMap<Guid, EntityHandle> UnregisteredActors;
    Array<TimelinePlayerActor> JoinedPlayerActors;
    HashMap<Guid, EntityHandle> RegisteredActors;
    HashSet<EntityHandle> RegisteredActorEntities;
    HashMap<Guid, EntityHandle> field_298;
    HashMap<Guid, Guid> ActorToTimeline;
    HashMap<Guid, PeanutSlotData> RegisteredPeanuts;
    HashMap<Guid, PeanutSlotData> InitialPeanuts;
    [[bg3::hidden]] LegacyRefMap<Guid, LegacyRefMap<FixedString, TLBaseComponent*>> RegisteredComponents;
    LegacyRefMap<Guid, EntityHandle> ActorLighting;
    HashMap<Guid, Guid> QuestionHoldCameraActorIdToSceneId;
    HashMap<Guid, Guid> CompanionCameraSceneIdToActorId;
    Guid SceneTriggerId;
    Guid InitiatorId;
    Guid Atmosphere;
    Guid Lighting;
    Guid SceneStageId;
    bool IsPreviewTimeline;
    bool IsGameplayTimeline;
    bool IsSystemicCombatTimeline;
    bool IsAuthoredCombatTimeline;
    bool IsMainMenuTimeline;
    uint32_t SkippingMode;
    bool DeferredRequestOngoing;
    bool NeedsFadeOnPlay;
    bool SoundEventsEnabled;
    bool ArePeanutsVisible;
    bool IsDuringQuestionHoldAutomation;
    bool ShouldForceUpdateCameraBehavior;
// PORT (linux): a Guid, not upstream's bool. As a bool it leaves everything from DeltaExposure on
// 0x18 low, and Ext.Types.Serialize(Ext.System.ClientTimeline) dies in CustomHeaders' keys.
// ls::Timeline's ctor, bin/bg3 0x2c33020 (vtable 0x7ac20c0; its slot 15,
// UpdateComponents, calls the matched lambda 0x7458bd0), stores the bool run as a dword at 0x494
// and a word at 0x498 and nothing at 0x49a..0x49f. In a live timeline those six bytes are stale
// heap bytes (0x20 in a live object), so 0x499 ends the run. The ctor then zeroes 16 bytes at
// 0x4a0, a byte at 0x4b0, DeltaExposure.Enabled at 0x4b4 and 16 bytes at 0x4b8, as the macOS
// ctor (af3f1c71 0x10713c790) does. The Guid at 0x4a0 is the automatic lighting id:
// ecl::TimelineLightingHelper::ClearAutomatedLighting (macOS 0x103e57454) tests it against the
// null Guid and resets it. The byte at 0x4b0 is ShouldUpdateAttachmentTransforms:
// ecl::TLSwitchLocationEvent::ChangeTimelineActorsPosition sets it on the component's timeline
// (macOS 0x103e0b658), and ecl::TimelineSystem::PostUpdateTimelines tests and clears it (bin/bg3
// 0x2244755 / 0x2245152). The type change is unconditional because make_property_map.py reads
// through #if, so an #if/#else pair would emit the property twice.
    Guid AutomaticLightingIdFromAtmosphere;
    bool ShouldUpdateAttachmentTransforms;
    DeltaExposure DeltaExposure;
    bool PausedForInGameControls;
    bool InGameTimelineFreeCamEnabled;
    bool IsHoldingAtZero;
    bool LSDialogStarted;
    bool NestedRemapResetRequested;
    Guid PreviousCamera;
    Guid CurrentCameraActor;
    Guid CurrentCameraActorOverride;
    Guid ActivatedLightingCamera;
    bool ShouldUpdateActiveCustomLighting;
    bool ShouldUpdateCustomLighting;
    bool ShouldUpdateConditionalStaging;
    bool ShouldUpdateAutomatedLighting;
    [[bg3::hidden]] TLVoice *PreviousVoice;
    [[bg3::hidden]] TLVoice *CurrentVoice;
    int32_t DialogInstanceId;
    TimelineNodeData NodeData;
    Array<EntityHandle> VisualStreamerHints;
    ComponentHandle FadeRequest;
    int64_t CurrentPrefetchPhaseIndex;
    [[bg3::hidden]] Array<void*> field_590_Arr_TimelineLocationPrefetch;
    [[bg3::hidden]] Array<void*> field_5A0_Arr_TimelineShotPrefetch;
    uint64_t Random;
    bool ShouldForceUpdateNextJCut;
    bool InitialLimitGroundSnappingHeight;
    bool LimitGroundSnappingHeight;
    [[bg3::hidden]] HashMap<int16_t, void*> PlayerRenderViewData_MHM_i16_PlayerRenderViewData;
    // Editor only
    // ls::GameRenderView *GetRenderViewOverride;
    // FixedString DefaultCameraOverride;
    // Transform PreviewTransform;
    // Transform PreviewBoundSceneWorldTransform;
    // Guid PreviewAtmosphere;
    // float PreviewAtmosphereFadeTime;
    // Guid PreviewLighting;
    // float PreviewLightingFadeTime;
    // bool IsPreviewSoundInitialized;
    // uint64_t PreviewOverridingFlags;
    // float EditorDesiredTime;
    // Guid CurrentLocationTriggerId;
    Guid ActiveTimeline;
    [[bg3::hidden]] HashMap<Guid, PrimitiveSet<aspk::Component*>> CustomComponents;
    HashMap<Guid, aspk::TimelineHeader> CustomHeaders;
    HashSet<EntityHandle> CharacterActors;
    UserId CharacterCreationUser;
    HashMap<EntityHandle, Transform> RootMotions;
    [[bg3::hidden]] void *PhysicsScene;
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(Timeline, IsGameplayTimeline) == 0x489,
    "ecl::TimelineHelper::GetCanWatchPlayers tests [arg0+0x489], bin/bg3 0x25c2c30");
static_assert(offsetof(Timeline, ShouldForceUpdateCameraBehavior) == 0x499,
    "the bin/bg3 ls::Timeline ctor ends the bool run with a word store at this+0x498 (0x2c33593)");
static_assert(offsetof(Timeline, AutomaticLightingIdFromAtmosphere) == 0x4A0,
    "the bin/bg3 ls::Timeline ctor zeroes 16 bytes at this+0x4a0 (0x2c335a9)");
static_assert(offsetof(Timeline, ShouldUpdateAttachmentTransforms) == 0x4B0,
    "ecl::TimelineSystem::PostUpdateTimelines clears [timeline+0x4b0], bin/bg3 0x2245152");
static_assert(offsetof(Timeline, DeltaExposure) == 0x4B4,
    "the bin/bg3 ls::Timeline ctor zeroes the byte at this+0x4b4 (0x2c3359b)");
static_assert(offsetof(Timeline, PausedForInGameControls) == 0x4C4,
    "ecl::TimelineSystem::ProcessSingleNodeRequest tests [timeline+0x4c4], bin/bg3 0x329d764");
static_assert(offsetof(Timeline, InGameTimelineFreeCamEnabled) == 0x4C5,
    "ecl::TimelineCameraHelper::ActivateCamera tests [timeline+0x4c5], bin/bg3 0x35d8765");
static_assert(offsetof(Timeline, LSDialogStarted) == 0x4C7,
    "ProcessSingleNodeRequest tests it at bin/bg3 0x329cb5b; macOS OnDialogStarted sets it (0x103e6fada)");
static_assert(offsetof(Timeline, ShouldUpdateCustomLighting) == 0x511,
    "ecl::TimelineLightingHelper::UpdateCustomLighting tests [arg0+0x511], bin/bg3 0x24d2dc0");
static_assert(offsetof(Timeline, DialogInstanceId) == 0x528,
    "ecl::TimelineHelper::GetTimelineDialogFromTimelineId reads [arg0+0x528], bin/bg3 0x24482b0");
static_assert(offsetof(Timeline, PlayerRenderViewData_MHM_i16_PlayerRenderViewData) == 0x5C0,
    "ecl::TimelineHelper::GetRenderView reads the map at [arg0+0x5c0], bin/bg3 0x2415c00");
static_assert(offsetof(Timeline, ActiveTimeline) == 0x600,
    "ecl::TimelineHelper::GetTimelinePlayedTime takes &[arg0+0x600], bin/bg3 0x66708f8");
static_assert(offsetof(Timeline, CustomHeaders) == 0x650,
    "GetTimelinePlayedTime reads the map at [arg0+0x650], Values at +0x680, bin/bg3 0x667091c");
static_assert(offsetof(Timeline, CharacterCreationUser) == 0x6C0,
    "ecl::TimelineHelper::GetWatchingPlayers reads [arg0+0x6c0], bin/bg3 0x2249070");
static_assert(offsetof(Timeline, PhysicsScene) == 0x708,
    "the bin/bg3 ls::Timeline ctor's last store is the qword at this+0x708 (0x2c3370f)");
static_assert(sizeof(Timeline) == 0x710,
    "operator new(0x710) ahead of both calls of the ls::Timeline ctor, bin/bg3 0x2c32a5c / 0x3d58ae6");
#endif


END_SE()

BEGIN_NS(ecl)

struct ClientTimelineActorControlComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClientTimelineActorControl, "ecl::ClientTimelineActorControlComponent")

    [[bg3::legacy(field_0)]] Guid Actor;
    [[bg3::legacy(field_10)]] Guid Timeline;
    [[bg3::legacy(field_20)]] uint8_t Slot;
    [[bg3::legacy(field_21)]] bool IsAdditional;
    [[bg3::legacy(field_22)]] bool IsListener;
};


struct ClientTimelineControlComponent : public BaseComponent
{
    DEFINE_COMPONENT(ClientTimelineControl, "ecl::ClientTimelineControlComponent")

    Guid InstanceId;
    Guid Timeline;
    Array<Guid> TimelineResources;
    int DialogInstanceId;
    uint8_t TimelineType;
    uint8_t CreationState;
    uint8_t field_36;
    uint8_t field_37;
    Transform Transform;
    glm::vec3 Extents;
    int field_6C;
    Array<TimelineNode> Nodes;
    uint8_t PlaybackState;
    uint8_t field_81;
    uint8_t field_82;
    uint8_t field_83;
    int field_84;
};


struct TLPreviewDummy : public BaseProxyComponent
{
    DEFINE_COMPONENT(TLPreviewDummy, "ecl::TLPreviewDummy")

    TLPreviewDummy() = delete;
    TLPreviewDummy(TLPreviewDummy const&) = delete;

    [[bg3::hidden]] void* VMT;
    [[bg3::hidden]] void* VMT2;
    ecs::EntityRef Entity;
    [[bg3::hidden]] void* field_20;
    FixedString CurrentLevel;
    uint16_t Flags;
    EntityHandle field_30;
    STDString Name;
    FixedString PhysicsResourceId;
    FixedString VisualResourceId;
    Array<FixedString> SpringCollisionResourceIds;
    Array<FixedString> ClothCollisionResourceIds;
    FixedString SourceTemplateOverride;
    FixedString ShapeShiftSourceTemplateOverride;
    FixedString OriginalCharacterTemplate;
    [[bg3::hidden]] void* field_88;
    EntityHandle Listener;
    [[bg3::legacy(field_98)]] Array<FixedString> Speakers;
    uint32_t VisibilityFlags;
    [[bg3::legacy(field_AC)]] VisualLoadFlags VisualLoadFlags;
    [[bg3::hidden]] void* VertexColorMaskTexture;
    uint8_t LightChannel;
    uint8_t TimelineType;
    std::optional<ArmorSetState> ArmorSetState;
    uint32_t ComponentLayoutFlags;
    bool ActiveDummy;
    bool ArmorVisibilityControlled;
    [[bg3::legacy(field_C2)]] bool WeaponsLoaded;
    [[bg3::legacy(ArmorVisibilityControlled2)]] bool EquipmentVisibilityControlled;
    [[bg3::legacy(field_C4)]] bool IsPlayer;
    bool IsWeaponUnsheathed;
    bool ShouldShowVanityWeapon;
#if 0
    //ls::DrawingContext DrawingContext;
    //Array<void*> DebugTexts;
#endif
};


struct TimelineActorDataCreateDestroyRequest
{
    EntityHandle Entity;
    Guid Timeline;
    bool Synced{ false };
    uint8_t field_19{ 0 };
};


struct TimelineActorJoinedEventRequest
{
    EntityHandle Timeline;
    EntityHandle Actor;
    PlayerId PlayerIndex;
};


struct TimelineActorLeaveRequest
{
    EntityHandle Entity;
    uint8_t TimelineType{ 0 };
    Guid Actor;
    Guid Timeline;
    bool IsDummy{ false };
    bool IsListening{ false };
};


struct TimelineActorLeftEventRequest
{
    EntityHandle Timeline;
    PlayerId PlayerIndex;
    EntityHandle Actor;
    Guid ActorGuid;
    int DialogInstanceId{ 0 };
};


struct TimelineBackgroundEntityRequest
{
    Guid Entity;
    Guid Timeline;
};


struct ClientOnlyTimelineDestroyRequest
{
    EntityHandle Entity;
};


struct TimelineDataCreateDestroyRequest
{
    EntityHandle Entity;
    Guid Timeline;
    bool Synced{ false };
    bool Create{ false };
};


struct TimelineUnloadActorsRequest
{
    Guid Actor;
};


struct TimelineControlCreateDestroyRequest
{
    EntityHandle Entity;
    Guid Timeline;
    int DialogInstanceId{ 0 };
    int field_1C{ 0 };
    __int64 field_20{ 0 };
    __int64 field_28{ 0 };
    __int64 field_30{ 0 };
    __int64 field_38{ 0 };
    int field_40{ 0 };
    uint8_t TimelineType{ 0 };
    bool Create{ false };
};


struct TimelineCinematicAnimationRequest
{
    EntityHandle Entity;
    bool ActivateTimeline{ false };
};


struct TimelineAutoContinueRequest
{
    Guid Timeline;
};


struct TimelineCharacterAssignedRequest
{
    EntityHandle Entity;
    EntityHandle Actor;
    PlayerId PlayerId;
};


struct TimelineVisibilityRequest
{
    Guid Timeline;
    PlayerId PlayerId;
    bool Visible;
};



struct TimelineSystem : public BaseSystem
{
    DEFINE_SYSTEM(ClientTimeline, "ecl::TimelineSystem")

    [[bg3::hidden]] void* GameEventListenerVMT;
// PORT (linux): upstream's `[[bg3::hidden]] void* field_18;` is not in the native build. The
// ctor, ecl::TimelineSystem::TimelineSystem(bin/bg3 0x50378a0, build a187af94), writes four
// vtable pointers -- [this], [this+0x10], [this+0x18], [this+0x20] -- with ecs::System's
// EntityWorld at [this+8], and then memsets `this+0x28` for 0x158 bytes: the fourteen
// Signal<> at 24 bytes each (0x150) plus the first SignalSubscriber's `Sig` word, whose
// RegistrantId it sets to -1 at [this+0x180]. Its `lea`s of each signal's Connections array
// run 0x30, 0x48 ... 0x168, step 0x18, so the signal run starts at 0x28 and not 0x30. The
// macOS oracle's ctor (0x103e88080) is the same object byte for byte. Upstream's offset names
// (`qword78`, `qwordC0`) are the Win32 layout, where the slot does exist.
    [[bg3::hidden]] void* GameStateEventListenerVMT;
    [[bg3::hidden]] void* DialogEventListenerAdapterVMT;
    [[bg3::hidden]] UnknownSignal TimelineCreatedSignal;
    [[bg3::hidden]] UnknownSignal AutoContinueSignal;
    [[bg3::hidden]] UnknownSignal TakeActorControlSignal;
    [[bg3::hidden]] UnknownSignal qword78;
    [[bg3::hidden]] UnknownSignal TimelineUnloadedSignal;
    [[bg3::hidden]] UnknownSignal NodeDataChangedFlagSignal;
    [[bg3::hidden]] UnknownSignal qwordC0;
    [[bg3::hidden]] UnknownSignal VOChangedFlagSignal;
    [[bg3::hidden]] UnknownSignal DialogHistoryUpdatedSignal;
    [[bg3::hidden]] UnknownSignal UINodeDataChangedSignal1;
    [[bg3::hidden]] UnknownSignal UINodeUpdatedSignal1;
    [[bg3::hidden]] UnknownSignal UINodeUpdatedSignal2;
    [[bg3::hidden]] UnknownSignal UINodeDataChangedSignal2;
    [[bg3::hidden]] UnknownSignal UIVOChangedSignal;
    [[bg3::hidden]] UnknownSignalSubscriber TimelinePlaybackStateChangeSignal;
    [[bg3::hidden]] UnknownSignalSubscriber TimelineCreationStateChangeSignal;
    [[bg3::hidden]] UnknownSignalSubscriber ClientTimelineControlChangeSignal;
    [[bg3::hidden]] UnknownSignalSubscriber DialogStateChangeSignal;
    [[bg3::hidden]] void* SoundSystem;
    [[bg3::hidden]] void* SoundRoutingSystem;
    [[bg3::hidden]] void* SpeakerManager;
    [[bg3::hidden]] void* ResourceManager;
    [[bg3::hidden]] void* AnimationBlueprintSystem;
    [[bg3::hidden]] void* SoundAttachmentSystem;
    [[bg3::hidden]] void* TransformSystem;
    [[bg3::hidden]] void* VisualSystem;
    [[bg3::hidden]] void* MusicManager;
    [[bg3::hidden]] void* TimelineInstanceManager;
    [[bg3::hidden]] void* NetworkManager;
    [[bg3::hidden]] void* PlayerManager;
    [[bg3::hidden]] void* DialogManager;
    [[bg3::hidden]] void* CameraManager;
    [[bg3::hidden]] void* AspkComponentFactoryCustom;
    [[bg3::hidden]] UnknownFunction SignalConnectionCollection;
    LightingDetails Lighting;
    MaterialGroupLookup MaterialGroups;
    TimelineEmotionalMaterials EmotionalMaterials;
    Array<TimelineActorDataCreateDestroyRequest> ActorDataCreateDestroyRequests;
    Array<TimelineActorLeaveRequest> ActorLeaveRequests;
    Array<TimelineControlCreateDestroyRequest> TimelineControlCreateDestroyRequests;
    Array<ClientOnlyTimelineDestroyRequest> ClientOnlyTimelineDestroyRequests;
    Array<TimelineDataCreateDestroyRequest> TimelineDataCreateDestroyRequests;
    HashMap<Guid, int32_t> RequestContentForTimelines;
    [[bg3::hidden]] SynchronizedMPMCQueueBounded<void*> TimelineDataCreateDestroyRequestQueue;
    Array<TimelineCinematicAnimationRequest> CinematicAnimationRequests;
    Array<TimelineAutoContinueRequest> AutoContinueRequests;
    [[bg3::hidden]] Array<void*> Arr_AnimationTextKeyRequest;
    Array<TimelineCharacterAssignedRequest> CharacterAssignedRequests;
    Array<TimelineVisibilityRequest> TimelineVisibilityRequests;
    [[bg3::hidden]] Array<void*> SetClientControlPlaybackState_Arr_PlaybackStateChangeRequest;
    [[bg3::hidden]] Array<void*> SetClientControlCreationState_Arr_CreationStateChangeRequest;
    [[bg3::hidden]] Array<void*> LevelUnloaded_Arr_UnloadRequest;
    [[bg3::hidden]] Array<void*> Arr_ReactivateCameraRequest;
    [[bg3::hidden]] Array<void*> Arr_TakeActorControlRequest;
    [[bg3::hidden]] Array<void*> Arr_FastForwardToCurrentNodeRequest;
    [[bg3::hidden]] Array<void*> Arr_JoinLeaveTransitionRequest;
    [[bg3::hidden]] Array<void*> Arr_RegisterPeanutRequest;
    Array<TimelineUnloadActorsRequest> UnloadActorsRequests;
    [[bg3::hidden]] Array<void*> Arr_UnloadTimelineRequest;
    [[bg3::hidden]] Array<void*> Arr_TimelineTransitionFadeCompletionRequest;
    Array<TimelineActorJoinedEventRequest> ActorJoinedEventRequests;
    Array<TimelineActorLeftEventRequest> ActorLeftEventRequests;
    Array<uint32_t> DialogTimelineRequests;
    [[bg3::hidden]] SynchronizedMPMCQueueBounded<void*> SystemicTimelineCreationRequests;
    [[bg3::hidden]] SynchronizedMPMCQueueBounded<void*> CombatTimelineCreationRequests;
    [[bg3::hidden]] HashMap<Guid, Array<void*>> MHM_Guid_Arr_PreviewNodeRequest;
    [[bg3::hidden]] HashMap<Guid, Array<void*>> NodeRequests_MHM_Guid_Arr_NodeRequest;
    [[bg3::hidden]] HashMap<EntityHandle, void*> MHM_EH_DialogueEffectRequest;
    [[bg3::hidden]] HashMap<EntityHandle, void*> MHM_EH_DialogueEffectRequest2;
    std::array<HashMap<EntityHandle, bool>, 2> FadeRequests;
    std::array<HashSet<EntityHandle>, 2> ChangeVisibilityRequests;
    HashMap<Guid, Timeline*> Timelines;
    HashMap<Guid, EntityHandle> TimelineData;
    HashMap<uint32_t, Guid> DialogIdToTimeline;
    HashSet<uint32_t> DialogStarted;
    HashMap<EntityHandle, bool> MaterialSwapped;
    std::array<HashMap<Guid, bool>, 2> FadeStates;
    HashSet<Guid> CombatTimelines;
    Guid SessionGuid;
    Guid GlobalTimelineGuid;
    HashMap<Guid, Array<TimelineBackgroundEntityRequest>> BackgroundEntityRequests;
    [[bg3::hidden]] HashMap<Guid, Array<void*>> DeferredNodeRequestState_MHM_Guid_Arr_DeferredNodeRequest;
    [[bg3::hidden]] HashMap<Guid, HashMap<Guid, void*>> MHM_Guid_MHM_Guid_DeferredDummyCreation;
    HashMap<Guid, float> SoundSyncs;
    HashMap<Guid, int32_t> SyncedTimelinePlayedNodesChanged;
    [[bg3::hidden]] HashMap<Guid, void*> MHM_Guid_TimelineToUpdate;
    TLSystemConfig SystemConfig;
    Path ConfigPath;
    [[bg3::hidden]] uint8_t TimelineCombatCameraEvaluator[168];
    [[bg3::hidden]] CRITICAL_SECTION RequestContentForTimelinesCS;
    bool RequestVisualUpdate;
    bool InGameTimelineControlsVisible;
    bool ClientSessionLoaded;
    bool ClientSessionUnloaded;
    bool LevelUnloaded;
    bool LevelStartingTimelineFinished;
    bool LevelDeactivated;
    // Editor only
    // HashSet<uint64_t> DisconnectTimelineCreatedSignal;
    // bool HideUserReservedCharacters;
    // [[bg3::hidden]] void* TimelineMetadataContainer;
    // HashSet<Guid> MHS2_Guid;
};

#if defined(BG3SE_LINUX)
// The Linux ctor (bin/bg3 0x50378a0, build a187af94) laid out member by member.
static_assert(offsetof(TimelineSystem, TimelineCreatedSignal) == 0x28,
    "memset(this+0x28, 0, 0x158) over the signal run, bin/bg3 0x5037929/0x5037941");
static_assert(offsetof(TimelineSystem, SoundSystem) == 0x2B8,
    "the first of fifteen constructor-argument pointers, bin/bg3 0x5037a0a");
static_assert(offsetof(TimelineSystem, SignalConnectionCollection) == 0x330,
    "pStorage_ = this+0x338 written to this+0x330, bin/bg3 0x5037ade");
static_assert(offsetof(TimelineSystem, Lighting) == 0x370,
    "bin/bg3 0x5037a9c; macOS stores _ZTVN2ls15LightingDetailsE+0x10 there, 0x103e88932");
static_assert(offsetof(TimelineSystem, MaterialGroups) == 0x3A8,
    "bin/bg3 0x5037aea");
static_assert(offsetof(TimelineSystem, TimelineDataCreateDestroyRequestQueue) == 0x500,
    "the queue's alignas(64) re-absorbs the eight bytes; bin/bg3 0x5037c92 lists 0x540 inside it");
static_assert(offsetof(TimelineSystem, Timelines) == 0xB20,
    "bin/bg3 0x5037e4b");
static_assert(offsetof(TimelineSystem, MaterialSwapped) == 0xC10,
    "bin/bg3 0x5037e96; UpdateMaterial reads it at 0x2250a80");
static_assert(offsetof(TimelineSystem, RequestContentForTimelinesCS) == 0x1028,
    "the ctor's last initialiser call, bin/bg3 0x5037bd7");
static_assert(offsetof(TimelineSystem, RequestVisualUpdate) == 0x1058,
    "the ctor zeroes the bool run as two dwords at 0x1058 and 0x105b, bin/bg3 0x5037bec");
#endif


END_NS()
