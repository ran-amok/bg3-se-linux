#pragma once

BEGIN_NS(esv)

struct MovementState : public ProtectedGameObject<MovementState>
{
    virtual ~MovementState() = 0;
    virtual MovementType GetType() const = 0;
    virtual bool Enter() = 0;
    virtual bool Continue() = 0;
    virtual void Update(GameTime const&, float) = 0;
    virtual bool VMT28() = 0;
    virtual void Exit() = 0;
    virtual void VMT38() = 0;
    virtual bool VMT40() = 0;
    virtual void VMT48() = 0;
    virtual void OnAnimationEvent(void*) = 0;

    MovementMachine* MovementMachine;
    int TransactionId;

    // Lua getter helpers
    MovementType LuaGetType() const;
    //# P_GETTER(Type, LuaGetType)
};


struct MovementMachine : public ProtectedGameObject<MovementMachine>
{
    std::array<MovementState*, 6> CachedStates;
    bool Active;
    std::array<MovementState*, 2> Layers;
    EntityHandle Entity;
};


struct MSMoveToSettings
{
    glm::vec3 TargetOriginal;
    glm::vec3 TargetAdjusted;
    EntityHandle TargetHandle;
    Array<EntityHandle> IgnoreEntities;
    uint32_t AiTargetCheck;
    uint64_t CollisionMaskMove;
    uint64_t CollisionMaskStand;
    uint16_t SearchHorizon;
    uint8_t PathClimbingMode;
    uint8_t PathDroppingMode;
    std::variant<uint8_t, float> Speed;
    uint32_t SurfaceAvoidance;
    uint8_t DangerousAuraAvoidance;
    float CloseEnoughMin;
    float CloseEnoughMax;
    float CloseEnoughFloor;
    int field_118;
    int CloseEnoughPreference;
    uint8_t WeightFuncType;
    bool AddSourceBoundsToMargin;
    bool IgnoreDynamics;
    bool IsAvoidingObstacles;
    bool IsAvoidingFallDamage;
    bool IsPreciseItemInteraction;
    bool HasSpliningNodes;
    bool HasTurningNodes;
};

struct MSMoveTo : public MovementState
{
    static constexpr auto Type = MovementType::MoveTo;

    int PathId2;
    int PathId;
    glm::vec3 TargetOriginal2;
    glm::vec3 TargetAdjusted2;
    glm::vec3 TargetAdjusted3;
    float NextTickTime;
    int field_48;
    float NextPathfindingTime;
    glm::vec3 field_50;
    int field_5C;
    bool Finished;
    int State;
    uint8_t field_68;
    bool PathfindingStarted;
    bool CheckEndPosition;
    bool IsAiEntity;
    bool NeedsPathfinding;
    [[bg3::hidden]] UnknownFunction DestinationFunc;
    MSMoveToSettings Settings;
    MSMoveToSettings CurrentSettings;
};

struct MSIdle : public MovementState
{
    static constexpr auto Type = MovementType::Idle;
};

struct MSMoveToSync : public MovementState
{
    static constexpr auto Type = MovementType::MoveToSync;

    Array<glm::vec3> Positions;
    Array<bool> field_28;
    Array<bool> field_38;
    glm::vec3 field_48;
    int field_54;
    int field_58;
    uint8_t field_5C;
    uint8_t field_5D;
    int PathId;
    glm::vec3 NextPosition;
};

struct ClimbResult
{
    int field_0;
    int field_4;
    int field_8;
    glm::vec3 field_C;
    int field_18;
};

struct ClimbBlendspaceData
{
    FixedString WorldClimbingBlendspace;
    float BlendedDuration;
    float StepUpHeightDelta;
};

struct ClimbData
{
    glm::vec3 CurrentPosition;
    glm::vec3 ClimbStartPosition;
    glm::vec3 ClimbEndPosition;
    glm::vec3 ClimbEndPosition2;
    glm::vec3 ClimbEndPosition3;
    float Bounds;
    float HeightDelta;
    float StepHeight;
    float WorldClimbingHeight;
    float MovementSpeed;
    bool ClimbUp;
};

struct MSClimbing : public MovementState
{
    static constexpr auto Type = MovementType::Climbing;

    ClimbBlendspaceData Blendspace;
    ClimbData ClimbData;
    ClimbResult ClimbResult;
    int State;
    [[bg3::hidden]] Array<void*> ClimbNodes;
    float TimeRemaining;
    bool GenomeUpdated;
};

struct MSFalling : public MovementState
{
    static constexpr auto Type = MovementType::Falling;

    float Height;
    glm::vec3 StartPosition;
    glm::vec3 FallPosition;
    bool FallAnimationStarted;
    bool ExpectedFall;
    bool OnDestroyedPlatform;
    uint8_t field_37;
    float TimeSpent;
    int State;
};

struct MSRepose : public MovementState
{
    static constexpr auto Type = MovementType::Repose;

    bool EndRepose;
};

#if defined(BG3SE_LINUX)
// PORT (linux): three of the four flagged esv::MovementState subclasses against bin/bg3
// c8da62b1.  The base has a vptr, a MovementMachine* at 0x08 and an int at 0x10, so its nvsize
// is 0x14 and its sizeof 0x18 -- four bytes of tail padding that Itanium hands to the derived
// class and MSVC would not, so every first member below would start at 0x18 under MSVC.
// The family keeps its macOS symbols and most of them are matched, so each class is named
// outright rather than inferred.  MovementMachine at 0x08 comes with them:
// esv::MSFalling::Enter 0x324e1a0 opens `mov rax, qword [rdi+8]` / `mov rdi, qword [rax+0x48]`.
//
//   esv::MSMoveTo::OnCalculatePathFinished 0x280fe70 (matched s2:0.94 to macOS 0x1055eb1d0):
//     `mov dword [r14+0x14], 0xfffffac7` at 0x2810031 -- a non-zero sentinel, so a value store,
//     not a widened zero-fill -- is PathId2, and `mov dword [r14+0x18], ebp` at 0x281002d is
//     PathId; 0x281014a writes both at once as `mov qword [r14+0x14], rax`.  FindPath 0x2815e40
//     (matched r6:2) reads the same dword at 0x14 and uses it as a path index
//     (`movsxd rsi, dword [rbx+0x14]` at 0x2816380).
//
//   esv::MSFalling, off Update 0x349a850 (matched s1:0.88 to macOS 0x1055e7420), Enter
//     0x324e1a0 (r3:2), FinishFall 0x324e630 (s1:1.00) and OnAnimationEvent 0x45ce370 (r3:2):
//       Height 0x14: `movss xmm0, dword [rbx+0x14]` at 0x349acc5, copied into the
//         esv::FallingSystem event payload at [r13+0xc] before the Anubis legacy event fires.
//       StartPosition 0x18: Enter passes `lea rsi, [r14+0x18]` to the matched
//         esv::Character::SetTranslate(Vector3f const&, bool, bool, bool) 0x24bb8f0, so the
//         callee's own signature types it; FinishFall hands the same 12 bytes to
//         esv::LegacyChasmHelpers::CheckMoveIntoChasm as `mov r8, qword [rbx+0x18]` +
//         `mov r9d, dword [rbx+0x20]`; Update refills it from a stack vec3 at 0x349ad8e.
//       FallPosition 0x24: `movsd xmm3, qword [rbx+0x24]` / `mulss xmm0, dword [rbx+0x2c]` at
//         0x349a8f4 reads it as a vec3 beside StartPosition, and 0x349ad7e clears the same
//         12 bytes as `mov qword [rbx+0x24], 0` / `mov dword [rbx+0x2c], eax`.
//       FallAnimationStarted 0x30, ExpectedFall 0x31, field_37 0x33: OnAnimationEvent sets
//         `byte [rbx+0x30]` to 1 and then 0 (0x45ce3bf, 0x45ce3c5); Enter tests
//         `cmp byte [r14+0x31], 0` before raising the character flag; Update tests and clears
//         `byte [rbx+0x33]` (0x349aaa0, 0x349ac06).  OnDestroyedPlatform 0x32 is touched by
//         none of them and gets no assert.
//       TimeSpent 0x34: `addss xmm0, dword [rbx+0x34]` / `movss [rbx+0x34], xmm0` at 0x349a8e6
//         accumulates the frame delta, and 0x349aa99 resets it.
//       State 0x38: the dword the state machine walks -- set to 2 at 0x349a9f3, 1 at 0x349ac31,
//         3 at 0x349ad9e and 3 by OnAnimationEvent at 0x45ce3a9, tested against 1 by FinishFall
//         and against 3 by Exit 0x324e4c0.
//
//   esv::MSClimbing::SetData 0x3247550 (matched r5:2 to macOS 0x1055e6cf0):
//     `lea rdi, [rbx+0x14]` into the matched ls::FixedString::operator=(FixedString&&)
//     0x242e570 at 0x324763d types the first member of Blendspace as a FixedString at 0x14, and
//     `mov rax, qword [r15+4]` / `mov qword [rbx+0x18], rax` at 0x3247649 carries the source's
//     two trailing floats to 0x18 and 0x1c -- ClimbBlendspaceData whole, 12 bytes at 0x14.
//     ClimbData follows at 0x20: the same function stores a 12-byte position there as
//     `mov qword [rbx+0x20], rax` + `mov dword [rbx+0x28], eax` (0x3247660, 0x3247666).
//
// sizeof is no witness for any of the three: nothing in this build allocates one by a constant,
// and MSVC would differ in size anyway only because of the same shift the offsets already show.
// MSRepose is the fourth flagged subclass and is NOT settled here -- the game has no MSRepose
// symbol at all, in either binary.
//
// PORT (linux): the fourth subclass, MSRepose, and the family's sizes.
// The game's own per-type factory is esv::MovementMachine::GetState(eoc::EMovementState)
// 0x34996d0 (matched r4:2 to macOS 0x1055fcd90).  It does `add esi, -2` and jumps through the
// table at .rodata 0x1bd4afc, so arm i builds MovementType i+2, and every arm is
// `mov edi, <sizeof>; call operator new`, then the base's two members, then the derived vtable
// and the derived defaults.  So one function states the whole family's sizeof -- which corrects
// the three rows above: they say "nothing in this build allocates one by a constant", and this
// is that allocation.  The arms, in enum order:
//
//   MoveTo 2       0x349977f  sizeof 0x1a0  vtable 0x7a74dc8
//   MoveToSync 3   0x349991d  sizeof  0x70  vtable 0x7a74e30
//   Idle 4         0x3499731  sizeof  0x18  vtable 0x7a74d60
//   Climbing 5     0x349996a  sizeof  0xb0  vtable 0x7a74c90 (installed by a real ctor 0x4bfccb0)
//   Falling 6      0x349998d  sizeof  0x40  vtable 0x7a74cf8
//   Repose 7       0x3499756  sizeof  0x18  vtable 0x7a74e98
//
// Only MSRepose's size is an ABI witness (0x18 Itanium vs 0x20 MSVC, because the bool would
// start at 0x18 and round up); for the others the base's 8-alignment absorbs the shift and both
// ABIs give the same number, so those asserts witness completeness, not the ABI -- the
// stats::Functor caveat, checked before use.  MSIdle at 0x18 is the family's first negative
// control: same 0x18 as MSRepose and no byte store at 0x14, because it has no members.
//
// MSRepose is the game's `esv::MSMovement` -- which is why the name was not findable.  Its
// vtable 0x7a74e98 carries the oracle's esv::MSMovement::{~,GetType,Update,Exit} at slots
// 1/2/5/7 (matched r4:2 to macOS 0x1055f24c0/2560/2570/25e0), and slot 2 is
// `mov eax, 7; ret` -- MovementType::Repose.  That closes the identification twice over: the
// factory arm for type 7 installs this vtable, and the vtable's own GetType returns 7.  All six
// runs in 0x7a74c90..0x7a74e98 are 11 slots with GetType at index 2, and the six GetTypes
// return 5, 6, 4, 2, 3, 7 -- one per MovementType, with Climbing and Falling named by symbol.
//
//   EndRepose 0x14: a value store of 1, twice.  esv::MSMoveTo::Update 0x25bb280 (matched r5:3)
//     does `mov rbx, qword [rbp+8]` / `mov rax, qword [rbx+0x10]` -- the machine's repose slot,
//     CachedStates[2], with GetState inlined at 0x25bc110 to fill it when null -- and then
//     `mov byte [rax+0x14], 1` at 0x25bbb1b.  esv::MSMoveToSync::Continue 0x2813740 (s1:0.85)
//     does the same at 0x2813d28.  The read side is the class's own
//     esv::MSMovement::Update 0x6f2d510: `cmp byte [rbx+0x14], 0` at 0x6f2d525, and only the
//     non-zero arm goes on to index ls::TypeId<esv::repose::StateSystem, ecs::SystemsContext>
//     (0x7cd7570) and clear the repose component -- which names the member as well as the
//     class.  esv::MSMovement::Exit 0x6f2d5d0 is a bare `ret`, so Update is the only body.
//
// The base's own two members come with every arm: `mov qword [rax+8], r14` is the machine and
// `mov dword [rax+0x10], 0` is TransactionId, bracketed by that pointer below and by
// MSRepose's bool at 0x14 above.  MSMoveToSync is the family's second negative control -- its
// first member is an 8-aligned Array, so the ctor passes `lea rdi, [rax+0x18]` (0x349993c) to
// the sub-object constructor and nothing moves into the padding; its PathId is witnessed by
// the same -1337 sentinel as MSMoveTo's, `mov dword [rbx+0x60], 0xfffffac7` at 0x3499948.
// Two of the asserts above are re-witnessed here for free: the MSMoveTo arm writes
// `mov qword [rbx+0x14], 0xfffffac7fffffac7` at 0x34997be (PathId2 and PathId at once), and the
// MSFalling arm writes `mov qword [rax+0x34], 0x100000000` at 0x34999c1 -- TimeSpent 0 at 0x34
// and State 1 at 0x38 in one value-bearing store.
static_assert(offsetof(MovementState, MovementMachine) == 0x08);

static_assert(offsetof(MSMoveTo, PathId2) == 0x14);
static_assert(offsetof(MSMoveTo, PathId) == 0x18);

static_assert(offsetof(MSFalling, Height) == 0x14);
static_assert(offsetof(MSFalling, StartPosition) == 0x18);
static_assert(offsetof(MSFalling, FallPosition) == 0x24);
static_assert(offsetof(MSFalling, FallAnimationStarted) == 0x30);
static_assert(offsetof(MSFalling, ExpectedFall) == 0x31);
static_assert(offsetof(MSFalling, field_37) == 0x33);
static_assert(offsetof(MSFalling, TimeSpent) == 0x34);
static_assert(offsetof(MSFalling, State) == 0x38);

static_assert(offsetof(MSClimbing, Blendspace) == 0x14);
static_assert(offsetof(MSClimbing, ClimbData) == 0x20);

static_assert(offsetof(MovementState, TransactionId) == 0x10);
static_assert(sizeof(MovementState) == 0x18);
static_assert(sizeof(MSIdle) == 0x18);
static_assert(offsetof(MSMoveToSync, Positions) == 0x18);
static_assert(offsetof(MSMoveToSync, PathId) == 0x60);
static_assert(sizeof(MSMoveToSync) == 0x70);
static_assert(sizeof(MSClimbing) == 0xb0);
static_assert(sizeof(MSFalling) == 0x40);
static_assert(sizeof(MSMoveTo) == 0x1a0);

static_assert(offsetof(MSRepose, EndRepose) == 0x14);
static_assert(sizeof(MSRepose) == 0x18);
#endif

END_NS()

BEGIN_NS(lua)

LUA_POLYMORPHIC(esv::MovementState);

END_NS()
