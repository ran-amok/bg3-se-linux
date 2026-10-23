#pragma once

#include <GameDefinitions/TranslatedString.h>

BEGIN_SE()

using PlayerId = int16_t;
using InputPlayerIndex = uint32_t;

// PORT (linux): upstream's last two members are both wrong on this build, and both were
// reachable from Lua -- `Ticks` read a second float and `Unknown` read an int32 plus the
// struct's tail padding as a double, which is why Ext.Events.Tick reported
// `Ticks=1023969327 Unknown=-nan` in game.  Measured against bin/bg3 (c8da62b1), from the
// two functions that *build* one:
//
//   ServerWorker::DoWork 0x2afbe70 keeps its GameTime at this+0xf8 and hands it to the
//   fan-out 0x2afc720 with `lea r13, [rbx+0xf8]` (0x2afc41f) -- that fan-out is the caller of
//   the hooked esv::GameStateMachine::Update 0x2c66a10, so this is the very object the
//   extender's PreUpdate receives.  Per frame it computes
//   `elapsed = (double)(t.sec - prev.sec) + (double)(t.usec - prev.usec) * 1e-6`, narrows it
//   to float, clamps it with `minss` against 0.2f (0x1b75a88) and then:
//     0x2afc4d3  movss [rbx+0x104], xmm0        <- +0x0c, the clamped real delta
//     0x2afc4e2  mulss xmm0, [Global + 0x40]       (Global = 0x7d9d198, the time scale)
//     0x2afc4e7  movss [rbx+0x100], xmm0        <- +0x08, the scaled delta
//     0x2afc4fb  movsd [rbx+0xf8], xmm0         <- +0x00, Time += DeltaTime
//     0x2afc503  inc dword ptr [rbx+0x108]      <- +0x10, one increment per frame
//   gui::GameUI::UpdateFromUI(ls::GameTime const&) 0x5f10560 builds its own at this+0x3180
//   with the same four stores in the same order (0x5f1059b, 0x5f105af, 0x5f10584, 0x5f105b7)
//   -- an independent second site, and a matched function.
//
// So +0x0c is a float and +0x10 is an int32, not the other way round.  The scale at Global+0x40
// is 1.0 in normal play, which is why the in-game reading found +0x08 and +0x0c bit-identical.
// Two more witnesses for the width of +0x10: the consumer 0x2afc720 copies the object as
// `movups xmm0, [rbx]` plus `mov ecx, dword ptr [rbx+0x10]` (0x2afc769/0x2afc760), a 16-byte
// block and a dword and nothing else; and sizeof is 0x18 because the macOS oracle's
// ls::GameTime::DONT_CARE 0x109788808 is followed by ls::GlobalAllocator::m_ptr at
// 0x109788820 (the Linux twin is 0x7d9ce18, and 0x365d0c9 passes it to the same Update).
// `UnscaledDeltaTime` is named for what the store sequence computes; the game has no symbol
// for the member.
struct GameTime
{
    double Time{ 0 };
    float DeltaTime{ 0 };
    float UnscaledDeltaTime{ 0 };
    int32_t Ticks{ 0 };
};

#if defined(BG3SE_LINUX)
static_assert(sizeof(GameTime) == 0x18);
static_assert(offsetof(GameTime, Time) == 0x00);
static_assert(offsetof(GameTime, DeltaTime) == 0x08);
static_assert(offsetof(GameTime, UnscaledDeltaTime) == 0x0c);
static_assert(offsetof(GameTime, Ticks) == 0x10);
#endif

struct ActionOriginator
{
    Guid ActionGuid;
    FixedString PassiveId;
    FixedString StatusId;
    FixedString InterruptId;
    bool CanApplyConcentration{ true };
};

struct SpellMetaId
{
    FixedString OriginatorPrototype;
    uint8_t _Pad[4];
    SpellSourceType SourceType{ SpellSourceType::Sentinel };
    Guid Source;
    Guid ProgressionSource;
};

struct SpellId : public SpellMetaId
{
    FixedString Prototype;

    inline bool operator == (SpellId const& o) const
    {
        return
            OriginatorPrototype == o.OriginatorPrototype
            && SourceType == o.SourceType
            && Source == o.Source
            && ProgressionSource == o.ProgressionSource
            && Prototype == o.Prototype;
    }
};

template <>
inline uint64_t HashMapHash<SpellId>(SpellId const& v)
{
    return HashMulti(v.ProgressionSource, v.SourceType, v.Prototype, v.OriginatorPrototype);
}

struct SpellIdWithPrototype : public SpellId
{
    stats::SpellPrototype* SpellProto{ nullptr };
    std::optional<Guid> SpellCastSource;
};

struct DamagePair
{
    int32_t Amount{ 0 };
    DamageType DamageType{ DamageType::None };
};

struct RollDefinition
{
    DiceSizeId DiceValue{ DiceSizeId::D6 };
    uint8_t AmountOfDices{ 0 };
    int DiceAdditionalValue{ 0 };
    [[bg3::legacy(field_8)]] bool DiceNegative{ false };
};

struct BoostParameters
{
    Guid Owner;
    uint8_t field_10{ 0 };
};

struct BoostSource
{
    BoostSourceType Type{ BoostSourceType::Undefined };
    FixedString Cause;
    EntityHandle Entity;
    ComponentHandle Status;
};

struct BoostDescription
{
    FixedString Boost;
    FixedString Params;
    FixedString Params2;
};

struct ResourceRollDefinition
{
    [[bg3::legacy(field_0)]] Guid Resource;
    [[bg3::legacy(field_10)]] uint8_t Amount{ 0 };
};

struct PathSettings
{
    uint8_t PathClimbingMode{ 0 };
    uint8_t PathDroppingMode{ 0 };
    std::variant<uint8_t, float> Speed;
    bool HasSmoothingNodes{ false };
    bool HasSpliningNodes{ false };
    bool HasTurningNodes{ false };
};

struct TextLine : TranslatedString
{
    uint8_t field_10{ 1 };
};

struct TemplateInfo
{
    FixedString TemplateId;
    TemplateType TemplateType{ TemplateType::RootTemplate };
};

END_SE()

BEGIN_NS(path)

struct Bezier3Trajectory
{
    float DistanceMin;
    float DistanceMax;
    std::array<float, 2> OffsetMin;
    std::array<float, 2> OffsetMax;
    float ShiftMin{ .5f };
    float ShiftMax{ .5f };
};

struct Bezier4Trajectory
{
    float DistanceMin;
    float DistanceMax;
    std::array<float, 2> OffsetAMin;
    std::array<float, 2> OffsetAMax;
    std::array<float, 2> OffsetBMin;
    std::array<float, 2> OffsetBMax;
    float ShiftAMin{ .5f };
    float ShiftAMax{ .5f };
    float ShiftBMin{ .5f };
    float ShiftBMax{ .5f };
};

struct ConstantVelocity
{
    float ConstantVelocity;
};

struct LinearVelocity
{
    float Acceleration;
    float InitialSpeed;
};

struct MappedVelocity
{
    FixedString Mapping;
};

struct Settings
{
    std::variant<Bezier3Trajectory, Bezier4Trajectory> Trajectory{ Bezier3Trajectory{} };
    [[bg3::hidden]] uint32_t _Pad;
    PathRotateMode RotateMode{ PathRotateMode::Follow };
    std::variant<ConstantVelocity, LinearVelocity, MappedVelocity> Velocity{ ConstantVelocity{} };
};

struct PathMover : public Settings
{
    glm::vec3 SourcePosition;
    glm::quat SourceRotation;
    glm::vec3 TargetPosition;
    glm::quat TargetRotation;
    float InterpolateValue{ .0f };
    std::array<float, 32> ComputedTrajectoryValues;
    Array<glm::vec3> ComputedVelocityValues;
    bool Initialized{ false };
};

END_NS()

BEGIN_NS(navigation)

struct TargetInfo
{
    glm::vec3 Position;
    float CloseEnoughMin;
    float CloseEnoughMax;
    float CloseEnoughFloor;
    float CloseEnoughCeiling;
    int CloseEnoughPreference;
    EntityHandle Target;
    Array<EntityHandle> IgnoreEntities;
    uint32_t AiTargetCheck;
    uint16_t CoverFlags;
    bool AddSourceBoundsToMargin;
    bool field_3F;
    bool SurfaceAvoidance;
    bool IsAvoidingObstacles;
    bool IsPreciseItemInteraction;
    EntityHandle PickupEntity;
    std::optional<glm::vec3> PickupPosition;
    uint8_t WeightFuncType;
};

END_NS()

BEGIN_NS(aio)

struct Priority
{
    int field_0{ 1 };
    float field_4{ .0f };
};

END_NS()
