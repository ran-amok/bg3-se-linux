#pragma once

BEGIN_NS(esv)

struct BehaviourMachine : public ProtectedGameObject<BehaviourMachine>
{
    std::array<BehaviourState*, 10> CachedStates;
    std::array<BehaviourState*, 2> Layers;
    EntityHandle Entity;
};

struct BehaviourState : public ProtectedGameObject<BehaviourMachine>
{
    virtual ~BehaviourState() = 0;
    virtual BehaviourType GetType() const = 0;
    virtual bool Enter() = 0;
    virtual bool Continue() = 0;
    virtual void Update(GameTime const&) = 0;
    virtual bool CanEnter() = 0;
    virtual void Exit() = 0;
    virtual void Exit2() = 0;
    virtual bool CanExit() = 0;
    virtual bool VMT48() = 0;
    virtual bool VMT50() = 0;
    virtual bool SavegameVisit(ObjectVisitor&) = 0;
    virtual bool HasStarted() = 0;
    virtual bool SyncData(ScratchBuffer&) = 0;
    virtual bool VMT70() = 0;

    BehaviourMachine* BehaviourMachine;
    int TransactionId;

    // Lua getter helpers
    BehaviourType LuaGetType() const;
    //# P_GETTER(Type, LuaGetType)
};

struct BSMoveCheckpoint : public AiPathCheckpoint
{
    // PORT (linux): upstream's second int is a phantom.  The game's class (it spells it
    // esv::BSMoveCheckPoint, capital P) is eoc::AiPathCheckpoint plus exactly ONE int, and
    // under Itanium that int lands at 0x2c inside the base's tail padding, so the whole
    // record is 0x30 -- the same size as the base.  Upstream, calibrated to MSVC where the
    // int cannot go there, has it at 0x30 and a second one at 0x34; field_34 was reading four
    // bytes past the end of the element, i.e. the next element's vptr.  The name is left
    // alone (it is a Lua-visible property and upstream's offset-derived names are kept
    // everywhere else) even though it now sits at 0x2c.
    int field_30;
};

#if defined(BG3SE_LINUX)
// Witnesses, read with tools/disasm.py on the build-id c8da62b1 binary:
//   esv::BSMove::AddCheckpointsFromPath, bin/bg3 0x2c70ab0 (matched, s3:1.00) -- three sites
//     that fill one checkpoint each, all of the shape `lea rax, [rax+rax*2]; shl rax, 4`
//     (element stride 0x30) followed by `movlps [rcx+rax+8]` / `movss [rcx+rax+0x10]`
//     (Position), `mov [rcx+rax+0x18], rsi` (Portal), `mov [rcx+rax+0x20], rdi` with the
//     0xffc0000000000000 null handle (field_20), `mov byte [rcx+rax+0x28], r8b` (Flags) and
//     `mov dword [rcx+rax+0x2c], <r9d | eax | 1>` -- a value store at 0x2c, and NOTHING at
//     0x30 anywhere in the function.  0x2c70dad, 0x2c70f2b and 0x2c71009.
//   The macOS oracle settles the size independently, with the family's own negative control:
//     ls::LEGACY_Set<esv::BSMoveCheckPoint, ...>::Ensure 0x1052a4f20 reallocates with
//     `mov ecx, 0x30; mul rcx`, and its Insert 0x1052a5290 shifts elements by 0x30 copying
//     +8 and +0x18 (two movups), the byte at +0x28 and the dword at +0x2c.  The same
//     template over the BASE, ls::LEGACY_Set<eoc::AiPathCheckpoint, ...>::Insert 0x103386fc0,
//     is byte-for-byte the same loop MINUS the dword at 0x2c -- one derived member, in the
//     base's tail padding, and the base is 0x30 too (its Ensure 0x1013d4b60, `mov ecx, 0x30`).
static_assert(offsetof(AiPathCheckpoint, Position) == 0x8, "the movlps/movss pair");
static_assert(offsetof(AiPathCheckpoint, Portal) == 0x18, "the first handle store");
static_assert(offsetof(AiPathCheckpoint, field_20) == 0x20, "the null-handle store");
static_assert(offsetof(AiPathCheckpoint, Flags) == 0x28, "the byte store");
static_assert(sizeof(AiPathCheckpoint) == 0x30, "LEGACY_Set<eoc::AiPathCheckpoint>::Ensure");
static_assert(offsetof(BSMoveCheckpoint, field_30) == 0x2c, "the dword store at 0x2c");
static_assert(sizeof(BSMoveCheckpoint) == 0x30, "the element stride, three sites in 0x2c70ab0");
#endif

struct BSMove : public BehaviourState
{
    static constexpr auto Type = BehaviourType::Move;

    Array<BSMoveCheckpoint> Checkpoints;
    uint16_t SearchHorizon;
    navigation::TargetInfo TargetInfo;
    float PathDistanceXZ;
    uint8_t Flags;
    PathSettings PathingSettings;
    PathSettings PathingSettings2;
    uint32_t SurfaceAvoidance;
    uint8_t DangerousAuraAvoidance;
    glm::vec3 LadderUsePosition;
    int State;
    int MovementTransactionId;
    int ActionTransactionId;
    EntityHandle UseItem;
    AiPathId PathId;
    AiPathId PathId2;
};

struct BSMoveAndX : public BSMove
{
    PathSettings ActionPathSettings;
    uint16_t ActionSearchHorizon;
    bool field_102;
};

struct BSMoveToAndTalk : public BSMoveAndX
{
    static constexpr auto Type = BehaviourType::MoveToAndTalk;

    EntityHandle TalkTo;
    FixedString Speed;
    FixedString DialogInstance;
    bool Failed;
    bool DialogRequested;
    float Timeout;
    float TimeSpent;
};

struct BSMoveItem : public BSMoveAndX
{
    static constexpr auto Type = BehaviourType::MoveItem;

    EntityHandle Item;
    glm::vec3 MoveToPosition;
    int Amount;
    int ItemActionTransactionId;
    int SteeringTransactionId;
    bool Failed;
};

struct BSMoveAndUseItem : public BSMoveAndX
{
    static constexpr auto Type = BehaviourType::MoveAndUseItem;

    EntityHandle Item;
    int ItemActionTransactionId;
    uint8_t Flags2;
    bool IsInteraction;
    bool Use;
};

struct BSPickupItem : public BSMoveAndX
{
    static constexpr auto Type = BehaviourType::PickupItem;

    EntityHandle Item;
    EntityHandle Inventory;
    Guid field_118;
    int Amount;
    int16_t Slot;
    bool Failed;
    uint8_t FailReason;
    uint8_t field_130;
};

struct BSWander : public BehaviourState
{
    static constexpr auto Type = BehaviourType::Wander;

    [[bg3::hidden]] void* Flood;
    float TimeRemaining;
    float Range;
    EntityHandle Target;
    // std::variant<EntityHandle, glm::vec3> Anchor;
    // std::variant alignment differs from larian variant, so we can't use it here :(
    [[bg3::hidden]] uint64_t _PAD[2];
    glm::vec3 TargetPosition;
    PathSettings WanderPathSettings;
    bool Failed;
    int MovementTransactionId;
};

struct BSAppear : public BehaviourState
{
    static constexpr auto Type = BehaviourType::Appear;

    FixedString SpawnEffect;
    int AppearActionTransactionId;
    bool Finished;
};

struct BSFollowCharacter : public BehaviourState
{
    static constexpr auto Type = BehaviourType::FollowCharacter;

    [[bg3::hidden]] uint64_t FollowOutput[4];
    EntityHandle FollowCharacter;
    int BehaviourTransactionId;
    bool FollowParty;
    bool IsRegisteredToPartyGroup;
};

struct BSPatrol : public BehaviourState
{
    static constexpr auto Type = BehaviourType::Patrol;

    [[bg3::hidden]] uint64_t PatrollingOutput[4];
};

struct BSForce : public BehaviourState
{
    static constexpr auto Type = BehaviourType::Force;

    bool HasAction;
    bool ActionLayer;
    ActionState* Action;
    bool HasMovement;
    int MovementLayer;
    MovementState* Movement;
    bool HasSteering;
    int SteeringLayer;
    SteeringState* Steering;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the two flagged records in this family, measured against bin/bg3 and both already
// right -- no mirror change.
//
// The family is named the way esv::MovementState's was: eleven vtable runs sit consecutively in
// .data.rel.ro from 0x7a73d00 to 0x7a74398, each with GetType at slot 2 as `mov eax, N; ret`, and
// the ten BehaviourType values come out one apiece (Move twice -- 0x7a74218 is the one whose slot 0
// matches esv::BSMove::~BSMove).  Appear (10) is 0x7a73e50 and Force (4) is 0x7a73d00.
//
// `esv::BehaviourMachine::CreateState(eoc::EBehaviourState)` 0x269dc90 (r3:2) is the family's
// factory and states all ten at once: a 12-arm jump table off `.rodata` 0x1bd4964, each arm
// `mov edi, <sizeof>; call operator new` then the base's BehaviourMachine* at 0x08 (an argument
// store) and TransactionId at 0x10, then either the derived ctor or the derived defaults inline.
//
// BSAppear is the game's esv::BSSpawn -- the factory arm at 0x269df6c allocates 0x20 and calls the
// ctor 0x6f24790, which installs vtable 0x7a73e50 and then acquires ONE FixedString through
// ls::gst::Acquire from the global at 0x7cc70e4, which matches.tsv names
// `EoCAN::UTIL_Spawn_01`, and stores its index to 0x14.  That is a value store whose source names
// the member, so SpawnEffect is settled without leaning on a score; 0x18 and 0x1c follow as the
// zeroed dword and byte, and 0x20 is the allocation.  MSVC would put SpawnEffect at 0x18 and make
// the record 0x28, so both the offset and the size discriminate the ABI.  esv::BSSpawn::Update
// 0x6f24820 (s5:0.80) and esv::BehaviourState::IsBSSpawn 0x6f235b0 (r2:2) corroborate the name.
//
// BSForce's arm is 0x269dfb7: `mov edi, 0x40`, vtable 0x7a73d00, then
// `mov word [rax+0x14], 0` (two bytes, HasAction and ActionLayer -- it cannot have been widened
// downwards because 0x10..0x14 is the base's live TransactionId), qword 0 at 0x18,
// byte 0 at 0x20, **`mov dword [rax+0x24], 1`** -- a non-zero value store, so MovementLayer is
// pinned outright -- qword 0 at 0x28, byte 0 at 0x30, dword 0 at 0x34 and qword 0 at 0x38.
// Every one of those differs under MSVC (0x18/0x19/0x20/0x28/0x2c/0x30/0x38/0x3c/0x40, sizeof 0x48).
static_assert(offsetof(BehaviourState, BehaviourMachine) == 0x8);
static_assert(offsetof(BehaviourState, TransactionId) == 0x10);
static_assert(sizeof(BehaviourState) == 0x18);

static_assert(offsetof(BSAppear, SpawnEffect) == 0x14);
static_assert(offsetof(BSAppear, AppearActionTransactionId) == 0x18);
static_assert(offsetof(BSAppear, Finished) == 0x1c);
static_assert(sizeof(BSAppear) == 0x20);

static_assert(offsetof(BSForce, HasAction) == 0x14);
static_assert(offsetof(BSForce, ActionLayer) == 0x15);
static_assert(offsetof(BSForce, Action) == 0x18);
static_assert(offsetof(BSForce, HasMovement) == 0x20);
static_assert(offsetof(BSForce, MovementLayer) == 0x24);
static_assert(offsetof(BSForce, Movement) == 0x28);
static_assert(offsetof(BSForce, HasSteering) == 0x30);
static_assert(offsetof(BSForce, SteeringLayer) == 0x34);
static_assert(offsetof(BSForce, Steering) == 0x38);
static_assert(sizeof(BSForce) == 0x40);
#endif

END_NS()

BEGIN_NS(lua)

LUA_POLYMORPHIC(esv::BehaviourState);

END_NS()
