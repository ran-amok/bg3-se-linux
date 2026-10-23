#pragma once

#include <GameDefinitions/Effect.h>
#include <GameDefinitions/ObjectFactory.h>

BEGIN_NS(esv)

struct Status : public ProtectedGameObject<Status>
{
    virtual ~Status() = 0;
    virtual void SetHandle(uint64_t handle) = 0;
    virtual ComponentHandle GetHandle(uint64_t handle) = 0;
    virtual StatusType GetStatusId() = 0;
    // 0 - Stackable
    // 1 - Apply only the first instance, discard new ones
    // 2 - Apply only one instance, new instances replace old ones
    // 3 - Apply only the first instance; triggers combat?
    virtual uint32_t GetStatusType() = 0;
    virtual void SetStatusSpecificParam1(uint32_t, uint32_t) = 0;
    virtual void SetStatusSpecificParam2(FixedString const&) = 0;
    virtual void VMT07() = 0;
    virtual uint32_t VMT08() = 0;
    virtual bool CanEnter() = 0;
    virtual void Init() = 0;
    virtual void InitStaticProperties() = 0;
    virtual bool Enter() = 0;
    virtual bool Resume() = 0;
    virtual void Update(GameTime const&) = 0;
    virtual void Tick(Guid const& combatGuid, float deltaTime, uint32_t type) = 0;
    virtual void Exit() = 0;
    virtual bool VMT17() = 0;
    virtual bool DisablesTarget() = 0;
    virtual void PrepareForLevelUnloading() = 0;
    // PORT (linux): the native build's esv::Status vtable has 31 slots where the declarations
    // above give 30, and the extra one sits here -- so without it every index from GetTarget
    // on is one low.  It is esv::StatusAura::ResetStats(): the base's slot is an empty body
    // (bin/bg3 0x6f3ace0, a bare `ret`, in 15 of the 23 runs) and StatusAura's override is
    // bin/bg3 0x712bf50, matched to the macOS _ZN3esv10StatusAura10ResetStatsEv and reading
    // its own Aura member at [rdi+0x188].  Pinned either side in the StatusAura run
    // (0x7a8c610): slot 20 PrepareForLevelUnloading 0x3c1d710, slot 22 GetTarget 0x6f3acf0,
    // slot 26 esv::StatusAura::SavegameVisit 0x2bb96f0.
    virtual void ResetStats() = 0;
    virtual EntityHandle GetTarget() const = 0;
    virtual uint32_t OnStatusEvent(uint32_t event, HitDesc const& hit, AttackDesc const& attack, FixedString const&, uint32_t removeCause) = 0;
    virtual bool GetSyncData(ScratchBuffer&) = 0;
    virtual bool VMT23() = 0;
    virtual bool SavegameVisit(ObjectVisitor*) = 0;
    virtual bool VMT25() = 0;
    virtual bool Activate() = 0;
    virtual void Deactivate() = 0;
    virtual void OnSetRequestDelete(bool) = 0;

    Guid field_8;
    __int64 field_18;
    int StoryActionID;
    ActionOriginator Originator;
    double field_48;
    FixedString StatusId;
    float StartTimer;
    float LifeTime;
    float CurrentLifeTime;
    float TurnTimer;
    float Strength;
    uint8_t CauseType;
    uint8_t OriginCauseType;
    EntityHandle SourceEquippedItem;
    EntityHandle SourceUsedItem;
    ComponentHandle StatusHandle;
    EntityHandle SyncEntity;
    EntityHandle Owner;
    Array<EntityHandle> StatusOwner;
    ecs::EntityRef Cause;
    Guid CauseGUID;
    EntityHandle StatusSource;
    ecs::EntityRef RootCause;
    EntityHandle field_E0;
    Guid field_E8;
    stats::ConditionId RemoveConditions;
    uint32_t RemoveEvents;
    [[bg3::flags]] StatusFlags Flags;
    [[bg3::flags]] StatusFlags2 Flags2;
    [[bg3::flags]] StatusFlags3 Flags3;
    bool FreezeDuration;
    uint8_t ExitCause;
    [[bg3::legacy(field_105)]] SurfaceType SourceSurface;
    uint8_t SpellCastingAbility;
    SpellId SourceSpell;
    Guid SpellCastSourceUuid;
    FixedString StackId;
    int StackPriority;
    bool IsRecoverable;
    FixedString DifficultyStatus;
    Guid StackedSpellCastSourceUuid;
    ConditionRolls ConditionRolls;
    uint8_t TickType;
    StatusFlags4 Flags4;
    // PORT (linux): upstream's "Flags5 is gone in patch3?" does not hold for the native
    // build -- the byte is live and savegame-visited.  Witness below.
    StatusFlags5 Flags5;
};

struct EntityAuraStatus
{
    EntityHandle Entity;
    uint32_t Time;
    FixedString StatusId;
};

struct Aura : public ProtectedGameObject<Aura>
{
    [[bg3::hidden]] void* VMT;
    float Radius;
    stats::Functors* Functors;
    uint8_t Flags;
    Array<Array<EntityAuraStatus>> EntityAuraStatuses;
    ecs::EntityRef Source;
    int32_t StoryActionId;
    Guid SpellCastSourceUuid;
};

struct StatusAura : public Status
{
    Aura* Aura;
};

struct StatusBoost : public StatusAura
{
    static constexpr auto Type = StatusType::BOOST;

    Array<FixedString> Spell;
    Array<FixedString> Items;
    bool LoseControl;
    Array<EntityHandle> ItemHandles;
    float EffectTime;
    glm::vec3 SourceDirection;
    Array<uint32_t> SurfaceChanges;
    bool HasStatusEffectOnTurn;
};

struct StatusClimbing : public Status
{
    static constexpr auto Type = StatusType::CLIMBING;

    __int64 field_120;
    int field_128;
    int field_12C;
    glm::vec3 MoveDirection_M;
    int field_13C;
    __int64 field_140;
    EntityHandle Item;
    FixedString Level;
    uint8_t Status;
    bool Direction;
    bool JumpUpLadders_M;
    bool Incapacitated_M;
    bool Started_M;
};

struct StatusIncapacitated : public StatusBoost
{
    static constexpr auto Type = StatusType::INCAPACITATED;

    float CurrentFreezeTime;
    float FreezeTime;
    uint8_t IncapacitateFlags;
};

struct StatusDowned : public StatusIncapacitated
{
    static constexpr auto Type = StatusType::DOWNED;

    int NumStableSuccess;
    int NumStableFailed;
    int StableRollDC;
    StatsExpressionRef StableRoll;
    stats::Functors* OnRollsFailed;
    stats::Functors* OnSuccess;
    [[bg3::hidden]] UnknownFunction RollSignals;
    bool IsStable;
    bool IsHealed;
    int RollSuccesses;
    int RollFailures;
    int DamageFailures;
};

struct StatusDying : public Status
{
    static constexpr auto Type = StatusType::DYING;
};

struct StatusHeal : public Status
{
    static constexpr auto Type = StatusType::HEAL;

    float EffectTime;
    int HealAmount;
    int HealEffect;
    FixedString HealEffectId;
    int TargetHealth;
};

struct SurfaceLayerCheck
{
    uint8_t EvaluateSurfaceApplyTypes;
    float SurfaceDistanceCheck;
    float SurfaceOnMoveDistanceMultiplier;
    int OnMoveCount;
    uint8_t field_10;
    bool FullyEntered;
    bool Inside;
    glm::vec3 Translate;
    int field_20;
};

struct SurfaceTypeCheck
{
    float TurnTimerCheck;
    HashSet<EntityHandle> Entities;
};

struct StatusInSurface : public Status
{
    static constexpr auto Type = StatusType::INSURFACE;

    HashMap<EntityHandle, std::array<SurfaceLayerCheck, 2>> TargetData;
    HashMap<SurfaceType, SurfaceTypeCheck> TypeData;
    HashMap<FixedString, bool> AlreadyAppliedStatuses;
};

struct StatusInvisible : public StatusBoost
{
    static constexpr auto Type = StatusType::INVISIBLE;

    // PORT (linux): upstream declares `glm::vec3 InvisiblePosition;` here; this build has no
    // such member.  esv::StatusInvisible is sizeof(StatusBoost) == 0x1f0 and its factory
    // initialises nothing of its own -- witnessed twice, and once by name: bin/bg3 0x3b66d30
    // (`mov edi, 0x1f0; call operator new`, then the INVISIBLE vtable 0x7a8d3d8 and no other
    // store) and the macOS oracle's
    // ls::NetworkObjectFactory<esv::Status>::Create<esv::StatusInvisible> 0x10599d8c0 /
    // Force<esv::StatusInvisible> 0x10599d580, both `mov edi, 0x1f0` with no derived store.
    // A 12-byte member at 0x1ec would need 0x1f8 bytes.  Its factory is byte-for-byte the
    // shape of StatusFear's and StatusDeactivated's, which declare no members either.
};

struct StatusKnockedDown : public Status
{
    static constexpr auto Type = StatusType::KNOCKED_DOWN;
};

struct StatusMaterial : public Status
{
    static constexpr auto Type = StatusType::MATERIAL;

    FixedString MaterialUUID;
    uint8_t ApplyFlags;
    bool IsOverlayMaterial;
    bool Fading;
    bool ApplyNormalMap;
    bool Force;
};

struct StatusPolymorphed : public StatusBoost
{
    static constexpr auto Type = StatusType::POLYMORPHED;

    Guid Id;
};

struct StatusReaction : public Status
{
    static constexpr auto Type = StatusType::REACTION;

    ecs::EntityRef Source;
    ecs::EntityRef Target;
    glm::vec3 TargetPosition;
    ComponentHandle PartnerStatus;
    SpellId Spell;
    uint8_t UpdatesSinceSpellCastRequest;
    bool IgnoreHasSpell;
    bool IgnoreChecks;
    bool FromInterrupt;
    bool ShowOverhead;
    bool CheckProjectileTargets;
};

struct StatusStoryFrozen : public Status
{
    static constexpr auto Type = StatusType::STORY_FROZEN;

};

struct StatusRotate : public Status
{
    static constexpr auto Type = StatusType::ROTATE;

    float Yaw;
    float RotationSpeed;
};

struct StatusTeleportFalling : public Status
{
    static constexpr auto Type = StatusType::TELEPORT_FALLING;

    glm::vec3 Target;
    float ReappearTime;
    SpellId Spell;
    bool HasDamage;
    bool HasDamageBeenApplied;
};

struct StatusUnlock : public Status
{
    static constexpr auto Type = StatusType::UNLOCK;

    EntityHandle Source;
    FixedString field_128;
    bool Success;
    int Unlocked;
    // PORT (linux): the mirror stopped at 0x1a0 and the object is 0x420.  Its factory
    // (bin/bg3 0x6f40d60, matched to the macOS Create<esv::StatusUnlock> 0x10599c740)
    // passes 0x420 to operator new -- `mov edi, 0x420` at 0x6f40d71 -- and then
    // initialises exactly the four members above, each where the mirror already put it:
    // Source 0x188 (the null-EntityHandle 0xffc0000000000000, 0x6f40d9c), field_128 0x190
    // (FixedString 0xffffffff, 0x6f40daf), Success 0x194 (0x6f40db9), Unlocked 0x198
    // (0x6f40dbf).  It then zeroes two more bytes that are its own, at 0x2d8 (0x6f40dc9)
    // and 0x418 (0x6f40dcf).  So this is a truncation and not a shift: the ~0x280 missing
    // bytes are all trailing, nothing above them moves, and the cost was paid only the
    // first time anything allocated, copied or sizeof'd one.  The filler below buys that
    // back; the two bytes the factory names are named, everything else is blind.  A status
    // is polymorphic-ref'd from Lua by StatusId (Status.inl), so a script can hold one of
    // these -- but only through the four properties above, which is why the asserts are
    // the whole witness and no in-game probe can see the difference.
    [[bg3::hidden]] uint8_t field_19C[0x2d8 - 0x19c];
    [[bg3::hidden]] uint8_t field_2D8;
    [[bg3::hidden]] uint8_t field_2D9[0x418 - 0x2d9];
    [[bg3::hidden]] uint8_t field_418;
    [[bg3::hidden]] uint8_t field_419[0x420 - 0x419];
};

struct StatusFear : public StatusBoost
{
    static constexpr auto Type = StatusType::FEAR;

};

struct StatusSmelly : public Status
{
    static constexpr auto Type = StatusType::SMELLY;

};

struct StatusSneaking : public StatusBoost
{
    static constexpr auto Type = StatusType::SNEAKING;

    bool ClientRequestStop;
};

struct StatusEffect : public Status
{
    static constexpr auto Type = StatusType::EFFECT;

};

struct StatusDeactivated : public StatusBoost
{
    static constexpr auto Type = StatusType::DEACTIVATED;

};


struct [[bg3::hidden]] StatusMachine
{
    using CreateStatusProc = Status* (StatusMachine* self, FixedString* statusId, uint64_t statusHandle, bool unkn1, bool unkn2);
    using ApplyStatusProc = Status* (StatusMachine* self, Status* status);

    Status* GetStatus(FixedString const& statusId) const;

    EntityHandle Owner;
    uint8_t field_18;
    Array<Status*> NewlyAddedStatuses;
    Array<Status*> Statuses;
    Array<Status*> CleanseStatuses;
    Array<Status*> UnknownStatuses;
    HashMap<FixedString, Status*> field_50;
    uint32_t field_90;
    uint32_t field_94;
    UnknownFunction field_98;
    void* field_D8;
    Array<void*> RemoveStatusRequests;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the esv::Status family, measured against bin/bg3 (c8da62b1).
// Status is 0x188 bytes with its last *modelled* member (Flags4, a byte) at 0x181, so the
// mirror's dsize stops at 0x182 and it has six bytes of tail padding.  Itanium lets a
// derived status reuse them; MSVC does not, so every status whose first derived member is
// 4-aligned and <= 4 bytes sits at 0x184 here and would sit at 0x188 under upstream's own
// compiler.  The game does the Itanium thing, in all four flagged classes.
//   The instrument is (b1), a matched Visit: every status has a
//     `bool SavegameVisit(eoc::SavegameVisitor*)`, each one calls
//     esv::Status::SavegameVisit (bin/bg3 0x2bb9860, matched s4:1.00) first and then
//     visits exactly its own members, naming each with an `EoCFS::str…` FixedString
//     global and choosing the visitor by type (0x2bb2810 float, 0x2bb8e90 byte,
//     0x2bb0bd0 int, 0x2bb4990 uint8, 0x26caba0 glm::vec3, 0x2bba4f0 EntityHandle,
//     0x2bbbc90 ComponentHandle, 0x2bba630 eoc::spell::LEGACY_Visit for a SpellId,
//     visitor vtable [+0x198] FixedString).
//   None of the four flagged classes is matched by name, and the family is large (21
//     concrete statuses), so they were named off their vtables by a route this family
//     supplies for free: **GetStatusId() returns the StatusType constant**, so run slot 4
//     of a 31-slot Status vtable is a `mov eax, <enum>; ret` that names the class
//     outright.  Enumerating every run whose slot 2 is the matched
//     esv::Status::SetHandle (0x3b66fb0) gives 23 runs -- the 21 StatusType values plus
//     the two abstract heads (Status and StatusAura, whose slot 4 is the shared
//     0x5a22420 stub) -- and every one of the 21 constants appears exactly once.  Five of
//     the 21 are corroborated independently, because their SavegameVisit at slot 26 *is*
//     matched by name: POLYMORPHED 0x2bba940, INSURFACE 0x2bbac00, CLIMBING 0x712c3e0,
//     REACTION 0x712ffd0, UNLOCK 0x7131c30.  The four wanted here:
//       0x7a8cfc0  HEAL              slot 4 = 0x712d8b0 (mov eax, 2)   slot 26 = 0x712d8f0
//       0x7a8d5e8  MATERIAL          slot 4 = 0x712f300 (mov eax, 0xe) slot 26 = 0x3fd6a80
//       0x7a8d950  ROTATE            slot 4 = 0x7130280 (mov eax, 0xd) slot 26 = 0x7130610
//       0x7a8dd70  TELEPORT_FALLING  slot 4 = 0x7130ad0 (mov eax, 4)   slot 26 = 0x71313d0
//     Those runs are 31 slots where upstream declares 30 (2 dtor + 28 virtuals): the game
//     has one more virtual than the mirror, esv::StatusAura::ResetStats (0x712bed0) at
//     slot 21, between PrepareForLevelUnloading (0x3c1d6c0, slot 20) and GetTarget
//     (0x6f3ac70, slot 22).  Nothing in the extender calls an esv::Status virtual,
//     so it costs nothing; it is recorded here because every upstream index
//     from GetTarget on is one too low.
//   The base, from 0x2bb9860: StatusId 0x50 (visited as a FixedString), StartTimer 0x54,
//     LifeTime 0x58 (EoCFS::strLifeTime 0x7ca6b4c), CurrentLifeTime 0x5c, TurnTimer 0x60
//     (EoCFS::strTurnTimer 0x7ca6b54), CauseType 0x68, OriginCauseType 0x69,
//     SourceEquippedItem 0x70 and SourceUsedItem 0x78 (both compared against the
//     0xffc0000000000000 null handle), StatusOwner 0x98, SourceSpell 0x108 (0x2bba0de),
//     SpellCastSourceUuid 0x140, IsRecoverable 0x158, DifficultyStatus 0x15c,
//     FreezeDuration 0x103, SpellCastingAbility 0x106
//     (EoCFS::strSpellCastingAbility 0x7ce4410), and the flag bytes at 0x100/0x101/0x102
//     (Flags/Flags2/Flags3, read and written together as `word [rbx+0x100]` +
//     `byte [rbx+0x102]`) and 0x181 (Flags4, `byte [rbx+0x181]` at 0x2bb9f1b).
//     TickType (0x180) gets no assert: the savegame visitor never touches it, and it is
//     pinned only by exclusion between ConditionRolls and Flags4.
//     sizeof(Status) == 0x188 has a witness of its own, independent of the visit: the
//     matched esv::StatusMachine::CreateStatus (0x27c4090) allocates `mov edi, 0x188;
//     call operator new` four times and stores the DYING vtable (0x7a8cc98) twice and the
//     EFFECT vtable (0x7a8cda0) twice -- both StatusDying and StatusEffect declare no
//     members of their own, so that allocation *is* the base's size
//     (0x27c44c1/0x27c44d6, 0x27c45ef/0x27c4604, 0x27c4904/0x27c491c,
//     0x27c49de/0x27c49f6).  It is not an ABI witness: MSVC would make Status 0x188 too.
//     One known defect, deliberately left: the game has a
//     *second* flag byte at 0x182 that the mirror does not model.  0x2bba053 reads
//     `word [rbx+0x181]` and 0x2bba065 writes it back after `and ecx, 0xfffffeff` /
//     `or eax, <byte><<8>`, i.e. it sets bit 0 of the byte at 0x182 -- which is where
//     upstream's commented-out `StatusFlags5 Flags5` sits.  Adding it would move
//     nothing (the derived members stay at 0x184 and sizeof stays 0x188) but it would be a new
//     Lua-visible member, so it wants its enum and its flag names decided first.
//   The four flagged derived classes, each by its own visit:
//       Heal      0x712d8f0: float [+0x184] EffectTime, int [+0x188] HealAmount,
//                 [+0x18c] HealEffect (read as a byte, written back as a dword at
//                 0x712d973), FixedString [+0x190] HealEffectId.  TargetHealth (0x194) is
//                 not visited and gets no assert.  The four names are the macOS block
//                 EoCFS::strEffectTime/strHealAmount/strHealEffect/strHealEffectId
//                 (0x10970d2e4..0x10970d2f0), in the same order and the same 4-byte
//                 spacing as the Linux block 0x7cc8064..0x7cc8070.
//       Material  0x3fd6a80: FixedString [+0x184] MaterialUUID, uint8 [+0x188] ApplyFlags,
//                 byte [+0x18a] Fading, byte [+0x189] IsOverlayMaterial, byte [+0x18b]
//                 ApplyNormalMap, byte [+0x18c] Force -- visited in that order, which is
//                 not the declaration order, and every one of the six names comes out of
//                 the oracle: EoCFS::strFading is matched independently at Linux
//                 0x7cc83c4 / mac 0x10970d61c, and that delta names its five neighbours
//                 strMaterialUUID/strApplyFlags/strIsOverlayMaterial/strApplyNormalMap/
//                 strForce at 0x7cc83bc/c0/c8/cc/d0 -- each landing on the member the
//                 mirror puts at the offset the visit used.  Six independent name-to-offset
//                 agreements make this the strongest of the four.
//       Rotate    0x7130610: float [+0x184] Yaw, float [+0x188] RotationSpeed
//                 (0x7ccd81c/0x7ccd820, the mac strYaw/strRotationSpeed pair
//                 0x10970d720/724).
//       Teleport  0x71313d0: vec3 [+0x184] Target (EoCFS::strTarget 0x7ccd7d4, matched),
//                 float [+0x190] ReappearTime, SpellId [+0x198] through the matched
//                 eoc::spell::LEGACY_Visit 0x2bba630, byte [+0x1d0] HasDamage, byte
//                 [+0x1d1] HasDamageBeenApplied.  0x1d0 == 0x198 + sizeof(SpellId), which
//                 is the cross-check that the SpellId really starts at 0x198.  The three
//                 trailing names are the mac block strReappearTime/strHasDamage/
//                 strHasDamageBeenApplied (0x10970d768/76c/770) against the Linux block
//                 0x7cc8640/44/48 -- strTarget itself is NOT in that Linux block, because
//                 bin/bg3 keeps the copy esv::OsirisSteerTask shares over at 0x7ccd7d4;
//                 taking one delta across the whole family would have shifted these three.
//   Two-sided, and needed: both controls are statuses whose first derived member is
//     8-aligned, and both put it at 0x188, not 0x184 --
//       esv::StatusUnlock::SavegameVisit 0x7131c30 (matched by name): EntityHandle
//         [+0x188] Source through the matched ls::SavegameVisitor::Visit for an
//         EntityHandle (0x2bba4f0), byte [+0x194] Success, [+0x198] Unlocked (read as a
//         byte, written back as a dword at 0x7131cb7)
//       esv::StatusReaction::SavegameVisit 0x712ffd0 (matched by name): EntityRef
//         [+0x188] Source and [+0x198] Target (both written as two qwords, 0x7130091/98
//         and 0x71300b1/b8), vec3 [+0x1a8] TargetPosition, ComponentHandle [+0x1b8]
//         PartnerStatus, SpellId [+0x1c0] through LEGACY_Visit, and the five bools at
//         0x1f9, 0x1fa, 0x1fb, 0x1fc and 0x1fd (0x1f8,
//         UpdatesSinceSpellCastRequest, is the one the game never visits)
static_assert(sizeof(Status) == 0x188);
static_assert(offsetof(Status, StatusId) == 0x50);
static_assert(offsetof(Status, StartTimer) == 0x54);
static_assert(offsetof(Status, LifeTime) == 0x58);
static_assert(offsetof(Status, CurrentLifeTime) == 0x5c);
static_assert(offsetof(Status, TurnTimer) == 0x60);
static_assert(offsetof(Status, CauseType) == 0x68);
static_assert(offsetof(Status, OriginCauseType) == 0x69);
static_assert(offsetof(Status, SourceEquippedItem) == 0x70);
static_assert(offsetof(Status, SourceUsedItem) == 0x78);
static_assert(offsetof(Status, StatusOwner) == 0x98);
static_assert(offsetof(Status, Flags) == 0x100);
static_assert(offsetof(Status, Flags2) == 0x101);
static_assert(offsetof(Status, Flags3) == 0x102);
static_assert(offsetof(Status, FreezeDuration) == 0x103);
static_assert(offsetof(Status, SpellCastingAbility) == 0x106);
static_assert(offsetof(Status, SourceSpell) == 0x108);
static_assert(offsetof(Status, SpellCastSourceUuid) == 0x140);
static_assert(offsetof(Status, IsRecoverable) == 0x158);
static_assert(offsetof(Status, DifficultyStatus) == 0x15c);
static_assert(offsetof(Status, Flags4) == 0x181);
// Flags5 (0x182) is witnessed twice in the same esv::Status::SavegameVisit (bin/bg3
// 0x2bb98b0, slot 26 of every run), both sites read-modify-writing the word at 0x181:
//   0x2bba0a3  movzx ecx, word [rbx+0x181] / and ecx, 0xfffffeff / or eax, ecx
//              with eax = (a bool the visitor filled through [rax+0xc0]) << 8
//              -- i.e. bit 0 of BYTE 0x182 written from a bool
//   0x2bba041  movzx ebp, word [rbx+0x181] / shr ebp, 8 / and bpl, 1 / mov [rsp+8], bpl
//              -- the same bit read back out as a bool and passed on as the next visit's
//              default (that site's own mask, 0xffffffef, is Flags4's IsInvulnerableVisible)
// So it is a live member, not padding.  The bit is esv::StatusFlags5::HasTriedEntering
// (Enumerations/ECS.inl, the bitmask's only bit) by the macOS oracle's visit order: mac
// 0x10594d9c0 references strIsInvulnerableVisible, strHasTriedEntering, strSpellCastingAbility
// in a row, and Linux visits Flags4 bit 4 (0x2bba053), then the 0x182 bit, then
// SpellCastingAbility at 0x106 (0x2bba0c6) in that same order.  Adding it moves nothing: the
// derived members stay at 0x184 and sizeof(Status) stays 0x188 under both ABIs, which the
// asserts above and below hold to.
static_assert(offsetof(Status, Flags5) == 0x182);

static_assert(offsetof(StatusHeal, EffectTime) == 0x184);
static_assert(offsetof(StatusHeal, HealAmount) == 0x188);
static_assert(offsetof(StatusHeal, HealEffect) == 0x18c);
static_assert(offsetof(StatusHeal, HealEffectId) == 0x190);

static_assert(offsetof(StatusMaterial, MaterialUUID) == 0x184);
static_assert(offsetof(StatusMaterial, ApplyFlags) == 0x188);
static_assert(offsetof(StatusMaterial, IsOverlayMaterial) == 0x189);
static_assert(offsetof(StatusMaterial, Fading) == 0x18a);
static_assert(offsetof(StatusMaterial, ApplyNormalMap) == 0x18b);
static_assert(offsetof(StatusMaterial, Force) == 0x18c);
static_assert(sizeof(StatusMaterial) == 0x190);

static_assert(offsetof(StatusRotate, Yaw) == 0x184);
static_assert(offsetof(StatusRotate, RotationSpeed) == 0x188);
static_assert(sizeof(StatusRotate) == 0x190);

static_assert(offsetof(StatusTeleportFalling, Target) == 0x184);
static_assert(offsetof(StatusTeleportFalling, ReappearTime) == 0x190);
static_assert(offsetof(StatusTeleportFalling, Spell) == 0x198);
static_assert(offsetof(StatusTeleportFalling, HasDamage) == 0x1d0);
static_assert(offsetof(StatusTeleportFalling, HasDamageBeenApplied) == 0x1d1);
static_assert(sizeof(StatusTeleportFalling) == 0x1d8);

// The two negative controls: an 8-aligned first derived member does not move into Status's
// tail padding, in either binary.
static_assert(offsetof(StatusUnlock, Source) == 0x188);
static_assert(offsetof(StatusUnlock, Success) == 0x194);
static_assert(offsetof(StatusUnlock, Unlocked) == 0x198);
// PORT (linux): and the same four members again from the other side -- the factory
// bin/bg3 0x6f40d60, which also gives the object's real size and the two trailing bytes
// it owns.
static_assert(offsetof(StatusUnlock, field_128) == 0x190);      // 0x6f40daf, = -1
static_assert(offsetof(StatusUnlock, field_2D8) == 0x2d8);      // 0x6f40dc9, = 0
static_assert(offsetof(StatusUnlock, field_418) == 0x418);      // 0x6f40dcf, = 0
static_assert(sizeof(StatusUnlock) == 0x420);                   // 0x6f40d71, operator new
static_assert(offsetof(StatusReaction, Source) == 0x188);
static_assert(offsetof(StatusReaction, Target) == 0x198);
static_assert(offsetof(StatusReaction, TargetPosition) == 0x1a8);
static_assert(offsetof(StatusReaction, PartnerStatus) == 0x1b8);
static_assert(offsetof(StatusReaction, Spell) == 0x1c0);
static_assert(offsetof(StatusReaction, IgnoreHasSpell) == 0x1f9);
static_assert(offsetof(StatusReaction, IgnoreChecks) == 0x1fa);
static_assert(offsetof(StatusReaction, FromInterrupt) == 0x1fb);
static_assert(offsetof(StatusReaction, ShowOverhead) == 0x1fc);
static_assert(offsetof(StatusReaction, CheckProjectileTargets) == 0x1fd);

// PORT (linux): the esv::StatusBoost family, measured against bin/bg3 (c8da62b1).
// StatusAura adds one 8-aligned pointer (Aura, 0x188) to Status, so it is not flagged; on top
// of it StatusBoost's own last member (HasStatusEffectOnTurn, a byte) is at 0x1e8, its dsize
// stops at 0x1e9 and it has seven bytes of tail padding.  Three of its subclasses put their
// first derived member inside those bytes, and bin/bg3 does the same, so the flag is not a
// defect in any of them; the fourth subclass in the worklist, StatusInvisible, is a real
// mirror defect and is fixed above.
//   The instrument is a *fourth* one, cheaper here than the matched Visit: the family's
//     per-class allocating factory.  Each concrete esv::Status has exactly one function that
//     stores its vtable, found by scanning .text for a `lea reg, [rip+disp32]` whose target is
//     the vtable (calibrated on the two sites read by hand,
//     0x27c44d6 -> 0x7a8cda0 and 0x27c4604 -> 0x7a8cc98, which come back and nothing else
//     does).  That function is `mov edi, <sizeof>; call operator new`, then the base ctor,
//     then the vtable, then the derived members' default values -- so one function gives the
//     game's sizeof *and* the derived offsets.  sizeof is an ABI witness wherever the MSVC
//     layout would differ in size, which for this family it does for all of
//     StatusSneaking / StatusIncapacitated / StatusDowned / StatusInvisible.
//   The macOS oracle names every one of them, which turns the vtable-constant naming
//     above into a named witness: the whole family exists there as
//     `ls::NetworkObjectFactory<esv::Status, ls::ObjectFactoryNullLocker>::Create<esv::StatusX>`
//     (and `Force<esv::StatusX>`), 21 classes, same Itanium ABI, same sizes, same offsets.
//     Create<StatusSneaking> 0x10599c130, Create<StatusIncapacitated> 0x10599f0c0,
//     Create<StatusInvisible> 0x10599d8c0, Create<StatusDowned> 0x1059a0f30,
//     Create<StatusBoost> 0x10599aed0, Create<StatusPolymorphed> 0x10599fd40,
//     Create<StatusFear> 0x10599cd40, Create<StatusDeactivated> 0x1059a08d0.
//   The calibration this instrument needs, and the case that supplied it:
//     **a zero-init store's offset is not by itself a member offset.**  clang widens and
//     merges the default-value stores, and the base's tail padding is dead in the derived
//     object, so a widened store may start *below* the first derived member.  macOS
//     Create<esv::StatusClimbing> 0x10599ea90 opens `movups [r14+0x184]` even though
//     StatusClimbing's first member is the qword at 0x188 -- which is settled by the matched
//     esv::StatusClimbing::SavegameVisit (bin/bg3 0x712c3e0): Item 0x1b0, Level 0x1b8,
//     Status 0x1bc, Direction 0x1bd, and 0x1b0 == 0x188 + 0x28 only if the first member is at
//     0x188.  So every assert below rests on a *value* store (a pointer, a non-zero constant,
//     a read-modify-write), on a matched visit, or on sizeof -- never on a bare zero-fill.
//   StatusAura, from the matched esv::StatusAura::SavegameVisit (bin/bg3 0x2bb96a0, which
//     calls esv::Status::SavegameVisit 0x2bb9860 and then works on the Aura): 0x2bb96ea
//     `cmp qword [r14+0x188], 0` and 0x2bb9736 `mov rdi, qword [r14+0x188]` followed by a
//     virtual call through it.
//   StatusBoost, from its matched SavegameVisit (bin/bg3 0x2bb9420) and its ctor (0x3b66da0):
//     Spell 0x190 (ctor 0x3b66def stores the buffer pointer, 0x3b66df6 the capacity 1),
//     LoseControl 0x1b0 (visit 0x2bb9490, bool visitor 0x2bb8e90 with default false),
//     ItemHandles 0x1b8 (visit 0x2bb9464 through the matched
//     ls::FileFormatHelpers::VisitSet<ls::ID<ecs::EntityHandleTraits>> 0x2bb94f0),
//     EffectTime 0x1c8 (visit 0x2bb942f, float visitor; ctor 0x3b66e15 stores a non-zero
//     constant there), SourceDirection 0x1cc (visit 0x2bb9448, vec3 visitor 0x26caba0; the
//     ctor's default is (0,1,0), `mov dword [rbx+0x1d0], 0x3f800000` at 0x3b66e1c naming .y),
//     SurfaceChanges 0x1d8 (the matched esv::StatusBoost::CheckSurfaceChanges 0x2683aa0 loads
//     buf_ at 0x2683d7a and size_ at 0x2683b14, 0x1e4 == 0x1d8 + 0xc),
//     HasStatusEffectOnTurn 0x1e8 (by dsize: StatusSneaking's byte sits at 0x1e9 and
//     sizeof(StatusBoost) is 0x1f0, so the last member's last byte is 0x1e8; the ctor's final
//     store, `mov qword [rbx+0x1e1], rax` at 0x3b66e2d, ends there).
//     sizeof 0x1f0 from four factories: bin/bg3 0x3b66d30 / 0x6f40bc0 / 0x6f40700 and macOS
//     Create<esv::StatusBoost> 0x10599aed0.
//   The three flagged classes that are *not* defects:
//     StatusSneaking   ClientRequestStop 0x1e9, sizeof 0x1f0.  Two witnesses: its matched
//                      SavegameVisit (bin/bg3 0x71307f0) calls StatusBoost's and then visits a
//                      single byte at 0x1e9 named EoCFS::strClientRequestStop (Linux
//                      0x7cc85f8, anchored by its neighbour EoCGE::OnSneakingChanged::s_ID,
//                      matched Linux 0x7cc85fc / mac 0x10970d754, same 4-byte spacing as mac
//                      strClientRequestStop 0x10970d750); and its ctor 0x3b66200 stores the
//                      byte at 0x1e9 after `mov edi, 0x1f0`.  MSVC would put it at 0x1f0 and
//                      make the class 0x1f8.
//     StatusIncapacitated  CurrentFreezeTime 0x1ec, FreezeTime 0x1f0, IncapacitateFlags 0x1f4,
//                      sizeof 0x1f8.  Its ctor (bin/bg3 0x3b66c50) is `mov edi, 0x1f8`, then
//                      `mov qword [rbx+0x1ec], 0` (both floats at once) and
//                      `or byte [rbx+0x1f4], 1` -- a read-modify-write, so 0x1f4 cannot be a
//                      widened zero-fill.  Corroborated twice: esv::StatusDowned's ctor
//                      (0x3b65ef0) repeats both stores, and macOS
//                      Create<esv::StatusIncapacitated> 0x10599f0c0 has the identical
//                      `movlps [r14+0x1ec]` / `or byte [r14+0x1f4], 1` after `mov edi, 0x1f8`.
//                      MSVC would make the class 0x200.
//     StatusDowned     first derived member 0x1f8, sizeof 0x270 (allocating wrapper bin/bg3
//                      0x3b65e90, `mov edi, 0x270`, calling the ctor 0x3b65ef0; macOS
//                      Create<esv::StatusDowned> 0x1059a0f30 the same).  Affected rather than
//                      flagged -- it inherits StatusIncapacitated's shift, and MSVC would put
//                      NumStableSuccess at 0x200 and make the class 0x278.  The ctor's value
//                      stores: RollSignals 0x220 (`lea rcx, [rbx+0x228]` stored to 0x220 at
//                      0x3b65f78, with the ls::Function<void()> method table's Call/Copy/
//                      MoveDestroy written to 0x228/0x230/0x238 -- the three matched
//                      ls::Function<void ()>::FunctorMethodsVSSO<...> symbols 0x5b0f220/230/
//                      250), IsStable 0x260 and IsHealed 0x261 (`mov word [rbx+0x260], 0` at
//                      0x3b65f7f), RollSuccesses 0x264 and RollFailures 0x268
//                      (`mov qword [rbx+0x264], 0` at 0x3b65f88), DamageFailures 0x26c
//                      (0x3b65f8f).  The 0x1f8..0x21f block (NumStableSuccess, NumStableFailed,
//                      StableRollDC, StableRoll, OnRollsFailed, OnSuccess) is only zero-filled
//                      in bulk, so only its start gets an assert; it is bounded above by
//                      RollSignals at 0x220 and fits exactly.
//   Three negative controls, all three needed because all three flagged classes show the
//     effect.  StatusPolymorphed's first derived member is 8-aligned and does *not* move into
//     the padding -- ctor bin/bg3 0x3b66be0, `mov edi, 0x200` then
//     `movups [rbx+0x1f0], xmm0` for the Guid, macOS Create<esv::StatusPolymorphed>
//     0x10599fd40 identical.  StatusFear (bin/bg3 0x6f40bc0, macOS 0x10599cd40) and
//     StatusDeactivated (bin/bg3 0x6f40700, macOS 0x1059a08d0) declare no members and
//     allocate exactly sizeof(StatusBoost), which is the control for the claim that the
//     factory's silence in StatusInvisible means silence.
static_assert(offsetof(StatusAura, Aura) == 0x188);

static_assert(sizeof(StatusBoost) == 0x1f0);
static_assert(offsetof(StatusBoost, Spell) == 0x190);
static_assert(offsetof(StatusBoost, LoseControl) == 0x1b0);
static_assert(offsetof(StatusBoost, ItemHandles) == 0x1b8);
static_assert(offsetof(StatusBoost, EffectTime) == 0x1c8);
static_assert(offsetof(StatusBoost, SourceDirection) == 0x1cc);
static_assert(offsetof(StatusBoost, SurfaceChanges) == 0x1d8);
static_assert(offsetof(StatusBoost, HasStatusEffectOnTurn) == 0x1e8);

static_assert(offsetof(StatusSneaking, ClientRequestStop) == 0x1e9);
static_assert(sizeof(StatusSneaking) == 0x1f0);

static_assert(offsetof(StatusIncapacitated, CurrentFreezeTime) == 0x1ec);
static_assert(offsetof(StatusIncapacitated, FreezeTime) == 0x1f0);
static_assert(offsetof(StatusIncapacitated, IncapacitateFlags) == 0x1f4);
static_assert(sizeof(StatusIncapacitated) == 0x1f8);

static_assert(offsetof(StatusDowned, NumStableSuccess) == 0x1f8);
static_assert(offsetof(StatusDowned, RollSignals) == 0x220);
static_assert(offsetof(StatusDowned, IsStable) == 0x260);
static_assert(offsetof(StatusDowned, IsHealed) == 0x261);
static_assert(offsetof(StatusDowned, RollSuccesses) == 0x264);
static_assert(offsetof(StatusDowned, RollFailures) == 0x268);
static_assert(offsetof(StatusDowned, DamageFailures) == 0x26c);
static_assert(sizeof(StatusDowned) == 0x270);

// StatusInvisible has no member to assert: the point of its row is that it has none.
static_assert(sizeof(StatusInvisible) == 0x1f0);

// The three negative controls.
static_assert(offsetof(StatusPolymorphed, Id) == 0x1f0);
static_assert(sizeof(StatusPolymorphed) == 0x200);
static_assert(sizeof(StatusFear) == 0x1f0);
static_assert(sizeof(StatusDeactivated) == 0x1f0);
#endif

END_NS()


BEGIN_NS(ecl)

struct [[bg3::hidden]] StatusVFXVMT
{
    using DestroyProc = void(StatusVFXVMT*, bool);
    using UpdateVisualsProc = void(StatusVFXVMT*);
    using RecreateVisualsProc = void(StatusVFXVMT*, void*, bool);
    using DestroyVisualsProc = void(StatusVFXVMT*);
    using ApplyProc = void(StatusVFXVMT*, void*);

    DestroyProc* Destroy;
    UpdateVisualsProc* UpdateVisuals;
    RecreateVisualsProc* RecreateVisuals;
    DestroyVisualsProc* DestroyVisuals;
    ApplyProc* Apply;
};

struct StatusVFX : public ProtectedGameObject<StatusVFX>
{
    [[bg3::hidden]] StatusVFXVMT* VMT;
    ComponentHandle Status;
};

struct ManagedStatusEffect
{
    Guid Group;
    uint8_t Type;
};


struct Status : public ProtectedGameObject<Status>
{
    virtual ~Status() = 0;
    virtual StatusType GetStatusId() = 0;
    virtual uint32_t GetStatusType() = 0;
    virtual void Init() = 0;
    virtual bool Enter() = 0;
    virtual bool Resume() = 0;
    virtual void Update(GameTime const&) = 0;
    virtual bool RequiresTick() = 0;
    virtual void Tick(Guid const& combatGuid, float deltaTime) = 0;
    virtual void Exit() = 0;
    virtual void VMT10() = 0;
    virtual FixedString GetIcon() const = 0;
    virtual int GetLevel() const = 0;
    virtual void CollectEffects(Array<StatusVFX*>) = 0;
    virtual bool GetManagedStatusEffectGroup(ManagedStatusEffect&) = 0;
    virtual void ApplyEndEffect(void*) = 0;
    virtual bool ShouldShowOverhead() const = 0;
    virtual bool HasVisuals() const = 0;
    virtual bool SyncData(ScratchBuffer&) = 0;
    virtual void DestroyVisuals() = 0;
    virtual void VMT20() = 0;
    virtual void RecreateVisuals() = 0;
    virtual void VMT22() = 0;
    virtual void OnSetRequestDelete(bool) = 0;

    Guid UUID;
    NetId NetworkId;
    EntityHandle Target;
    ComponentHandle StatusHandle;
    ecs::EntityRef Cause;
    ecs::EntityRef TickingOwner;
    FixedString StatusId;
    uint8_t TickType;
    uint8_t CauseType;
    SpellId Spell;
    int StillAnimationType;
    int StillAnimationPriority;
    float TickTime;
    float LifeTime;
    float TickTimeLeft;
    StatusFlags Flags;
    StatusFlags2 Flags2;
    bool DisableTicking;
};


struct StatusAuraData
{
    FixedString FX;
    float Radius;
};


struct StatusAura : public Status
{
    StatusAuraData Aura;
};

struct StatusEffectData
{
    Array<Guid> Tags;
    FixedString Effect;
};

struct StatusMaterialParam
{
    FixedString Param;
    FixedString Value;
};

struct StatusVFXBoostMaterialDefinition
{
    uint32_t MaterialType;
    FixedString Material;
    OverlayMaterialApplyFlags ApplyFlags;
    uint8_t ApplyNormalMap;
    float FadeAmount;
    float OverlayOffset;
    int field_14;
    Array<StatusMaterialParam> MaterialParams;
    glm::vec3 SourceDirection;
};

struct StatusVFXData
{
    FixedString Material;
    OverlayMaterialApplyFlags ApplyFlags;
    bool IsOverlayMaterial;
    bool Fading;
    bool ApplyNormalMap;
    bool Force;
    float FadeTime;
};

struct StatusVisualDefinition
{
    uint64_t field_0;
    HashMap<FixedString, Array<FixedString>> Visuals;
    float FadeIn;
    float FadeOut;
};

struct StatusBoost : public StatusAura
{
    static constexpr auto Type = StatusType::BOOST;

    //bool HasData;
    FixedString Icon;
    StatusEffectData BoostEffect;
    StatusEffectData EffectOnTurn;
    ManagedStatusEffect ManagedStatusEffect;
    FixedString ApplyEffect;
    FixedString EndEffect;
    StatusVFXBoostMaterialDefinition Material;
    int field_12C;
    StatusVisualDefinition VisualDefinition;
    //bool NeedsEffectStart;
    EffectHandler EffectHandler;
};

struct StatusSneaking : public StatusBoost
{
    static constexpr auto Type = StatusType::SNEAKING;

    bool SneakingFailed;
    bool SneakingFailed2;
    bool IsPlayer;
    EntityHandle Effect;
    uint8_t Obscurity;
};

struct StatusInvisible : public StatusBoost
{
    static constexpr auto Type = StatusType::INVISIBLE;

};

struct StatusIncapacitated : public StatusBoost
{
    static constexpr auto Type = StatusType::INCAPACITATED;

    float field_2B0;
    float field_2B4;
};

struct StatusDowned : public StatusIncapacitated
{
    static constexpr auto Type = StatusType::DOWNED;

    uint8_t field_2B8;
    uint8_t field_2B9;
    uint8_t field_2BA;
    int field_2BC;
    int field_2C0;
    int field_2C4;
    uint8_t Entered;
    uint8_t DownedFlags;
};

struct StatusFear : public StatusBoost
{
    static constexpr auto Type = StatusType::FEAR;
};

struct StatusInSurface : public Status
{
    static constexpr auto Type = StatusType::INSURFACE;
};

struct StatusPolymorphed : public StatusBoost
{
    static constexpr auto Type = StatusType::POLYMORPHED;
};

struct StatusDeactivated : public StatusBoost
{
    static constexpr auto Type = StatusType::DEACTIVATED;
};

struct StatusUnlock : public Status
{
    static constexpr auto Type = StatusType::UNLOCK;

    int State;
};

struct StatusTeleportFalling : public Status
{
    static constexpr auto Type = StatusType::TELEPORT_FALLING;

    glm::vec3 field_98;
    float FallTimer;
    SpellId TeleportSpell;
    uint8_t field_D0;
    EffectHandler EffectHandler;
};

struct StatusStoryFrozen : public Status
{
    static constexpr auto Type = StatusType::STORY_FROZEN;
};

struct StatusSmelly : public Status
{
    static constexpr auto Type = StatusType::SMELLY;

    StatusEffectData Effect;
    StatusVFXData Material;
    __int64 field_C0;
};

struct StatusRotate : public Status
{
    static constexpr auto Type = StatusType::ROTATE;

    float field_98;
    float field_9C;
};

struct StatusReaction : public Status
{
    static constexpr auto Type = StatusType::REACTION;

    ecs::EntityRef ReactionSource;
    ecs::EntityRef ReactionTarget;
    uint8_t field_B8;
};

struct StatusMaterial : public Status
{
    static constexpr auto Type = StatusType::MATERIAL;

    ecl::StatusVFXData VFX;
};

struct StatusKnockedDown : public Status
{
    static constexpr auto Type = StatusType::KNOCKED_DOWN;

    StatusEffectData Effect;
};

struct StatusHeal : public Status
{
    static constexpr auto Type = StatusType::HEAL;

    float HealTimer;
    int HealAmount;
    int HealEffect;
    int field_A4;
    StatusEffectData Effect;
};

struct StatusBeamEffectData
{
    FixedString BeamEffect;
    EntityHandle Source;
};

struct StatusEffect : public Status
{
    static constexpr auto Type = StatusType::EFFECT;

    bool IsPlayer;
    bool PlayerSameParty;
    bool PeaceOnly;
    FixedString PlayerTag;
    StatusEffectData Effect;
    StatusBeamEffectData BeamEffect;
};

#if defined(BG3SE_LINUX)
// PORT (linux): ecl::StatusEffect measured on bin/bg3 itself, and named there without the
// oracle.  Its vtable is the 24-slot run at 0x79e4260: slot 2 is `mov eax, 0x13; ret`, i.e.
// GetStatusType() returning StatusType::EFFECT (19), which is the family's naming slot
// (as esv::Status's GetStatusId is), and slot 9 is the matched
// ecl::StatusEffect::Tick.  IsPlayer at 0xa7 is the flagged offset: ecl::Status's last
// member (DisableTicking) ends at 0xa7, so the first derived bool lands in the base's tail
// padding, where MSVC would put it at 0xa8 and shift the whole record by 1.
//
// Witnesses, all read with tools/disasm.py on the build-id c8da62b1 binary:
//   ecl::StatusEffect::GetDestroyStatusVFXImmediately(), bin/bg3 0x64cf6c0 (vtable slot 21)
//     -- `cmp byte [rdi+0xa7], 0; sete al; ret`, eleven bytes.  It is the ONLY occurrence of
//     that sequence in the binary and the 0xa8 form MSVC would need occurs zero times.
//   ecl::StatusEffect::Tick(ls::Guid const&, float), bin/bg3 0x2f8d080 (matched) --
//     `cmp byte [rbx+0xa7], al` then `mov byte [rbx+0xa7], al`, a read-modify-write of the
//     same bool, and `test byte [rbx+0xa4], 0x40` on the base's Flags.
//   ecl::StatusEffect::CheckShowEffects(), bin/bg3 0x2f8d220 (matched) --
//     `cmp byte [rdi+0xa9], 0`, `cmp byte [rdi+0xa8], 0`, `cmp dword [rdi+0xac], -1` and
//     `mov esi, dword [r14+0xac]` (the FixedString handed to a tag lookup).
//   ecl::StatusEffect::~StatusEffect(), bin/bg3 0x64cf640 (vtable slot 0) -- stores the
//     vtable, then `add rdi, 0xc8` / ls::FixedString::~FixedString and `lea rdi, [rbx+0xc0]`
//     for the second: BeamEffect.BeamEffect at 0xc8 and Effect.Effect at 0xb0 + 0x10.
//   ecl::StatusEffect::RecreateVisuals(), bin/bg3 0x3735f20 (vtable slot 22) --
//     `cmp byte [rdi+0xa7], 0`, `cmp dword [rdi+0xc8], -1`, then `mov rax, [rdi+0x30]` (the
//     base's Cause) stored to `[rdi+0xd0]`, which is BeamEffect.Source.
// The macOS oracle agrees offset for offset (ecl::StatusEffect::GetStatusType 0x1037d8f90
// returns 0x13, GetDestroyStatusVFXImmediately 0x1037d9770 reads byte [rdi+0xa7]) but was
// not needed for any of it.
static_assert(offsetof(Status, Flags) == 0xa4, "Tick's `test byte [rbx+0xa4], 0x40`");
static_assert(offsetof(StatusEffect, IsPlayer) == 0xa7, "GetDestroyStatusVFXImmediately, 0x64cf6c0");
static_assert(offsetof(StatusEffect, PlayerSameParty) == 0xa8, "CheckShowEffects 0x2f8d232");
static_assert(offsetof(StatusEffect, PeaceOnly) == 0xa9, "CheckShowEffects 0x2f8d225");
static_assert(offsetof(StatusEffect, PlayerTag) == 0xac, "CheckShowEffects 0x2f8d23b/0x2f8d264");
static_assert(offsetof(StatusEffect, Effect) == 0xb0, "its FixedString is the dtor's second release");
static_assert(offsetof(StatusEffectData, Effect) == 0x10, "so that release lands at 0xc0");
static_assert(offsetof(StatusEffect, BeamEffect) == 0xc8, "the dtor's first FixedString release");
static_assert(offsetof(StatusBeamEffectData, Source) == 0x8, "RecreateVisuals writes Cause at 0xd0");
#endif

struct StatusDying : public Status
{
    static constexpr auto Type = StatusType::DYING;
};

struct StatusClimbing : public Status
{
    static constexpr auto Type = StatusType::CLIMBING;

    glm::quat Rotation;
    glm::vec3 MoveDirection;
    uint8_t State;
    EntityHandle Ladder;
    bool field_C0;
    uint8_t field_C1;
    uint8_t field_C2;
};


struct EntityStatusData
{
    Array<StatusVFX*> VFX;
    bool HasVFX1;
    bool HasVFX2;
};


struct ManagedStatusFXGroup
{
    int References0;
    int References1;
    uint8_t Type;
    StatusVFX* VFX;
};


struct ManagedStatusFXs
{
    HashMap<Guid, ManagedStatusFXGroup> FXs;
    bool field_40;
};


struct [[bg3::hidden]] StatusMachine : public NetworkObjectFactory<Status>
{
    void* VMT2;
    EntityHandle OwnerHandle;
    uint8_t Flags;
    HashMap<EntityHandle, EntityStatusData> StatusFX;
    HashMap<EntityHandle, ManagedStatusFXs> ManagedFX;
    HashSet<ComponentHandle> StatusHandles;
    void* unk;
    Array<Status*> Statuses;
    Array<Status*> ExternalStatuses;
    uint32_t unk2;
};


struct ManagedStatusVFX : public StatusVFX
{
    ManagedStatusEffect ManagedStatusEffect;
    EffectHandler EffectHandler;
};

struct StatusVisual : public StatusVFX
{
    StatusVisualDefinition* Definition;
    EntityHandle Entity;
    uint8_t State;
    [[bg3::hidden]] void* VisualLoader;
    float FadeOut;
};

struct StatusVFXMaterial : public StatusVFX
{
    ecl::StatusVFXData* VFX;
    bool Created;
};

struct StatusVFXEffect : public StatusVFX
{
    StatusEffectData* Effect;
    EffectHandler EffectHandler;
};

struct StatusVFXApplyEffect : public StatusVFX
{
    FixedString* Effect;
    EffectHandler EffectHandler;
};

struct StatusVFXBoostMaterial : public StatusVFX
{
    StatusVFXBoostMaterialDefinition* Material;
    int MaterialType;
    FixedString MaterialUUID;
    bool Created;
};

struct StatusVFXBeam : public StatusVFX
{
    StatusBeamEffectData* Beam;
    EffectHandler EffectHandler;
    bool Initialized;
    bool Created;
};

struct StatusVFXAura : public StatusVFX
{
    StatusAuraData* Aura;
    EntityHandle Effect;
};


END_NS()

BEGIN_NS(lua)

LUA_POLYMORPHIC(esv::Status)
LUA_POLYMORPHIC(ecl::Status)
LUA_POLYMORPHIC(ecl::StatusVFX)

END_NS()
