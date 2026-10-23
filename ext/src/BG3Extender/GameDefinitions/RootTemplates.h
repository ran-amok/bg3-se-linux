#pragma once

#include <GameDefinitions/Base/Base.h>

BEGIN_SE()

struct TemplateTagContainer;

struct [[bg3::hidden]] TemplateHandle
{
public:
    inline TemplateHandle(uint32_t index, TemplateType type)
        : handle_(index | (((uint32_t)type) << 29))
    {}

    inline uint32_t GetHandle() const
    {
        return handle_;
    }

    inline uint32_t GetIndex() const
    {
        return handle_ & ~0xe0000000u;
    }

    inline TemplateType GetType() const
    {
        return TemplateType(handle_ >> 29);
    }

private:
    uint32_t handle_;
};

struct GameObjectTemplate : public ProtectedGameObject<GameObjectTemplate>
{
    virtual ~GameObjectTemplate() = 0;
    virtual void* GetName(void*) const = 0;
    virtual void* DebugDump(void*) = 0;
    virtual FixedString* GetType() const = 0;
    virtual FixedString* GetRealType() const = 0;
    virtual void SetTransform(Transform const&, void*) = 0;
    virtual bool IsValidV40026() = 0;
    virtual GameObjectTemplate* RealClone() = 0;
    virtual bool HasRotation() const = 0;
    virtual void UpdateTransformFromLevel() = 0;
    virtual void Visit(ObjectVisitor&) = 0;
    virtual void Noop() = 0;
    virtual bool IsValid() const = 0;
    virtual Guid* GetHLOD() = 0;
    virtual GameObjectTemplate* Clone() = 0;
    virtual GameObjectTemplate* CloneFrom(GameObjectTemplate* other) = 0;
    virtual void OverrideFromParent(GameObjectTemplate* other, bool force) = 0;
    virtual void UpdateOverrideFlagsFromParent(GameObjectTemplate* other) = 0;
    virtual bool PostLoad() = 0;
    virtual bool HasAIBounds() const = 0;
    virtual void* GetAIBounds1(void*, uint16_t) const = 0;
    virtual void* GetAIBounds2(void*, uint16_t) const = 0;
    virtual double GetAIBounds3(void*, uint16_t) const = 0;
    virtual void* GetAIBounds4(void*, uint16_t) const = 0;
    virtual double GetAIBounds5(void*, uint16_t) const = 0;
    virtual void UpdateFadeFromParent(GameObjectTemplate* parent, bool) = 0;
    virtual void UpdateHierarchyOnlyFadeFromParent(GameObjectTemplate* parent, bool) = 0;
    virtual void UpdateSeeThroughFromParent(GameObjectTemplate* parent, bool) = 0;
    virtual void UpdateCollideWithCameraFromParent(GameObjectTemplate* parent, bool) = 0;
    virtual void UnknownFunc(bool) = 0;
    virtual uint16_t* GetVisualFlags(uint16_t&) const = 0;
    virtual OverrideableProperty<bool>* IsPlatformOwner() const = 0;
    virtual void* GetOnUsePeaceActions() const = 0;
    virtual OverrideableProperty<bool>* IsTrap() const = 0;

    FixedString GetTemplateType() const;
    TemplateType GetTemplateStorageType() const;

    //# P_GETTER(TemplateType, GetTemplateType)
    //# P_GETTER(TemplateStorageType, GetTemplateStorageType)

    TemplateTagContainer* Tags;
    [[bg3::readonly]] FixedString Id;
    [[bg3::readonly]] FixedString TemplateName;
    [[bg3::readonly]] FixedString ParentTemplateId;
    [[bg3::hidden]] TemplateHandle TemplateHandle;
    [[bg3::readonly]] STDString Name;
    OverrideableProperty<uint32_t> GroupID;
    FixedString LevelName;
    [[bg3::hidden]] uint32_t _Pad;
    OverrideableProperty<Transform> Transform;
    OverrideableProperty<FixedString> VisualTemplate;
    OverrideableProperty<FixedString> PhysicsTemplate;
    OverrideableProperty<bool> CastShadow;
    OverrideableProperty<bool> ReceiveDecal;
    OverrideableProperty<bool> AllowReceiveDecalWhenAnimated;
    OverrideableProperty<bool> IsReflecting;
    OverrideableProperty<bool> IsShadowProxy;
    uint8_t GlobalDeletedFlag;
    OverrideableProperty<RenderChannel> RenderChannel;
    OverrideableProperty<uint8_t> ParentTemplateFlags;
    STDString FileName;
};

struct GlobalConfigParameter
{
    using ValueType = std::variant<STDString, int, double, bool, glm::vec3>;

    STDString Name;
    std::variant<Array<ValueType>, ValueType> Value;
};

// bin/bg3 0x2bd3200, ls::GlobalConfigParameter::operator!= (matched s3:1.00 to the macOS
// oracle's _ZNK2ls21GlobalConfigParameterneERKS0_), reads every one of these: the Name
// string's long flag at +0x0f with its data at +0x00 and its size at +0x08 (0x2bd3208,
// 0x2bd3234, 0x2bd3230), the *outer* discriminant at +0x28 (0x2bd326e / 0x2bd3272), the
// *inner* one at +0x20 (0x2bd3285 / 0x2bd3288) and the inner storage at +0x10 (0x2bd3296 --
// its string arm's flag at +0x1f, its int arm a dword and its double arm a qword, all at
// +0x10). libc++ puts a variant's one-byte index at round_up(max sizeof, max alignof) past
// the union, so the three sizes below pin those two discriminants where the binary has them.
// PORT (linux): the union is at +0 only because the sfinae-tag overlay (ext/toolchain/) stops
// the inner variant's empty bases colliding with the outer's; stock libc++ puts it at +8
// and this struct at 56.
static_assert(sizeof(STDString) == 0x10, "the Name string, bin/bg3 0x2bd3208 and 0x2bd3234");
static_assert(sizeof(GlobalConfigParameter::ValueType) == 0x18, "inner index at +0x10 of +0x10, bin/bg3 0x2bd3285");
static_assert(sizeof(decltype(GlobalConfigParameter::Value)) == 0x20, "outer index at +0x18 of +0x10, bin/bg3 0x2bd326e");
static_assert(offsetof(GlobalConfigParameter, Value) == 0x10, "bin/bg3 0x2bd3296");
static_assert(sizeof(GlobalConfigParameter) == 0x30, "macOS 0x1014d8c39 sizes the buffer n*3<<4 and 0x1014d8c57 strides it by 0x30");


struct TemplateTagContainer
{
    HashSet<Guid> DirectTags;
    HashSet<Guid> Tags;
    [[bg3::hidden]] GameObjectTemplate* Template;
    [[bg3::hidden]] void* GetTagsCallback;
};


struct BoundData
{
    [[bg3::legacy(Type)]] float Height;
    float ActualHeight;
    [[bg3::legacy(Radius2)]] float Size;
    glm::vec3 Min;
    glm::vec3 Max;
    float Radius;
    [[bg3::legacy(field_28)]] AIBoundType BoundType;
    [[bg3::legacy(AiType)]] AIShapeType Shape;
    uint8_t field_2A;
};


struct EoCGameObjectTemplate : public GameObjectTemplate
{
    OverrideableProperty<Array<BoundData>> AIBounds;
    OverrideableProperty<TranslatedString> DisplayName;
    OverrideableProperty<bool> Fadeable;
    OverrideableProperty<bool> SeeThrough;
    OverrideableProperty<bool> CollideWithCamera;
    OverrideableProperty<bool> HierarchyOnlyFade;
    OverrideableProperty<FixedString> FadeGroup;
    Array<FixedString> FadeChildren;
};


struct SceneryTemplate : public EoCGameObjectTemplate
{
    OverrideableProperty<bool> CoverAmount;
    OverrideableProperty<bool> CanClimbOn;
    OverrideableProperty<bool> CanShootThrough;
    OverrideableProperty<bool> WalkThrough;
    OverrideableProperty<bool> WalkOn;
    OverrideableProperty<bool> Wadable;
    OverrideableProperty<bool> CanClickThrough;
    OverrideableProperty<bool> IsPointerBlocker;
    OverrideableProperty<bool> IsBlocker;
    OverrideableProperty<bool> IsDecorative;
    OverrideableProperty<bool> AllowCameraMovement;
    OverrideableProperty<bool> CanShineThrough;
    OverrideableProperty<bool> BlockAoEDamage;
    OverrideableProperty<ShootThroughType> ShootThroughType;
    OverrideableProperty<SurfaceType> WadableSurfaceType;
    bool ReferencedInTimeline;
    OverrideableProperty<FixedString> LoopSound;
    OverrideableProperty<FixedString> SoundInitEvent;
    OverrideableProperty<Guid> HLOD;
    OverrideableProperty<FixedString> ShadowPhysicsProxy;
    OverrideableProperty<int16_t> SoundAttenuation;
}; 


struct CombatComponentTemplate : public ProtectedGameObject<CombatComponentTemplate>
{
    [[bg3::hidden]] void* VMT;
    OverrideableProperty<FixedString> Archetype;
    OverrideableProperty<FixedString> SwarmGroup;
    OverrideableProperty<Guid> Faction;
    OverrideableProperty<bool> CanFight;
    OverrideableProperty<bool> CanJoinCombat;
    OverrideableProperty<FixedString> CombatGroupID;
    OverrideableProperty<bool> IsBoss;
    OverrideableProperty<bool> StayInAiHints;
    OverrideableProperty<Guid> AiHint;
    OverrideableProperty<bool> IsInspector;
    uint8_t Unknown;
    uint8_t Unknown2;
    OverrideableProperty<float> StartCombatRange;
    OverrideableProperty<FixedString> AiUseCombatHelper;
    OverrideableProperty<Guid> ProxyOwner;
    OverrideableProperty<FixedString> ProxyAttachment;
};


struct AiTargetConditions
{
    int32_t MinimumHealthPercentage;
    int32_t MaximumHealthPercentage;
    Array<Guid> Tags;
};

struct AiActionConditions
{
    float ScoreModifier;
    int32_t StartRound;
    int32_t FallbackStartRound;
    int32_t MinimumImpact;
    bool OnlyCastOnSelf;
    uint16_t AIFlags;
    AiTargetConditions SourceConditions;
    AiTargetConditions TargetConditions;
    Array<Guid> ExcludeInNPCLoadout;
    Array<Guid> OnlyInNPCLoadout;
};

struct InventoryItemData
{
    AiActionConditions Conditions;
    STDString ItemName;
    STDString LevelName;
    FixedString UUID;
    FixedString TemplateID;
    int Amount;
    IsTradableType IsTradable;
    uint8_t Type;
    bool CanBePickpocketed;
    bool IsDroppedOnDeath;
};

struct CharacterSpellData
{
    FixedString Spell;
    AbilityId SpellCastingAbility;
    SpellLearningStrategy LearningStrategy;
    AiActionConditions Conditions;
};

struct FootStepInfo
{
    FixedString Name;
    FixedString FootPrintEffectName;
    FixedString FootSmearEffectName;
    FixedString FootSlideEffectName;
    FixedString FootHearingEffectName;
    FixedString FootBoneName;
    FixedString FootSoundEventName;
};


struct CharacterTemplate : public EoCGameObjectTemplate
{
    CombatComponentTemplate CombatComponent;
    OverrideableProperty<Array<GlobalConfigParameter>> ScriptConfigGlobalParameters;
    OverrideableProperty<FixedString> Icon;
    OverrideableProperty<FixedString> Stats;
    OverrideableProperty<FixedString> SpellSet;
    OverrideableProperty<FixedString> Equipment;
    OverrideableProperty<Array<Guid>> EquipmentTypes;
    OverrideableProperty<Array<FixedString>> Treasures;
    OverrideableProperty<Array<FixedString>> TradeTreasures;
    OverrideableProperty<FixedString> BloodType;
    OverrideableProperty<FixedString> CriticalHitType;
    OverrideableProperty<FixedString> DefaultDialog;
    HashSet<Guid>* SpeakerGroupList;
    OverrideableProperty<STDString> GeneratePortrait;
    OverrideableProperty<float> LadderAttachOffset;
    OverrideableProperty<float> LadderLoopSpeed;
    OverrideableProperty<bool> CanShootThrough;
    OverrideableProperty<ShootThroughType> ShootThroughType;
    OverrideableProperty<bool> WalkThrough;
    OverrideableProperty<bool> CanClimbLadders;
    OverrideableProperty<bool> CanConsumeItems;
    OverrideableProperty<bool> IsPlayer;
    OverrideableProperty<bool> SpotSneakers;
    OverrideableProperty<bool> CanOpenDoors;
    OverrideableProperty<bool> CanWalkThroughDoors;
    OverrideableProperty<bool> AvoidTraps;
    OverrideableProperty<bool> InfluenceTreasureLevel;

    OverrideableProperty<bool> UseOcclusion;
    OverrideableProperty<FixedString> SoundInitEvent;
    OverrideableProperty<FixedString> SoundMovementStartEvent;
    OverrideableProperty<FixedString> SoundMovementStopEvent;
    OverrideableProperty<int8_t> SoundObjectIndex;
    OverrideableProperty<int16_t> SoundAttenuation;
    OverrideableProperty<bool> UseSoundClustering;

    OverrideableProperty<Array<Guid>> ExcludeInDifficulty;
    OverrideableProperty<Array<Guid>> OnlyInDifficulty;
    OverrideableProperty<bool> JumpUpLadders;
    OverrideableProperty<bool> AnubisNonPersistent;
    OverrideableProperty<SurfaceType> BloodSurfaceType;
    OverrideableProperty<bool> ForceLifetimeDeath;
    OverrideableProperty<Guid> EquipmentRace;
    OverrideableProperty<Array<IActionData*>> OnDeathActions;
    OverrideableProperty<float> DeathRaycastMinLength;
    OverrideableProperty<float> DeathRaycastMaxLength;
    OverrideableProperty<float> DeathRaycastHeight;
    OverrideableProperty<float> DeathRaycastVerticalLength;
    OverrideableProperty<Guid> DeathEffect;
    OverrideableProperty<FixedString> ExplodedResourceID;
    OverrideableProperty<FixedString> ExplosionFX;
    OverrideableProperty<FixedString> DisintegratedResourceID;
    OverrideableProperty<FixedString> DisintegrateFX;
    OverrideableProperty<FixedString> AnubisConfigName;
    OverrideableProperty<Array<CharacterSpellData>> SkillList;
    OverrideableProperty<Array<InventoryItemData>> ItemList;
    OverrideableProperty<Array<FixedString>> StatusList;
    OverrideableProperty<FixedString> TrophyID;

    OverrideableProperty<FixedString> FoleyShortResourceID;
    OverrideableProperty<FixedString> FoleyMediumResourceID;
    OverrideableProperty<FixedString> FoleyLongResourceID;

    OverrideableProperty<FixedString> VocalNoneResourceID;
    OverrideableProperty<FixedString> VocalAttackResourceID;
    OverrideableProperty<FixedString> VocalDeathResourceID;
    OverrideableProperty<FixedString> VocalDodgeResourceID;
    OverrideableProperty<FixedString> VocalPainResourceID;
    OverrideableProperty<FixedString> VocalAnticipationResourceID;
    OverrideableProperty<FixedString> VocalFallResourceID;
    OverrideableProperty<FixedString> VocalShoutResourceID;
    OverrideableProperty<FixedString> VocalSpawnResourceID;
    OverrideableProperty<FixedString> VocalAlertResourceID;
    OverrideableProperty<FixedString> VocalAngryResourceID;
    OverrideableProperty<FixedString> VocalAwakeResourceID;
    OverrideableProperty<FixedString> VocalBoredResourceID;
    OverrideableProperty<FixedString> VocalVictoryResourceID;
    OverrideableProperty<FixedString> VocalRebornResourceID;
    OverrideableProperty<FixedString> VocalRelaxedResourceID;
    OverrideableProperty<FixedString> VocalSnoreResourceID;
    OverrideableProperty<FixedString> VocalExhaustedResourceID;
    OverrideableProperty<FixedString> VocalEffortsResourceID;
    OverrideableProperty<FixedString> VocalInitiativeResourceID;
    OverrideableProperty<FixedString> VocalWeakResourceID;
    OverrideableProperty<FixedString> VocalLaughterResourceID;
    OverrideableProperty<FixedString> VocalRecoverResourceID;
    OverrideableProperty<FixedString> VocalBuffResourceID;
    OverrideableProperty<FixedString> VocalIdle1ResourceID;
    OverrideableProperty<FixedString> VocalIdle2ResourceID;
    OverrideableProperty<FixedString> VocalIdle3ResourceID;
    OverrideableProperty<FixedString> VocalIdleCombat1ResourceID;
    OverrideableProperty<FixedString> VocalIdleCombat2ResourceID;
    OverrideableProperty<FixedString> VocalIdleCombat3ResourceID;
    OverrideableProperty<FixedString> VocalGaspResourceID;
    OverrideableProperty<FixedString> VocalLaughterManiacalResourceID;
    OverrideableProperty<FixedString> VocalCinematicSuffix;

    OverrideableProperty<FixedString> CharacterVisualResourceID;
    OverrideableProperty<uint8_t> CoverAmount;
    OverrideableProperty<bool> CanBeTeleported;
    OverrideableProperty<int32_t> LevelOverride;
    OverrideableProperty<FixedString> ActivationGroupId;
    OverrideableProperty<InventoryAppearanceType> AliveInventoryType;
    OverrideableProperty<InventoryAppearanceType> InventoryType;
    OverrideableProperty<LegacyRefMap<FixedString, FixedString>> PickingPhysicsTemplates;
    OverrideableProperty<FixedString> SoftBodyCollisionTemplate;
    OverrideableProperty<FixedString> RagdollTemplate;
    OverrideableProperty<Array<FootStepInfo>> FootStepInfos;
    OverrideableProperty<CharacterState> DefaultState;
    OverrideableProperty<bool> IsLootable;
    OverrideableProperty<bool> IsEquipmentLootable;
    OverrideableProperty<bool> CanBePickedUp;
    OverrideableProperty<bool> CanBePickpocketed;
    OverrideableProperty<IsTradableType> IsTradable;
    OverrideableProperty<bool> IsDroppedOnDeath;
    OverrideableProperty<LightChannelValue> LightChannel;
    OverrideableProperty<Guid> Race;
    OverrideableProperty<TranslatedString> Title;
    OverrideableProperty<FixedString> AnimationSetResourceID;
    OverrideableProperty<bool> HasPlayerApprovalRating;
    OverrideableProperty<bool> DisableEquipping;
    OverrideableProperty<bool> IsSimpleCharacter;
    OverrideableProperty<float> WorldClimbingSpeed;
    OverrideableProperty<bool> UseStandAtDestination;
    OverrideableProperty<bool> IsMovementEnabled;
    OverrideableProperty<float> MovementAcceleration;
    OverrideableProperty<float> MovementSpeedStroll;
    OverrideableProperty<float> MovementSpeedWalk;
    OverrideableProperty<float> MovementSpeedRun;
    OverrideableProperty<float> MovementSpeedSprint;
    OverrideableProperty<float> MovementSpeedDash;
    OverrideableProperty<float> MaxDashDistance;
    OverrideableProperty<float> MovementStepUpHeight;
    OverrideableProperty<FixedString> MovementTiltToRemap;
    OverrideableProperty<float> TurningNodeAngle;
    OverrideableProperty<float> TurningNodeOffset;
    OverrideableProperty<bool> IsSteeringEnabled;
    OverrideableProperty<FixedString> SteeringSpeed_CastingCurve;
    OverrideableProperty<FixedString> SteeringSpeed_MovingCurve;
    OverrideableProperty<FixedString> SteeringSpeedCurveWithoutTransitions;
    OverrideableProperty<float> SteeringSpeedFallback;
    OverrideableProperty<bool> IsWorldClimbingEnabled;
    OverrideableProperty<FixedString> WorldClimbingBlendspace_DownA;
    OverrideableProperty<FixedString> WorldClimbingBlendspace_DownB;
    OverrideableProperty<float> WorldClimbingBlendspace_DownBHeight;
    OverrideableProperty<FixedString> WorldClimbingBlendspace_UpA;
    OverrideableProperty<FixedString> WorldClimbingBlendspace_UpB;
    OverrideableProperty<float> WorldClimbingBlendspace_UpBHeight;
    OverrideableProperty<float> WorldClimbingHeight;
    OverrideableProperty<float> WorldClimbingRadius;
    OverrideableProperty<float> ProbeLookAtOffset;
    OverrideableProperty<float> ProbeSpineAOffset;
    OverrideableProperty<float> ProbeSpineBOffset;
    OverrideableProperty<float> ProbeTiltToOffset;
    OverrideableProperty<FixedString> LadderBlendspace_Attach_Down;
    OverrideableProperty<FixedString> LadderBlendspace_Attach_Up;
    OverrideableProperty<FixedString> LadderBlendspace_Detach_Down;
    OverrideableProperty<FixedString> LadderBlendspace_Detach_Up;
    OverrideableProperty<float> VFXScale;
};


struct EquipmentData : public ProtectedGameObject<EquipmentData>
{
    HashMap<Guid, FixedString> ShortHair;
    HashMap<Guid, FixedString> LongHair;
    HashMap<Guid, FixedString> WavyShortHair;
    HashMap<Guid, FixedString> WavyLongHair;
    HashMap<Guid, FixedString> CurlyShortHair;
    HashMap<Guid, FixedString> CurlyLongHair;
    HashMap<Guid, FixedString> DreadShortHair;
    HashMap<Guid, FixedString> DreadLongHair;
    HashMap<Guid, FixedString> AfroShortHair;
    HashMap<Guid, FixedString> AfroLongHair;
    HashMap<Guid, Array<FixedString>> Visuals;
    HashMap<Guid, Guid> ParentRace;
    HashSet<Guid> SyncWithParent;
    resource::VisualSet* VisualSet;
    LegacyArray<FixedString> Slot;
};


struct ItemTemplate : public SceneryTemplate
{
    CombatComponentTemplate CombatComponent;
    OverrideableProperty<Array<FixedString>> InventoryList;
    HashSet<Guid>* SpeakerGroups;
    OverrideableProperty<FixedString> Icon;
    OverrideableProperty<bool> CanBePickedUp;
    OverrideableProperty<bool> CanBePickpocketed;
    OverrideableProperty<bool> IsDroppedOnDeath;
    OverrideableProperty<bool> CanBeMoved;
    OverrideableProperty<bool> Destroyed;
    OverrideableProperty<bool> IsInteractionDisabled;
    OverrideableProperty<bool> StoryItem;
    OverrideableProperty<bool> DestroyWithStack;
    OverrideableProperty<bool> IsPlatformOwner;
    OverrideableProperty<bool> IsKey;
    OverrideableProperty<bool> IsTrap;
    OverrideableProperty<bool> IsSurfaceBlocker;
    OverrideableProperty<bool> IsSurfaceCloudBlocker;
    OverrideableProperty<bool> TreasureOnDestroy;
    OverrideableProperty<bool> UsePartyLevelForTreasureLevel;
    OverrideableProperty<bool> Unimportant;
    OverrideableProperty<bool> Hostile;
    OverrideableProperty<bool> UseOnDistance;
    OverrideableProperty<bool> UseRemotely;
    OverrideableProperty<bool> PhysicsFollowAnimation;
    OverrideableProperty<bool> CanBeImprovisedWeapon;
    OverrideableProperty<bool> ForceAffectedByAura;
    OverrideableProperty<bool> IsBlueprintDisabledByDefault;
    OverrideableProperty<Array<Guid>> ExcludeInDifficulty;
    OverrideableProperty<Array<Guid>> OnlyInDifficulty;
    OverrideableProperty<TranslatedString> UnknownDisplayName;
    OverrideableProperty<bool> ShowAttachedSpellDescriptions;
    uint8_t GravityType;
    uint8_t FreezeGravity;
    OverrideableProperty<uint32_t> Tooltip;
    OverrideableProperty<FixedString> Stats;
    OverrideableProperty<Array<IActionData*>> OnUsePeaceActions;
    OverrideableProperty<Array<IActionData*>> OnDestroyActions;
    OverrideableProperty<TranslatedString> OnUseDescription;
    OverrideableProperty<bool> IsPortal;
    OverrideableProperty<bool> AttackableWhenClickThrough;
    [[bg3::hidden]] OverrideableProperty<Array<void*>> Scripts;
    [[bg3::hidden]] OverrideableProperty<HashMap<FixedString, void*>> ScriptOverrides;
    OverrideableProperty<FixedString> AnubisConfigName;
    OverrideableProperty<Array<GlobalConfigParameter>> ScriptConfigGlobalParameters;
    OverrideableProperty<FixedString> ConstellationConfigName;
    OverrideableProperty<Array<GlobalConfigParameter>> ConstellationConfigGlobalParameters;
    OverrideableProperty<Array<InventoryItemData>> ItemList;
    OverrideableProperty<Array<FixedString>> StatusList;
    OverrideableProperty<FixedString> DefaultState;
    OverrideableProperty<FixedString> Owner;
    OverrideableProperty<FixedString> Key;
    OverrideableProperty<FixedString> BloodType;
    OverrideableProperty<FixedString> CriticalHitType;
    OverrideableProperty<FixedString> MapMarkerStyle;
    OverrideableProperty<Guid> LockDifficultyClassID;
    OverrideableProperty<Guid> DisarmDifficultyClassID;
    OverrideableProperty<int32_t> Amount;
    OverrideableProperty<int32_t> MaxStackAmount;
    OverrideableProperty<int32_t> TreasureLevel;
    OverrideableProperty<EquipmentData*> Equipment;
    OverrideableProperty<FixedString> DropSound;
    OverrideableProperty<FixedString> PickupSound;
    OverrideableProperty<FixedString> UseSound;
    OverrideableProperty<FixedString> EquipSound;
    OverrideableProperty<FixedString> UnequipSound;
    OverrideableProperty<FixedString> InventoryMoveSound;
    OverrideableProperty<FixedString> ImpactSound;
    OverrideableProperty<FixedString> PhysicsCollisionSound;
    OverrideableProperty<bool> UseOcclusion;
    OverrideableProperty<SurfaceType> BloodSurfaceType;
    OverrideableProperty<uint8_t> BookType;
    OverrideableProperty<InventoryType> InventoryType;
    OverrideableProperty<TranslatedString> DisplayNameAlchemy;
    OverrideableProperty<TranslatedString> Description;
    OverrideableProperty<TranslatedString> TechnicalDescription;
    OverrideableProperty<TranslatedString> ShortDescription;
    OverrideableProperty<STDString> TechnicalDescriptionParams;
    OverrideableProperty<STDString> ShortDescriptionParams;
    OverrideableProperty<TranslatedString> UnknownDescription;
    OverrideableProperty<FixedString> PermanentWarnings;
    OverrideableProperty<bool> ContainerAutoAddOnPickup;
    OverrideableProperty<STDString> ContainerContentFilterCondition;
    HashSet<Guid>* InteractionFilterList;
    OverrideableProperty<InteractionFilterType> InteractionFilterType;
    OverrideableProperty<InteractionFilterRequirement> InteractionFilterRequirement;
    OverrideableProperty<FixedString> ActivationGroupId;
    OverrideableProperty<int32_t> LevelOverride;
    OverrideableProperty<bool> IsSourceContainer;
    OverrideableProperty<bool> IsPublicDomain;
    OverrideableProperty<bool> IgnoreGenerics;
    OverrideableProperty<bool> AllowSummonGenericUse;
    OverrideableProperty<bool> IsPortalProhibitedToPlayers;
    OverrideableProperty<LightChannelValue> LightChannel;
    OverrideableProperty<Guid> EquipmentTypeID;
    OverrideableProperty<uint32_t> CinematicArenaFlags;
    OverrideableProperty<Guid> TimelineCameraRigOverride;
    OverrideableProperty<Guid> MaterialPreset;
    OverrideableProperty<Guid> ColorPreset;
    OverrideableProperty<glm::vec3> ExamineRotation;

    // Lua helpers
    //# P_FUN(AddUseAction, ItemTemplate::AddUseAction)
    //# P_FUN(RemoveUseAction, ItemTemplate::RemoveUseAction)
    IActionData* AddUseAction(ActionDataType type);
    void RemoveUseAction(int32_t index);
};


struct ProjectileTemplate : public EoCGameObjectTemplate
{
    OverrideableProperty<float> LifeTime;
    OverrideableProperty<FixedString> CastBone;
    OverrideableProperty<FixedString> ImpactFX;
    OverrideableProperty<STDString> GroundImpactFX;
    OverrideableProperty<FixedString> TrailFX;
    OverrideableProperty<bool> DestroyTrailFXOnImpact;
    OverrideableProperty<bool> NeedsImpactSFX;
    OverrideableProperty<FixedString> BeamFX;
    OverrideableProperty<FixedString> PreviewPathMaterial;
    OverrideableProperty<FixedString> PreviewPathImpactFX;
    OverrideableProperty<FixedString> ImpactSoundResourceID;
    OverrideableProperty<float> PreviewPathRadius;
    OverrideableProperty<bool> RotateImpact;
    OverrideableProperty<bool> IgnoreRoof;
    OverrideableProperty<bool> DetachBeam;
    OverrideableProperty<PathTrajectoryType> ProjectilePath;
    OverrideableProperty<float> DistanceMin_Bezier3;
    OverrideableProperty<float> DistanceMax_Bezier3;
    OverrideableProperty<glm::vec2> OffsetMin_Bezier3;
    OverrideableProperty<glm::vec2> OffsetMax_Bezier3;
    OverrideableProperty<float> ShiftMin_Bezier3;
    OverrideableProperty<float> ShiftMax_Bezier3;
    OverrideableProperty<float> DistanceMin_Bezier4;
    OverrideableProperty<float> DistanceMax_Bezier4;
    OverrideableProperty<glm::vec2> OffsetAMin_Bezier3;
    OverrideableProperty<glm::vec2> OffsetAMax_Bezier3;
    OverrideableProperty<glm::vec2> OffsetBMin_Bezier3;
    OverrideableProperty<glm::vec2> OffsetBMax_Bezier3;
    OverrideableProperty<float> ShiftAMin_Bezier3;
    OverrideableProperty<float> ShiftAMax_Bezier3;
    OverrideableProperty<float> ShiftBMin_Bezier3;
    OverrideableProperty<float> ShiftBMax_Bezier3;
    OverrideableProperty<PathRotateMode> RotateMode;
    OverrideableProperty<PathVelocityMode> VelocityMode;
    OverrideableProperty<float> InitialSpeed;
    OverrideableProperty<float> Acceleration;
    OverrideableProperty<FixedString> CurveResourceId;
};

struct SurfaceVisualData
{
    FixedString Visual;
    glm::vec2 Height;
    glm::ivec2 Rotation;
    glm::vec2 Scale;
    float GridSize;
    int SpawnCell;
    int RandomPlacement;
    int SurfaceNeeded;
    int SurfaceRadiusMax;
};

struct SurfaceStatusData
{
    FixedString StatusId;
    SurfaceStatusApplyType ApplyTypes;
    float Chance;
    float Duration;
    bool Remove;
    bool ApplyToCharacters;
    bool ApplyToItems;
    bool KeepAlive;
    bool DontTickWhileOnSurface;
    bool VanishOnApply;
    bool Force;
    bool AffectedByRoll;
    bool OnlyOncePerTurn;
};


struct SurfaceTemplate : public GameObjectTemplate
{
    SurfaceType SurfaceType;
    FixedString SurfaceName;
    uint32_t field_158;
    OverrideableProperty<TranslatedString> DisplayName;
    OverrideableProperty<TranslatedString> Description;
    OverrideableProperty<Guid> CursorMessage;
    OverrideableProperty<FixedString> Icon;
    OverrideableProperty<FixedString> DecalMaterial;
    OverrideableProperty<MaterialType> MaterialType;
    OverrideableProperty<uint8_t> SurfaceCategory;
    OverrideableProperty<bool> CanEnterCombat;
    OverrideableProperty<bool> AlwaysUseDefaultLifeTime;
    OverrideableProperty<float> DefaultLifeTime;
    OverrideableProperty<float> SurfaceGrowTimer;
    OverrideableProperty<float> FadeInSpeed;
    OverrideableProperty<float> FadeOutSpeed;
    OverrideableProperty<float> FallDamageMultiplier;
    OverrideableProperty<int32_t> Seed;
    OverrideableProperty<float> NormalBlendingFactor;
    Array<SurfaceVisualData> InstanceVisual;
    Array<SurfaceVisualData> IntroFX;
    Array<SurfaceVisualData> FX;
    Array<SurfaceStatusData> Statuses;
    OverrideableProperty<FixedString> Summon;
    OverrideableProperty<bool> RemoveDestroyedItems;
    OverrideableProperty<bool> CanSeeThrough;
    OverrideableProperty<bool> CanShootThrough;
    OverrideableProperty<STDString> RollConditions;
    OverrideableProperty<ObscuredState> ObscuredStateOverride;
    OverrideableProperty<float> OnEnterDistanceOverride;
    OverrideableProperty<float> OnMoveDistanceOverride;
    OverrideableProperty<bool> AiPathVisible;
    OverrideableProperty<glm::vec3> AiPathColor;
    OverrideableProperty<FixedString> AiPathIconFX;
    OverrideableProperty<bool> EnableLightSpawning;
    OverrideableProperty<FixedString> SpawnLightTemplateID;
    OverrideableProperty<glm::vec2> SpawnLightRandomVerticalRangeOffset;
};


struct TimelineReferencedTemplate
{
    bool ReferencedInTimeline;
    HashSet<Guid> Timelines;
};


struct LightTemplate : public GameObjectTemplate
{
    OverrideableProperty<glm::vec3> Color;
    OverrideableProperty<glm::vec2> Angle;
    OverrideableProperty<float> Kelvin;
    OverrideableProperty<float> Radius;
    OverrideableProperty<float> Intensity;
    OverrideableProperty<float> ScatteringScale;
    OverrideableProperty<float> Gain;
    OverrideableProperty<float> Speed;
    OverrideableProperty<float> Amount;
    OverrideableProperty<float> MovementSpeed;
    OverrideableProperty<float> MovementAmount;
    OverrideableProperty<bool> UseTemperature;
    OverrideableProperty<bool> Enabled;
    OverrideableProperty<bool> IsFlickering;
    OverrideableProperty<bool> IsMoving;
    OverrideableProperty<bool> Shadow;
    OverrideableProperty<bool> VolumetricShadow;
    OverrideableProperty<bool> PreExpose;
    // PORT (linux): the game's visitor key for this member is `FillLight`, not `FlatFalloff`
    // (ls::LightTemplate::Visit, macOS 0x106eab3d9, writing 0x112/0x113 -- the mirror's offset
    // exactly). Left named as upstream has it: same layout, and renaming it would move a
    // Lua-visible property.
    OverrideableProperty<bool> FlatFalloff;
    OverrideableProperty<uint8_t> LightChannelFlag;
    OverrideableProperty<LightType> LightType;
    OverrideableProperty<FixedString> LightCookieTexture;
    OverrideableProperty<glm::vec3> DirectionLightDimensions;
    OverrideableProperty<uint8_t> DirectionLightAttenuationFunction;
    OverrideableProperty<float> DirectionLightAttenuationStart;
    OverrideableProperty<float> DirectionLightAttenuationEnd;
    OverrideableProperty<float> DirectionLightAttenuationSide;
// PORT (linux): one trailing byte upstream does not have, and without it every derived class puts
// its first member one byte too low. ls::LightTemplate's copy constructor (macOS 0x106eac1a0)
// copies DirectionLightAttenuationSide's float from [src+0x144] and its flag from [src+0x148] and
// then does `mov byte [dst+0x14c], 0` at 0x106eac4d5 -- a store, not a copy, so it is a runtime
// flag rather than serialised state, which is why Visit never mentions it and upstream never saw
// it. It puts ls::LightTemplate's data end at 0x14d, and eoc::CombinedLightTemplate::GameplayIsActive
// is at 0x14d in both that class's Visit (0x1014f5394) and its Clone (0x1014f58ed) -- which is
// 0x14c without this member. It does not change sizeof(LightTemplate), which stays 0x150.
    [[bg3::hidden]] uint8_t field_14C;
    // Editor only?
    // TimelineReferencedTemplate ReferencedInTimelines;
};


struct CombinedLightTemplate : public LightTemplate
{
    OverrideableProperty<bool> GameplayIsActive;
    OverrideableProperty<float> GameplayRadius;
    OverrideableProperty<float> GameplayEdgeSharpening;
    OverrideableProperty<bool> GameplayCheckLOS;
    OverrideableProperty<float> GameplaySpotlightAngle;
    OverrideableProperty<glm::vec3> GameplayDirectionalDimensions;
    OverrideableProperty<FixedString> LightCookieResource;
    OverrideableProperty<bool> IsHalfLit;
    OverrideableProperty<bool> IsSunlight;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the whole derived block off eoc::CombinedLightTemplate::Clone(), macOS 0x1014f5830
// -- the family's cheapest instrument: it mallocs the class's own size at 0x1014f58a6
// (`mov edi, 0x188`), calls ls::LightTemplate::LightTemplate(LightTemplate const&) 0x106eac1a0 for
// the base, stores the vtable, and then copies exactly the derived members, one load/store pair
// each, in declaration order: 0x14d/0x14e, 0x150/0x154, 0x158/0x15c, 0x160/0x161, 0x164/0x168 and
// then the 0x16c block. eoc::CombinedLightTemplate::Visit 0x1014f5330 names them: strGameplayIsActive
// writes 0x14d/0x14e (0x1014f5394), strGameplayRadius 0x150/0x154, strGameplayEdgeSharpening
// 0x158/0x15c, strGameplayCheckLOS 0x160/0x161. GameplayIsActive lands at 0x14d, inside
// ls::LightTemplate's tail padding (sizeof 0x150) where MSVC would put it at 0x150 and push every
// member after it up by 4.
static_assert(offsetof(CombinedLightTemplate, GameplayIsActive) == 0x14D, "macOS 0x1014f5394 and 0x1014f58ed");
static_assert(offsetof(CombinedLightTemplate, GameplayRadius) == 0x150, "macOS 0x1014f5404");
static_assert(offsetof(CombinedLightTemplate, GameplayEdgeSharpening) == 0x158, "macOS 0x1014f5474");
static_assert(offsetof(CombinedLightTemplate, GameplayCheckLOS) == 0x160, "macOS 0x1014f54dd");
static_assert(offsetof(CombinedLightTemplate, GameplaySpotlightAngle) == 0x164, "macOS 0x1014f5963, the Clone pair");
static_assert(offsetof(CombinedLightTemplate, GameplayDirectionalDimensions) == 0x16C, "macOS 0x1014f5982, the Clone pair");
static_assert(sizeof(CombinedLightTemplate) == 0x188, "macOS 0x1014f58a6 mallocs 0x188 in Clone()");
#endif

struct ConstructionVertexElement
{
    glm::vec3 Position;
    glm::vec2 UV;
    int field_14;
    int field_18;
    int field_1C;
    int field_20;
};

struct ConstructionPointSchema
{
    glm::vec4 Rotation;
    glm::vec3 Position;
    glm::vec3 Scale;
    Guid InstanceId;
    uint8_t field_38;
    uint8_t field_39;
};

struct ConstructionPoint : public ConstructionPointSchema
{
    uint8_t field_40;
    uint8_t field_41;
    Array<ConstructionPoint*> Branches;
};

struct ConstructionSpline : public ProtectedGameObject<ConstructionSpline>
{
    Guid InstanceId;
    [[bg3::hidden]] HashSet<ConstructionPoint*> Points;
    [[bg3::hidden]] LegacyRefMap<void*, void*>* field_40;
};

struct ConstructionTileTemplate;

struct TileSetNeighbourPointTileList
{
    uint32_t NonOptimalTilesStart;
    uint32_t NonOptimalTilesEnd;
    Array<ConstructionTileTemplate*> LeftCornerTiles;
    Array<ConstructionTileTemplate*> RightCornerTiles;
    Array<ConstructionTileTemplate*> StraightTiles;
};

struct TileSetConstructionNeighbourPoint
{
    Array<Guid> NeighbourPoints;
    HashMap<uint8_t, TileSetNeighbourPointTileList*> NeighbourLists;
};

struct TileSetConstructionLine;

struct TileSetConstructionPoint : public ProtectedGameObject<TileSetConstructionPoint>
{
    [[bg3::hidden]] void* VMT;
    Array<TileSetConstructionLine*> Lines;
    [[bg3::hidden]] Array<void*> field_18;
    Array<TileSetConstructionNeighbourPoint*> NeighbourPoints;
    ConstructionPoint* Point;
    uint8_t byte40;
    uint8_t byte41;
    bool ConstructionPointStop;
};

struct TileSetConstructionLine
{
    Array<TileSetConstructionPoint*> Points;
    TileSetConstructionPoint* Start;
    TileSetConstructionPoint* End;
    Guid InstanceId;
};


struct ConstructionFillingTemplate : public ProtectedGameObject<ConstructionFillingTemplate>
{
    virtual ~ConstructionFillingTemplate();

    Array<FixedString> FadeChildren;
    Array<ConstructionVertexElement> Vertices;
    Array<uint16_t> Indices;
    STDString Name;
    FixedString Id;
    FixedString Material;
    FixedString Physics;
    FixedString FadeGroup;
    float Tiling;
    float UVRotation;
    glm::vec2 UVOffset;
    float Scale;
    glm::vec4 Rotation;
    glm::vec3 Translate;
    glm::vec3 BoundMin;
    glm::vec3 BoundMax;
    bool WalkOn;
    bool SeeThrough;
    bool Fadeable;
    bool HierarchyOnlyFade;
    Guid HLOD;
    // Editor only
    // [[bg3::hidden]] void* gapC0;
    // Array<ConstructionSpline*> Splines;
    // uint8_t field_D8;
    // bool HiddenFromMinimapRendering;
};

struct ConstructionTileTemplate
{
    virtual ~ConstructionTileTemplate();

    Array<Transform> Transforms;
    Guid Tile;
    Guid HLOD;
    FixedString UUID;
    FixedString OverridePhysicsResource;
    glm::vec4 Rotation;
    glm::vec3 Translate;
    float ScaleZ;
    float Scale;
    uint8_t ShootThroughType;
    bool WalkOn;
    bool Climbable;
    bool ShootThrough;
    bool CanSeeThrough;
    bool WalkThrough;
    bool ClickThrough;
    // Editor only
    // [[bg3::hidden]] HashSet<void*> ReferenceFromNeighbourPoint; // TileSetNeighbourPointTileList*
    // Guid TemplateGuid;
    // int Side;
    // uint8_t gapA4;
    // bool Flip;
    // bool Stretchable;
};

struct ConstructionTemplate : public GameObjectTemplate
{
    __int64 field_100;
    __int64 field_108;
    Array<ConstructionTileTemplate*> Tiles;
    Array<FixedString> FadeChildren;
    OverrideableProperty<FixedString> FadeGroup;
    OverrideableProperty<FixedString> TileSet;
    OverrideableProperty<bool> ConstructionBend;
    OverrideableProperty<bool> Fadeable;
    OverrideableProperty<bool> SeeThrough;
    OverrideableProperty<bool> HierarchyOnlyFade;
    // Editor only?
    // [[bg3::hidden]] Array<TileSetConstructionPoint*> Points;
    // [[bg3::hidden]] Array<TileSetConstructionLine*> Lines;
    // [[bg3::hidden]] void* Spline;
    // [[bg3::hidden]] LegacyRefMap<int, int>* field_170;
    // [[bg3::hidden]] LegacyRefMap<int, int>* field_178;
    // OverrideableProperty<bool> WalkOn;
    // OverrideableProperty<bool> Climbable;
    // OverrideableProperty<bool> ShootThrough;
    // OverrideableProperty<uint8_t> ShootThroughType;
    // OverrideableProperty<bool> CanSeeThrough;
    // OverrideableProperty<bool> WalkThrough;
    // OverrideableProperty<bool> ClickThrough;
    // OverrideableProperty<bool> ConstructionPlaceTwoTiles;
};

struct ITriggerData : public ProtectedGameObject<ITriggerData>
{
    virtual ~ITriggerData() = 0;
};

struct ITriggerPhysicsData : public ProtectedGameObject<ITriggerPhysicsData>
{
    virtual ~ITriggerPhysicsData() = 0;
};

struct TriggerPhysicsSphere : public ITriggerPhysicsData
{
    float Radius;
};

struct TriggerPhysicsBox : public ITriggerPhysicsData
{
    glm::vec3 Extents;
};

struct TriggerPhysicsPoly : public ITriggerPhysicsData
{
    Array<glm::vec2> Points;
    float Height;
};

struct TriggerTemplate : public GameObjectTemplate
{
    [[bg3::hidden]] ITriggerData* TriggerData;
    [[bg3::hidden]] ITriggerPhysicsData* PhysicsData;
    OverrideableProperty<FixedString> TriggerType;
    OverrideableProperty<TriggerPhysicsType> PhysicsType;
    OverrideableProperty<glm::aligned_highp_vec4> Color;
    OverrideableProperty<FixedString> TriggerGizmoOverride;
    OverrideableProperty<bool> Fadeable;
    OverrideableProperty<bool> HierarchyOnlyFade;
    OverrideableProperty<bool> FadeGroupOnly;
    OverrideableProperty<bool> IgnoreRaycast;
    OverrideableProperty<FixedString> FadeGroup;
    OverrideableProperty<Array<FixedString>> FadeChildren;
    OverrideableProperty<glm::aligned_highp_vec4> GizmoColorOverride;
    OverrideableProperty<bool> UsingGizmoColorOverride;
    OverrideableProperty<bool> HasCustomPoint;
    OverrideableProperty<bg3se::Transform> CustomPointTransform;
    OverrideableProperty<Array<GlobalConfigParameter>> ConstellationConfigGlobalParameters;
    OverrideableProperty<FixedString> ConstellationConfigName;
    OverrideableProperty<uint8_t> EventSendingMode;
    OverrideableProperty<bool> OnlyCharacterEvents;
};

struct LevelTemplate : public GameObjectTemplate
{
    OverrideableProperty<FixedString> SubLevelName;
    OverrideableProperty<uint8_t> LevelTemplateType;
    OverrideableProperty<bool> IsStartingLoaded;
    OverrideableProperty<bool> IsMovablePlatform;
    OverrideableProperty<bool> IsCinematic;
    OverrideableProperty<FixedString> MovablePlatformStartSound;
    OverrideableProperty<FixedString> MovablePlatformStopSound;
    OverrideableProperty<float> SoundActivationRange;
    OverrideableProperty<bool> UseSoundOcclusion;
    OverrideableProperty<bool> IsScrollingObject;
    OverrideableProperty<bool> IsDynamicLayer;
    OverrideableProperty<glm::vec3> ScrollingOrigin;
    OverrideableProperty<glm::vec3> ScrollingDirection;
    OverrideableProperty<float> ScrollingSpeed;
    OverrideableProperty<float> ScrollingDistance;
    OverrideableProperty<float> ScrollingOffset;
    OverrideableProperty<FixedString> TemplateAfterDestruction;
    OverrideableProperty<FixedString> ConstellationConfigName;
    OverrideableProperty<Array<GlobalConfigParameter>> ConstellationConfigGlobalParameters;
};

struct DecalTemplate : public GameObjectTemplate
{
    OverrideableProperty<FixedString> MaterialUUID;
    OverrideableProperty<glm::vec2> Tiling;
    OverrideableProperty<glm::vec2> Offset;
    OverrideableProperty<glm::vec3> Dimensions;
    OverrideableProperty<float> NormalBlendingFactor;
    OverrideableProperty<float> AngleCutoff;
    OverrideableProperty<uint32_t> Layer;
    OverrideableProperty<float> Opacity;
};

struct PrefabTemplate : public GameObjectTemplate
{
    OverrideableProperty<Array<FixedString>> Children;
    OverrideableProperty<Array<bg3se::Transform>> ChildrenTransforms;
};

struct InterEntityConnection
{
    Guid OutputEntityUUID;
    FixedString OutputSocketExt;
    Guid InputEntityUUID;
    FixedString InputSocketExt;
};

struct ConstellationTemplate : public GameObjectTemplate
{
    OverrideableProperty<Guid> SchematicId;
    OverrideableProperty<FixedString> ConstellationConfigName;
    OverrideableProperty<Array<GlobalConfigParameter>> ConstellationConfigGlobalParameters;
    OverrideableProperty<Array<Guid>> Objects;
    Array<InterEntityConnection> Connections;
};

struct [[bg3::hidden]] GlobalTemplateBank : public ProtectedGameObject<GlobalTemplateBank>
{
    void* VMT;
    LegacyMap<FixedString, GameObjectTemplate*> Templates;
    Array<GameObjectTemplate*> RootTemplates;
    Array<void*> RootTemplateMap;
    Array<void*> GlobalTemplateMap;
    Array<void*> TemplateTypes;
    uint32_t NextRootTemplateHandle;
    uint32_t NextGlobalTemplateHandle;
    FixedString field_68;
};


struct [[bg3::hidden]] GlobalTemplateManager : public ProtectedGameObject<GlobalTemplateManager>
{
    void* VMT;
    LegacyMap<FixedString, GameObjectTemplate*> Templates;
    std::array<GlobalTemplateBank*, 2> Banks;
};


struct [[bg3::hidden]] CacheTemplateManagerBase : public ProtectedGameObject<CacheTemplateManagerBase>
{
    virtual ~CacheTemplateManagerBase() = 0;
    virtual void Clear(bool) = 0;
    virtual bool ShouldSave(ObjectVisitor*, GameObjectTemplate const*) = 0;
    virtual void OnLoad(ObjectVisitor*, GameObjectTemplate*) = 0;
    virtual uint32_t GetNewIndex() = 0;
    virtual bool PostVisit(ObjectVisitor*) = 0;

    GameObjectTemplate* CacheTemplate(GameObjectTemplate* tmpl, FixedString const& levelName, FixedString const& templateId);
    void RegisterTemplate(TemplateHandle handle, GameObjectTemplate* tmpl);
    void IncTemplateRef(TemplateHandle handle);
    void DecTemplateRef(TemplateHandle handle);

    TemplateType TemplateManagerType;
    HashMap<FixedString, GameObjectTemplate*> Templates;
    HashMap<uint32_t, GameObjectTemplate*> TemplatesByHandle;
    HashMap<uint32_t, uint32_t> RefCountsByHandle;
    SRWLock Lock;
    Array<GameObjectTemplate*> AddedTemplates;
    Array<GameObjectTemplate*> RemovedTemplates;
    bool EnableTemplateSync;
};


struct [[bg3::hidden]] GlobalCacheTemplateManager : public CacheTemplateManagerBase
{
    uint32_t NextTemplateId;
};


struct [[bg3::hidden]] LevelCacheTemplateManager : public CacheTemplateManagerBase
{
    FixedString LevelName;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the whole of CacheTemplateManagerBase past its three HashMaps, read off
// bin/bg3.  The lock is upstream's SRWLock class by value -- 0x48 bytes here, not MSVC's
// 0x10 (Base/Lock.h carries that measurement) -- so everything behind it moved by 0x38.
//   Lock at 0xd0 and AddedTemplates at 0x118: ls::CacheTemplateManagerBase::CacheTemplate
//     (bin/bg3 0x35559d0) takes the write side on `rbx + 0xd0` and then push_backs the new
//     template into the Array at {ptr [rbx+0x118], Capacity [rbx+0x120], Size [rbx+0x124]}.
//   EnableTemplateSync at 0x138: the same function's `cmp byte [rbx+0x138], 0` gate, which is
//     upstream's `if (EnableTemplateSync)` around exactly that push_back.
//   RemovedTemplates is not read by either matched function; it is pinned by the two
//     witnesses bracketing it -- one 16-byte Array between 0x118 and 0x138 leaves it 0x128
//     and nothing else fits.
//   sizeof: EnableTemplateSync is the last member, so 0x139 rounded to the lock's 8.
//   GlobalCacheTemplateManager::NextTemplateId sits in the base's tail padding, which is the
//     Itanium behaviour and the game's: the macOS oracle's GetNewIndex (0x1033ae090) reads
//     [rdi+0x114] where that build's EnableTemplateSync is at 0x110 -- +3 past the base's
//     dsize in both builds.  LevelCacheTemplateManager::LevelName has no witness and no
//     assert, though the same reasoning would put it at 0x13c.
static_assert(offsetof(CacheTemplateManagerBase, Lock) == 0xd0);
static_assert(offsetof(CacheTemplateManagerBase, AddedTemplates) == 0x118);
static_assert(offsetof(CacheTemplateManagerBase, RemovedTemplates) == 0x128);
static_assert(offsetof(CacheTemplateManagerBase, EnableTemplateSync) == 0x138);
static_assert(sizeof(CacheTemplateManagerBase) == 0x140);
static_assert(offsetof(GlobalCacheTemplateManager, NextTemplateId) == 0x13c);
#endif

struct [[bg3::hidden]] LocalTemplateData
{
    GameObjectTemplate* Template;
    std::atomic<uint32_t> RefCount{ 0 };
};

struct [[bg3::hidden]] LocalTemplateManager : public ProtectedGameObject<LocalTemplateManager>
{
    void IncTemplateRef(TemplateHandle handle);
    void DecTemplateRef(TemplateHandle handle);

#if defined(BG3SE_LINUX)
    // PORT (linux): the game's SRW lock, a 56-byte pthread_rwlock_t, not Win32's 8-byte
    // handle (ext/linux/shim/windows.h). Every field below was 48 low.
    LS_SRWLOCK Lock;
#else
    SRWLOCK Lock;
#endif
    LegacyRefMap<FixedString, GameObjectTemplate*> Templates;
    LegacyRefMap<uint16_t, Array<GameObjectTemplate*>*> TemplatesByType;
    LegacyRefMap<uint32_t, LocalTemplateData> TemplatesByHandle;
    LevelDataManager* LevelDataManager;
    CRITICAL_SECTION CriticalSection;
    LegacyRefMap<FixedString, LegacyRefMap<FixedString, GameObjectTemplate*>> TemplatesByLevel;
    int field_78;
};

#if defined(BG3SE_LINUX)
// PORT (linux): read off ls::LocalTemplateManager::DeleteLocalTemplate (bin/bg3 0x23986e0,
// matched s2:0.81), which walks all three maps in declaration order
// and names each by its key type:
//   pthread_rwlock_trywrlock(this) at entry, pthread_rwlock_unlock(this) at exit -- the lock
//     is at +0 and spans to the first map
//   TemplatesByHandle  count [rbx+0x58], hash [rbx+0x5c], buckets [rbx+0x60], u32 key
//   Templates          count [rbx+0x38], hash [rbx+0x3c], buckets [rbx+0x40]; the erased key
//                      is DecRef'd through gGlobalStringTable, so it is a FixedString
//   TemplatesByType    hash [rbx+0x4c], buckets [rbx+0x50], `movzx ecx, word` key
//   LevelDataManager   [rbx+0x68], then a hash walk and a tail-call to ls::LevelData::Unload
// ls::LegacyRefMap is {u32 ItemCount; u32 HashSize; Node** Buckets} = 16 bytes, by the same
// reading. Nothing witnesses CriticalSection, TemplatesByLevel or field_78; they follow from
// the measured prefix and the two lock sizes, and are not asserted.
static_assert(offsetof(LocalTemplateManager, Templates) == 0x38);
static_assert(offsetof(LocalTemplateManager, TemplatesByType) == 0x48);
static_assert(offsetof(LocalTemplateManager, TemplatesByHandle) == 0x58);
static_assert(offsetof(LocalTemplateManager, LevelDataManager) == 0x68);
#endif

END_SE()

BEGIN_NS(esv)

void IncTemplateRef(GameObjectTemplate* tmpl);
void DecTemplateRef(GameObjectTemplate* tmpl);

END_NS()

BEGIN_NS(lua)

LUA_POLYMORPHIC(GameObjectTemplate)
LUA_INFINITE_LIFETIME(GameObjectTemplate)
LUA_INFINITE_LIFETIME(EoCGameObjectTemplate)
LUA_INFINITE_LIFETIME(SceneryTemplate)
LUA_INFINITE_LIFETIME(ItemTemplate)
LUA_INFINITE_LIFETIME(CharacterTemplate)
LUA_INFINITE_LIFETIME(ProjectileTemplate)
LUA_INFINITE_LIFETIME(SurfaceTemplate)
LUA_INFINITE_LIFETIME(LightTemplate)
LUA_INFINITE_LIFETIME(CombinedLightTemplate)

END_NS()
