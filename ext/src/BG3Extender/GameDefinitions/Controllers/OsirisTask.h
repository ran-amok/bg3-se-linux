#pragma once

#include <GameDefinitions/Controllers/Controller.h>

BEGIN_NS(esv)

struct MoveTask : public Task
{
    virtual void OnArrive() = 0;
    virtual void* GetBehavior() = 0;
    virtual bool GetTarget(glm::vec3&) = 0;

    STDString ArriveEvent;
    [[bg3::hidden]] Array<void*> MoveTaskElement;
    glm::vec3 CurrentTarget;
    float TimeSpent;
    float LastTimeSpent;
    int BehaviorTransactionId;
    int StoryTransactionId;
    uint8_t field_7C;
    bool AvoidAoO;
    Array<EntityHandle> IgnoreEntities;
};

struct OsirisMoveToLocationTask : public MoveTask
{
    static constexpr auto Type = OsirisTaskType::MoveToLocation;

    float TargetRotation;
    float MinDistance;
    float MaxDistance;
    EntityHandle TargetToIgnore;
    bool TargetRotationSet;
};

struct OsirisMoveToObjectTask : public MoveTask
{
    static constexpr auto Type = OsirisTaskType::MoveToObject;

    float MinDistance;
    float MaxDistance;
    EntityHandle Target;
    float SpeedMultiplier;
    float DefaultSpeed;
    bool IncreaseSpeed;
};

struct OsirisFleeFromGridTask : public MoveTask
{
    static constexpr auto Type = OsirisTaskType::FleeFromGrid;

    glm::vec3 Position;
    bool field_9C;
    float Range;
};

struct OsirisMoveInRangeTask : public MoveTask
{
    static constexpr auto Type = OsirisTaskType::MoveInRange;

    EntityHandle Target;
    glm::vec3 TargetPos;
    float MinRange;
    float WantedRange;
    float MaxRange;
    EntityHandle MoveToTarget;
    glm::vec3 MoveToTargetPos;
    float MoveToMinRange;
    float MoveToWantedRange;
    float MoveToMaxRange;
    EntityHandle WeaponHandle;
    glm::vec3 CachedTargetPos;
    glm::vec3 CachedTarget;
    float CachedCloseEnough;
    bool CachedResult;
    bool MustBeInTrigger;
    bool FallbackMoveCloser;
    bool OnlyUseMoveSpellIfRequired;
    uint8_t field_F8;
    glm::vec3 OverrideSourcePosition;
    [[bg3::hidden]] Array<void*> pAIHintAreaTrigger;
    SpellId Spell;
    stats::SpellPrototype* SpellPrototype;
    __int64 field_148;
    __int64 field_150;
    uint8_t field_158;
    __int64 field_160;
    uint8_t field_168;
};

struct OsirisTeleportToLocationTask : public Task
{
    static constexpr auto Type = OsirisTaskType::TeleportToLocation;

    glm::vec3 Position;
    FixedString PreviousLevel;
    FixedString Level;
    bool SetRotation;
    bool LeaveCombat;
    bool FindPosition;
    bool FindFleePosition;
    bool Executed;
    uint8_t field_51;
    bool UnchainFollowers;
    bool SnapToGround;
    bool AllowUnloadedLevel;
    glm::quat Rotation;
    STDString ArriveEvent;
};

struct OsirisAppearTask : public Task
{
    static constexpr auto Type = OsirisTaskType::Appear;

    STDString FinishedEvent;
    FixedString Animation;
    EntityHandle Target;
    glm::vec3 TargetPos;
    float Angle;
    float Time;
    int SpawnState;
    bool PlayerSpawn;
    bool OutOfSight;
    bool OnTrail;
    bool PreventForceComplete;
    bool InvisibilityUpdated;
};

struct OsirisDisappearTask : public Task
{
    static constexpr auto Type = OsirisTaskType::Disappear;

    STDString FinishedEvent;
    glm::vec3 TargetPos;
    float Angle;
    int BehaviourTransactionId;
    int StoryTransactionId;
    float SpeedMultiplier;
    float DefaultSpeed;
    int field_70;
    bool IncreaseSpeed;
    bool OutOfSight;
    bool Offstage;
    bool ValidTarget;
};

struct OsirisFollowNPCTask : public Task
{
    static constexpr auto Type = OsirisTaskType::FollowNPC;

    EntityHandle Target;
};

struct OsirisFollowOwnerOrLeaderTask : public Task
{
    static constexpr auto Type = OsirisTaskType::FollowOwnerOrLeader;

    EntityHandle Leader;
    int BehaviourTransactionId;
};

struct OsirisWanderTask : public Task
{
    static constexpr auto Type = OsirisTaskType::Wander;

    float Range;
    EntityHandle Trigger;
    // PORT (linux): upstream's std::variant<EntityHandle, glm::vec3> is 24 bytes here --
    // libc++ rounds the 12-byte union up to EntityHandle's 8-alignment and then puts the
    // index at +0x10. The game's is 16 with the discriminant byte at +0xc, which is
    // exactly upstream's own EntityOrVec3Variant (CommonTypes.h, #pragma pack(4)); see
    // the witness block at the bottom of this header.
    EntityOrVec3Variant Anchor;
    float Duration;
    glm::vec3 Start;
    int BehaviourTransactionId;
};

struct OsirisSteerTask : public Task
{
    static constexpr auto Type = OsirisTaskType::Steer;

    EntityHandle Target;
    glm::vec3 TargetPos;
    float AngleTolerance;
    bool LookAt;
    bool SnapToTarget;
    int BehaviourTransactionId;
};

struct OsirisDropTask : public Task
{
    static constexpr auto Type = OsirisTaskType::Drop;

    EntityHandle Item;
    glm::vec3 TargetPos;
    std::optional<glm::vec3> DesiredPosition;
};

struct OsirisPickupItemTask : public Task
{
    static constexpr auto Type = OsirisTaskType::PickupItem;

    EntityHandle Item;
    STDString ArriveEvent;
    int BehaviourTransactionId;
    bool MoveAvoidAoO;
};

struct OsirisUseItemTask : public Task
{
    static constexpr auto Type = OsirisTaskType::UseItem;

    EntityHandle Item;
    bool IsInteraction;
    bool UseItem;
    bool MoveAvoidAoO;
    STDString ArriveEvent;
    int BehaviourTransactionId;
};

struct OsirisMoveItemTask : public Task
{
    static constexpr auto Type = OsirisTaskType::MoveItem;

    EntityHandle Item;
    glm::vec3 Position;
    int Amount;
    STDString ArriveEvent;
    int BehaviourTransactionId;
    bool SplitProcessed;
    bool CheckSplitEvent;
    bool CheckRemovedFromInventory;
};

struct OsirisResurrectTask : public Task
{
    static constexpr auto Type = OsirisTaskType::Resurrect;

    float Chance;
    float HPPercentage;
    bool IsResurrected;
    FixedString Animation;
};

struct OsirisUseSpellTask : public Task
{
    static constexpr auto Type = OsirisTaskType::UseSpell;

    SpellId Spell;
    glm::vec3 CastPosition;
    EntityHandle Target;
    std::optional<glm::vec3> TargetPos;
    EntityHandle Target2;
    std::optional<glm::vec3> TargetPos2;
    EntityHandle Item;
    bool Success;
    uint8_t field_A9;
    uint8_t State;
    bool WithoutMove;
    bool MoveAvoidAoO;
    uint8_t SpellCastOptions;
    ActionOriginator Originator;
    Guid SpellCastGuid;
};

struct OsirisMoveToAndTalkTask : public Task
{
    static constexpr auto Type = OsirisTaskType::MoveToAndTalk;

    EntityHandle Target;
    int BehaviourTransactionId;
    FixedString Speed;
    FixedString DialogInstance;
    float Timeout;
};

struct OsirisLookAtTask : public Task
{
    static constexpr auto Type = OsirisTaskType::LookAt;

    Guid TransactionGuid;
    EntityHandle EntityHandle;
    FixedString Bone;
    float Duration;
    int TransactionId;
};

struct OsirisTimerTask : public Task
{
    static constexpr auto Type = OsirisTaskType::Timer;

    float Time;
};

struct OsirisAutomatedDialogTask : public Task
{
    static constexpr auto Type = OsirisTaskType::AutomatedDialog;

    [[bg3::hidden]] void* DialogEventListenerVMT;
    Guid Dialog;
    Guid Trigger;
    Array<EntityHandle> SpeakerList;
    bool WaitForCompletion;
    int DialogInstanceId;
};

struct OsirisRateLimitedAutomatedDialogTask : public Task
{
    static constexpr auto Type = OsirisTaskType::RateLimitedAutomatedDialog;

    Guid Dialog;
    bool WaitForCompletion;
    Array<EntityHandle> SpeakerList;
    float MinDelay;
    float CullingDistance;
    Guid AdHistoryId;
};

struct OsirisFleeFromRelationTask : public MoveTask
{
    static constexpr auto Type = OsirisTaskType::FleeFromRelation;

    glm::vec3 Position;
    bool field_9C;
    float FleeRange;
    int field_A4;
    int FleeFromRelation;
};

struct OsirisFleeFromEntityTask : public MoveTask
{
    static constexpr auto Type = OsirisTaskType::FleeFromEntity;

    glm::vec3 Position;
    bool field_9C;
    float FleeRange;
    int field_A4;
    EntityHandle FleeFromEntity;
};

struct OsirisCombineTask : public Task
{
    static constexpr auto Type = OsirisTaskType::Combine;

    EntityHandle CombineTarget;
    Array<EntityHandle> CombineIngredients;
    int CombineAmount;
};



#if defined(BG3SE_LINUX)
// PORT (linux): the esv::Task family, measured against bin/bg3 (c8da62b1).
// Task is 0x38 bytes with its last member (Flags, a dword) at 0x30, so its dsize stops at
// 0x34 and it has four bytes of tail padding.  Itanium lets a derived task reuse them;
// MSVC does not, so every task whose first derived member is 4-aligned and <= 4 bytes
// sits at 0x34 here and would sit at 0x38 under upstream's own compiler.  The game does
// the Itanium thing.
//   The instrument is (b1), a matched Visit: every task has a
//     `bool SavegameVisit(eoc::SavegameVisitor*)` at run slot 3 of its 13-slot vtable, and
//     each one calls esv::Task::SavegameVisit (bin/bg3 0x3e45660) first and then visits
//     exactly its own members, naming each with an `EoCFS::str…` FixedString global and
//     choosing the visitor by type (0x2bb2810 float, 0x2bb8e90 byte, 0x2bb0bd0 int,
//     0x26caba0 glm::vec3, 0x2bba4f0 EntityHandle, vtable [+0x198] FixedString,
//     [+0x1a0] STDString).  Only esv::OsirisResurrectTask::SavegameVisit is matched by
//     name; the other three were reached through their vtable runs, whose slot map is
//     fixed by Resurrect's (0x7a8bc58 slot 3 == 0x7124340, the matched one) and by the
//     three slots all four runs share (SetObjectHandle 0x3400520, and 0x711f760/70/80):
//       0x7a8bc58 Resurrect            slot 3 = 0x7124340
//       0x7a8bd48 TeleportToLocation   slot 3 = 0x7124720
//       0x7a8bdc0 Timer                slot 3 = 0x71249e0
//       0x7a8beb0 Wander               slot 3 = 0x7124ab0
//   The base, member for member, from 0x3e45660 and the PathSettings helper it calls at
//     0x3e45740 (that one takes `this + 0x18`, so its offsets are PathSettings-relative):
//       0x3e456a6  mov [rbx+0x14]         TaskState
//       0x3e456a9  lea rdi, [rbx+0x18]    PathSettings, into 0x3e45740:
//                    0x3e45791 byte [+0]     PathClimbingMode
//                    0x3e457d2 byte [+1]     PathDroppingMode
//                    0x3e457dc lea rsi,[+4]  Speed, into 0x3e46760, which writes the
//                              storage at [+0] and the discriminant at [+4] -- so the
//                              game's variant<uint8_t,float> is 8 bytes with the index at
//                              4, exactly where libc++ puts it (see the Wander note below
//                              for the case where the two disagree)
//                    0x3e457fb byte [+0xc]   HasSmoothingNodes
//                    0x3e45813 byte [+0xd]   HasSpliningNodes
//                    0x3e4582e byte [+0xe]   HasTurningNodes
//       0x3e456c5  byte [rbx+0x28]        Failed
//       0x3e456da  int  [rbx+0x2c]        Reason
//       0x3e456fe  dword [rbx+0x30]       Flags, named EoCFS::strFlags (0x7ce5bdc)
//     Entity (0x8) and Type (0x10) get no assert: the savegame visitor writes neither, and
//     they are pinned only by exclusion between the vptr and TaskState.
//   The four flagged derived classes, each by its own visit:
//       Resurrect  0x7124340: movss [+0x34] Chance (EoCFS::strChance 0x7cc716c),
//                  movss [+0x38] HPPercentage (0x7cc7170), byte [+0x3c] IsResurrected
//                  (EoCFS::strIsResurrected 0x7cc7174, matched), FixedString [+0x40]
//                  Animation (EoCFS::strAnimation 0x7cc7100, matched)
//       Timer      0x71249e0: movss [+0x34] Time (0x7cc71a0), and nothing else -- so
//                  sizeof is 0x38, the base's own size, which MSVC could not produce
//       Wander     0x7124ab0: [+0x34] Range (EoCFS::strRange 0x7cc71a4, matched),
//                  EntityHandle [+0x38] Trigger (0x7cc76fc), the Anchor variant at [+0x40]
//                  through 0x7124bc0
//       Teleport   0x7124720: vec3 [+0x34] Position, FixedString [+0x40] PreviousLevel
//                  (EoCFS::strPreviousLevel 0x7ccc7cc, matched) and [+0x44] Level
//                  (EoCFS::strLevel 0x7ccc77c, matched), the eight bools at 0x48, 0x49,
//                  0x4c, 0x4a, 0x4b, 0x4e, 0x4f and 0x50 in that order (0x4d, field_51,
//                  is the one the game never visits), quat [+0x54] Rotation, STDString
//                  [+0x68] ArriveEvent (EoCFS::strArriveEvent 0x7ccfd24, matched)
//   Two-sided, and needed: both controls are tasks whose first derived member is 8-aligned,
//     and both put it at 0x38, not 0x34 --
//       esv::OsirisSteerTask::SavegameVisit 0x71243d0: EntityHandle [+0x38] Target
//         (EoCFS::strTarget, matched), vec3 [+0x40] TargetPos (EoCFS::strTargetPos,
//         matched), float [+0x4c] AngleTolerance, byte [+0x50] LookAt, byte [+0x51]
//         SnapToTarget (EoCFS::strSnapToTarget, matched)
//       esv::OsirisDisappearTask::SavegameVisit 0x3e45430: STDString [+0x38] FinishedEvent
//         (0x3e45546), vec3 [+0x48] TargetPos, float [+0x54] Angle (EoCFS::strAngle,
//         matched), float [+0x60] SpeedMultiplier, float [+0x64] DefaultSpeed, int [+0x5c]
//         StoryTransactionId, bytes [+0x6c..0x6f] IncreaseSpeed, OutOfSight, Offstage,
//         ValidTarget
//   OsirisWanderTask::Anchor is the one member whose upstream type is wrong here.
//     Upstream declares std::variant<EntityHandle, glm::vec3>, which libc++ lays out as a
//     16-byte union (12 rounded up to the 8-alignment of EntityHandle) with the index at
//     +0x10, sizeof 24.  The game's is 16 bytes with the discriminant byte at +0xc, read
//     on a187af94 off the Anchor accessor 0x7124c40: the EntityHandle arm is
//     `mov [rbx], rax; mov byte [rbx+0xc], 0` (0x7124cb3, 0x7124cb6), the vec3 arm
//     `mov qword [rbx], 0; mov dword [rbx+8], 0; mov byte [rbx+0xc], 1`
//     (0x7124d01..0x7124d0f), and the function reads the tag back at 0x7124cd7
//     (`movzx eax, byte [rbx+0xc]`).  So Duration is at 0x50 and Start at 0x54 in
//     esv::OsirisWanderTask::SavegameVisit 0x7124b30 -- [rbx+0x50] through the float
//     visitor 0x2bb2860, rbx+0x54 through the vec3 visitor 0x26cabf0 -- where
//     upstream's variant puts them at 0x58 and 0x5c, so Duration, Start and
//     BehaviourTransactionId would all read 8 bytes too far.
//     No new type is needed: upstream's own EntityOrVec3Variant
//     (GameDefinitions/Base/CommonTypes.h) is a `#pragma pack(4)` union of exactly these
//     two alternatives with a trailing uint8_t Type, which is 16 bytes with the tag at
//     +0xc -- the game's layout member for member.  It is already a by-value type with a
//     push, a do_get, a validator and a registered type name, and upstream uses it for
//     the same game shape in Components/Script.h, so nothing in the Lua layer changes.
//     This is not the *leading* hole a variant nested in a variant gets (that one is libc++'s
//     empty bases colliding, closed in the library by the sfinae-tag overlay, ext/toolchain/);
//     it is the *trailing* rounding before the tag, which no header patch changes.
//     MSVC rounds as libc++ does, so upstream reads these three members wrong on Windows
//     too; that is a note for a future upstream patch and not a scope question here --
//     ext/CMakeLists.txt:175 defines BG3SE_LINUX=1 unconditionally and libbg3se-ext.so is
//     the only artefact this repo builds.
//     PathSettings::Speed above is the negative control: variant<uint8_t,float> needs no
//     rounding and the two layouts agree there.
static_assert(offsetof(Task, TaskState) == 0x14);
static_assert(offsetof(Task, PathSettings) == 0x18);
static_assert(offsetof(::bg3se::PathSettings, PathClimbingMode) == 0x0);
static_assert(offsetof(::bg3se::PathSettings, PathDroppingMode) == 0x1);
static_assert(offsetof(::bg3se::PathSettings, Speed) == 0x4);
static_assert(offsetof(::bg3se::PathSettings, HasSmoothingNodes) == 0xc);
static_assert(offsetof(::bg3se::PathSettings, HasSpliningNodes) == 0xd);
static_assert(offsetof(::bg3se::PathSettings, HasTurningNodes) == 0xe);
static_assert(sizeof(::bg3se::PathSettings) == 0x10);
static_assert(offsetof(Task, Failed) == 0x28);
static_assert(offsetof(Task, Reason) == 0x2c);
static_assert(offsetof(Task, Flags) == 0x30);
static_assert(sizeof(Task) == 0x38);

static_assert(offsetof(OsirisResurrectTask, Chance) == 0x34);
static_assert(offsetof(OsirisResurrectTask, HPPercentage) == 0x38);
static_assert(offsetof(OsirisResurrectTask, IsResurrected) == 0x3c);
static_assert(offsetof(OsirisResurrectTask, Animation) == 0x40);
static_assert(sizeof(OsirisResurrectTask) == 0x48);

static_assert(offsetof(OsirisTimerTask, Time) == 0x34);
static_assert(sizeof(OsirisTimerTask) == 0x38);

static_assert(offsetof(OsirisWanderTask, Range) == 0x34);
static_assert(offsetof(OsirisWanderTask, Trigger) == 0x38);
static_assert(offsetof(OsirisWanderTask, Anchor) == 0x40);
// PORT (linux): the Anchor union is 16 bytes with the tag at +0xc, not libc++'s 24 with the
// index at +0x10 -- bin/bg3 a187af94 0x7124c40 (the accessor; both arms write the tag at
// +0xc, and 0x7124cd7 reads it back).
static_assert(sizeof(::bg3se::EntityOrVec3Variant) == 0x10);
static_assert(offsetof(::bg3se::EntityOrVec3Variant, Type) == 0xc);
// Duration and Start follow it, each named by its visitor in
// esv::OsirisWanderTask::SavegameVisit 0x7124b30 -- [rbx+0x50] through the float visitor
// 0x2bb2860 and rbx+0x54 through the vec3 visitor 0x26cabf0.
static_assert(offsetof(OsirisWanderTask, Duration) == 0x50);
static_assert(offsetof(OsirisWanderTask, Start) == 0x54);
// BehaviourTransactionId gets no assert: SavegameVisit never visits it, so 0x60 is only
// implied by Start's width.

static_assert(offsetof(OsirisTeleportToLocationTask, Position) == 0x34);
static_assert(offsetof(OsirisTeleportToLocationTask, PreviousLevel) == 0x40);
static_assert(offsetof(OsirisTeleportToLocationTask, Level) == 0x44);
static_assert(offsetof(OsirisTeleportToLocationTask, SetRotation) == 0x48);
static_assert(offsetof(OsirisTeleportToLocationTask, LeaveCombat) == 0x49);
static_assert(offsetof(OsirisTeleportToLocationTask, FindPosition) == 0x4a);
static_assert(offsetof(OsirisTeleportToLocationTask, FindFleePosition) == 0x4b);
static_assert(offsetof(OsirisTeleportToLocationTask, Executed) == 0x4c);
static_assert(offsetof(OsirisTeleportToLocationTask, UnchainFollowers) == 0x4e);
static_assert(offsetof(OsirisTeleportToLocationTask, SnapToGround) == 0x4f);
static_assert(offsetof(OsirisTeleportToLocationTask, AllowUnloadedLevel) == 0x50);
static_assert(offsetof(OsirisTeleportToLocationTask, Rotation) == 0x54);
static_assert(offsetof(OsirisTeleportToLocationTask, ArriveEvent) == 0x68);
static_assert(sizeof(OsirisTeleportToLocationTask) == 0x78);

// The two negative controls: an 8-aligned first derived member does not move into Task's
// tail padding, in either binary.
static_assert(offsetof(OsirisSteerTask, Target) == 0x38);
static_assert(offsetof(OsirisSteerTask, TargetPos) == 0x40);
static_assert(offsetof(OsirisSteerTask, AngleTolerance) == 0x4c);
static_assert(offsetof(OsirisSteerTask, LookAt) == 0x50);
static_assert(offsetof(OsirisSteerTask, SnapToTarget) == 0x51);
static_assert(offsetof(OsirisDisappearTask, FinishedEvent) == 0x38);
static_assert(offsetof(OsirisDisappearTask, TargetPos) == 0x48);
static_assert(offsetof(OsirisDisappearTask, StoryTransactionId) == 0x5c);
static_assert(offsetof(OsirisDisappearTask, Angle) == 0x54);
static_assert(offsetof(OsirisDisappearTask, SpeedMultiplier) == 0x60);
static_assert(offsetof(OsirisDisappearTask, DefaultSpeed) == 0x64);
static_assert(offsetof(OsirisDisappearTask, IncreaseSpeed) == 0x6c);
static_assert(offsetof(OsirisDisappearTask, OutOfSight) == 0x6d);
static_assert(offsetof(OsirisDisappearTask, Offstage) == 0x6e);
static_assert(offsetof(OsirisDisappearTask, ValidTarget) == 0x6f);
#endif
END_NS()

BEGIN_NS(lua)

LUA_POLYMORPHIC(esv::Task);

END_NS()
