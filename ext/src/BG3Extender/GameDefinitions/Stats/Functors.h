#pragma once

#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/Stats/Common.h>
#include <GameDefinitions/Interrupt.h>
#include <GameDefinitions/Hit.h>
#include <GameDefinitions/Misc.h>
#include <Lua/LuaHelpers.h>

BEGIN_NS(stats)

struct ExportedConditionalRoll
{
    RollType Type;
    stats::ConditionId Conditions;
};

struct Functor
{
    virtual ~Functor();
    virtual bool ParseParams(std::span<StringView>& params, STDString& key, PropertyContext context);
    virtual Functor* Clone() const;

    FixedString UniqueName;
    Guid FunctorUuid;
    Array<ExportedConditionalRoll> RollConditions;
    stats::ConditionId StatsConditions;
    PropertyContext PropertyContext{ 0 };
    int32_t StoryActionId{ 0 };
    ObserverType ObserverType{ ObserverType::None };
    FunctorId TypeId{ FunctorId::CustomDescription };
    FunctorFlags Flags{ 0 };

    inline FixedString const& GetElementName() const
    {
        return UniqueName;
    }

    template <class T>
    std::optional<T*> Cast()
    {
        if (TypeId == T::FunctorType) {
            return static_cast<T*>(this);
        } else {
            return {};
        }
    }
};


struct ContextData
{
    virtual ~ContextData();

    [[bg3::readonly]] FunctorContextType Type{ 0 };
    int32_t StoryActionId{ 0 };
    PropertyContext PropertyContext{ 0 };
    ActionOriginator Originator;
    [[bg3::hidden]] resource::GuidResourceBankBase* ClassResources{ nullptr };
    EntityHandle HistoryEntity;
    EntityHandle StatusSource;
    HashMap<EntityHandle, int32_t> EntityToThothContextIndex;
    int field_98{ 0 };
    uint8_t ConditionCategory{ 0 };
};

struct AttackTargetContextData : public ContextData
{
    static constexpr auto ContextType = FunctorContextType::AttackTarget;

    virtual ~AttackTargetContextData() override;

    ecs::EntityRef Caster;
    ecs::EntityRef CasterProxy;
    ecs::EntityRef Target;
    ecs::EntityRef TargetProxy;
    glm::vec3 Position{ FLT_MAX };
    bool IsFromItem{ false };
    SpellIdWithPrototype SpellId;
    HitDesc Hit;
    AttackDesc Attack;
    float SomeRadius{ 2.4f };
    HitWith HitWith{ HitWith::None };
    uint32_t StatusEvent{ 0 };
    FixedString StatusId;
    [[bg3::legacy(field_26C)]] FixedString PassiveId;
    uint8_t StatusExitCause{ 3 };
    [[bg3::legacy(field_315)]] AbilityId CheckedAbility{ AbilityId::None };
    [[bg3::legacy(field_316)]] SkillId CheckedSkill{ SkillId::Sentinel };
};

struct AttackPositionContextData : public ContextData
{
    static constexpr auto ContextType = FunctorContextType::AttackPosition;

    virtual ~AttackPositionContextData() override;

    ecs::EntityRef Caster;
    glm::vec3 Position{ FLT_MAX };
    float HitRadius{ -1.0f };
    SpellIdWithPrototype SpellId;
    HitDesc Hit;
    AttackDesc Attack;
    float SurfaceChangeHeight{ 2.4f };
    bool IsFromItem{ false };
};

struct MoveContextData : public ContextData
{
    static constexpr auto ContextType = FunctorContextType::Move;

    virtual ~MoveContextData() override;

    ecs::EntityRef Caster;
    ecs::EntityRef Target;
    ecs::EntityRef Source;
    glm::vec3 Position{ FLT_MAX };
    float Distance{ 0.0f };
};

struct TargetContextData : public ContextData
{
    static constexpr auto ContextType = FunctorContextType::Target;

    virtual ~TargetContextData() override;

    ecs::EntityRef Source;
    ecs::EntityRef SourceProxy;
    glm::vec3 Position{ FLT_MAX };
    uint8_t StatusExitCause{ 3 };
    uint8_t field_C5{ 0 };
    uint8_t field_C6{ 19 };
    SpellIdWithPrototype SpellId;
    HitDesc Hit;
    AttackDesc Attack;
    uint32_t StatusEvent{ 0 };
    FixedString StatusId;
};

struct NearbyAttackedContextData : public ContextData
{
    static constexpr auto ContextType = FunctorContextType::NearbyAttacked;

    virtual ~NearbyAttackedContextData() override;

    ecs::EntityRef OriginalSource;
    ecs::EntityRef Source;
    ecs::EntityRef SourceProxy;
    ecs::EntityRef Target;
    ecs::EntityRef TargetProxy;
    glm::vec3 Position;
    bool IsFromItem{ false };
    SpellIdWithPrototype SpellId;
    HitDesc Hit;
    AttackDesc Attack;
    float SurfaceChangeHeight{ 2.4f };
    uint8_t field_314{ 2 };
};

struct NearbyAttackingContextData : public NearbyAttackedContextData
{
    static constexpr auto ContextType = FunctorContextType::NearbyAttacking;

    virtual ~NearbyAttackingContextData() override;
};

struct EquipContextData : public ContextData
{
    static constexpr auto ContextType = FunctorContextType::Equip;

    virtual ~EquipContextData() override;

    ecs::EntityRef Caster;
    ecs::EntityRef Target;
    bool UseCasterStats{ false };
};

struct SourceContextData : public ContextData
{
    static constexpr auto ContextType = FunctorContextType::Source;

    virtual ~SourceContextData() override;

    ecs::EntityRef Source;
    ecs::EntityRef SourceProxy;
};

struct InterruptContextData : public ContextData
{
    static constexpr auto ContextType = FunctorContextType::Interrupt;

    virtual ~InterruptContextData() override;

    bool OnlyAllowRollAdjustments{ false };
    ecs::EntityRef Source;
    ecs::EntityRef SourceProxy;
    ecs::EntityRef Target;
    ecs::EntityRef TargetProxy;
    ecs::EntityRef Observer;
    ecs::EntityRef ObserverProxy;
    std::optional<interrupt::RollAdjustments> RollAdjustments;
    interrupt::InterruptEvent Interrupt;
    HitDesc Hit;
    AttackDesc Attack;
    uint8_t field_428{ 0 };
    interrupt::AppliedChange Changes;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the game's class is esv::functor::InterruptContextData. bin/bg3 has no symbol for
// it and match.py resolves none of its functions, so this is read off the macOS oracle -- same
// Itanium ABI, and no lock anywhere in the record, so its absolute offsets transfer.
// esv::functor::InterruptContextData::~InterruptContextData 0x1060031f0 stores
// vtable+0x10 (0x109499dc8) at [this], then destroys exactly two members by a *named* type:
// `add rdi, 0x430` / `call eoc::interrupt::AppliedChange::~AppliedChange` at 0x106003211, and
// `lea rdi, [rbx+0x260]` / `call eoc::HitDesc::~HitDesc` at 0x1060033bd. Both land on the mirror's
// offsets as declared. That is what pins OnlyAllowRollAdjustments: stats::ContextData's data ends
// at 0x95 and its sizeof is 0x98, so Itanium puts the bool at 0x95 and Source at 0x98, while MSVC
// would put the bool at 0x98 and shift every member after it by 8 -- Hit to 0x268 and Changes to
// 0x438. The rest of the dtor corroborates the members it does not name: the array it frees at
// 0x418 with capacity/size at 0x420/0x424 is inside AttackDesc (0x408..0x428) and the one at
// 0x228/0x230/0x234 is inside Interrupt (0x158..0x260).
static_assert(offsetof(InterruptContextData, Hit) == 0x260, "macOS 0x1060033bd, the named eoc::HitDesc dtor call");
static_assert(offsetof(InterruptContextData, Changes) == 0x430, "macOS 0x106003211, the named eoc::interrupt::AppliedChange dtor call");
static_assert(offsetof(InterruptContextData, OnlyAllowRollAdjustments) == 0x95, "forced by the two above: at 0x98 both would be 8 higher");
static_assert(offsetof(InterruptContextData, Source) == 0x98, "the same chain");
#endif

struct Functors : public stats::CNamedElementManager<Functor>
{
    FixedString UniqueName;

    // Lua getter helpers
    Array<Functor*>* LuaGetFunctorList() const;
    Functor* AddNew(FunctorId action);
    //# P_GETTER(FunctorList, LuaGetFunctorList)
    //# P_FUN(AddNew, stats::Functors::AddNew)
    //# P_FUN(Remove, stats::Functors::Remove)
};

#if defined(BG3SE_LINUX)
// PORT (linux): the game's class is eoc::StatsFunctorList, and it *derives* from
// CNamedElementManager<eoc::StatsFunctorBase, int, 0, -1> exactly as the mirror does, so Itanium
// puts UniqueName in that base's tail padding -- the base's last member is `int NextHandle` at
// 0x58, its data ends at 0x5c and its sizeof is 0x60, where MSVC would put UniqueName at 0x60 and
// make this class 0x68. Measured on bin/bg3: eoc::InterruptPrototype::Init 0x2fc33d0 (the
// eoc__InterruptPrototype__Init manifest row) copies each of the prototype's three Functors from
// the stats::Object's, and each copy is the same two calls -- `lea rdi, [proto+N]` into the
// manager copy 0x2fc3ea0, then `mov esi, dword [src+0x5c]` / `lea rdi, [proto+N+0x5c]` into
// 0x3883660, which is FixedString::operator=(index): it Acquires the new index, Releases the old
// through ls::gst::Map::Release and stores. So +0x5c is a FixedString on both the source and the
// destination. The three N are 0xb8, 0x118 and 0x178 (0x2fc37cd, 0x2fc3d39, 0x2fc3da0) -- three
// consecutive members of this type at a 0x60 stride, which is the size. The macOS oracle names
// the class and re-reads the member: eoc::StatsFunctorList::~StatsFunctorList 0x101ea5f90 stores
// _ZTVN3eoc16StatsFunctorListE+0x10 at [this], then releases the FixedString at [this+0x5c]
// through ls::gst::Map::Release, then stores the CNamedElementManager<eoc::StatsFunctorBase>
// vtable and destroys the base's members at +0x48, +0x18 and +8.
static_assert(offsetof(Functors, UniqueName) == 0x5C, "bin/bg3 0x2fc3d4b / 0x2fc3d50");
static_assert(sizeof(Functors) == 0x60, "the 0x60 stride between InterruptPrototype's three Functors, bin/bg3 0x2fc37cd / 0x2fc3d39 / 0x2fc3da0");
#endif


#define DEFN_FUNCTOR(ty) \
    static constexpr auto FunctorType = FunctorId::ty; \
    ~ty##Functor() override; \
    Functor* Clone() const override;
    

struct CustomDescriptionFunctor : public Functor
{
    DEFN_FUNCTOR(CustomDescription)

    FixedString Description; // Arg0
};

struct ApplyStatusFunctor : public Functor
{
    DEFN_FUNCTOR(ApplyStatus)

    FixedString StatusId; // Arg1
    FixedString StatusSpecificParam1; // Arg4
    STDString StatusConditions; // Arg7
    stats::ConditionId Conditions; // Arg7
    int StatusSpecificParam2{ -1 }; // Arg5
    int StatusSpecificParam3{ -1 }; // Arg6
    bool RequiresConcentration{ false };
    StatsExpressionRef Duration; // Arg3
    [[bg3::legacy(HasParam6)]] bool KeepAlive{ false };
};

struct SurfaceChangeFunctor : public Functor
{
    DEFN_FUNCTOR(SurfaceChange)

    float Chance{ 1.0f };
    [[bg3::legacy(field_24)]] float DetourMaxWaitTime{ 0.0f };
    float field_28{ 0.0f };    
    [[bg3::legacy(field_2C)]] float Radius{ -1.0f };
    SurfaceChange SurfaceChange{ SurfaceChange::None };
};

struct ResurrectFunctor : public Functor
{
    DEFN_FUNCTOR(Resurrect)

    float Probability{ 1.0f }; // Arg0
    float HealthPercentage{ 1.0f }; // Arg1
    HealingType HealingType{ 0 };
};

struct SabotageFunctor : public Functor
{
    DEFN_FUNCTOR(Sabotage)

    int Amount{ 1 };
};

struct SummonFunctor : public Functor
{
    DEFN_FUNCTOR(Summon)

    FixedString Template; // Arg1
    [[bg3::legacy(AIHelper)]] FixedString AiSpellOverride; // Arg 3
    std::variant<SummonLifetimeType, float> SpawnLifetime;
    HashSet<FixedString> StatusesToApply;
    FixedString StackId;
    [[bg3::legacy(Arg5)]] bool ExtendExistingConcentration;
    [[bg3::legacy(Arg9)]] bool LateJoinPenalty;
    bool UseOwnerPassives;
};

struct ForceFunctor : public Functor
{
    DEFN_FUNCTOR(Force)

    FixedString Distance; // Arg0
    ForceFunctorOrigin Origin{ ForceFunctorOrigin::OriginToEntity }; // Arg1
    ForceFunctorAggression Aggression{ ForceFunctorAggression::Aggressive }; // Arg2
    StatsExpressionRef DistanceExpression;
    bool ControlArc{ false };
    bool PullToOrigin{ false };
};

struct DouseFunctor : public Functor
{
    DEFN_FUNCTOR(Douse)

    [[bg3::legacy(field_20)]] float Radius{ -1.0f };
    float field_24{ 1.0f };
};

struct SwapPlacesFunctor : public Functor
{
    DEFN_FUNCTOR(SwapPlaces)

    FixedString Animation; // Arg0
    bool Arg1{ false };
    bool Arg2{ false };
};

struct PickupFunctor : public Functor
{
    DEFN_FUNCTOR(Pickup)

    [[bg3::legacy(Arg0)]] FixedString Effect;
};

struct CreateSurfaceFunctor : public Functor
{
    DEFN_FUNCTOR(CreateSurface)

    FixedString SurfaceType; // Arg2 - SurfaceType or DamageType
    float Radius{ -1.0f }; // Arg0
    float Arg4{ 0.0f };
    float Duration{ -1.0f }; //Arg1
    bool IsControlledByConcentration{ true }; // Arg3
    [[bg3::legacy(Arg5)]] bool ExtendExistingConcentration{ false };
};

struct CreateConeSurfaceFunctor : public Functor
{
    DEFN_FUNCTOR(CreateConeSurface)

    FixedString SurfaceType;
    float Radius{ -1.0f };
    float Duration{ -1.0f };
    bool IsControlledByConcentration{ false };
    bool field_55{ false };
};

struct RemoveStatusFunctor : public Functor
{
    DEFN_FUNCTOR(RemoveStatus)

    FixedString StatusId; // Arg0
};

struct DealDamageFunctor : public Functor
{
    DEFN_FUNCTOR(DealDamage)

    DamageType DamageType{ DamageType::None }; // Arg2
    DealDamageWeaponType WeaponType{ DealDamageWeaponType::None }; // Arg1
    DealDamageWeaponType WeaponTypeOverride{ DealDamageWeaponType::None };
    DealDamageWeaponDamageType WeaponDamageType{ DealDamageWeaponDamageType::None }; // Arg2
    StatsExpressionRef Damage; // Arg1
    int CoinMultiplier{ 0 };
    bool Nonlethal{ false }; // Arg4
    bool Magical{ false }; // Arg3
    bool IgnoreDamageBonus{ false };
    bool IgnoreEvents{ false };
    bool ConsumeCoin{ false };
    bool IgnoreImmunities{ false };
};

struct ExecuteWeaponFunctorsFunctor : public Functor
{
    DEFN_FUNCTOR(ExecuteWeaponFunctors)

    ExecuteWeaponFunctorsType WeaponType;
};

struct RegainHitPointsFunctor : public Functor
{
    DEFN_FUNCTOR(RegainHitPoints)

    StatsExpressionRef HitPoints;
    HealingType HealingType{ HealingType::Living };
};

struct TeleportSourceFunctor : public Functor
{
    DEFN_FUNCTOR(TeleportSource)

    [[bg3::legacy(Arg1)]] bool FindValidPosition{ false };
    [[bg3::legacy(Arg2)]] bool RotateTowardsTarget{ false };
};

struct SetStatusDurationFunctor : public Functor
{
    DEFN_FUNCTOR(SetStatusDuration)

    FixedString StatusId; // Arg0
    float Duration{ 6.0f }; // Arg1
    SetStatusDurationType ChangeType{ SetStatusDurationType::ForceSet }; // Arg2
};

struct UseSpellFunctor : public Functor
{
    DEFN_FUNCTOR(UseSpell)

    FixedString SpellId; // Arg0
    bool IgnoreHasSpell{ false }; // Arg1
    bool IgnoreChecks{ false }; // Arg2
    bool Arg4{ false };
    Guid SpellCastGuid;
    [[bg3::legacy(Arg6)]] bool IgnoreBlindness{ false };
};

struct UseActionResourceFunctor : public Functor
{
    DEFN_FUNCTOR(UseActionResource)

    Guid ActionResource; // Arg0
    double Amount{ 0.0 }; // Arg1
    int Level{ 0 }; // Arg2
    bool IsPercentage{ false }; // Arg1
    bool IgnoreResourceConsumeBoosts{ false }; // Arg3
};

struct UseAttackFunctor : public Functor
{
    DEFN_FUNCTOR(UseAttack)

    bool IgnoreChecks{ false }; // Arg1
    bool IgnoreBlindness{ false }; // Arg2
};

struct CreateExplosionFunctor : public Functor
{
    DEFN_FUNCTOR(CreateExplosion)

    FixedString SpellId; // Arg0
};

struct BreakConcentrationFunctor : public Functor
{
    DEFN_FUNCTOR(BreakConcentration)
};

struct ApplyEquipmentStatusFunctor : public ApplyStatusFunctor
{
    DEFN_FUNCTOR(ApplyEquipmentStatus)

    StatsItemSlot EquipmentSlot{ StatsItemSlot::MainHand };
};

struct RestoreResourceFunctor : public Functor
{
    DEFN_FUNCTOR(RestoreResource)

    Guid ActionResource; // Arg0
    [[bg3::legacy(Hex)]] int Level{ 0 }; // Arg2
    StatsExpressionRef AmountExpression; // Arg1
    double Amount{ 0.0 }; // Arg1
    RestoreResourceAmountType AmountType{ RestoreResourceAmountType::None };
};

struct SpawnFunctor : public Functor
{
    DEFN_FUNCTOR(Spawn)

    FixedString TemplateId; // Arg0
    FixedString AIHelper;
    HashSet<FixedString> StatusesToApply;
    [[bg3::legacy(Arg7)]] bool ApplyLateJoinPenalty{ false };
};

struct StabilizeFunctor : public Functor
{
    DEFN_FUNCTOR(Stabilize)
};

struct UnlockFunctor : public Functor
{
    DEFN_FUNCTOR(Unlock)
};

struct ResetCombatTurnFunctor : public Functor
{
    DEFN_FUNCTOR(ResetCombatTurn)
};

struct RemoveAuraByChildStatusFunctor : public Functor
{
    DEFN_FUNCTOR(RemoveAuraByChildStatus)

    FixedString StatusId; // Arg0
};

struct SummonInInventoryFunctor : public Functor
{
    DEFN_FUNCTOR(SummonInInventory)

    FixedString TemplateId;
    [[bg3::legacy(Arg8)]] FixedString AiSpellOverride;
    std::variant<SummonLifetimeType, float> Lifetime;
    [[bg3::legacy(AdditionalArgs)]] HashSet<FixedString> StatusesToApply;
    [[bg3::legacy(Arg9)]] FixedString SpellContainer;
    int32_t Amount{ 0 };
    bool PlaceInInventory{ false };
    bool Equip{ false };
    [[bg3::legacy(Arg6)]] bool AllowOffHand{ false };
    [[bg3::legacy(Arg7)]] bool ExtendExistingConcentration{ false };
};

struct SpawnInInventoryFunctor : public Functor
{
    DEFN_FUNCTOR(SpawnInInventory)

    FixedString TemplateId;
    [[bg3::legacy(Arg7)]] FixedString AiSpellOverride;
    [[bg3::legacy(Arg3)]] int32_t Amount{ 0 };
    [[bg3::legacy(Arg4)]] bool PlaceInInventory{ false };
    [[bg3::legacy(Arg5)]] bool Equip{ false };
    [[bg3::legacy(Arg6)]] bool AllowOffHand{ false };
    [[bg3::legacy(AdditionalArgs)]] HashSet<FixedString> StatusesToApply;
};

struct RemoveUniqueStatusFunctor : public Functor
{
    DEFN_FUNCTOR(RemoveUniqueStatus)

    FixedString StatusId; // Arg0
};

struct DisarmWeaponFunctor : public Functor
{
    DEFN_FUNCTOR(DisarmWeapon)
};

struct DisarmAndStealWeaponFunctor : public Functor
{
    DEFN_FUNCTOR(DisarmAndStealWeapon)
};

struct SwitchDeathTypeFunctor : public Functor
{
    DEFN_FUNCTOR(SwitchDeathType)

    DeathType DeathType;
};

struct TriggerRandomCastFunctor : public Functor
{
    DEFN_FUNCTOR(TriggerRandomCast)

    [[bg3::legacy(Arg1)]] uint8_t DC;
    [[bg3::legacy(Arg2)]] float Delay;
    Array<FixedString> RandomCastOutcomes;
};

struct GainTemporaryHitPointsFunctor : public Functor
{
    DEFN_FUNCTOR(GainTemporaryHitPoints)

    StatsExpressionRef HitPoints;
};

struct FireProjectileFunctor : public Functor
{
    DEFN_FUNCTOR(FireProjectile)

    [[bg3::legacy(Arg1)]] FixedString ProjectileTemplate;
};

struct ShortRestFunctor : public Functor
{
    DEFN_FUNCTOR(ShortRest)
};

struct CreateZoneFunctor : public Functor
{
    DEFN_FUNCTOR(CreateZone)

    ZoneShape Shape; // Arg1
    FixedString Arg4;
    float Arg2;
    float Duration; // Arg3
    bool Arg5;
};

struct DoTeleportFunctor : public Functor
{
    DEFN_FUNCTOR(DoTeleport)

    float Arg1;
};

struct RegainTemporaryHitPointsFunctor : public Functor
{
    DEFN_FUNCTOR(RegainTemporaryHitPoints)

    StatsExpressionRef HitPoints;
};

struct RemoveStatusByLevelFunctor : public Functor
{
    DEFN_FUNCTOR(RemoveStatusByLevel)

    std::variant<FixedString, StatusGroup> NameOrStatusGroup; // Arg1
    [[bg3::legacy(Arg2)]] int8_t Level;
    std::optional<AbilityId> Ability;
};

struct SurfaceClearLayerFunctor : public Functor
{
    DEFN_FUNCTOR(SurfaceClearLayer)

    HashSet<SurfaceLayer8> Layers;
};

struct UnsummonFunctor : public Functor
{
    DEFN_FUNCTOR(Unsummon)
};

struct CreateWallFunctor : public Functor
{
    DEFN_FUNCTOR(CreateWall)
};

struct CounterspellFunctor : public Functor
{
    DEFN_FUNCTOR(Counterspell)
};

struct AdjustRollFunctor : public Functor
{
    DEFN_FUNCTOR(AdjustRoll)

    StatsExpressionRef Expression; // Arg1
    RollAdjustmentType Type; // Arg2
    DamageType DamageType; // Arg2
};

struct SpawnExtraProjectilesFunctor : public Functor
{
    DEFN_FUNCTOR(SpawnExtraProjectiles)

    [[bg3::legacy(Arg1)]] FixedString ProjectileSpell;
};

struct KillFunctor : public Functor
{
    DEFN_FUNCTOR(Kill)
};

struct TutorialEventFunctor : public Functor
{
    DEFN_FUNCTOR(TutorialEvent)

    Guid Event;
};

struct DropFunctor : public Functor
{
    DEFN_FUNCTOR(Drop)

    [[bg3::legacy(Arg1)]] FixedString Effect;
};

struct ResetCooldownsFunctor : public Functor
{
    DEFN_FUNCTOR(ResetCooldowns)

    SpellCooldownType CooldownType;
};

struct SetRollFunctor : public Functor
{
    DEFN_FUNCTOR(SetRoll)

    int Roll;
    RollAdjustmentType Type; // Arg2
    DamageType DamageType; // Arg2
};

struct SetDamageResistanceFunctor : public Functor
{
    DEFN_FUNCTOR(SetDamageResistance)

    DamageType DamageType; // Arg1
};

struct SetRerollFunctor : public Functor
{
    DEFN_FUNCTOR(SetReroll)

    uint8_t Roll;
    [[bg3::legacy(Arg2)]] bool KeepNew;
};

struct SetAdvantageFunctor : public Functor
{
    DEFN_FUNCTOR(SetAdvantage)
};

struct SetDisadvantageFunctor : public Functor
{
    DEFN_FUNCTOR(SetDisadvantage)
};

struct MaximizeRollFunctor : public Functor
{
    DEFN_FUNCTOR(MaximizeRoll)

    DamageType DamageType;
};

struct CameraWaitFunctor : public Functor
{
    DEFN_FUNCTOR(CameraWait)

    float Arg1;
};

struct ModifySpellCameraFocusFunctor : public Functor
{
    DEFN_FUNCTOR(ModifySpellCameraFocus)

    // Unmapped
};

struct ExtenderFunctor : public Functor
{
    DEFN_FUNCTOR(Extender)
};

#if defined(BG3SE_LINUX)
// PORT (linux): the stats::Functor family, measured against bin/bg3 (c8da62b1).
// Functor is 0x48 bytes with its last member (Flags, one byte) at 0x46, so it has a
// single byte of tail padding at 0x47, and Itanium lets a derived functor reuse it.  MSVC
// does not, so every functor whose first derived member is one byte sits at 0x47 here and
// would sit at 0x48 under upstream's own compiler.  The game does the Itanium thing.
//   The whole family is built in one function, eoc::StatsFunctorBase::ParseStatsFunctor
//     (bin/bg3 0x2b87470): a 62-way jump table over FunctorId, each arm
//     `mov edi, <sizeof>; call operator new; mov esi, <FunctorId>; call 0x2b893c0` (the
//     base ctor) and then the derived vtable plus the derived members' default values.
//     So the arm states the game's sizeof AND the derived offsets, and `esi` names which
//     class it is without needing a symbol.  All 62 of the game's allocation sizes equal
//     sizeof() of the matching mirror, base included.
//   The base, from eoc::StatsFunctorBase::StatsFunctorBase(FunctorId) (bin/bg3 0x2b893c0):
//     [rdi] = vtable, [rdi+8] = -1 dword (UniqueName, the null FixedString index), two
//     qwords at +0x10/+0x18 (FunctorUuid), zeroes over +0x20..+0x2f (RollConditions),
//     [+0x30] = -1 dword (StatsConditions), [+0x38] = 3 as a *qword* (PropertyContext,
//     8 bytes wide -- corroborated by the ExecuteWeaponFunctors arm's
//     `mov qword [r14+0x38], 0xffffffffffff9fff` at 0x2b882b3), [+0x40] = 0 dword
//     (StoryActionId), [+0x44] = 0 byte (ObserverType), [+0x45] = the ctor's FunctorId
//     argument (TypeId) and `and byte [+0x46], 0xf8` (Flags).  Nothing is written at
//     +0x47: the base stops at 0x46 and leaves that byte to the derived class.
//   sizeof(Functor) == 0x48 is stated by the thirteen functors that declare no members of
//     their own -- BreakConcentration, Stabilize, Unlock, ResetCombatTurn, DisarmWeapon,
//     DisarmAndStealWeapon, ShortRest, Unsummon, CreateWall, Counterspell, Kill,
//     SetAdvantage, SetDisadvantage -- every one of which allocates 0x48.
//   The derived classes whose first member lands in that padding byte, each by its arm's
//     own store (address = the store, not the arm's head):
//       0x2b882ae  mov byte [+0x47], 0        ExecuteWeaponFunctors  (alloc 0x48)
//       0x2b88710  mov byte [+0x47], 0        SwitchDeathType, ResetCooldowns (0x48)
//       0x2b88964  mov byte [+0x47], 0xe      SetDamageResistance, MaximizeRoll (0x48)
//       0x2b887d4  mov word [+0x47], 0        TeleportSource   (0x50, two bools)
//       0x2b88992  mov word [+0x47], 1        UseAttack        (0x50, two bools)
//       0x2b88b05  mov word [+0x47], 0x101    SetReroll        (0x50, u8 + bool)
//       0x2b88903  byte [+0x47], dword [+0x48] = -1.0f, xmm0 over [+0x50..0x5f]
//                                            TriggerRandomCast (0x60: DC, Delay, Array)
//       0x2b88def  byte [+0x47], dword [+0x48] = -1, movsd [+0x4c] = {-1.0f,-1.0f},
//                  byte [+0x54]                CreateZone      (0x58, all five members)
//       0x2b8805d  dword [+0x47] = 0          DealDamage       (0x68) -- a 4-byte store at
//                  an odd offset, which only four consecutive 1-byte members allow
//   Two-sided, from the same table: an arm whose first derived member is wider than the
//     padding byte puts it at 0x48, not 0x47 -- Sabotage's `int Amount` is
//     `mov dword [+0x48], 1` (0x2b88f93), CustomDescription's and Drop's FixedString is
//     `mov dword [+0x48], -1` (0x2b881b7), DoTeleport's float is `mov dword [+0x48], 0`
//     (0x2b88873).  All three allocate 0x50.  So 0x47 is not an artefact of looking only
//     at classes that would show the effect.
//   DealDamage's members past the enums are named individually by
//     eoc::StatsFunctorDealDamage::Parse (bin/bg3 0x2b95e80): byte [+0x47] (0x2b960ca),
//     byte [+0x48] (0x2b96233), byte [+0x49] (0x2b95eda), byte [+0x4a] (0x2b960b7),
//     dword [+0x58] (0x2b96064), then the six bools at 0x5c (0x2b96083), 0x5d (0x2b960a3),
//     0x5e (0x2b95fb2), 0x5f (0x2b95f7f), 0x60 (0x2b95f4c) and 0x61 (0x2b95f13).
//     Damage (+0x50) gets no assert of its own: nothing stores it in either function, and
//     it is pinned only by exclusion between WeaponDamageType and CoinMultiplier.
//   ApplyStatus and its one derived class are a matched pair of stores.  The
//     ApplyStatus arm (0x2b87681..0x2b876b8) writes -1 qword at +0x48 (StatusId 0x48 and
//     StatusSpecificParam1 0x4c, both null FixedStrings), zeroes +0x50..0x5f
//     (StatusConditions, an STDString), -1 qword at +0x60 (Conditions 0x60,
//     StatusSpecificParam2 0x64), -1 dword at +0x68, 0 byte at +0x6c, 0 qword at +0x70
//     and `mov byte [+0x78], 0` (KeepAlive).  The ApplyEquipmentStatus arm is the same
//     sequence with that last store widened to `mov word [r14+0x78], 0x1600`
//     (0x2b884d5): KeepAlive 0 at 0x78 and EquipmentSlot 0x16 at 0x79, inside
//     ApplyStatusFunctor's own tail padding.  Both allocate 0x80.
//   Three default values differ from upstream's initialisers, and all three are the game's
//     own defaults rather than a mirror error -- each member re-witnessed a second time, off
//     its class's ParseParams (vtable slot 2), independently of the arm.  Addresses here are
//     a187af94, the build the second reading was taken on.
//       UseAttackFunctor::IgnoreChecks is true here (upstream `{false}`).  ParseParams
//       (vtable 0x7914168 slot 2 = 0x4044c60) puts param[0] in [+0x47] (0x4044cc9) and
//       param[1] in [+0x48] (0x4044cab), which is upstream's own Arg1/Arg2 order, so 0x47
//       is IgnoreChecks and not IgnoreBlindness; with no params it writes neither
//       (0x4044c88), so the ctor's `mov word [+0x47], 1` is what a `UseAttack()` gets.
//       ApplyEquipmentStatusFunctor::EquipmentSlot is StatsItemSlot::Sentinel (0x16, upstream
//       `MainHand`).  ParseParams (vtable 0x79141f8 slot 2 = 0x2c65400) compares param[0]
//       against "Helmet".."MainHand"/"OffHand" and stores the ordinal with
//       `mov byte [rdi+0x79], cl` (0x2c6578e) -- 0x79 named a second time; with no params it
//       branches to 0x2c65814 and never stores, so Sentinel survives.
//       Functor::PropertyContext is 3 = TARGET|AOE (upstream `{0}`).  ParseStatsFunctor ends
//       `cmp r12, 3; jne` -> `mov qword [r14+0x38], r12` (0x2b878bc/0x2b880d5), r12 being the
//       PropertyContext it also passes as ParseParams' 4th argument (0x2b87756): the parse
//       context is assigned over the default, and the compare against 3 is the store elided
//       when it would be a no-op -- which only holds if the ctor wrote 3 to that member.
//     No assert on any of the three: an initialiser only ever runs for a functor the
//     extender itself constructs (RPGStats::ConstructFunctor -> GameAlloc<T>, reached from
//     Lua by Functors::AddNew), never for one the game parsed, so it cannot make a mirror
//     read a game object wrongly -- and upstream's value is the same on Windows, where the
//     game's parser-side default is the same too, so changing it here would make Linux mods
//     behave differently from Windows ones.  The offsets are asserted; the defaults are not.
static_assert(sizeof(Functor) == 0x48);
static_assert(offsetof(Functor, UniqueName) == 0x8);
static_assert(offsetof(Functor, FunctorUuid) == 0x10);
static_assert(offsetof(Functor, RollConditions) == 0x20);
static_assert(offsetof(Functor, StatsConditions) == 0x30);
static_assert(offsetof(Functor, PropertyContext) == 0x38);
static_assert(offsetof(Functor, StoryActionId) - offsetof(Functor, PropertyContext) == 8);
static_assert(offsetof(Functor, StoryActionId) == 0x40);
static_assert(offsetof(Functor, ObserverType) == 0x44);
static_assert(offsetof(Functor, TypeId) == 0x45);
static_assert(offsetof(Functor, Flags) == 0x46);

static_assert(offsetof(ExecuteWeaponFunctorsFunctor, WeaponType) == 0x47);
static_assert(sizeof(ExecuteWeaponFunctorsFunctor) == 0x48);
static_assert(offsetof(SwitchDeathTypeFunctor, DeathType) == 0x47);
static_assert(sizeof(SwitchDeathTypeFunctor) == 0x48);
static_assert(offsetof(ResetCooldownsFunctor, CooldownType) == 0x47);
static_assert(sizeof(ResetCooldownsFunctor) == 0x48);
static_assert(offsetof(SetDamageResistanceFunctor, DamageType) == 0x47);
static_assert(sizeof(SetDamageResistanceFunctor) == 0x48);
static_assert(offsetof(MaximizeRollFunctor, DamageType) == 0x47);
static_assert(sizeof(MaximizeRollFunctor) == 0x48);

static_assert(offsetof(TeleportSourceFunctor, FindValidPosition) == 0x47);
static_assert(offsetof(TeleportSourceFunctor, RotateTowardsTarget) == 0x48);
static_assert(sizeof(TeleportSourceFunctor) == 0x50);
static_assert(offsetof(UseAttackFunctor, IgnoreChecks) == 0x47);
static_assert(offsetof(UseAttackFunctor, IgnoreBlindness) == 0x48);
static_assert(sizeof(UseAttackFunctor) == 0x50);
static_assert(offsetof(SetRerollFunctor, Roll) == 0x47);
static_assert(offsetof(SetRerollFunctor, KeepNew) == 0x48);
static_assert(sizeof(SetRerollFunctor) == 0x50);

static_assert(offsetof(TriggerRandomCastFunctor, DC) == 0x47);
static_assert(offsetof(TriggerRandomCastFunctor, Delay) == 0x48);
static_assert(offsetof(TriggerRandomCastFunctor, RandomCastOutcomes) == 0x50);
static_assert(sizeof(TriggerRandomCastFunctor) == 0x60);

static_assert(offsetof(CreateZoneFunctor, Shape) == 0x47);
static_assert(offsetof(CreateZoneFunctor, Arg4) == 0x48);
static_assert(offsetof(CreateZoneFunctor, Arg2) == 0x4c);
static_assert(offsetof(CreateZoneFunctor, Duration) == 0x50);
static_assert(offsetof(CreateZoneFunctor, Arg5) == 0x54);
static_assert(sizeof(CreateZoneFunctor) == 0x58);

static_assert(offsetof(DealDamageFunctor, DamageType) == 0x47);
static_assert(offsetof(DealDamageFunctor, WeaponType) == 0x48);
static_assert(offsetof(DealDamageFunctor, WeaponTypeOverride) == 0x49);
static_assert(offsetof(DealDamageFunctor, WeaponDamageType) == 0x4a);
static_assert(offsetof(DealDamageFunctor, CoinMultiplier) == 0x58);
static_assert(offsetof(DealDamageFunctor, Nonlethal) == 0x5c);
static_assert(offsetof(DealDamageFunctor, Magical) == 0x5d);
static_assert(offsetof(DealDamageFunctor, IgnoreDamageBonus) == 0x5e);
static_assert(offsetof(DealDamageFunctor, IgnoreEvents) == 0x5f);
static_assert(offsetof(DealDamageFunctor, ConsumeCoin) == 0x60);
static_assert(offsetof(DealDamageFunctor, IgnoreImmunities) == 0x61);
static_assert(sizeof(DealDamageFunctor) == 0x68);

static_assert(offsetof(ApplyStatusFunctor, StatusId) == 0x48);
static_assert(offsetof(ApplyStatusFunctor, StatusSpecificParam1) == 0x4c);
static_assert(offsetof(ApplyStatusFunctor, StatusConditions) == 0x50);
static_assert(offsetof(ApplyStatusFunctor, Conditions) == 0x60);
static_assert(offsetof(ApplyStatusFunctor, StatusSpecificParam2) == 0x64);
static_assert(offsetof(ApplyStatusFunctor, StatusSpecificParam3) == 0x68);
static_assert(offsetof(ApplyStatusFunctor, RequiresConcentration) == 0x6c);
static_assert(offsetof(ApplyStatusFunctor, Duration) == 0x70);
static_assert(offsetof(ApplyStatusFunctor, KeepAlive) == 0x78);
static_assert(sizeof(ApplyStatusFunctor) == 0x80);
static_assert(offsetof(ApplyEquipmentStatusFunctor, EquipmentSlot) == 0x79);
static_assert(sizeof(ApplyEquipmentStatusFunctor) == 0x80);

// The three sizes that need no measurement of their own but calibrate the arm-size
// reading above: an 8-aligned first derived member cannot use the padding byte.
static_assert(offsetof(SabotageFunctor, Amount) == 0x48);
static_assert(sizeof(SabotageFunctor) == 0x50);
static_assert(offsetof(CustomDescriptionFunctor, Description) == 0x48);
static_assert(sizeof(CustomDescriptionFunctor) == 0x50);
static_assert(offsetof(DoTeleportFunctor, Arg1) == 0x48);
static_assert(sizeof(DoTeleportFunctor) == 0x50);
#endif



END_NS()

BEGIN_NS(lua)

LUA_POLYMORPHIC(bg3se::stats::ContextData)
LUA_POLYMORPHIC(bg3se::stats::Functor)

END_NS()
