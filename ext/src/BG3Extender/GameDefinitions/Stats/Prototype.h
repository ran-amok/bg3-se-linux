#pragma once

#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/Hit.h>
#include <GameDefinitions/Misc.h>
#include <GameDefinitions/Stats/Functors.h>

BEGIN_NS(stats)

struct DescriptionInfo
{
    TranslatedString DisplayName;
    FixedString Icon;
    TranslatedString Description;
    STDString DescriptionParams;
    TranslatedString ExtraDescription;
    STDString ExtraDescriptionParams;
    TranslatedString ShortDescription;
    STDString ShortDescriptionParams;
    TranslatedString LoreDescription;
};

struct TextKeyHitCosts
{
    FixedString TextKey;
    Array<ActionResourceCost> Costs;
};

struct VariableResourceCost
{
    Guid Resource;
    float Amount;
};

struct SpellPrototypeAnimationData
{
    std::array<FixedString, 3> Part0;
    std::array<FixedString, 3> Part6;
    std::array<FixedString, 3> Part4;
    std::array<FixedString, 3> Part1;
    std::array<FixedString, 3> Part5;
    Array<std::array<FixedString, 3>> Part2;
    std::array<FixedString, 3> Part3;
    std::array<FixedString, 3> Part7;
    std::array<FixedString, 3> Part8;
    uint8_t Flags{ 0 };
};

struct SpellMetaCondition
{
    ConditionId Condition;
    bool OverrideOriginal;
};

struct SpellPrototype : public Noncopyable<SpellPrototype>
{
    [[bg3::readonly]] int StatsObjectIndex;
    SpellType SpellTypeId;
    FixedString SpellId;
    uint8_t SpellSchool;
    SpellFlags SpellFlags;
    uint8_t SpellActionType;
    uint8_t SpellAnimationType;
    uint8_t SpellJumpType;
    uint8_t SpellHitAnimationType;
    uint8_t SpellAnimationIntentType;
    uint8_t HitAnimationType;
    uint32_t LineOfSightFlags;
    uint32_t CinematicArenaFlags;
    Guid CinematicArenaTimelineOverride;
    uint32_t SpellCategory;
    int Level;
    int PowerLevel;
    bool HasMemoryCost;
    FixedString SpellContainerId;
    int RechargeValuesFrom;
    int RechargeValuesTo;
    DiceSizeId DiceValue;
    int8_t Cooldown;
    uint32_t WeaponTypes;
    DescriptionInfo Description;
    uint8_t AiFlags;
    uint8_t field_101;
    DamageType DamageType;
    SpellPrototype* ParentPrototype;
    Array<SpellPrototype*> ChildPrototypes;
    Array<ActionResourceCost> UseCosts;
    Array<ActionResourceCost> RitualCosts;
    Array<ActionResourceCost> DualWieldingUseCosts;
    Array<TextKeyHitCosts> HitCostGroups;
    Array<VariableResourceCost> VariableUseCosts;
    Array<VariableResourceCost> VariableDualWieldingUseCosts;
    Array<VariableResourceCost> VariableRitualCosts;
    uint32_t VerbalIntent;
    SpellPrototypeAnimationData SpellAnimation;
    SpellPrototypeAnimationData DualWieldingSpellAnimation;
    FixedString PrepareEffect;
    FixedString PrepareSound;
    FixedString PrepareLoopSound;
    FixedString CastSound;
    uint8_t CastSoundType;
    [[bg3::legacy(field_299)]] bool IsWeaponAttack;
    uint8_t Sheathing;
    Array<FixedString> AlternativeCastTextEvents;
    int8_t SourceLimbIndex;
    Array<FixedString> ContainerSpells;
    Array<Array<FixedString>> Trajectories;
    uint32_t PathfindTemplateId;
    bool HasTrajectoryRules;
    uint32_t RequirementEvents;
    HashMap<SpellMetaConditionType, Array<SpellMetaCondition>> MetaConditions;
    FixedString ItemWall;
    FixedString InterruptPrototype;
    FixedString CombatAIOverrideSpell;
    Array<FixedString> CombatAIOverrideSpells;
    float SteerSpeedMultipler;

    Object* GetStats() const;
};

// PORT (linux): witnesses off eoc::SpellPrototype::Init, bin/bg3 0x5e35980 -- the function upstream
// calls through eoc__SpellPrototype__Init (manifest row). It takes (rdi = this,
// rsi = FixedString const*) and fills the prototype from the stats object it looks up in
// RPGStats::m_ptr, so each offset below is a store it makes or a member it reads by name.
static_assert(offsetof(SpellPrototype, StatsObjectIndex) == 0x0, "bin/bg3 0x5e35980 stores the looked-up index at [proto+0] (0x5e35a3a) and -1 when the name misses (0x5e35a21)");
static_assert(offsetof(SpellPrototype, SpellTypeId) == 0x4, "bin/bg3 0x5e35e2b compares dword [proto+4] with the SpellType value 6");
static_assert(offsetof(SpellPrototype, SpellId) == 0x8, "bin/bg3 0x5e359d4 stores the argument's FixedString index at [proto+8] after gst Acquire/Release on the old one");
static_assert(offsetof(SpellPrototype, SpellFlags) == 0x10, "bin/bg3 0x5e35e07 reads [proto+0x10] as one 64-bit word, bts's up to 58 bits of it from GetMappedValue(EoCFS::strSpellFlags) and stores it back at 0x5e35e1e");
static_assert(sizeof(SpellFlags) == 8, "bin/bg3 0x5e35e07 loads [proto+0x10] with a single qword mov and bts's bit 0x39 of it");
static_assert(offsetof(SpellPrototype, Level) == 0x3C, "bin/bg3 0x5e35a8e stores GetMappedValue(EoCFS::strLevel) into dword [proto+0x3c]");


struct [[bg3::hidden]] SpellPrototypeManager : public ProtectedGameObject<SpellPrototypeManager>
{
    void* VMT;
    HashMap<FixedString, SpellPrototype*> Spells;
    HashMap<FixedString, SpellPrototype*> CombatAIOverrideSpells;
    Array<FixedString> SpellNames;
    bool Initialized;

    void SyncStat(Object* object);
    bool SyncStat(Object* object, SpellPrototype* proto);
};

struct StatsSoundInfo
{
    FixedString Key;
    uint8_t Type;
};

struct StatusPrototype : public Noncopyable<StatusPrototype>
{
    [[bg3::readonly]] int StatsObjectIndex{ -1 };
    StatusType StatusId;
    FixedString StatusName;
    uint64_t StatusPropertyFlags;
    uint64_t StatusGroups;
    DescriptionInfo Description;
    uint32_t StackType;
    uint8_t LEDEffect;
    uint8_t TickType;
    uint32_t ImmuneFlag;
    uint8_t Flags;
    ObjectSet<SurfaceType>* AbsorbSurfaceTypes{ nullptr };
    Array<Guid> Boosts;
    uint32_t RemoveEvents;
    Array<StatsSoundInfo> SoundStart;
    Array<StatsSoundInfo> SoundLoop;
    Array<StatsSoundInfo> SoundStop;
    uint8_t HitAnimationType;
    uint8_t Sheathing;
    uint8_t AuraFlags;

    Object* GetStats() const;
};

// PORT (linux): witnesses off eoc::StatusPrototype::Init, bin/bg3 0x27e4d90 -- the function upstream
// calls through eoc__StatusPrototype__Init (manifest row). (rdi = this,
// rsi = FixedString const*, edx = the uint8_t flags.) Every offset it touches on `this` matches this
// mirror member for member; the five below are the ones a named argument or the third parameter pins.
// Boosts (+0xC0) is the one member it never writes, which is why upstream calls eoc__ParseStaticBoosts
// after it.
static_assert(offsetof(StatusPrototype, StatsObjectIndex) == 0x0, "bin/bg3 0x27e4e38 stores -1 into dword [proto+0] when the name misses, the looked-up index at 0x27e4e71");
static_assert(offsetof(StatusPrototype, StatusName) == 0x8, "bin/bg3 0x27e4dea stores the argument's FixedString index at [proto+8] after gst Acquire/Release on the old one");
static_assert(offsetof(StatusPrototype, Flags) == 0xB4, "bin/bg3 0x27e4e3e puts bit 0 of the third argument into [proto+0xb4]: mov al,[proto+0xb4]; and al,0xfe; or al,bpl");
static_assert(offsetof(StatusPrototype, AbsorbSurfaceTypes) == 0xB8, "bin/bg3 0x27e5abd stores the new(0x10) ObjectSet it fills from the Surface_Type column (EoCFS::strSurface_Type, 0x27e5a86) at [proto+0xb8]");
static_assert(offsetof(StatusPrototype, HitAnimationType) == 0x108, "bin/bg3 0x27e511b stores GetMappedValue(eoc::hit::strHitAnimationType) into byte [proto+0x108]");


struct [[bg3::hidden]] StatusPrototypeManager : public ProtectedGameObject<StatusPrototypeManager>
{
    void* VMT;
    HashMap<FixedString, StatusPrototype*> Statuses;
    Array<FixedString> StatusNames;
    bool Initialized;

    void SyncStat(Object* object);
    bool SyncStat(Object* object, StatusPrototype* proto);
};


struct PassivePrototype : public Noncopyable<PassivePrototype>
{
    uint32_t Properties{ 0 };
    FixedString Name;
    DescriptionInfo Description;
    stats::ConditionId EnabledConditions;
    uint64_t EnabledContext{ 0 };
    FixedString ToggleOnEffect;
    FixedString ToggleOffEffect;
    uint64_t StatsFunctorContext{ 0 };
    stats::ConditionId ConditionsIndex;
    stats::Functors StatsFunctors;
    stats::Functors ToggleOnFunctors;
    stats::Functors ToggleOffFunctors;
    FixedString ToggleGroup;
    uint64_t ToggleOffContext{ 0 };
    uint64_t BoostContext{ 0 };
    stats::ConditionId BoostConditionsIndex;
    Array<Guid> Boosts;
    int32_t PriorityOrder{ 0 };
    FixedString TooltipConditionalDamage;
};

#if defined(BG3SE_LINUX)
// PORT (linux): witnesses off eoc::Passives::Init, bin/bg3 0x2fc1b00 -- the manager Init that LTO
// inlined the per-prototype Init into (targets-hand.tsv eoc__PassivePrototype__Init). It
// allocates one RefMap node per passive at 0x2fc1c1f (`mov edi, 0x220`), writes
// Next at [node], the key FixedString at [node+8] and constructs the value at [node+0x10]
// (0x2fc1c45, ctor 0x2fc3190), so every offset below is a node offset minus 0x10. The four
// FixedString members it fills are unmistakable -- each is the same read/Acquire/Release/store
// dance around ls::gst -- and they bracket the record: Name at node+0x14 (0x2fc1d8b),
// TooltipConditionalDamage, the last member, at node+0x21c (0x2fc29f7). Description at node+0x18
// goes to the shared description helper 0x2b85570 (0x2fc1d96), whose caller set is exactly the
// four prototype families. The three Functors are `lea rbp, [node+0xc8]`, `[node+0x128]` and
// `[node+0x188]` (0x2fc2234, 0x2fc2362, 0x2fc24bb) -- a 0x60 stride -- and each one's UniqueName
// is read at +0x5c into it (node+0x124, +0x184, +0x1e4), which is a second independent witness for
// the Functors layout asserted in Functors.h. This record is ABI-sensitive only through those
// three: under MSVC each Functors would be 0x68, putting ToggleOnFunctors at 0x120 and
// ToggleOffFunctors at 0x188, TooltipConditionalDamage at 0x224 and the allocation at 0x238.
static_assert(sizeof(PassivePrototype) == 0x210, "bin/bg3 0x2fc1c1f allocates 0x220 for a {Next, Key, PassivePrototype} node whose value starts at +0x10");
static_assert(offsetof(PassivePrototype, Name) == 0x4, "bin/bg3 0x2fc1d8b stores the stats object's Name index at node+0x14");
static_assert(offsetof(PassivePrototype, Description) == 0x8, "bin/bg3 0x2fc1d96 passes node+0x18 to the shared description helper 0x2b85570");
static_assert(offsetof(PassivePrototype, StatsFunctors) == 0xB8, "bin/bg3 0x2fc2234 takes the address of node+0xc8");
static_assert(offsetof(PassivePrototype, ToggleOnFunctors) == 0x118, "bin/bg3 0x2fc2362 takes the address of node+0x128");
static_assert(offsetof(PassivePrototype, ToggleOffFunctors) == 0x178, "bin/bg3 0x2fc24bb takes the address of node+0x188");
static_assert(offsetof(PassivePrototype, TooltipConditionalDamage) == 0x20C, "bin/bg3 0x2fc29f7 stores a FixedString index at node+0x21c, the last member");
#endif


struct [[bg3::hidden]] PassivePrototypeManager : public ProtectedGameObject<PassivePrototypeManager>
{
    void* VMT;
    LegacyRefMap<FixedString, PassivePrototype> Passives;
    bool Initialized;

    void SyncStat(stats::Object* object);
    bool SyncStat(stats::Object* object, PassivePrototype* proto);
};

#if defined(BG3SE_LINUX)
// The node of Passives, from the same allocation: eoc::Passives::Init 0x2fc1b00 writes
// `mov qword [node], 0` (Next), `mov dword [node+8], 0xffffffff` (the null key FixedString) at
// 0x2fc1c35 / 0x2fc1c3d and constructs the value at node+0x10 (0x2fc1c45), out of the 0x220 it
// asked for at 0x2fc1c1f. Affected only -- its own three members are ABI-neutral -- but the size
// still discriminates, because the value inside it does: MSVC would make the node 0x238.
// (the alias is only here because offsetof() is a macro and cannot take a comma in a template
// argument list)
namespace port_layout { using PassiveMapNode = bg3se::MapNode<FixedString, PassivePrototype>; }
static_assert(offsetof(port_layout::PassiveMapNode, Key) == 0x8, "bin/bg3 0x2fc1c3d");
static_assert(offsetof(port_layout::PassiveMapNode, Value) == 0x10, "bin/bg3 0x2fc1c45");
static_assert(sizeof(port_layout::PassiveMapNode) == 0x220, "bin/bg3 0x2fc1c1f");
#endif


struct InterruptPrototype
{
    FixedString Name;
    DescriptionInfo Description;
    uint16_t InterruptContext{ 0 };
    uint8_t InterruptContextScope{ 0 };
    stats::ConditionId Conditions;
    FixedString Container;
    stats::ConditionId RollCondition;
    Array<ActionResourceCost> Costs;
    FixedString Stack;
    Functors PropertiesFunctors;
    Functors SuccessFunctors;
    Functors FailureFunctors;
    uint8_t InterruptDefaultValue{ 0 };
    stats::ConditionId EnableCondition;
    uint64_t EnableContext{ 0 };
    uint8_t InterruptFlags{ 0 };
};

// PORT (linux): witnesses off eoc::InterruptPrototype::Init, bin/bg3 0x2fc33d0 -- the function
// upstream calls through eoc__InterruptPrototype__Init (manifest row).
// (rdi = this, rsi = stats::Object*.) All 18 distinct offsets the body touches on `this` land on a
// declared member here; the four below are the ones the reading pins outright, and sizeof is pinned
// by the caller's stride rather than by hand-counting. The three Functors' +0x5c stores are
// Functors::UniqueName in the tail padding of its CNamedElementManager base, so this function is a
// third independent witness for that layout.
static_assert(sizeof(InterruptPrototype) == 0x1F0, "bin/bg3 0x2fb8f10 strides the manager's values array by 0x1f0 per element and 0x2fb8f23 memsets 0x1f0 bytes of a new one");
static_assert(offsetof(InterruptPrototype, Name) == 0x0, "bin/bg3 0x2fc33de reads dword [proto+0] as the old FixedString and 0x2fc3418 stores object->Name there after gst Acquire/Release");
static_assert(offsetof(InterruptPrototype, Description) == 0x8, "bin/bg3 0x2fc341a passes [proto+8] to the shared description helper 0x2b85570, whose only other callers are the Spell (+0x60), Status (+0x20) and Passive Inits");
static_assert(offsetof(InterruptPrototype, InterruptContext) == 0x90, "bin/bg3 0x2fc3426 zeroes word [proto+0x90] before the property reads");
static_assert(offsetof(InterruptPrototype, PropertiesFunctors) + 0x5C == 0x114, "bin/bg3 0x2fc33d0 touches +0x114, +0x174 and +0x1d4 -- Functors::UniqueName at 0x5c into each of the three Functors");


struct [[bg3::hidden]] InterruptPrototypeManager : public ProtectedGameObject<InterruptPrototypeManager>
{
    void* VMT;
    HashMap<FixedString, InterruptPrototype> Interrupts;
    bool IsLoaded;

    void SyncStat(Object* object);
    bool SyncStat(Object* object, InterruptPrototype* proto);
};

// PORT (linux): witnesses off eoc::InterruptPrototypeManager::Init, bin/bg3 0x2fb8e20 -- the one
// caller of eoc::InterruptPrototype::Init, reached from the DoLoadStats lambda 0x6486a60 as
// [eoc__InterruptPrototypeManager 0x7bc1f70]. The HashMap's own members are not
// asserted here -- they are shared and asserted where the type is -- but the two edges of it are
// what this function reads, and they are what makes Interrupts.try_get() land on the right element.
static_assert(offsetof(InterruptPrototypeManager, Interrupts) == 0x8, "bin/bg3 0x2fb8ed4 reads the HashKeys buffer at [mgr+8] and 0x2fb8ed0 divides by the count at [mgr+0x10]");
static_assert(offsetof(InterruptPrototypeManager, Interrupts) + sizeof(HashSet<FixedString>) == 0x38, "bin/bg3 0x2fb8f0c loads the values buffer from [mgr+0x38] and 0x2fb8f01 compares Keys.capacity_ with the size at [mgr+0x40]; HashMap's Values follows its HashSet base, and Values is private, so the edge is spelled as that base's size");
static_assert(offsetof(InterruptPrototypeManager, IsLoaded) == 0x48, "bin/bg3 0x2fb8e20 opens `cmp byte [mgr+0x48], 0` and 0x2fb8e38 sets it to 1");


struct BoostPrototype : public ProtectedGameObject<BoostPrototype>
{
    STDString Description;
    FixedString TypeName;
    [[bg3::hidden]] void* BoostData;
    BoostType Boost;
    ConditionId Condition;
};


struct [[bg3::hidden]] BoostPrototypeManager : public ProtectedGameObject<InterruptPrototypeManager>
{
    bool IsLoading;
    thoth::shared::ConditionManager* ConditionsManager;
    HashMap<Guid, BoostPrototype*> Boosts;
    Array<ConditionId> ConditionHandles;
};

END_NS()
