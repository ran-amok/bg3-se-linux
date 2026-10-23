#pragma once

#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/EntitySystem.h>
#include <GameDefinitions/Misc.h>
#include <GameDefinitions/ObjectFactory.h>

BEGIN_SE()

struct SurfaceMetaData
{
    Guid TeamId;
    ComponentHandle SurfaceHandle;
    EntityHandle Owner;
    float LifeTime;
    uint8_t Level;
};

END_SE()

BEGIN_NS(esv)

struct SubSurface : ProtectedGameObject<SubSurface>
{
    Surface* Surface;
    uint64_t SurfaceStateFlags;
    Array<AiTilePos> Cells;
    Array<AiTilePos> GrowCells;
    [[bg3::readonly]] uint16_t SurfaceIndex;
    AiTilePos Pos;
};

struct Surface : ProtectedGameObject<Surface>
{
    ComponentHandle SurfaceHandle;
    EntityHandle SurfaceEntity;
    SurfaceType Type;
    uint8_t Flags;
    SurfaceMetaData Meta;
    bool HasLifeTime;
    bool IsControlledByConcentration;
    Array<AiTilePos> NeighbourMergeCells;
    std::array<ComponentHandle, 14> TransformActions;
    [[bg3::readonly]] int16_t Index;
    [[bg3::readonly]] int16_t SurfaceConcentrationTarget;
    [[bg3::hidden]] SurfaceManager* SurfaceManager;
    bool NeedsSplitEvaluation;
    float OwnershipTimer;
    int StoryActionID;
    ActionOriginator Originator;
    AbilityId SpellCastingAbility;
    Guid SpellCastSourceUuid;
    Guid Parent;
    Array<SubSurface*> SubSurfaces;
};

struct SurfaceAction : ProtectedGameObject<SurfaceAction>
{
    virtual ~SurfaceAction() = 0;
    virtual SurfaceActionType GetTypeId() = 0;
    virtual bool Visit(ObjectVisitor* visitor) = 0;
    virtual void Unknown4() = 0;
    virtual SurfaceType GetSurfaceType() = 0;
    virtual EntityHandle GetOwner() = 0;
    virtual glm::vec3 const& GetPosition() = 0;
    virtual Guid* GetTeam(Guid& teamId, bool) = 0;
    virtual Guid* GetCombat(Guid& combatId, bool) = 0;
    virtual std::optional<uint16_t> GetSurfaceConcentrationTarget() = 0;
    virtual void BreakConcentration() = 0;
    virtual void Enter() = 0;
    virtual void Update(GameTime const& time) = 0;
    virtual void Exit() = 0;
    virtual bool IsFinished() = 0;
    virtual void SetPosition(glm::vec3 const&) = 0;
    virtual void Unknown16() = 0;
    virtual void ApplySurfaceChanges() = 0;

    [[bg3::hidden]] Level* Level;
    int StoryActionID;
    ActionOriginator Originator;
    AbilityId SpellCastingAbility;
    Guid SpellCastUuid;
    [[bg3::hidden]] resource::GuidResourceBankBase* ClassDescriptionMgr;
    EntityHandle SurfaceEntity;
    Array<AiTilePos> BaseCells;
    ComponentHandle Handle;
    uint8_t field_78;
};

struct CreateSurfaceActionBase : public SurfaceAction
{
    EntityHandle Owner;
    float Duration;
    glm::vec3 Position;
    SurfaceType SurfaceType;
    SpellId Spell;
    uint16_t SurfaceConcentrationTarget;
    bool ConcentrationBroke;
    std::array<ComponentHandle, (unsigned)SurfaceType::Sentinel> SurfaceHandlesByType;
    std::array<Array<AiTilePos>*, (unsigned)SurfaceType::Sentinel> SurfaceChanges;
    std::array<Array<AiTilePos>*, 2> SurfaceCellsByLayer;
};

struct CreateSurfaceAction : public CreateSurfaceActionBase
{
    static constexpr SurfaceActionType Type = SurfaceActionType::CreateSurface;

    bool InitialChangesPushed;
    float Radius;
    float ExcludeRadius;
    float MaxHeight;
    bool IgnoreIrreplacableSurfaces;
    bool CheckExistingSurfaces;
    uint64_t LineCheckBlock;
    float Timer;
    float GrowTimer;
    int GrowStep;
    int CurrentCellCount;
    Array<AiTilePos> SurfaceCells;
    SurfaceLayer8 SurfaceLayer;
};

struct CreatePuddleAction : public CreateSurfaceActionBase
{
    static constexpr SurfaceActionType Type = SurfaceActionType::CreatePuddle;

    int SurfaceCells;
    int Step;
    float GrowSpeed;
    int field_454;
    bool IsFinished;
    bool IgnoreIrreplacableSurfaces;
    Array<AiTilePos> CellAtGrow;
    Array<AiTilePos> ClosedCells;
    float GrowTimer;
};

struct ForceCreateSurfaceAction : public CreateSurfaceActionBase
{
    static constexpr SurfaceActionType Type = SurfaceActionType::ForceCreateSurface;
};

struct CapsuleSurfaceAction : public CreateSurfaceActionBase
{
    static constexpr SurfaceActionType Type = SurfaceActionType::CapsuleSurface;

    glm::vec3 AnimationCenter;
    glm::vec3 End;
    float Radius;
    float ElapsedTime;
    float AnimationDuration;
    HashSet<AiTilePos> Cells;
};

struct RemoveSurfaceAction : public CreateSurfaceActionBase
{
    static constexpr SurfaceActionType Type = SurfaceActionType::RemoveSurface;

    bg3se::SurfaceType RemoveSurfaceType;
    glm::vec3 RemovePosition;
    float Radius;
    float Percentage;
    float GrowTimer;
    float Step;
    int TotalCellCount;
    float CurrentGrowTimer;
    Array<AiTilePos> Cells;
    Array<AiTilePos> GrowCells;
};


struct ZoneActionParams
{
    // PORT (linux): upstream models this as 28 bytes (int Shape + five floats + a flag byte);
    // the game's is 52, and every member of esv::ZoneAction past it was 24 bytes early as a
    // result -- which is what the property validator refused as `Targets`.  Measured off
    // the savegame visitor, which names each member:
    // esv::ZoneAction::SavegameVisit is bin/bg3 0x6f9dd10 (vtable 0x7a7cb98 slot 3) and it
    // calls esv::ZoneActionParams::Visit 0x5e4a1f0 on this+0x5e8; the macOS oracle inlines the
    // same code into esv::ZoneAction::SavegameVisit 0x105bc78b0, where the EoCFS globals still
    // carry their symbols.  Offsets below are that function's, relative to the params base.
    //   +0x00 byte  strShape          (enum visitor, vtable slot 0xb8, a byte temp)
    //   +0x04 float strRangeMinimum   +0x08 strRangeWanted   +0x0c strRangeMaximum
    //   +0x10 float strRangeCeiling   +0x14 strRangeFloor    +0x18 strZoneParam
    //   +0x1c float strFrontOffset    +0x20 strHeight
    //   +0x24 vec3  strSourcePosition (ls::details::SaveWorldPos)
    //   +0x30 byte  bitfield: bit 0 strIgnoreVerticalCap, bit 1 strMelee
    // Upstream kept five of the eight floats and gave two of them names the game does not use
    // (`Radius`, `MaxHeight`); the savegame names win.  Nothing could read this class before
    // the fix -- validation had disabled proxying of esv::ZoneAction outright -- so there is no
    // Lua-visible name to preserve.
    uint8_t Shape;
    float RangeMinimum;
    float RangeWanted;
    float RangeMaximum;
    float RangeCeiling;
    float RangeFloor;
    float ZoneParam;
    float FrontOffset;
    float Height;
    glm::vec3 SourcePosition;
    uint8_t Flags;
};

#if defined(BG3SE_LINUX)
static_assert(sizeof(ZoneActionParams) == 0x34);
static_assert(offsetof(ZoneActionParams, RangeMinimum) == 0x04);
static_assert(offsetof(ZoneActionParams, ZoneParam) == 0x18);
static_assert(offsetof(ZoneActionParams, Height) == 0x20);
static_assert(offsetof(ZoneActionParams, SourcePosition) == 0x24);
static_assert(offsetof(ZoneActionParams, Flags) == 0x30);
#endif

struct ZoneAction : public CreateSurfaceActionBase
{
    static constexpr SurfaceActionType Type = SurfaceActionType::Zone;

    SpellId Spell;
    FixedString TextKey;
    glm::vec3 Target;
    ZoneActionParams Params;
    float GrowTimer;
    int GrowStep;
    [[bg3::hidden]] void* SpellProperties;
    float CurrentGrowTimer;
    Array<AiTilePos> Cells;
    Array<EntityHandle> Targets;
    Array<HitDesc> Hits;
    int64_t CurrentCellCount;
    uint8_t Flags;
};

#if defined(BG3SE_LINUX)
// PORT (linux): esv::ZoneAction measured against bin/bg3 (c8da62b1).
// Nothing in this struct's own declaration changed -- the 24-byte shift that made
// `Validation of property 'Targets' failed on esv::ZoneAction` fire came entirely from
// ZoneActionParams above, and fixing that put every member back where the game has it.
// Witnesses:
//   * the constructor, bin/bg3 0x3ee72a0 (its factory 0x3ee7240 is `mov edi, 0x678;
//     call operator new` -- so sizeof is 0x678, not upstream's 0x660), storing
//     SpellId at 0x5a0 (`mov dword [rbx+0x5a0], -1`, `mov byte [rbx+0x5a8], 0x17`,
//     two `movups` at 0x5b0/0x5c0, `mov dword [rbx+0x5d0], -1`), TextKey 0x5d8,
//     Target 0x5dc, Params 0x5e8..0x61c, GrowTimer 1.0f at 0x61c and GrowStep 10 at 0x620
//     (one `movabs rcx, 0xa3f800000` for the pair), SpellProperties 0x628,
//     CurrentGrowTimer 0x630, then Cells at 0x638 with a 250-element buffer
//     (`ls::MemoryManager::Allocate(0x7d0, ...)`, capacity 0xfa at 0x640) and
//     `and byte [rbx+0x670], 0xe0` for Flags.  The macOS oracle's
//     esv::ZoneAction::ZoneAction 0x105bcc190 is the same store sequence at the same
//     offsets from 0x5a0 on -- no lock in the base, so the offsets transfer.
//   * SavegameVisit 0x6f9dd10 (vtable 0x7a7cb98 slot 3) visits, in order: the base's,
//     SpellId 0x5a0 (eoc::spell::LEGACY_Visit), TextKey 0x5d8, strTarget 0x5dc,
//     Params 0x5e8, strGrowTimer 0x61c, strGrowStep 0x620, strSurfaceCells 0x638,
//     strTargets 0x648, strCurrentCellCount 0x668, then the 0x670 bitfield.
static_assert(sizeof(ZoneAction) == 0x678);
static_assert(offsetof(ZoneAction, Spell) == 0x5A0);
static_assert(offsetof(ZoneAction, TextKey) == 0x5D8);
static_assert(offsetof(ZoneAction, Target) == 0x5DC);
static_assert(offsetof(ZoneAction, Params) == 0x5E8);
static_assert(offsetof(ZoneAction, GrowTimer) == 0x61C);
static_assert(offsetof(ZoneAction, GrowStep) == 0x620);
static_assert(offsetof(ZoneAction, SpellProperties) == 0x628);
static_assert(offsetof(ZoneAction, CurrentGrowTimer) == 0x630);
static_assert(offsetof(ZoneAction, Cells) == 0x638);
static_assert(offsetof(ZoneAction, Targets) == 0x648);
static_assert(offsetof(ZoneAction, Hits) == 0x658);
static_assert(offsetof(ZoneAction, CurrentCellCount) == 0x668);
static_assert(offsetof(ZoneAction, Flags) == 0x670);
#endif

struct TransformSurfaceAction : public SurfaceAction
{
    static constexpr SurfaceActionType Type = SurfaceActionType::TransformSurface;

    float Timer;
    SurfaceTransformActionType SurfaceTransformAction;
    SurfaceType OriginSurface;
    SurfaceLayer8 SurfaceLayer;
    float GrowCellPerSecond;
    bool Finished;
    [[bg3::hidden]] void* CellSearcher;
    EntityHandle OwnerHandle;
    glm::vec3 Position;
    float SurfaceLifetime;
    LegacyRefMap<SurfaceType, ComponentHandle> SurfaceMap;
    LegacyRefMap<SurfaceType, Array<AiTilePos>> SurfaceCellMap;
    Array<AiTilePos> SurfaceRemoveGroundCellMap;
    Array<AiTilePos> SurfaceRemoveCloudCellMap;
    bool PlayerCharacterNearby;
    Array<SurfaceType> SurfaceTypes;
    LegacyRefMap<SurfaceType, SurfaceType> SurfaceTypeMap;
};

#if defined(BG3SE_LINUX)
// PORT (linux): witnessed by esv::TransformSurfaceAction::Init (bg3+0x2682850),
// which is the only function that writes all three enums. Its first four instructions after the
// prologue are `mov byte [this+0x80], sil` (arg1, the transform action type), `mov byte [this+0x81], cl`
// (arg3, the origin surface type), `mov byte [this+0x82], dl` (arg2, the layer) and
// `mov byte [this+0x88], 0`; the grow rate is stored as `mov dword [this+0x84], <800.0|400.0|200.0>`
// at 0x26828c8 / 0x26828f8 / 0x268290e, and the previous cell searcher is destroyed through
// `mov rdi, [this+0x90]` / `call [vtable+8]` at 0x2682875.
//
// Nothing here is a lock-relative offset: they are absolute, because SurfaceAction holds no lock.
// The three enums land at 0x80 only because Itanium packs `float Timer` into the base's tail padding
// (SurfaceAction's last member is `uint8_t field_78`, so dsize is 0x79 and Timer goes at 0x7c) --
// which makes Timer's own offset a consequence of these three rather than a separate guess.
static_assert(offsetof(TransformSurfaceAction, SurfaceTransformAction) == 0x80);
static_assert(offsetof(TransformSurfaceAction, OriginSurface) == 0x81);
static_assert(offsetof(TransformSurfaceAction, SurfaceLayer) == 0x82);
static_assert(offsetof(TransformSurfaceAction, GrowCellPerSecond) == 0x84);
static_assert(offsetof(TransformSurfaceAction, Finished) == 0x88);
static_assert(offsetof(TransformSurfaceAction, CellSearcher) == 0x90);

// PORT (linux): the class's own constructor, bg3+0x4cd6c40, found through the creator 0x26827f0
// (`mov edi, 0x118` / operator new / this ctor) that the manifest row's caller census named as the
// neighbour of esv::SurfaceActionFactory::DoCreateAction.  It initialises the base and then the
// whole derived block, and it re-witnesses the six offsets above from a second function:
// `mov word [rdi+0x80], 0` zeroes the first two enums together, `mov byte [rdi+0x82], 2` is a
// non-zero value (the default SurfaceLayer), `mov dword [rdi+0x84], 0x3f800000` is 1.0f, and the
// byte at 0x88 and qword at 0x90 follow.  New here:
//   Timer 0x7c   `mov dword [rdi+0x7c], r14d` at 0x4cd6caa -- its own 4-byte store, and it cannot
//                have been widened downwards because 0x78 is the base's live field_78, written one
//                instruction earlier by `mov byte [rdi+0x78], r14b` at 0x4cd6ca3.  This is the
//                direct witness for the tail-padding member itself.
//   OwnerHandle 0x98  `mov qword [rdi+0x98], rax` with rax = 0xffc0000000000000, the null
//                EntityHandle sentinel -- a non-zero value store.
//   Position 0xa0     `movups [rbx+0xa0], xmm0`, a block bounded below by the OwnerHandle store.
//   SurfaceMap 0xb0   `add rdi, 0xb0` handed to the map's ctor 0x4cd6d70 -- an argument, not a fill.
//   SurfaceCellMap 0xc0  `lea rdi, [rbx+0xc0]` handed to 0x4cd6e40, likewise.
//   SurfaceRemoveGroundCellMap 0xd0  the fill at 0xd0..0xf0 starts here, bounded below by the
//                live sub-object at 0xc0; the split at 0xe0 is sizeof(Array), not a witness.
//   PlayerCharacterNearby 0xf0  `mov byte [rbx+0xf0], r14b` at 0x4cd6d1a, its own byte store
//                between the two 16-byte fills.
//   SurfaceTypeMap 0x108  `lea rdi, [rbx+0x108]` at 0x4cd6d34; with the map's 16 bytes that is
//                exactly the 0x118 the allocation asked for, so the size is a completeness witness.
// sizeof 0x118 is NOT an ABI witness (checked, per the stats::Functor caveat): CellSearcher is
// 8-aligned and lands at 0x90 under both ABIs, so everything from OwnerHandle on is unmoved and
// MSVC would reach 0x118 as well.  What discriminates is Timer 0x7c against MSVC's 0x80, and the
// five members between it and CellSearcher.
static_assert(offsetof(TransformSurfaceAction, Timer) == 0x7c);
static_assert(offsetof(TransformSurfaceAction, OwnerHandle) == 0x98);
static_assert(offsetof(TransformSurfaceAction, Position) == 0xa0);
static_assert(offsetof(TransformSurfaceAction, SurfaceMap) == 0xb0);
static_assert(offsetof(TransformSurfaceAction, SurfaceCellMap) == 0xc0);
static_assert(offsetof(TransformSurfaceAction, SurfaceRemoveGroundCellMap) == 0xd0);
static_assert(offsetof(TransformSurfaceAction, PlayerCharacterNearby) == 0xf0);
static_assert(offsetof(TransformSurfaceAction, SurfaceTypeMap) == 0x108);
static_assert(sizeof(TransformSurfaceAction) == 0x118);
#endif

struct ChangeSurfaceOnPathAction : public CreateSurfaceActionBase
{
    static constexpr SurfaceActionType Type = SurfaceActionType::ChangeSurfaceOnPath;

    EntityHandle FollowHandle;
    float Radius;
    bool IsFinished;
    Array<AiTilePos> Cells;
    bool IgnoreIrreplacableSurfaces;
    bool CheckExistingSurfaces;
    bool IgnoreOwnerCells;
    bool field_5C3;
};

struct RectangleSurfaceAction : public CreateSurfaceActionBase
{
    static constexpr SurfaceActionType Type = SurfaceActionType::RectangleSurface;

    // PORT (linux): three corrections to upstream's declaration, measured against bin/bg3
    // (c8da62b1).  None of them is an ABI difference: every member
    // here is a scalar or a 16-byte set, so MSVC lays them out the same way and upstream's
    // numbers are simply short.  They are what makes `Validation of property 'Cells' failed on
    // esv::RectangleSurfaceAction` fire on some launches and not others (see below).
    //
    // The class is named on the Linux binary alone.  Its creator 0x3ee7380 is
    // `mov edi, 0x638; call operator new; call 0x3ee73e0` -- so the constructor is 0x3ee73e0
    // and sizeof is 0x638 -- and the vtable it installs, 0x7a7c808, has `mov al, 7; ret` in
    // slot 2 (GetTypeId, SurfaceActionType::RectangleSurface = 7) and 0x6f99f10 in slot 3,
    // which match.py names esv::RectangleSurfaceAction::SavegameVisit (macOS 0x105813c00).
    // The macOS oracle only confirms it: its constructor 0x1058181e0 is the same store
    // sequence at the same offsets, and it keeps the ctor/dtor symbols LTO inlined away here.
    //
    // SavegameVisit 0x6f99f10 names the serialised members through the EoCFS FixedString
    // globals (the {FixedString* global, name, len} init table in .data.rel.ro):
    // Target 0x5a0, Width 0x5b8, Length 0x5bc, GrowTimer 0x5c0,
    // GrowStep 0x5c8, LineCheckBlock 0x5e8, MaxHeight 0x5c4, SurfaceCells 0x600,
    // Characters 0x610, Items 0x620, CurrentCellCount 0x630.  The constructor supplies the
    // non-zero defaults that fix the order inside each pair: `movsd [rbx+0x5c0]` from the
    // 8-byte constant at 0x1b73890 = {0.1f, FLT_MAX}, so GrowTimer 0.1 precedes MaxHeight
    // FLT_MAX; `mov dword [rbx+0x5c8], 0xa` is GrowStep 10; `mov qword [rbx+0x5e8], 0x800008`
    // is LineCheckBlock; and Cells gets an ls::MemoryManager::Allocate(0x7d0, 0x10) buffer at
    // 0x600 with capacity 0xfa at 0x608 (250 * sizeof(AiTilePos)).  The tail fill
    // (`movups [rbx+0x60c]`, `movups [rbx+0x61c]`, `mov qword [rbx+0x62c], 0`) stops at 0x634,
    // which with 8-alignment is the 0x638 the allocation asked for -- a completeness witness.
    //
    // 1. Upstream's `float SurfaceArea_M` at 0x5ac is the x of a three-float block.  macOS
    //    esv::RectangleSurfaceAction::Enter 0x105813f70 computes normalize(Target - Position)
    //    -- Target 0x5a0 minus the base's Position 0x8c, divided by its length, zeroed when
    //    that length is below epsilon -- and writes it with `movlps [rbx+0x5ac], xmm0` (x, y)
    //    and `movss [rbx+0x5b4], xmm6` (z).  The savegame visitor does not serialise it and no
    //    other function in the class reads it, so `Direction` is named for what Enter computes
    //    into it, not from a game string.  Width/Length at 0x5b8/0x5bc are read by
    //    SavegameVisit, so they cannot be at 0x5b0/0x5b4 as upstream has them.
    // 2. `SpellProperties` and `CurrentGrowTimer` at 0x5f0/0x5f8 are missing entirely --
    //    the same pair, in the same order, that esv::ZoneAction has at 0x628/0x630.  macOS
    //    Grow 0x105814da0 hands [this+0x5f0] to esv::functor::ExecuteStatsFunctors as its
    //    `eoc::StatsFunctorList const*`; macOS Update 0x105814d00 reads the float at 0x5f8,
    //    subtracts GameTime::DeltaTime, calls Grow(GrowStep@0x5c8) when it crosses zero and
    //    reloads it from GrowTimer@0x5c0 -- a countdown, and Enter seeds it the same way
    //    (`movss xmm0, [rdi+0x5c0]` / `movss [rdi+0x5f8], xmm0`).
    // 3. The three sets are 16 bytes in the game, not ObjectSet's 24: the macOS destructor
    //    0x105817d30 frees, in reverse declaration order, the buffer at 0x620, the buffer at
    //    0x610, the Array at 0x600 (capacity 0x608, size 0x60c) and the set at 0x5d0
    //    (capacity 0x5d8, size 0x5dc).  Characters and Items are 16 apart, and two 24-byte
    //    sets from 0x610 would end at 0x640, past the 0x638 allocation.  TrackedCompactSet is
    //    {Buf, Capacity, Size} with no CapacityIncrementSize, which is the shape the game has;
    //    the macOS visitor's own template argument is ls::ObjectSet<T, 0ul, Comparator>, the
    //    increment being a template parameter there rather than a member.
    //
    // Why upstream's layout is an intermittent validator refusal rather than a steady one: it
    // puts `Cells` at 0x5f0, so Array's three words read the game's
    // {SpellProperties, CurrentGrowTimer, <padding>}: buf_ = SpellProperties (0 on an action
    // freshly made by Ext.Level.CreateSurfaceAction), capacity_ = CurrentGrowTimer's four
    // bytes (the constructor writes 0 there), and size_ = the four bytes at 0x5fc, which
    // neither constructor ever writes -- the padding between CurrentGrowTimer and Cells.
    // ValidateLinearContainer takes its `buf == nullptr` branch and requires size == 0 and
    // capacity == 0; capacity is always 0, and size is whatever operator new(0x638)
    // last left at that address.  Zero on a quiet heap, garbage on a busy one.
    glm::vec3 Target;
    glm::vec3 Direction;
    float Width;
    float Length;
    float GrowTimer;
    float MaxHeight;
    int GrowStep;
    [[bg3::hidden]] TrackedCompactSet<void*> DamageList;
    stats::DeathType DeathType;
    uint64_t LineCheckBlock;
    [[bg3::hidden]] void* SpellProperties;
    float CurrentGrowTimer;
    Array<AiTilePos> Cells;
    TrackedCompactSet<EntityHandle> Characters;
    TrackedCompactSet<EntityHandle> Items;
    int CurrentCellCount;
};

#if defined(BG3SE_LINUX)
static_assert(sizeof(RectangleSurfaceAction) == 0x638);
static_assert(offsetof(RectangleSurfaceAction, Target) == 0x5a0);
static_assert(offsetof(RectangleSurfaceAction, Direction) == 0x5ac);
static_assert(offsetof(RectangleSurfaceAction, Width) == 0x5b8);
static_assert(offsetof(RectangleSurfaceAction, Length) == 0x5bc);
static_assert(offsetof(RectangleSurfaceAction, GrowTimer) == 0x5c0);
static_assert(offsetof(RectangleSurfaceAction, MaxHeight) == 0x5c4);
static_assert(offsetof(RectangleSurfaceAction, GrowStep) == 0x5c8);
static_assert(offsetof(RectangleSurfaceAction, DamageList) == 0x5d0);
static_assert(offsetof(RectangleSurfaceAction, DeathType) == 0x5e0);
static_assert(offsetof(RectangleSurfaceAction, LineCheckBlock) == 0x5e8);
static_assert(offsetof(RectangleSurfaceAction, SpellProperties) == 0x5f0);
static_assert(offsetof(RectangleSurfaceAction, CurrentGrowTimer) == 0x5f8);
static_assert(offsetof(RectangleSurfaceAction, Cells) == 0x600);
static_assert(offsetof(RectangleSurfaceAction, Characters) == 0x610);
static_assert(offsetof(RectangleSurfaceAction, Items) == 0x620);
static_assert(offsetof(RectangleSurfaceAction, CurrentCellCount) == 0x630);
#endif

struct PolygonSurfaceAction : public CreateSurfaceActionBase
{
    static constexpr SurfaceActionType Type = SurfaceActionType::PolygonSurface;

    FixedString field_448;
    Array<glm::vec3> PolygonVertices;
    int field_468;
    [[bg3::hidden]] Array<void*> DamageList;
    uint8_t field_488;
    __int64 field_490;
    float CurrentGrowTimer;
    float GrowTimer;
    int GrowStep;
    int LastSurfaceCellCount;
    Array<AiTilePos> SurfaceCells;
    Array<EntityHandle> Characters;
    Array<EntityHandle> Items;
    uint8_t field_500;
    bool PolygonCreated;
};

struct [[bg3::hidden]] SurfaceAiFlood : public AiFlood
{
    AiTilePos field_68;
    __int64 field_70;
    __int64 field_78;
    __int16 field_80;
    Array<AiTilePos> Tiles;
};

struct SurfaceCreatedEventRequest : ProtectedGameObject<SurfaceCreatedEventRequest>
{
    Surface* Surface;
    bool NewlyCreated;
};

struct SurfaceRemovedEventRequest
{
    EntityHandle field_0;
    EntityHandle field_8;
    Guid SpellCastUuid;
};

struct SurfacePosition
{
    ComponentHandle Surface;
    glm::vec3 Position;
};

struct SurfaceConcentrationTarget
{
    HashSet<ComponentHandle> Surfaces;
    uint16_t Index;
};

struct SurfaceTransformActionRequirement
{
    uint8_t Type; // SurfaceTransformRequirementType
    std::variant<SurfaceType, uint8_t /*SurfaceModifierType*/, int8_t /*SurfaceCategory*/> Requirement;
};

struct SurfaceTransformActionRequirements
{
    SurfaceTransformActionType TransformType;
    Array<SurfaceTransformActionRequirement> Requirements;
};


struct [[bg3::hidden]] SurfaceManager : public ObjectFactory<Surface>
{
    void* VMT2;
    __int64 field_68;
    std::array<Array<SurfaceTransformActionRequirements>, 16> TransformActionRequirements;
    Level* Level;
    Array<SurfaceAction*> Actions;
    LegacyArray<Surface*> Surfaces;
    LegacyArray<SurfaceConcentrationTarget*> SurfaceConcentrationTargets;
    Array<AiTilePos> RemoveCells;
    SurfaceAiFlood AiFlood;
    Array<Array<AiTilePos>> HandledCellTransitions;
    LegacyRefMap<FixedString, Array<AiTilePos>> Chasms;
    Array<SurfacePosition> CheckFTBTeamRequests;
    Array<SurfaceCreatedEventRequest> CreatedEvents;
    Array<SurfaceRemovedEventRequest> RemovedEvents;

    SurfaceAction* CreateAction(SurfaceActionType type);
    void AddAction(SurfaceAction* action);
};

END_NS()

BEGIN_NS(lua)

LUA_POLYMORPHIC(esv::SurfaceAction)

END_NS()
