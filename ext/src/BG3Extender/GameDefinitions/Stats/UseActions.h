#pragma once

#include <GameDefinitions/Base/Base.h>

BEGIN_SE()

struct IActionData;

struct [[bg3::hidden]] IActionDataVMT
{
    using DtorProc = void (IActionData*, bool);
    using CloneProc = IActionData* (IActionData*);
    using IsEqualProc = bool (IActionData*, IActionData const&);
    using VisitProc = bool (IActionData*, ObjectVisitor&);
    using Unknown5proc = bool (IActionData*);

    DtorProc* Dtor;
    CloneProc* Clone;
    IsEqualProc* IsEqual;
    VisitProc* Visit;
    Unknown5proc* Unknown5;
};

struct IActionData
{
    [[bg3::hidden]] IActionDataVMT* vftable{ nullptr };
    FixedString Animation;
    STDString Conditions;
    [[bg3::readonly]] ActionDataType Type{ ActionDataType::Unknown };
};

struct OpenCloseActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::OpenClose;
};

struct DestroyActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Destroy;
};

struct TeleportActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Teleport;

    FixedString Source;
    uint8_t SourceType{ 0 };
    FixedString Target;
    uint8_t TargetType{ 0 };
    FixedString EventID;
    uint8_t Visibility{ 0 };
    bool AiUseInCombat{ false };
    bool BlockMapMarkerNavigation{ false };
    bool SnapToGround{ false };
};

struct CreateSurfaceActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::CreateSurface;

    SurfaceType SurfaceType{ SurfaceType::None };
    float LifeTime{ 0.0f };
    float Radius{ 0.0f };
    bool ExternalCauseAsSurfaceOwner{ false };
};

struct DestroyParametersActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::DestroyParameters;

    uint8_t TargetItemState{ 0 };
    bool SnapToGround{ false };
    FixedString ExplodeFX;
    FixedString VisualDestruction;
    FixedString VisualWithDynamicPhysics;
    float FadeOutDelay{ 0.0f };
    FixedString FadeOutFX;
    FixedString TemplateAfterDestruction;
};

struct EquipActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Equip;
};

struct ConsumeActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Consume;

    FixedString StatsId;
    int32_t StatusDuration{ 0 };
    bool Consume{ false };
    bool IsHiddenStatus{ false };
};

struct StoryUseActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::StoryUse;
};

struct DoorActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Door;

    bool SecretDoor{ false };
    // PORT (linux): uncommented.  The game visits two bools here, not one:
    // eoc::DoorActionData::Visit bin/bg3 0x3858730 hands `lea rdx, [r14+0x21]` with
    // EoCFS::strSecretDoor and then `add r14, 0x22` with
    // (anonymous namespace)::strScriptControlledDoor, both through the visitor's bool slot
    // [rax+0xc0] (macOS 0x10138d380 names both globals).  sizeof stays 0x28, so nothing moves.
    bool ScriptControlledDoor{ false };
};

struct CreatePuddleActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::CreatePuddle;

    SurfaceType SurfaceType{ SurfaceType::None };
    float LifeTime{ 0.0f };
    float Timeout{ 0.0f };
    int TotalCells{ 0 };
    int CellsAtGrow{ 0 };
    float GrowTimer{ 0.0f };
    bool ExternalCauseAsSurfaceOwner{ false };
    bool ApplyDeathTypeBloodCheck{ false };
};

struct BookActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Book;

    FixedString BookId;
};

struct UseSpellActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::UseSpell;

    FixedString Spell;
    bool Consume{ false };
    Guid Class;
};

struct SpellBookActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::SpellBook;

    FixedString Spell;
    bool Consume{ false };
};

struct SitActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Sit;

    float Heal{ 0.0f };
};

struct LieActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Lie;

    float Heal{ 0.0f };
};

struct InsertActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Insert;

    int8_t InsertSlots{ 0 };
};

struct StandActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Stand;
};

struct LockpickActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Lockpick;

    bool Consume{ false };
};

struct StoryUseInInventoryActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::StoryUseInInventory;
};

struct DisarmTrapActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::DisarmTrap;

    bool Consume{ false };
};

struct ShowStoryElementUIActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::ShowStoryElementUI;

    int UIType{ 0 };
    STDString UIStoryInstance;
};

struct CombineActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Combine;

    int8_t CombineSlots{ 0 };
    bool IsBase{ false };
};

struct LadderActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Ladder;

    float BottomHorizontalOffset{ 0.0f };
    float BottomVerticalOffset{ 0.0f };
    float TopMidOffset{ 0.0f };
    float TopDetachOffset{ 0.0f };
    float NodeLadderOffest{ 0.0f };
    float TopAttachNearOffset{ 0.0f };
    float TopMidToPlatformFixedLength{ 0.0f };
    float TopLineTolerance{ 0.0f };
    int ClimbDirection{ 0 };
    bool AllowScaling{ false };
};

struct PlaySoundActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::PlaySound;

    FixedString ActivateSoundEvent;
    bool PlayOnHUD{ false };
};

struct SpawnCharacterActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::SpawnCharacter;

    FixedString RootTemplate;
    FixedString LocalTemplate;
    FixedString SpawnFX;
};

struct ConstrainActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Constrain;

    float Damage{ 0.0f };
};

struct RecipeActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Recipe;

    FixedString Recipe;
};

struct Unknown31ActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Unknown31;
};

struct ThrowActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Throw;

    float FallbackPreviewRadius{ 0.0f };
};

struct LearnSpellActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::LearnSpell;

    FixedString Spell;
    bool Consume{ false };
};

struct Unknown34ActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Unknown34;
};

struct Unknown35ActionData : public IActionData
{
    static constexpr auto StaticType = ActionDataType::Unknown35;
};


// PORT (linux): the eoc::IActionData family, measured against bin/bg3
// c8da62b137ca.  Every subclass's first derived member sits inside the base's tail
// padding -- the base is `{vftable, FixedString Animation, STDString Conditions,
// ActionDataType Type}`, dsize 0x21 and sizeof 0x28, so 0x21..0x27 is padding that the
// Itanium ABI hands to the derived class and MSVC does not.  Three instruments, all on
// this build:
//
//   1. The per-class allocator.  Every concrete class has exactly one function that
//      stores its vtable run, and it is `mov edi, <sizeof>; call operator new`, the
//      vtable, `mov byte/word [rax+0x20], <EActionType id>` and the derived defaults.
//      The id is what NAMES the class in a binary with no symbol for it: it is the
//      ActionDataType above.  The addresses are in the per-class comments below.
//   2. `Visit(ls::ObjectVisitor*, ls::GameObjectTemplate const*)`, which hands the
//      visitor `&this->member` together with the FixedString global that is the member's
//      serialised key -- so one read gives offset, type and name.  The visitor slots in
//      this build: [rax+0x48] a "has value" predicate, [+0xb0] int8, [+0xb8] int32 via a
//      stack temp, [+0xc0] bool, [+0xe0] int32, [+0xf0] float, [+0x198] FixedString,
//      [+0x1a0] STDString, [+0x1b0] Guid.
//   3. The macOS oracle, which keeps all 24 `eoc::*ActionData::Visit` symbols and the
//      names of every key global, and is the same Itanium ABI.  It is the corroboration,
//      not the measurement: every offset asserted below is also read off bin/bg3.
//
// The base's own vtable run is 0x790f4e0 (six slots: D1, D0, Clone, IsEqual, Visit,
// slot 5); the 24 derived runs are at 0x790f520..0x790fb00 on a 0x40 stride and the eight
// that do not override Visit at 0x7a8fa68..0x7a8fc28.
//
// `sizeof` is an ABI witness for 18 of the 23 records (MSVC, refusing the padding, would
// be 8 bytes longer) and NOT for TeleportActionData, LadderActionData,
// PlaySoundActionData, SpellBookActionData or LearnSpellActionData, where the next
// member's alignment absorbs the shift and MSVC lands on the same size; for those five
// only the member offsets discriminate.  The family's internal negative controls are the
// 8-aligned members, which alignment keeps out of the padding whatever the ABI:
// ShowStoryElementUIActionData::UIStoryInstance at 0x28 behind UIType at 0x24, and
// UseSpellActionData::Class at 0x30 behind Consume at 0x28.  The nine subclasses with no
// members of their own all allocate 0x28, which is the control on the base's own size.
// No phantom trailing member anywhere in this family: the mirror's `sizeof` already
// equalled the game's allocation for all 32 classes.

static_assert(offsetof(IActionData, Animation) == 0x8);
static_assert(offsetof(IActionData, Conditions) == 0x10);
static_assert(offsetof(IActionData, Type) == 0x20);
static_assert(sizeof(IActionData) == 0x28);
static_assert(sizeof(OpenCloseActionData) == 0x28);          // allocator 0x37b5e40, id 1
static_assert(sizeof(DestroyActionData) == 0x28);            // allocator 0x44bdf60, id 2
static_assert(sizeof(EquipActionData) == 0x28);              // allocator 0x7154320, id 6
static_assert(sizeof(StoryUseActionData) == 0x28);           // allocator 0x2fe42c0, id 8
static_assert(sizeof(StandActionData) == 0x28);              // allocator 0x7154390, id 17
static_assert(sizeof(StoryUseInInventoryActionData) == 0x28);// allocator 0x3e5bc20, id 19
static_assert(sizeof(Unknown31ActionData) == 0x28);          // allocator 0x403f280, id 31
static_assert(sizeof(Unknown34ActionData) == 0x28);          // allocator 0x44bdf90, id 34
static_assert(sizeof(Unknown35ActionData) == 0x28);          // allocator 0x3bdb850, id 35

// id 3, allocator 0x3982400 (sizeof 0x40; `mov [rax+0x28], sil` and `mov [rax+0x30], sil`
// are the two byte members, `mov [rax+0x34], rdx` the block from EventID on).  Visit
// 0x3858780 writes every one back by name: EoCFS::strSource -> 0x24, strType(SourceType)
// -> `mov byte [rbx+0x28], al` 0x38587fa, strTarget -> 0x2c, strType(TargetType) ->
// 0x3858868, strEventID -> 0x34, strVisibility -> `mov byte [rbx+0x38], al` 0x3858924,
// then strAiUseInCombat 0x39, strBlockMapMarkerNavigation 0x3a, strTeleportSnapToGround
// 0x3b.  sizeof does not discriminate here (MSVC is 0x40 too) -- the offsets do.
static_assert(offsetof(TeleportActionData, Source) == 0x24);
static_assert(offsetof(TeleportActionData, SourceType) == 0x28);
static_assert(offsetof(TeleportActionData, Target) == 0x2c);
static_assert(offsetof(TeleportActionData, TargetType) == 0x30);
static_assert(offsetof(TeleportActionData, EventID) == 0x34);
static_assert(offsetof(TeleportActionData, Visibility) == 0x38);
static_assert(offsetof(TeleportActionData, AiUseInCombat) == 0x39);
static_assert(offsetof(TeleportActionData, BlockMapMarkerNavigation) == 0x3a);
static_assert(offsetof(TeleportActionData, SnapToGround) == 0x3b);
static_assert(sizeof(TeleportActionData) == 0x40);

// id 4, allocator 0x2fe4420: `mov edi, 0x30`, `mov word [rax+0x20], 0xff04` -- so
// SurfaceType's default is 0xff at 0x21, a non-zero value store -- then
// `mov qword [rax+0x24], 0` and `mov byte [rax+0x2c], 1`.  Visit 0x3858170 passes
// `lea rdx, [rbx+0x21]` with EoCFS::strSurfaceType to
// eoc::VisitSurface(…, eoc::ESurfaceType&, …) 0x2b78d00, which types 0x21 as well as
// placing it, then strLifeTime 0x24, strRadius 0x28,
// strExternalCauseAsSurfaceOwner 0x2c.
static_assert(offsetof(CreateSurfaceActionData, SurfaceType) == 0x21);
static_assert(offsetof(CreateSurfaceActionData, LifeTime) == 0x24);
static_assert(offsetof(CreateSurfaceActionData, Radius) == 0x28);
static_assert(offsetof(CreateSurfaceActionData, ExternalCauseAsSurfaceOwner) == 0x2c);
static_assert(sizeof(CreateSurfaceActionData) == 0x30);

// id 5, allocator 0x2fe4360: `mov edi, 0x40`, `mov byte [rax+0x20], 5`,
// `mov word [rax+0x21], 0x100` (the 0x22 lane is 1, which fixes the pair's boundary at
// 0x21), the xmm block at 0x24 and `mov qword [rax+0x34], -1`.  Visit 0x3858500 names all
// eight; the macOS oracle has no DestroyParametersActionData::Visit symbol, so this one
// is Linux-only and was identified by the allocator's id 5.
static_assert(offsetof(DestroyParametersActionData, TargetItemState) == 0x21);
static_assert(offsetof(DestroyParametersActionData, SnapToGround) == 0x22);
static_assert(offsetof(DestroyParametersActionData, ExplodeFX) == 0x24);
static_assert(offsetof(DestroyParametersActionData, VisualDestruction) == 0x28);
static_assert(offsetof(DestroyParametersActionData, VisualWithDynamicPhysics) == 0x2c);
static_assert(offsetof(DestroyParametersActionData, FadeOutDelay) == 0x30);
static_assert(offsetof(DestroyParametersActionData, FadeOutFX) == 0x34);
static_assert(offsetof(DestroyParametersActionData, TemplateAfterDestruction) == 0x38);
static_assert(sizeof(DestroyParametersActionData) == 0x40);

// id 7, allocator 0x3632ea0 (sizeof 0x30, `mov qword [rax+0x24], rcx`,
// `mov word [rax+0x2c], 1`).  Visit 0x3858240: strStatsId 0x24, strStatusDuration 0x28
// (int32 slot), strConsume 0x2c, strIsHiddenStatus 0x2d.
static_assert(offsetof(ConsumeActionData, StatsId) == 0x24);
static_assert(offsetof(ConsumeActionData, StatusDuration) == 0x28);
static_assert(offsetof(ConsumeActionData, Consume) == 0x2c);
static_assert(offsetof(ConsumeActionData, IsHiddenStatus) == 0x2d);
static_assert(sizeof(ConsumeActionData) == 0x30);

// id 9, allocator 0x3c196f0 (sizeof 0x28, `mov byte [rax+0x20], 9` then
// `mov word [rax+0x21], 0` -- two bytes, not one).  Visit 0x3858730 hands
// EoCFS::strSecretDoor with `lea rdx, [r14+0x21]` and strScriptControlledDoor with
// `add r14, 0x22`, both through the bool slot: see the member comment above.
static_assert(offsetof(DoorActionData, SecretDoor) == 0x21);
static_assert(offsetof(DoorActionData, ScriptControlledDoor) == 0x22);
static_assert(sizeof(DoorActionData) == 0x28);

// id 10, allocator 0x2fe43e0: `mov edi, 0x40`, `mov word [rax+0x20], 0xff0a` (0xff at
// 0x21 again), the xmmword at 0x24, `mov dword [rax+0x34], 0`,
// `mov word [rax+0x38], 0x101`.  Visit 0x5e4efd0: SurfaceType 0x21 through
// eoc::VisitSurface, strLifeTime 0x24, strTimeout 0x28, strTotalCells 0x2c,
// strCellsAtGrow 0x30, strGrowTimer 0x34, strExternalCauseAsSurfaceOwner 0x38,
// strApplyDeathTypeBloodCheck 0x39.
static_assert(offsetof(CreatePuddleActionData, SurfaceType) == 0x21);
static_assert(offsetof(CreatePuddleActionData, LifeTime) == 0x24);
static_assert(offsetof(CreatePuddleActionData, Timeout) == 0x28);
static_assert(offsetof(CreatePuddleActionData, TotalCells) == 0x2c);
static_assert(offsetof(CreatePuddleActionData, CellsAtGrow) == 0x30);
static_assert(offsetof(CreatePuddleActionData, GrowTimer) == 0x34);
static_assert(offsetof(CreatePuddleActionData, ExternalCauseAsSurfaceOwner) == 0x38);
static_assert(offsetof(CreatePuddleActionData, ApplyDeathTypeBloodCheck) == 0x39);
static_assert(sizeof(CreatePuddleActionData) == 0x40);

// id 11, allocator 0x30b2640 (sizeof 0x28, `mov dword [rax+0x24], ecx` -- the acquired
// FixedString index).  Visit hands EoCFS::strBookId at 0x24 (macOS 0x10138ced0; the two
// Linux runs with this one-FixedString shape are Book's and Recipe's, each pinned by its
// own allocator id).
static_assert(offsetof(BookActionData, BookId) == 0x24);
static_assert(sizeof(BookActionData) == 0x28);

// id 12, allocator 0x3782f50 (sizeof 0x40, `mov dword [rax+0x24], ecx`,
// `mov byte [rax+0x28], 1`, `movups [rax+0x30], xmm1`).  Visit 0x38580b0: EoCFS::strSkillID
// 0x24, strConsume 0x28, strClassId through the Guid slot [rax+0x1b0] at 0x30.  Class is
// 8-aligned, so it stays at 0x30 under either ABI -- the family's negative control.
static_assert(offsetof(UseSpellActionData, Spell) == 0x24);
static_assert(offsetof(UseSpellActionData, Consume) == 0x28);
static_assert(offsetof(UseSpellActionData, Class) == 0x30);
static_assert(sizeof(UseSpellActionData) == 0x40);

// id 13, allocator 0x7154350 (sizeof 0x30, `mov dword [rax+0x24], ecx`,
// `mov byte [rax+0x28], 1`); id 33, allocator 0x380b510, byte for byte the same shape.
// Their Visits are the two Linux runs at 0x3858010 and 0x5e4f5d0 (EoCFS::strSkillID /
// strSpellId at 0x24, strConsume at 0x28; macOS 0x10138ffa0 and 0x101393f70).  sizeof
// does not discriminate for either -- MSVC is 0x30 too -- so the two offsets carry it.
static_assert(offsetof(SpellBookActionData, Spell) == 0x24);
static_assert(offsetof(SpellBookActionData, Consume) == 0x28);
static_assert(sizeof(SpellBookActionData) == 0x30);
static_assert(offsetof(LearnSpellActionData, Spell) == 0x24);
static_assert(offsetof(LearnSpellActionData, Consume) == 0x28);
static_assert(sizeof(LearnSpellActionData) == 0x30);

// ids 14, 15, 28, 32: one float at 0x24, each with its own allocator storing the default
// there -- Sit 0x3c3f270, Lie 0x38b2860, Constrain 0x3fb81a0, Throw 0x3b3ab00, all
// `mov edi, 0x28` and `mov dword [rax+0x24], 0`.  The four Linux Visit runs with this
// shape are 0x3857fd0, 0x3858200, 0x3858310 and 0x38589d0; the macOS oracle names the
// keys EoCFS::strHeal (Sit and Lying), strDamage (Constrain) and
// strFallbackPreviewRadius (Throw).
static_assert(offsetof(SitActionData, Heal) == 0x24);
static_assert(sizeof(SitActionData) == 0x28);
static_assert(offsetof(LieActionData, Heal) == 0x24);
static_assert(sizeof(LieActionData) == 0x28);
static_assert(offsetof(ConstrainActionData, Damage) == 0x24);
static_assert(sizeof(ConstrainActionData) == 0x28);
static_assert(offsetof(ThrowActionData, FallbackPreviewRadius) == 0x24);
static_assert(sizeof(ThrowActionData) == 0x28);

// id 16, allocator 0x3e59750: `mov word [rax+0x20], 0x110` -- id 0x10 at 0x20 and the
// default 1 at 0x21, a non-zero value store.  InsertActionData::Visit visits nothing at
// all (Linux run 0x3857f30 or 0x5e4f860, macOS 0x101393ab0 is a bare tail call), so the
// allocator is the only witness this record has.
static_assert(offsetof(InsertActionData, InsertSlots) == 0x21);
static_assert(sizeof(InsertActionData) == 0x28);

// ids 18 and 20: one bool at 0x21.  Lockpick's allocator 0x406a020 writes
// `mov word [rax+0x20], 0x12` (0 at 0x21, a zero lane inside a value-bearing two-byte
// store) and DisarmTrap's 0x45273d0 writes `mov word [rax+0x20], 0x114` (1 at 0x21, a
// value).  Both Visits hand `[this+0x21]` to the bool slot with EoCFS::strConsume
// (Linux 0x3857d10 and 0x3858a10; macOS 0x101391110 and 0x10138ca10).
static_assert(offsetof(LockpickActionData, Consume) == 0x21);
static_assert(sizeof(LockpickActionData) == 0x28);
static_assert(offsetof(DisarmTrapActionData, Consume) == 0x21);
static_assert(sizeof(DisarmTrapActionData) == 0x28);

// id 22, allocator 0x71543c0 (sizeof 0x38, xmmword at 0x24, `mov dword [rax+0x34], 0`).
// Visit 0x5e4f9c0: EoCFS::strType through the int32-via-temp slot [rax+0xb8] writing back
// to 0x24, then strUIStoryInstance through the STDString slot [rax+0x1a0] at 0x28.  The
// STDString is 8-aligned, so 0x28 under either ABI: the second negative control.
static_assert(offsetof(ShowStoryElementUIActionData, UIType) == 0x24);
static_assert(offsetof(ShowStoryElementUIActionData, UIStoryInstance) == 0x28);
static_assert(sizeof(ShowStoryElementUIActionData) == 0x38);

// id 23, allocator 0x3b35a20 (sizeof 0x28, `mov byte [rax+0x20], 0x17` then
// `mov word [rax+0x21], 0x101` -- both lanes non-zero).  Visit 0x3857f70:
// (anonymous namespace)::strCombineSlots through the int8 slot [rax+0xb0] at 0x21 and
// strCombineBase through the bool slot at 0x22.
static_assert(offsetof(CombineActionData, CombineSlots) == 0x21);
static_assert(offsetof(CombineActionData, IsBase) == 0x22);
static_assert(sizeof(CombineActionData) == 0x28);

// id 24, allocator 0x3b359d0: `mov edi, 0x50`, xmmword at 0x24, xmmword at 0x34,
// `mov dword [rax+0x44], 2` (non-zero) and `mov byte [rax+0x48], 0`.  Visit 0x3857d50
// hands the visitor ten members in this order: 0x24, 0x28, 0x2c, 0x30, 0x34, 0x3c, 0x40,
// ClimbDirection through the int32 slot, 0x38, 0x48 -- the macOS names being
// strBotomHorizontalOffset, strBotomVerticalOffset (Larian's spelling), strTopMidOffset,
// strTopDetachOffset, strNodeLadderOffest, strTopMidToPlatformFixedLength,
// strTopLineTolerance, strClimbDirection, strTopAttachNearOffset, strAllowScaling.
// MSVC would also land on 0x50, so the offsets carry this one, not the size.
static_assert(offsetof(LadderActionData, BottomHorizontalOffset) == 0x24);
static_assert(offsetof(LadderActionData, BottomVerticalOffset) == 0x28);
static_assert(offsetof(LadderActionData, TopMidOffset) == 0x2c);
static_assert(offsetof(LadderActionData, TopDetachOffset) == 0x30);
static_assert(offsetof(LadderActionData, NodeLadderOffest) == 0x34);
static_assert(offsetof(LadderActionData, TopAttachNearOffset) == 0x38);
static_assert(offsetof(LadderActionData, TopMidToPlatformFixedLength) == 0x3c);
static_assert(offsetof(LadderActionData, TopLineTolerance) == 0x40);
static_assert(offsetof(LadderActionData, ClimbDirection) == 0x44);
static_assert(offsetof(LadderActionData, AllowScaling) == 0x48);
static_assert(sizeof(LadderActionData) == 0x50);

// id 26, allocator 0x2fe4320 (sizeof 0x30, `mov dword [rax+0x24], ecx`,
// `mov byte [rax+0x28], 0`).  Visit 0x38583d0: EoCFS::strActivateSoundEvent 0x24,
// strPlayOnHUD 0x28.  MSVC would be 0x30 as well, so the offsets carry it.
static_assert(offsetof(PlaySoundActionData, ActivateSoundEvent) == 0x24);
static_assert(offsetof(PlaySoundActionData, PlayOnHUD) == 0x28);
static_assert(sizeof(PlaySoundActionData) == 0x30);

// id 27, allocator 0x4035030 (sizeof 0x30, `mov qword [rax+0x24], -1` covering the first
// two FixedStrings, `mov dword [rax+0x2c], ecx`).  Visit 0x3858a50: EoCFS::strRootTemplate
// 0x24, strLocalTemplate 0x28, strSpawnFX 0x2c.
static_assert(offsetof(SpawnCharacterActionData, RootTemplate) == 0x24);
static_assert(offsetof(SpawnCharacterActionData, LocalTemplate) == 0x28);
static_assert(offsetof(SpawnCharacterActionData, SpawnFX) == 0x2c);
static_assert(sizeof(SpawnCharacterActionData) == 0x30);

// id 30, allocator 0x3bdad90 (sizeof 0x28, `mov dword [rax+0x24], ecx`).  Its Visit hands
// EoCFS::strRecipeID at 0x24 (macOS 0x101392720).
static_assert(offsetof(RecipeActionData, Recipe) == 0x24);
static_assert(sizeof(RecipeActionData) == 0x28);

END_SE()

BEGIN_NS(lua)

LUA_POLYMORPHIC(IActionData)

END_NS()
