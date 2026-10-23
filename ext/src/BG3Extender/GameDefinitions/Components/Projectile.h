#pragma once

#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/Misc.h>
#include <GameDefinitions/EntitySystem.h>
#include <GameDefinitions/Hit.h>
#include <GameDefinitions/Components/SpellCastShared.h>

BEGIN_NS(projectile)

struct ProjectileResult : public path::Settings
{
    [[bg3::legacy(field_50)]] FixedString TemplateId;
    EntityHandle field_58;
    glm::vec3 field_60;
    float InterpolateValue{ .0f };
};

struct SourceInfoComponent : public BaseComponent
{
    DEFINE_COMPONENT(ProjectileSource, "eoc::projectile::SourceInfoComponent")

    SpellId Spell;
    EntityHandle Entity;
};

END_NS()

BEGIN_NS(esv::projectile)

struct OnHitActionBase : public ProtectedGameObject<OnHitActionBase>
{
    [[bg3::hidden]] void* VMT;
    EntityHandle Caster;
    bool IsFromItem;
    bool IgnoreTargetChecks;
    SpellId Spell;
};

struct PierceOnHit : public OnHitActionBase
{
    int PierceCount;
    glm::vec3 Direction;
    Array<EntityHandle> HitTargets;
};

struct ForkOnHit : public OnHitActionBase
{
    int ForkCount;
    int ForkLevels;
    Array<EntityHandle> HitTargets;
};

struct AttachmentComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerProjectileAttachment, "esv::projectile::AttachmentComponent")

    EntityHandle Attachment;
};

struct SpellComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerProjectileSpell, "esv::projectile::SpellComponent")

    EntityHandle Spell;
    bg3se::spell_cast::IntermediateTarget Target;
    int Index;
};

struct InitializationData
{
    FixedString Projectile;
    glm::vec3 StartPosition;
    glm::vec3 TargetPosition;
    int field_1C;
    SpellId Spell;
    Guid SpellCastUuid;
    bool IsFromItem;
    bool IgnoreTargetChecks;
    EntityHandle Caster;
    EntityHandle Source;
    EntityHandle BeamSource;
    EntityHandle SourceWeapon;
    Guid HistoryUuid;
    bool MainDamageType;
    AbilityId SpellCastingAbility;
    bool IsTrap;
    [[bg3::legacy(field_93)]] bool MovesProjectileFromCaster;
    bool CanDeflect;
    bool IgnoreObjects;
    CauseType CauseType;
    int StoryActionId;
    ActionOriginator Originator;
    FixedString TextKey;
    HitDesc Hit;
    std::optional<glm::vec3> BeamTargetPos;
    EntityHandle TargetObject;
    float ExplodeRadius;
    bool NoMovement;
    OnHitActionBase* HitAction;
    EntityHandle ThrownObject;
    bool DamageMovingObjectOnLand;
    std::optional<bg3se::projectile::ProjectileResult> Path;
    std::optional<path::Settings> MovementSettings;
    EntityHandle MovingObject;
    SpellComponent SpellData;
};

struct InitializationComponent : public BaseProxyComponent
{
    DEFINE_COMPONENT(ServerProjectileInitialization, "esv::projectile::InitializationComponent")

// PORT (linux): upstream's `[[bg3::hidden]] void* field_0;` is not in the native build -- the
// component *is* the initialization data, with nothing in front of it. Witnessed twice over:
// the game's component registry says "'esv::projectile::InitializationComponent' OOB size
// mismatch: local 1104, ECS 1096" of upstream's layout, and the
// macOS oracle -- same Itanium ABI, and it keeps its symbols -- says where the 8 bytes are.
// The game calls this struct `esv::projectile::InitializationInfo`, and its constructor
// (macOS 0x1064d7ba0) opens with `dword [this+0x00] = 0xffffffff`, the null FixedString
// `Projectile` -- not a vtable (there is no `vtable for esv::projectile::InitializationInfo`
// in the binary at all) and not a pointer. Every store after it lands on this mirror's own
// `InitializationData` offsets: the four null EntityHandles at 0x70/0x78/0x80/0x88
// (Caster, Source, BeamSource, SourceWeapon), `eoc::HitDesc::HitDesc()` called on `this+0xd8`
// = 216 = `Hit`, the null handle at 0x290 = 656 = `TargetObject`, `SpellData`'s at 0x390 =
// 912. Its last write is `byte [this+0x444] = 0`, so InitializationInfo ends at 0x448 = 1096
// on its own -- which is the whole component, so `field_0` is not there.
    InitializationData Data;
};

#if defined(BG3SE_LINUX)
static_assert(sizeof(InitializationData) == 1096, "esv::projectile::InitializationInfo ends at 0x448 (the macOS ctor's last store, 0x1064d7ba0)");
static_assert(offsetof(InitializationComponent, Data) == 0, "the macOS ctor's first store is the FixedString at +0x00");
static_assert(sizeof(InitializationComponent) == 1096, "esv::projectile::InitializationComponent is 1096 bytes (the game's registry)");
#endif


struct Projectile : public BaseProxyComponent
{
    DEFINE_COMPONENT(ServerProjectile, "esv::Projectile")

    [[bg3::hidden]] void* VMT;
// PORT (linux): upstream's `uint32_t field_8; uint32_t field_C;` are not in the native build.
// The eight EntityHandles start straight after the vtable, at 0x08, and every offset in this
// mirror from `Entity` on is 8 lower than upstream's. Witnessed twice over: the game's
// component registry says "'esv::Projectile' OOB size mismatch: local 1472, ECS 1464" of
// upstream's layout, and the macOS oracle -- same Itanium ABI, and
// it keeps its symbols -- says where. `esv::Projectile::Projectile()` (macOS 0x1057c7ed0)
// stores `vtable for esv::Projectile` + 0x10 (0x10948fd78 + 0x10) at [this+0x00], then the
// null handle 0xffc0000000000000 at 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38 and 0x40 --
// eight handles, Entity..SourceWeapon -- then zeroes 0x48..0x6c (three vec3s) and the null
// FixedString at 0x6c, and calls `eoc::HitDesc::HitDesc()` on `this+0x70` = 112 =
// `SpellRollResult`. The -8 then holds to the end of the object, store for store: the two
// `path::PathMover`s at 0x228 and 0x350 (0x128 = 296 apart, which is `sizeof(PathMover)`),
// `SpellId` at 0x480, `MovingObjectTransform` at 0x4e0, `MoveFromSource`/`CanDeflect` set to 1
// at 0x51d/0x521, `Level` at 0x580, and a last write of `byte [this+0x5b0] = 0` = `ShouldFall`,
// so the object ends at 0x5b8 = 1464. `sizeof(eoc::HitDesc)` is 424 in both this mirror and
// the native build (536 - 112 here, 656 - 216 in InitializationInfo), so nothing else moved.
// Deleted rather than #if'd out because these two are not [[bg3::hidden]]:
// make_property_map.py ignores preprocessor lines and would emit members that no longer exist.
    EntityHandle Entity;
    EntityHandle Caster;
    EntityHandle Source;
    EntityHandle TargetObject;
    EntityHandle HitObject;
    EntityHandle ClientHitObject;
    EntityHandle BeamSource;
    EntityHandle SourceWeapon;
    glm::vec3 SourcePos;
    glm::vec3 TargetPos;
    glm::vec3 TargetObjectPos;
    FixedString TextKey;
    [[bg3::legacy(Hit)]] HitDesc SpellRollResult;
    DamageType MainDamageType;
    CauseType Cause{ CauseType::Attack };
    float LifeTime{ 10.0f };
    float HitInterpolation{ -1.0f };
    float FallbackTimer{ 15.0f };
    path::PathMover Path;
    path::PathMover Path2;
    float ExplodeRadius;
    [[bg3::legacy(ExplodeRadius2)]] float InfluenceRadius;
    SpellId Spell;
    stats::Functors* HitFunctors;
    AbilityId SpellCastingAbility;
    Guid SpellCastUuid;
    EntityHandle MovingObject;
    Transform MovingObjectTransform;
    FixedString SpawnEffect;
    bool SpawnFXOverridesImpactFX;
    EntityHandle BeamEffect;
    bool RequestDelete;
    bool Launched;
    bool IsOnHold;
    bool IsTrap;
    bool IsThrown;
    [[bg3::legacy(MoveFromOwner)]] bool MoveFromSource{ true };
    bool IsFromItem;
    bool IgnoreTargetChecks;
    bool IgnoreRoof;
    bool CanDeflect{ true };
    bool IgnoreObjects;
    bool IgnoreSurfaces;
    bool Used;
    bool Success;
    uint8_t field_526;
    bool DamageMovingObjectOnLand;
    [[bg3::legacy(NoMovement)]] bool ExplodeImmediate;
    bool Reacted;
    bool ItemActivated;
    bool IsChasmRecovery;
    OnHitActionBase* HitAction;
    int32_t StoryActionId;
    ActionOriginator Originator;
    uint64_t Flags;
    glm::vec3 SourcePosition;
    glm::vec3 field_574;
    FixedString Level;
    ProjectileTemplate* Template;
    [[bg3::hidden]] Array<void*> field_590;
    ecs::EntityRef Owner;
    bool ShouldFall;
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(Projectile, Entity) == 0x08, "the macOS ctor's first null handle, 0x1057c7ed0");
static_assert(offsetof(Projectile, SpellRollResult) == 0x70, "the macOS ctor calls eoc::HitDesc::HitDesc() on this+0x70");
static_assert(offsetof(Projectile, ShouldFall) == 0x5B0, "the macOS ctor's last store");
static_assert(sizeof(Projectile) == 1464, "esv::Projectile is 1464 bytes (the game's registry)");
#endif

struct ImpactEventOneFrameComponent : public BaseComponent
{
    DEFINE_ONEFRAME_COMPONENT(ProjectileImpactEvent, "esv::projectile::ImpactEventOneFrameComponent")

    bg3se::spell_cast::IntermediateTarget Target;
    EntityHandle field_A8;
    bool field_B0;
};

struct RequestTargetTrackingOneFrameComponent : public BaseComponent
{
    DEFINE_ONEFRAME_COMPONENT(ProjectileRequestTargetTracking, "esv::projectile::RequestTargetTrackingOneFrameComponent")

    Guid field_0;
    HashSet<EntityHandle> Targets;
};

struct UpdateTargetTrackingOneFrameComponent : public BaseComponent
{
    DEFINE_ONEFRAME_COMPONENT(ProjectileUpdateTargetTracking, "esv::projectile::UpdateTargetTrackingOneFrameComponent")

    Guid field_0;
    HashSet<EntityHandle> Targets;
};

DEFINE_ONEFRAME_TAG_COMPONENT(esv::projectile, SplitThrowableObjectRequestOneFrameComponent, ProjectileSplitThrowableObjectRequest)

END_NS()

BEGIN_NS(spell_cast)

struct ProjectileTargetData
{
    glm::vec3 field_0;
    IntermediateTarget Target;
    FixedString TextKey;
    int field_BC{ 0 };
    HitDesc Hit;
    std::optional<path::Settings> PathSettings;
    std::optional<projectile::ProjectileResult> Result;
    EntityHandle ThrownObject;
};

END_NS()

BEGIN_NS(esv::spell_cast)

struct ProjectileResultsExtraData
{
    glm::vec3 field_0;
    glm::vec3 field_C;
    Array<bg3se::projectile::ProjectileResult> Results;
};


struct ProjectileCacheComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerProjectileCache, "esv::spell_cast::ProjectileCacheComponent")

    std::optional<bg3se::spell_cast::ProjectileTargetData> Target;
    HashMap<int, float> ProjectileDelayTimers;
    [[bg3::hidden]] HashMap<int, void*> field_390; // ObjectQueue<ProjectileTargetData>
    Array<ProjectileResultsExtraData> ExtraData;
};

END_NS()
