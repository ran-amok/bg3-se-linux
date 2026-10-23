#pragma once

BEGIN_NS(progression)

struct SpellDefinition
{
    FixedString SpellId;
    SpellPrepareType PrepareType{ SpellPrepareType::AlwaysPrepared };
    Guid SpellList;
    AbilityId CastingAbility{ AbilityId::None };
    SpellCooldownType CooldownType{ SpellCooldownType::Default };
    Guid Class;
};

struct AbilityImprovementsComponent : public BaseComponent
{
    DEFINE_COMPONENT(ProgressionAbilityImprovements, "eoc::progression::AbilityImprovementsComponent")

    Array<AbilityId> Abilities;
    std::array<int32_t, 7> AbilityImprovements;
};

struct FeatComponent : public BaseComponent
{
    DEFINE_COMPONENT(ProgressionFeat, "eoc::progression::FeatComponent")

    Guid Feat;
    int field_10;
    std::optional<int> ConditionId;
    Array<SkillId> Proficiencies;
    Array<SkillId> Expertise;
    Array<SpellDefinition> AddSpells;
    Array<FixedString> AddPassives;
    std::array<int32_t, 7> AbilityImprovements;
};

struct SpellsComponent : public BaseComponent
{
    DEFINE_COMPONENT(ProgressionSpells, "eoc::progression::SpellsComponent")

    Array<SpellDefinition> AddSpells;
    Array<SpellDefinition> RemoveSpells;
};

struct SkillsComponent : public BaseComponent
{
    DEFINE_COMPONENT(ProgressionSkills, "eoc::progression::SkillsComponent")

    Array<SkillId> Proficiencies;
    Array<SkillId> Expertise;
};

struct PassivesComponent : public BaseComponent
{
    DEFINE_COMPONENT(ProgressionPassives, "eoc::progression::PassivesComponent")

    Array<FixedString> AddPassives;
    Array<FixedString> RemovePassives;
};

struct ReplicatedFeatComponent : public BaseComponent
{
    DEFINE_COMPONENT(ProgressionReplicatedFeat, "eoc::progression::ReplicatedFeatComponent")

    Guid Feat;
    uint8_t field_10;
    int field_14;
};

struct LevelUpComponent : public BaseComponent
{
    DEFINE_COMPONENT(LevelUp, "eoc::progression::LevelUpComponent")

    [[bg3::legacy(field_18)]] Array<LevelUpData> LevelUps;
};

struct ContainerComponent : public BaseComponent
{
    DEFINE_COMPONENT(ProgressionContainer, "eoc::ProgressionContainerComponent")

    Array<Array<EntityHandle>> Progressions;
};

struct MetaComponent : public BaseComponent
{
    DEFINE_COMPONENT(ProgressionMeta, "eoc::progression::MetaComponent")

    uint8_t field_18;
    Guid Progression;
    [[bg3::legacy(Race)]] Guid Source;
    [[bg3::legacy(field_40)]] int ClassLevel;
    [[bg3::legacy(field_44), bg3::legacy(ProgressionType)]] SpellSourceType SpellSourceType;
    [[bg3::legacy(field_48)]] int Level;
    EntityHandle Owner;
    [[bg3::legacy(field_50)]] HashMap<Guid, Guid> MulticlassSpellSlotOverride;
};

END_NS()

BEGIN_NS(esv::progression)

struct ChangedContainersComponent : public BaseComponent
{
    DEFINE_COMPONENT(ProgressionChangedContainers, "esv::progression::ChangedContainersComponent")

    HashMap<EntityHandle, HashMap<EntityHandle, Array<EntityHandle>>> Changes;
};

DEFINE_ONEFRAME_TAG_COMPONENT(esv::progression, LevelUpChangedOneFrameComponent, ProgressionLevelUpChanged)

struct ManagementSystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerProgression, "esv::progression::ManagementSystem")

    HashSet<EntityHandle> DestroyedProgressions;
    HashMap<EntityHandle, int> ProgressionUpdates;
    [[bg3::hidden]] void* ImmutableDataHeadmaster;
    [[bg3::hidden]] void* RPGStats;
#if !defined(BG3SE_LINUX)
    [[bg3::hidden]] CriticalSection CS_;
#endif
// PORT (linux): upstream's trailing `CriticalSection CS_` is not in the native build. Its
// registration (bin/bg3 0x45fdac0) is operator new(0x90): vptr at 0, [+8] = 0, zeroes
// 0x10..0x80 and stores two pointers at 0x80/0x88, and that is the whole object. The D1
// (0x6f0d710) destroys the map at 0x40 and the set at 0x10 (arrays 0x30/0x20, HashKeys 0x10)
// and nothing past them. The macOS oracle (af3f1c71: new(0x90) at 0x1043c8064) has the same
// stores, so this is not the macOS-small lock either.
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(ManagementSystem, ProgressionUpdates) == 0x40,
    "bin/bg3 D1 0x6f0d714 destroys the map at this+0x40");
static_assert(offsetof(ManagementSystem, ImmutableDataHeadmaster) == 0x80,
    "bin/bg3 0x45fdb4f stores the first pointer at this+0x80");
static_assert(sizeof(ManagementSystem) == 0x90,
    "operator new(0x90) at bin/bg3 0x45fdb0d");
#endif

END_NS()
