#pragma once

#include <GameDefinitions/AllSparkShared.h>

BEGIN_SE()

struct Timeline;

END_SE()

BEGIN_NS(aspk)

struct Property : public ProtectedGameObject<Property>
{
    struct Parameter
    {
        STDString Name;
        float Value;
    };

    virtual ~Property() = 0;
    virtual void Clone(Property const& other) = 0;
    virtual bool Visit(ObjectVisitor* visitor) = 0;
    virtual PropertyType GetPropertyType() = 0;
    virtual void SetInputValueString(FixedString const&) = 0;
    virtual void SetInputValueVec4(glm::vec4 const&) = 0;
    virtual void SetInputValueVec3(glm::vec3 const&) = 0;
    virtual void SetInputValueVec2(glm::vec2 const&) = 0;
    virtual void SetInputValueFloat(float) = 0;

    FixedString FullName;
    FixedString AttributeName;
    LegacyArray<Parameter*> Parameters;
    FixedString Input;
};

struct Input : public ProtectedGameObject<Input>
{
    virtual ~Input() = 0;
    virtual PropertyType GetInputType() = 0;

    FixedString Name;
    Property* Input;
};

struct BooleanProperty : public Property
{
    bool Value;
};

struct IntegerProperty : public Property
{
    int32_t Value;
};

struct IntegerRangeProperty : public Property
{
    int32_t Min;
    int32_t Max;
};

struct ColorARGBKeyFrameData
{
    float Time;
    [[bg3::hidden]] __int32 field_4;
    [[bg3::hidden]] __int64 field_8;
    glm::fvec4 Color;
};

struct ColorARGBKeyFrameProperty : public Property
{
    LegacyArray<ColorARGBKeyFrameData>* Frames;
};

struct FloatProperty : public Property
{
    float Value;
};

struct FloatRangeProperty : public Property
{
    float Min;
    float Max;
};

struct FloatKeyFrame : public ProtectedGameObject<FloatKeyFrame>
{
    virtual ~FloatKeyFrame() = 0;
    virtual void Clone(FloatKeyFrame const&) = 0;
    virtual bool Visit(ObjectVisitor*) = 0;
};

struct LinearFloatKeyFrame : public FloatKeyFrame
{
    float Time;
    [[bg3::hidden]] __int32 field_c; // Padding
    float Value;
};


struct CubicFloatKeyFrame : public FloatKeyFrame
{
    float Time;
    [[bg3::hidden]] __int32 field_c; // Padding
    float A;
    float B;
    float C;
    float D;
};


struct KeyFrameData : public ProtectedGameObject<KeyFrameData>
{
    virtual ~KeyFrameData() = 0;
    virtual int GetType() = 0;
};

struct LinearFloatKeyFrameData : public KeyFrameData
{
    LegacyArray<LinearFloatKeyFrame> Frames;
};

struct CubicFloatKeyFrameData : public KeyFrameData
{
    LegacyArray<CubicFloatKeyFrame> Frames;
};

struct FloatKeyFrameProperty : public Property
{
    LegacyArray<KeyFrameData*> KeyFrames;
};

struct StringProperty : public Property
{
    STDString Value;
};

struct Vector3Property : public Property
{
    Vector3 Value;
    [[bg3::hidden]] __int32 field_44;
};

// Types: 0, 1, 2, 3 is null
struct FixedFunction : public ProtectedGameObject<FixedFunction>
{
    virtual ~FixedFunction() = 0;
    virtual void unknown1() = 0;
    virtual void unknown2() = 0;
    virtual void unknown3() = 0;
    virtual int GetFunctionType() = 0;
};

struct FixedFunction0 : public FixedFunction
{
    float A;
    float B;
};

struct FixedFunction1 : public FixedFunction
{
    float A;
    float B;
    float C;
};

struct FixedFunction2 : public FixedFunction
{
    float A;
    float B;
    float C;
    float D;
};

struct FixedFunctionProperty : public Property
{
    FixedFunction* Function;
};

struct FixedStringProperty : public Property
{
    FixedString Value;
};

struct BaseProperty : public Property
{
};

#if defined(BG3SE_LINUX)
// PORT (linux): the aspk::Property family, measured against bin/bg3 (c8da62b1).
// Property is 0x38 bytes with its last member (Input, a FixedString) at 0x30, so it has
// four bytes of tail padding, and Itanium lets a derived property reuse them.  MSVC does
// not, so every property whose first member is 4-aligned or smaller sits at 0x34 here
// and would sit at 0x38 under upstream's own compiler.  The game does the Itanium thing.
//   The base, from the macOS aspk::PropertyFactory::Create (0x107215f10), which is an
//     inlined construction of one property: `mov edi, 0x38` to the allocator, then
//     [rbx] = vtable, [rbx+8] = -1 as a qword (FullName 0x8 and AttributeName 0xc, both
//     the null FixedString index), [rbx+0x10] = a second vtable (Parameters, the
//     LegacyArray's own vptr), zeroes over 0x18..0x27 (buf_ 0x18, capacity_ 0x20,
//     size_ 0x24), [rbx+0x28] = 0 (Used) and [rbx+0x2c] = 0xffffffff00000001
//     (GrowSize 1 at 0x2c, Input -1 at 0x30).
//   The same offsets on the Linux side: aspk::Property::Clone (bin/bg3 0x30afa10, the
//     matched counterpart of macOS 0x107214b50) refcounts the FixedStrings at +0x8 and
//     +0xc and copies +0x18, +0x20, +0x24, +0x28 and +0x30.
//   Each derived property, by its own Clone in bin/bg3 -- every one of them tail-calls
//     0x30afa10 and then touches exactly one region starting at 0x34:
//       0x754dae0  byte                  BooleanProperty::Value
//       0x7550730  dword                 IntegerProperty::Value
//       0x7550780  qword (two int32)     IntegerRangeProperty::Min, Max
//       0x754e0d0  movss                 FloatProperty::Value
//       0x754e140  movsd (two float)     FloatRangeProperty::Min, Max
//       0x754e240  qword + dword at 0x3c Vector3Property::Value (12 bytes, 0x34..0x3f)
//       0x75507d0  dword, then FixedString::operator= (0x3883660) on this+0x34
//                                        FixedStringProperty::Value
//     The instruction kind is what tells Integer from Float and Range from scalar; the
//     same seven shapes carry symbols on the macOS oracle (aspk::<X>Property::Clone at
//     0x10720d080, 0x1072141f0, 0x107214310, 0x107213e90, 0x107213fe0, 0x10721cfa0,
//     0x107210470 in that order), which is what assigns a shape to a name.
//   Two-sided: aspk::StringProperty::Clone (bin/bg3 0x754e1a0) puts its 8-aligned
//     STDString at 0x38, not 0x34 -- the tail padding is reused only when the member
//     fits it, so the 0x34 finding is not an artefact of the census.
//   No sizeof assert on the derived classes and none on Vector3Property::field_44: the
//     game frees them through ls::MemoryManager, which takes no size, and nothing reads
//     0x40.
static_assert(offsetof(Property, FullName) == 0x8);
static_assert(offsetof(Property, AttributeName) == 0xc);
static_assert(offsetof(Property, Parameters) == 0x10);
static_assert(offsetof(Property, Input) == 0x30);
static_assert(sizeof(Property) == 0x38);

static_assert(offsetof(BooleanProperty, Value) == 0x34);
static_assert(offsetof(IntegerProperty, Value) == 0x34);
static_assert(offsetof(IntegerRangeProperty, Min) == 0x34);
static_assert(offsetof(IntegerRangeProperty, Max) == 0x38);
static_assert(offsetof(FloatProperty, Value) == 0x34);
static_assert(offsetof(FloatRangeProperty, Min) == 0x34);
static_assert(offsetof(FloatRangeProperty, Max) == 0x38);
static_assert(offsetof(Vector3Property, Value) == 0x34);
static_assert(offsetof(FixedStringProperty, Value) == 0x34);
#endif

struct CustomModules
{
    [[bg3::hidden]] Array<void*> CustomFloatModules;
    [[bg3::hidden]] Array<void*> CustomFloat3Modules;
};

struct Component : public ProtectedGameObject<Component>
{
    virtual ~Component() = 0;
    virtual void Clone(Component const&) = 0;
    virtual bool Visit(ObjectVisitor*) = 0;
    virtual uint32_t GetPriorityIndex() const = 0;
    virtual FixedString const& GetTypeName() const = 0;
    virtual bool SetStayAlive(float) = 0;
    virtual void Update(float) = 0;
    virtual void PostUpdate(float) = 0;
    virtual void Preload(void*, double) = 0;
    virtual void Activate() = 0;
    virtual void Deactivate() = 0;
    virtual void Reset() = 0;
    virtual void Reload() = 0;
    virtual void OnPause(bool) = 0;
    virtual void SetTimelineOwner(Timeline*) = 0;
    virtual void SetPreRollData(Component const&) = 0;
    virtual void SetTargetVisual(uint64_t) = 0;
    virtual void ResetTargetVisual(uint64_t) = 0;
    virtual void UpdateTargetVisual() = 0;
    virtual bool HasResource(Guid const&) = 0;
    virtual void OnPreviewUpdated() = 0;
    virtual void ResetState() = 0;
    virtual bool VisitProperties(ObjectVisitor*) = 0;
    virtual void CreateVisual() = 0;
    virtual void DestroyVisual() = 0;
    virtual void Unknown01() = 0;
    virtual void FillSnapshotValidChannels(Array<int>&) const = 0;
    virtual void UpdateInternal(float) = 0;
    virtual void PostUpdateInternal(float) = 0;
    virtual void Unknown02() = 0;
    virtual void AttachMaterial() = 0;
    virtual void DetachMaterial() = 0;
    virtual void Unknown05() = 0;

    // Lua getter helpers
    FixedString LuaGetTypeName() const;

    //# P_GETTER(TypeName, LuaGetTypeName)

    Guid ID;
    [[bg3::hidden]] Timeline* Timeline;
    uint32_t TrackGroupIndex;
    float StartTime;
    float EndTime;
    bool IsActive;
    bool StayAlive;
};

struct Module
{
    STDString Name;
    Array<FixedString> FullName;
    Array<Property*> Properties;
};

// Note: There are a LOT of different EffectComponents, all seemingly over 0x100 in size
// The polymorphic choice should be made based on vtable[4]'s FixedString return value
struct FxBaseComponent : public Component
{
    float LastUpdateTimeStep;
    float LastUpdate;
    float TimeStepEdge0;
    float TimeStep;
    float TimeStepEdge1;
    ecs::EntityRef Entity;
    LegacyMap<FixedString, Property*> Properties;
    Array<Module*> Modules;
    [[bg3::hidden]] Array<Property*> PropertyList;
    uint32_t FxType;
    [[bg3::hidden]] resource::EffectResource* EffectResource;
};

struct TLActor
{
    [[bg3::hidden]] void* vtable;
    bg3se::Guid UUID;
    bool PeanutOverride;
};

struct TLBaseComponent : public Component
{
    TLActor Actor;
    uint8_t field_68x;
    uint32_t TypeId;
    int64_t PhaseIndex;
    bool IsSnappedToEnd;
    bool IsMimicry;
};

BEGIN_BARE_NS(keys)
    struct KeyBase : public ProtectedGameObject<KeyBase>
    {
        virtual ~KeyBase() = 0;
        virtual void Clone(KeyBase const&) = 0;
        virtual bool Visit(ObjectVisitor*) = 0;

        float Time;
        uint8_t InterpolationType;
    };
END_BARE_NS()

template<typename Key>
struct TLKeyBaseComponent : public TLBaseComponent
{
    float field_68;
    [[bg3::hidden]] Array<Key*> Keys;
    bool HasTargetKey;
    Key* TargetKey;
    bool NeedsKeyUpdate;
    bool field_91;
    std::array<uint32_t, 2> Random;
};

template struct TLKeyBaseComponent<keys::KeyBase>;

template<typename Key>
struct TLInterpolationKeyComponent : public TLKeyBaseComponent<Key>
{
};

template struct TLInterpolationKeyComponent<keys::KeyBase>;

template<typename Key>
struct TLEventKeyComponent : public TLKeyBaseComponent<Key>
{
};

template struct TLEventKeyComponent<keys::KeyBase>;

BEGIN_BARE_NS(keys)
    template <typename T>
    struct ValueKey : public KeyBase
    {
        T Value;
    };

    struct AtmosphereAndLightingChannelKey : public KeyBase
    {
        bg3se::Guid Id;
        float FadeTime;
    };

    struct AttachToEventKey : public KeyBase
    {
        bg3se::Guid Target;
        FixedString Bone;
        bool IsPersistent;
        glm::fvec3 offset;
    };

    struct AttitudeEventKey : public KeyBase
    {
        FixedString Pose;
        FixedString Transition;
    };

    struct CameraDoFChannelKey : public KeyBase
    {
        float Value;
    };

    struct CameraFoVKey : public KeyBase
    {
        float FoV;
    };

    struct CameraLookAtKey : public KeyBase
    {
        bg3se::Guid Target;
        FixedString Bone;
        glm::fvec2 Framing;
        glm::fvec2 FramingOffset;
        glm::fvec2 FreeBounds;
        glm::fvec2 SoftBounds;
        float DampingStrength;
        float FreeZoneDelay;
        float SoftZoneDelay;
        float SoftZoneRampTime;
    };

    struct EffectPhaseEventKey : public KeyBase
    {
        int32_t EffectPhase;
    };

    struct EmotionEventKey : public KeyBase
    {
        int32_t Emotion;
        int32_t Variation;
        bool IsSustainedEmotion;
        bool AppliesMaterials;
    };

    struct FloatRTPCKey : public KeyBase
    {
        float FloatRTPCValue;
    };

    struct GenomeTextEventKey : public KeyBase
    {
        FixedString EventName;
    };

    struct HandsIKKey : public KeyBase
    {
        bool InverseKinematics;
    };

    struct LookAtEventKey : public KeyBase
    {
        bg3se::Guid Target;
        FixedString Bone;
        uint8_t TurnMode;
        uint8_t TrackingMode;
        float TurnSpeedMultiplier;
        float TorsoTurnSpeedMultiplier;
        float HeadTurnSpeedMultiplier;
        std::optional<double> Weight;
        std::optional<double> SaveZoneAngle;
        std::optional<double> HeadSafeZoneAngle;
        glm::fvec3 Offset;
        bool Reset;
        uint8_t LookAtMode;
        uint8_t LookAtInterpMode;
        bool IsEyeLookAtEnabled;
        bg3se::Guid EyeLookAtTargetId;
        FixedString EyeLookAtBone;
        glm::fvec3 EyeLookAtOffset;
    };

    struct MaterialTextureKeyKey : public KeyBase
    {
        bg3se::Guid Value;
    };

    struct PhysicsKey : public KeyBase
    {
        bool InverseKinematics;
    };

    struct PlayEffectEventKey : public KeyBase
    {
        bool PlayEffect;
    };

    struct PlayRateKey : public KeyBase
    {
        float Speed;
    };

    struct ShapeShiftKey : public KeyBase
    {
        bg3se::Guid TemplateId;
    };

    struct ShowArmorChannelKey : public KeyBase
    {
        bool Value;
    };

    struct ShowHUDKey : public KeyBase
    {
        bool ShowHUD;
    };

    struct ShowPeanutsKey : public KeyBase
    {
        bool ShowPeanuts;
    };

    struct ShowVisualKey : public KeyBase
    {
        bool ShowVisual;
    };

    struct ShowWeaponKey : public KeyBase
    {
        bool ShowWeapon;
    };

    struct SoundEventKey : public KeyBase
    {
        struct Type0
        {
            bg3se::Guid SoundEventID;
            uint8_t SoundObjectIndex;
            bool KeepAlive;
            float LoopLifetime;
        };
        struct Type1
        {
            bg3se::Guid SoundEventID;
        };
        struct Type2
        {
            bg3se::Guid SoundEventID;
        };
        struct Type3
        {
            bg3se::Guid SoundEventID;
        };
        struct Type4
        {
            uint8_t FoleyType;
            uint8_t FoleyIntensity;
            uint8_t SoundObjectIndex;
        };
        struct Type5
        {
            uint8_t VocalType;
            uint8_t SoundObjectIndex;
        };
        std::variant<Type0, Type1, Type2, Type3, Type4, Type5> data;
    };

    struct SplatterChannelKey : public KeyBase
    {
        float Value;
        uint8_t SplatterChangeMode;
    };

    struct SpringsKey : public KeyBase
    {
        bool EnableSprings;
    };

    struct SteppingFadeKey : public KeyBase
    {
        bool SteppingInOut;
    };

    struct SwitchLocationEventKey : public KeyBase
    {
        bg3se::Guid SwitchLocationEventID;
        bg3se::Guid s_LevelTemplateID;
    };

    struct SwitchStageEventKey : public KeyBase
    {
        bg3se::Guid SwitchStageEventID;
        bool ForceTransformUpdate;
        bool ForceUpdateCameraBehavior;
    };

    struct TransformChannelFloatKey : public KeyBase
    {
        float Value;
    };

    struct TransformChannelFrameOfReferenceKey : public KeyBase
    {
        struct FrameOfReference
        {
            bg3se::Guid targetId;
            FixedString targetBone;
            bool OneFrameOnly;
            bool KeepScale;
        };
        FrameOfReference frameOfReference;
    };

    struct TransformChannelQuatKey : public KeyBase
    {
        glm::fvec4 Value;
    };
END_BARE_NS()

#if defined(BG3SE_LINUX)
// PORT (linux): the aspk::keys::KeyBase family, measured against bin/bg3 (c8da62b1).
// KeyBase is 16 bytes -- vptr, Time at 8, InterpolationType at 0xc -- so it has three
// bytes of tail padding, and Itanium lets a derived class reuse them.  MSVC does not, so
// every key whose first member is a bool sits at 0xd here and would sit at 0x10 under
// upstream's own compiler.  The game does the Itanium thing, so the mirror is already
// right and these asserts pin it:
//   The whole family at once, off bin/bg3's vtables: twelve 4-slot runs of the shape
//     {D1, D0, Clone, Visit} whose Clone is a 23-byte copy of `Time` (movss [rsi+8]),
//     `InterpolationType` (byte [rsi+0xc]) and one more byte at [rsi+0xd] --
//     0x745f6b0, 0x7460a50, 0x7461580, 0x7462400, 0x74625a0, 0x7462cb0, 0x7462e60,
//     0x7462f20, 0x74630c0, 0x7463ba0, 0x7463c00, and 0x7463420 (SoundEventKey, which
//     goes on past 0xd).  Every one of the twelve Visit slots visits a bool at +0xd.
//     The same census over the macOS oracle finds twelve as well, and there the classes
//     carry symbols: ls::TL<X>::<X>Key::Clone for HandsIK (0x1070a2380), Physics
//     (0x1070aa1a0), PlayEffect (0x1070aa4e0), ShowArmorChannel (0x1070d4060),
//     ShowPeanuts (0x1070d5120), ShowVisual (0x1070d5460), ShowWeapon (0x1070d57a0),
//     Springs (0x1070d8e90), SteppingFade (0x1070d9260), plus TLVfxChannel::ChannelKey,
//     TLVisibilityChannel::ChannelKey and TLSoundEvent::SoundEventKey.
//   Two are named on the Linux side directly: ecl::TLShowVisual::PostUpdateInternal
//     (bin/bg3 0x2f51760) loads `movzx esi, byte [rbx+0xd]` at 0x2f5179c and passes it as
//     the bool argument of ecl::TLShowVisual::ShowVisual(ecs::EntityRef const&, bool), and
//     ecl::TLSteppingFade::PostUpdateInternal (0x6689be0) branches on
//     `cmp byte [r14+0xd], 0` at 0x6689c5f to pick the GEGE::TimelineSteppingFade payload.
//   sizeof: each component's macOS CreateKeyInternal allocates `mov edi, 0x10` before
//     running the key's ctor, one per class -- TLHandsIK 0x1070a2200, TLPhysics
//     0x1070aa020, TLPlayEffectEvent 0x1070aa360, TLShowArmorChannel 0x1070d3ec0,
//     TLShowPeanuts 0x1070d4f90, TLShowVisual 0x1070d52e0, TLShowWeapon 0x1070d5620,
//     TLSprings 0x1070d90e0, TLSteppingFade 0x1070d9430.
//   Member names come from the macOS Visit methods, which pass an anonymous-namespace
//     FixedString per member -- strTime, strInterpolationType, strSteppingInOut for
//     ls::TLSteppingFade::SteppingKey::Visit (0x1070d9280).
// keys::ShowHUDKey gets no assert: `TLShowHUD` is in upstream's timeline-component list
// but appears in none of the builds we have (0 occurrences as a string -- not even inside a
// longer string -- in bin/bg3, in the macOS oracle and in the Windows bg3.exe, against one
// exact string each for the other 37 names in that list; and the toolkit's mangled exports
// name ls::TLShowArmor in three DLLs and TLShowHUD in none of its 1376 files), so there is
// no game class to measure.
static_assert(sizeof(keys::KeyBase) == 0x10);
static_assert(offsetof(keys::KeyBase, Time) == 0x8);
static_assert(offsetof(keys::KeyBase, InterpolationType) == 0xc);

static_assert(offsetof(keys::HandsIKKey, InverseKinematics) == 0xd);
static_assert(sizeof(keys::HandsIKKey) == 0x10);
static_assert(offsetof(keys::PhysicsKey, InverseKinematics) == 0xd);
static_assert(sizeof(keys::PhysicsKey) == 0x10);
static_assert(offsetof(keys::PlayEffectEventKey, PlayEffect) == 0xd);
static_assert(sizeof(keys::PlayEffectEventKey) == 0x10);
static_assert(offsetof(keys::ShowArmorChannelKey, Value) == 0xd);
static_assert(sizeof(keys::ShowArmorChannelKey) == 0x10);
static_assert(offsetof(keys::ShowPeanutsKey, ShowPeanuts) == 0xd);
static_assert(sizeof(keys::ShowPeanutsKey) == 0x10);
static_assert(offsetof(keys::ShowVisualKey, ShowVisual) == 0xd);
static_assert(sizeof(keys::ShowVisualKey) == 0x10);
static_assert(offsetof(keys::ShowWeaponKey, ShowWeapon) == 0xd);
static_assert(sizeof(keys::ShowWeaponKey) == 0x10);
static_assert(offsetof(keys::SpringsKey, EnableSprings) == 0xd);
static_assert(sizeof(keys::SpringsKey) == 0x10);
static_assert(offsetof(keys::SteppingFadeKey, SteppingInOut) == 0xd);
static_assert(sizeof(keys::SteppingFadeKey) == 0x10);
#endif

BEGIN_BARE_NS(channels)
    template <typename T>
    struct EventKeyChannel : public TLEventKeyComponent<keys::ValueKey<T>>
    {
        T defaultVal;
    };
            
    struct TimelineActorPropertiesReflectionKeyComponent : public TLInterpolationKeyComponent<keys::ValueKey<float>>
    {
        float KeyDefault;
    };

    struct TLAtmosphereAndLightingChannelComponent : public TLInterpolationKeyComponent<keys::AtmosphereAndLightingChannelKey>
    {
        __int64 field_b0;
        __int64 field_b8;
        __int64 field_c0;
    };

    struct TLCameraDoFChannelComponent : public TLInterpolationKeyComponent<keys::CameraDoFChannelKey>
    {
        float field_b0;
        uint8_t channelNum;
        __int8 field_b5;
        __int16 field_b6;
    };

    struct TLCameraExposureChannelComponent : public TLCameraDoFChannelComponent
    {
    };

    struct TLMaterialKeyComponent : public TLInterpolationKeyComponent<keys::ValueKey<float>>
    {
        // PORT (linux): upstream's `__int32 field_b4` here was MSVC trailing padding, not a
        // member -- the game allocates 0x98 (ls::TLMaterial's ctor, bin/bg3 0x3094e85) and
        // ls::TLMaterialKeyComponent::Clone 0x3098d50 copies only the float at 0x94.
        float defaultVal;
    };

    struct TLMaterialTextureKeyComponent : public TLInterpolationKeyComponent<keys::MaterialTextureKeyKey>
    {
        __int64 field_b0;
        __int64 field_b8;
    };

    struct TLShowArmorChannelComponent : public TLEventKeyComponent<keys::ShowArmorChannelKey>
    {
        // PORT (linux): upstream's single `__int64 field_b0` at 0x98 is three members merged.  The
        // game puts a bool at 0x94 -- inside TLKeyBaseComponent's four bytes of tail padding, so
        // MSVC would have started the block at 0x98 -- a flag byte at 0x95 and a uint32 at 0x98,
        // sizeof 0xa0.  Vtable slot 30 (bin/bg3 0x7462cd0) copies the key's Value from [rax+0xd] to
        // [rdi+0x94] as a byte and then stores `mov byte [rdi+0x95], 1`; the ctor 0x3094350 writes
        // the uint32 at 0x98 from r13d, which starts at 1 and doubles per channel over the eleven
        // channels it builds, so it is a per-channel bit.
        bool defaultVal;
        bool field_ad;
        __int32 field_b0;
    };

    struct TLSplatterChannelComponent : public TLInterpolationKeyComponent<keys::SplatterChannelKey>
    {
        // PORT (linux): upstream's `__int64 field_b0` at 0x98 plus `SplatterType` at 0xa0 (sizeof
        // 0xa8) is four members merged into two, and one byte off: the game is sizeof 0xa0 with the
        // block starting at 0x94, in TLKeyBaseComponent's tail padding.  Vtable slot 30 (bin/bg3
        // 0x7463930) is the whole block -- `mov byte [rdi+0x9c], 1`, then the key's
        // SplatterChangeMode from byte [rax+0x14] to byte [rdi+0x94], then the key's Value float
        // from [rax+0x10] to [rdi+0x98] with movss -- and ls::TLSplatterChannel::Clone 0x7463890
        // copies exactly the byte at 0x9d while ::Visit 0x74638c0 visits it through the visitor's
        // uint8 slot [rax+0xb8] with a FixedString of its own, which is what names SplatterType and
        // puts it at 0x9d rather than 0xa0.
        uint8_t defaultChangeMode;
        float defaultVal;
        uint8_t field_b4;
        uint8_t SplatterType;
    };

    struct TLTransformChannelFloatComponent : public TLInterpolationKeyComponent<keys::TransformChannelFloatKey>
    {
        // PORT (linux): upstream's `__int32 field_b4` between the two was MSVC padding, not a
        // member -- the game allocates 0xa8 and keeps the array at 0x98 (ls::TLTransform's ctor,
        // bin/bg3 0x309f673; the dtor 0x2d2f780 frees [0x98]).
        float defaultVal;
        Array<__int64> field_b8;
    };

    struct TLTransformChannelFrameOfReferenceComponent : public TLInterpolationKeyComponent<keys::TransformChannelFrameOfReferenceKey>
    {
        keys::TransformChannelFrameOfReferenceKey::FrameOfReference defaultVal;
        Array<__int64> field_b8;
    };

    struct TLTransformChannelQuatComponent : public TLInterpolationKeyComponent<keys::TransformChannelQuatKey>
    {
        glm::fvec4 defaultVal;
        Array<__int64> field_c0;
    };
END_BARE_NS()

struct TimelineActorPropertiesReflectionComponent : public TLBaseComponent
{
    struct Parameter : ProtectedGameObject<Parameter>
    {
        struct Range
        {
            channels::TimelineActorPropertiesReflectionKeyComponent** begin;
            channels::TimelineActorPropertiesReflectionKeyComponent** end;
        };
        virtual ~Parameter() = 0;
        virtual bool readData(void* dataReader) = 0;
        virtual Range& getRange(Range& r) = 0;

        [[bg3::hidden]] void* vtable;
        FixedString ParameterName;
        __int32 field_c;
        __int64 field_10;
        __int64 field_18;
        __int64 field_20;
        __int64 field_28;
        __int64 field_30;
        __int64 field_38;
        __int64 field_40;
        __int64 field_48;
        __int64 field_50;

        [[bg3::getter]] inline std::span<channels::TimelineActorPropertiesReflectionKeyComponent*> PropertyParameters()
        {
            [[bg3::hidden]] Range range;
            getRange(range);
            return std::span(range.begin, range.end - range.begin);
        }
    };
            
    __int64 field_80;
    Array<Parameter*> PropertyParameters;
};

struct TLAnimationComponent : public TLBaseComponent
{
    __int64 field_80;
    bg3se::Guid AnimationSourceId;
    __int64 field_98;
    __int64 field_a0;
    __int64 field_a8;
    // channels::EventKeyChannel<bool> field_b0;
    [[bg3::hidden]] std::array<__int8, 0xb8> raw_field_b0;
    [[bg3::getter]] channels::EventKeyChannel<bool>* field_b0()
    {
        return reinterpret_cast<channels::EventKeyChannel<bool>*>(&raw_field_b0);
    }
    FixedString AnimationSlot;
    bool Continuous;
    bool HoldAnimation;
    bool EnableRootMotion;
    bool IsMirrored;
    double FadeIn;
    double FadeOut;
    double FadeInOffset;
    double FadeOutOffset;
    __int64 field_190;
    double AnimationPlayRate;
    double AnimationPlayStartOffset;
    uint8_t OffsetType;
    __int8 field_1a9;
    __int16 field_1aa;
    __int32 field_1ac;
    bg3se::Guid AnimationGroup;
    struct TargetTransforms
    {
        glm::fvec4 RotationQuat;
        glm::fvec3 Position;
        float field_1c;
        float field_20;
        float Scale;
    };
    TargetTransforms TargetTransform;
    bg3se::Guid BoneGroupId;
    __int64 field_1f8;
    // channels::EventKeyChannel<bool> HideVfxChannel;
    [[bg3::hidden]] std::array<__int8, 0xb8> raw_HideVfxChannel;
    [[bg3::getter]] channels::EventKeyChannel<bool>* HideVfxChannel()
    {
        return reinterpret_cast<channels::EventKeyChannel<bool>*>(&raw_HideVfxChannel);
    }
    __int64 field_2b8;
    __int64 field_2c0;
};

struct TLAdditiveAnimationComponent : public TLAnimationComponent
{
};

struct TLAtmosphereAndLightingComponent : public TLBaseComponent
{
    std::array<channels::TLAtmosphereAndLightingChannelComponent*, 2> Channels;
};

struct TLAttachToEventComponent : public TLInterpolationKeyComponent<keys::AttachToEventKey>
{
};

struct TLAttitudeEventComponent : public TLKeyBaseComponent<keys::AttitudeEventKey>
{
    // PORT (linux): upstream's `__int32 field_b4` was MSVC trailing padding, not a member --
    // the game allocates 0x98 (bin/bg3 0x309daf5) and its ctor 0x309f080 touches nothing past
    // the FixedString at 0x94.
    FixedString field_b0;
};

struct TLCameraDoFComponent : public TLBaseComponent
{
    Array<channels::TLCameraDoFChannelComponent*> Channels;
};

struct TLCameraExposureComponent : public TLBaseComponent
{
    Array<channels::TLCameraExposureChannelComponent*> Channels;
    __int64 field_90;
};

struct TLCameraFoVComponent : public TLInterpolationKeyComponent<keys::CameraFoVKey>
{
};

struct TLCameraLookAtComponent : public TLInterpolationKeyComponent<keys::CameraLookAtKey>
{
};

struct TLEffectPhaseEventComponent : public TLEventKeyComponent<keys::EffectPhaseEventKey>
{
};

struct TLEmotionEventComponent : public TLEventKeyComponent<keys::EmotionEventKey>
{
};

struct TLFloatRTPCComponent : public TLInterpolationKeyComponent<keys::FloatRTPCKey>
{
    __int64 field_b0;
    FixedString RTPCName;
    uint8_t SoundType;
    uint8_t SoundObjectIndex;
    [[bg3::hidden]] __int16 field_be;
};

struct TLGenomeTextEventComponent : public TLEventKeyComponent<keys::GenomeTextEventKey>
{
};

struct TLHandsIKComponent : public TLEventKeyComponent<keys::HandsIKKey>
{
};

struct TLLayeredAnimationComponent : public TLAnimationComponent
{
};

struct TLLookAtEventComponent : public TLEventKeyComponent<keys::LookAtEventKey>
{
};

struct TLMaterialComponent : public TLBaseComponent
{
    struct Parameter
    {
        struct Range
        {
            Component** begin;
            Component** end;
        };
        virtual ~Parameter() = 0;
        virtual bool parseXML(void*) = 0;
        virtual Range& getRange(Range&) = 0;
        FixedString MaterialParameterName;
        __int32 field_c;
        __int64 field_10;
    };

    struct TextureParameter : public Parameter
    {
        bool TextureParam;
        bool IsVirtual;
        __int16 field_1a;
        __int32 field_1c;

        [[bg3::getter]] inline std::span<channels::TLMaterialTextureKeyComponent*> PropertyParameters()
        {
            [[bg3::hidden]] Range range;
            getRange(range);
            return std::span((channels::TLMaterialTextureKeyComponent**)range.begin, range.end - range.begin);
        }
    };

    struct MaterialParameter : public Parameter
    {
        [[bg3::getter]] inline std::span<channels::TLMaterialKeyComponent*> PropertyParameters()
        {
            [[bg3::hidden]] Range range;
            getRange(range);
            return std::span((channels::TLMaterialKeyComponent**)range.begin, range.end - range.begin);
        }
    };

    Array<Parameter*> MaterialParameters;
    // channels::EventKeyChannel<bool> VisibilityChannel;
    [[bg3::hidden]] std::array<__int8, 0xb8> raw_VisibilityChannel;
    [[bg3::getter]] channels::EventKeyChannel<bool>* VisibilityChannel()
    {
        return reinterpret_cast<channels::EventKeyChannel<bool>*>(&raw_VisibilityChannel);
    }
    bg3se::Guid GroupId;
    bool IsContinuous;
    bool IsOverlay;
    __int16 field_15a;
    float OverlayPriority;
};

struct TLPhysicsComponent : public TLEventKeyComponent<keys::PhysicsKey>
{
};

struct TLPlayEffectEventComponent : public TLEventKeyComponent<keys::PlayEffectEventKey>
{
    // PORT (linux): the game's ls::TLPlayEffectEvent is sizeof 0x98 -- the base's -- so upstream's
    // `__int64 field_b0` at 0x98 read eight bytes PAST the end of the allocation.  The factory arm
    // bin/bg3 0x309e0c9 allocates 0x98 and, after the vtable store, writes `mov word [rbx+0x94],
    // r14w` and nothing else, so the record's own members are the two bytes at 0x94, in
    // TLKeyBaseComponent's tail padding (MSVC would put them at 0x98 and make it 0xa0 again).  The
    // class overrides no Clone, Visit or key-apply slot, so the two bytes carry no name; by its
    // siblings' shape they are the default-value / has-default pair TLShowArmorChannelComponent has
    // at 0x94 / 0x95, but that is reasoning and not a witness.
    uint16_t field_b0;
};

struct TLPlayRateComponent : public TLKeyBaseComponent<keys::PlayRateKey>
{
};

struct TLShapeShiftComponent : public TLEventKeyComponent<keys::ShapeShiftKey>
{
};

struct TLShotComponent : public TLBaseComponent
{
    __int64 field_80;
    bg3se::Guid FirstCamera;
    Array<bg3se::Guid> CameraContainers;
    bool AutomatedCamera;
    bool AutomatedLighting;
    bool DisableConditionalStaging;
    bool IsLooping;
    bool SwitchInterval;
    bool IsJCutEnabled;
    __int8 field_b1;
    __int16 field_b2;
    float JCutLength;
    __int64 field_b8;
    __int64 field_c0;
    __int64 field_c8;
    __int64 field_d0;
    bool IsLogicEnabled;
    __int8 field_d9;
    __int16 field_da;
    __int32 field_dc;
    bg3se::Guid CompanionCameraA;
    bg3se::Guid CompanionCameraB;
    bg3se::Guid CompanionCameraC;
    __int64 field_110;
    __int64 field_118;
    __int64 field_120;
    __int64 field_128;
    __int64 field_130;
    __int64 field_138;
    __int64 field_140;
    __int64 field_148;
    __int64 field_150;
};

struct TLShotHoldPreviousComponent : public TLBaseComponent
{
};

struct TLShotZoomComponent : public TLBaseComponent
{
    float StartOffset;
    float EndOffset;
};

struct TLShowArmorComponent : public TLBaseComponent
{
// PORT (linux): upstream's `__int64 field_80;` is not in the native build, and its
// `__int64 field_98;` is four bytes wide here.  The game has NO member between the base and
// Channels, so upstream puts Channels at 0x70 where the game has it at 0x68 and all three of
// this class's exposed properties (PropertyMaps.inl:3399) read the wrong words.  This is not an
// ABI difference and the Itanium rule never flagged it -- the first derived member is 8-aligned
// either way -- so it is found by hand off the family census.  The addresses below are
// a187af94's, carried over with `rebase.py map`.
//
// The class is named without any symbol in bin/bg3, by the family's GetTypeName instrument:
// 0x7462d70 is the only `lea rax, [rip -> _ZN4TLFS14strTLShowArmorE (0x7d54c60)]; ret` in
// .text, and it is slot 5 of both of this class's vtables, 0x7ac6948 (ls::) and 0x79ffd90
// (ecl::).  Four of their slots measure the object, and no two disagree:
//
//   slot 2, ls::TLShowArmor::Clone 0x3098d10 -- inlines ls::TLBaseComponent::Clone's 65-byte
//     base copy verbatim (through byte 0x61, IsMimicry), then `mov eax, [rsi+0x78]` /
//     `mov [rdi+0x78], eax` -- a DWORD at 0x78, not a qword -- and then iterates the array as
//     buf `qword [+0x68]` with count `dword [+0x74]`, which is Array<T*>'s buf/capacity/size
//     at 0x68/0x70/0x74.  Nothing between the base and 0x68 is touched.
//   slot 3, ls::TLShowArmor::Visit 0x3095040 -- calls ls::TLBaseComponent::Visit (0x3099fd0)
//     and tail-calls the helper 0x3095070, which hands the visitor the FixedString the game
//     itself uses for this field, GEFS::strChannels (0x7d24a80), and works only buf [+0x68]
//     and size [+0x74].  So Channels is named by the game, not inferred.  It visits nothing
//     at 0x78.
//   slot 0, ls::TLShowArmor::~TLShowArmor 0x38e6150 -- restores its own vptr 0x7ac6958,
//     destroys each element of the array at buf [+0x68] for `dword [+0x74]` elements,
//     `operator delete[]`s (0x26dcc60) the buffer, zeroes [+0x68], and restores the base's
//     vptr 0x7ac4180.  It frees nothing at 0x78, so the member there owns nothing.
//   the inlined constructor, 0x30943a0 (it stores this vptr at 0x30943bb) -- `movups
//     [rdi+0x64], xmm0` zeroes through 0x73 and `mov qword [rdi+0x74], 0x100000000` sets the
//     size dword at 0x74 to 0 and the dword at 0x78 to **1**.  A non-zero value store, so 0x78
//     is a real member and the initialised extent ends at 0x7c.
//
// No sizeof assert, as for the rest of this family: both deleting destructors (0x38e6110,
// 0x38e6130) tail-call the UNSIZED _ZdlPv, and no `operator new` site in .text stores either
// vtable, so nothing in the disassembly states this class's size.  field_98 keeps its
// upstream name because it is a generated property (P_NOTIFY(field_98)); like the rest of the
// TLBaseComponent subclasses that name is the stale MSVC offset and not this one.
    Array<channels::TLShowArmorChannelComponent*> Channels;
    uint32_t field_98;
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(TLShowArmorComponent, Channels) == 0x68);
static_assert(offsetof(TLShowArmorComponent, field_98) == 0x78);
#endif

struct TLShowHUDComponent : public TLEventKeyComponent<keys::ShowHUDKey>
{
};

struct TLShowPeanutsComponent : public TLEventKeyComponent<keys::ShowPeanutsKey>
{
};

struct TLShowVisualComponent : public TLEventKeyComponent<keys::ShowVisualKey>
{
};

struct TLShowWeaponComponent : public TLEventKeyComponent<keys::ShowWeaponKey>
{
};

struct TLSoundEventComponent : public TLEventKeyComponent<keys::SoundEventKey>
{
    Array<TLFloatRTPCComponent*> RTPCChannels;
};

struct TLSplatterComponent : public TLBaseComponent
{
    Array<channels::TLSplatterChannelComponent*> Channels;
};

struct TLSpringsComponent : public TLEventKeyComponent<keys::SpringsKey>
{
    // PORT (linux): upstream's `__int32 field_b4` was MSVC trailing padding, not a member --
    // the game allocates 0x98 (bin/bg3 0x309f013) and ls::TLSprings::Clone 0x3098ab0 copies
    // only the dword at 0x94.
    uint32_t VisualFlag;
};

struct TLSteppingFadeComponent : public TLEventKeyComponent<keys::SteppingFadeKey>
{
};

struct TLSwitchLocationEventComponent : public TLEventKeyComponent<keys::SwitchLocationEventKey>
{
};

struct TLSwitchStageEventComponent : public TLEventKeyComponent<keys::SwitchStageEventKey>
{
    __int64 field_b0;
    __int64 field_b8;
};

struct TLTransformComponent : public TLBaseComponent
{
    channels::TLTransformChannelFloatComponent* field_80;
    channels::TLTransformChannelFloatComponent* field_88;
    channels::TLTransformChannelFloatComponent* field_90;
    channels::TLTransformChannelQuatComponent* field_98;
    channels::TLTransformChannelFloatComponent* field_a0;
    channels::TLTransformChannelFrameOfReferenceComponent* field_a8;
    bool Continuous;
    __int8 field_b1;
    __int16 field_b2;
    __int32 field_b4;
};

struct TLVoiceComponent : public TLBaseComponent
{
    bg3se::Guid DialogNodeId;
    bg3se::Guid ReferenceId;
    uint8_t FaceType;
    __int8 field_a1;
    __int16 field_a2;
    __int32 field_a4;
    double LeftBuffer;
    bool HoldMocap;
    bool DisableMocap;
    bool IsAliased;
    bool IsMirrored;
    __int32 field_b4;
    double PerformanceFade;
    double FadeIn;
    double FadeOut;
    double FadeInOffset;
    double FadeOutOffset;
    __int64 field_e0;
    uint8_t PerformanceDriftType;
    __int8 field_e9;
    __int16 field_ea;
    __int32 field_ec;
    double HeadPitchCorrection;
    double HeadYawCorrection;
    double HeadRollCorrection;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the aspk::TLBaseComponent family, measured against bin/bg3 (c8da62b1).
// TLBaseComponent is sizeof 0x68 with its last member (IsMimicry) at 0x61, so six bytes
// of tail padding, and under Itanium a derived member that fits lands at 0x64 where MSVC
// would put it at 0x68. Upstream names TLKeyBaseComponent's first member `field_68`, which
// is the MSVC offset; here it is 0x64, and the game does the same packing.
//   The base, all of it in one function: ls::TLBaseComponent::Clone (bin/bg3 0x745fe40,
//     matched to macOS 0x107098a00) is 65 bytes and copies exactly [8..0x17] (Component::ID),
//     dword 0x20 (TrackGroupIndex), qword 0x24 (StartTime + EndTime as one movsd),
//     dword 0x54 (TypeId), [0x38..0x47] (Actor.UUID), byte 0x48 (Actor.PeanutOverride),
//     qword 0x58 (PhaseIndex), byte 0x60 (IsSnappedToEnd), byte 0x61 (IsMimicry). Actor at
//     0x30 with UUID at +8 and PeanutOverride at +0x18 is what puts TLActor at sizeof 0x20,
//     and TypeId at 0x54 is what leaves field_68x the single byte at 0x50.
//     aspk::Component::Visit (0x309a180, called by ls::TLBaseComponent::Visit 0x3099f80)
//     visits the same ID / TrackGroupIndex / StartTime / EndTime set. IsActive and StayAlive
//     are runtime state: neither Clone nor Visit touches them, so they carry no assert.
//   TLShotZoomComponent is the positive case, named on the Linux side three ways:
//     ls::TLShotZoom::Clone (0x7462b90) inlines the 65-byte base copy above verbatim and
//     then copies eight bytes at 0x64 in one movsd; ls::TLShotZoom::Visit (0x7462be0)
//     calls ls::TLBaseComponent::Visit and then visits a float at 0x64 and a float at 0x68
//     through the visitor's float slot; ecl::TLShotZoom::PostUpdateInternal (0x6689d60)
//     reads `movss [rbx+0x64]` and `addss [rbx+0x68]`. StartOffset 0x64, EndOffset 0x68.
//   TLKeyBaseComponent<Key>, from the two methods the whole family shares -- the class is
//     identified by the vtable's GetTypeName slot, which is a two-instruction
//     `lea rax, [rip+X]; ret` naming a TLFS::str<Name> global, so all 63 TL component
//     vtables can be named from the binary alone. 50 of them hold 0x3099750 in the Clone
//     slot, covering 28 distinct type names (ls:: and ecl:: halves of TLAttachToEvent,
//     TLAttitudeEvent, TLCameraFoV, TLCameraLookAt, TLEmotionEvent, TLGenomeTextEvent,
//     TLHandsIK, TLLookAtEvent, TLPhysics, TLPlayEffectEvent, TLPlayRate, TLShapeShift,
//     TLShowPeanuts, TLShowVisual, TLShowWeapon, TLSteppingFade, TLSwitchLocationEvent,
//     TLSwitchStageEvent, TLEffectPhaseEvent, the five channel components and the three
//     bases TLKeyBaseComponent / TLEventKeyComponent / TLInterpolationKeyComponent):
//       ls::TLKeyBaseComponent::Clone (0x3099750) works Keys as buf 0x68 / capacity 0x70 /
//         size 0x74, copies the qword at 0x80 and zeroes the byte at 0x88;
//       ls::TLKeyBaseComponent::Visit (0x3099cc0) calls ls::TLBaseComponent::Visit, then
//         `imul eax, [r14+0x58], 0xa455` and stores it to 0x8c and 0x90 -- Random, seeded
//         from PhaseIndex -- and works the same Keys array;
//       ls::TLKeyBaseComponent::Activate (0x3124df0) sets qword 0x80 and byte 0x78 to zero;
//       ls::TLKeyBaseComponent::PostUpdateInternal (0x2793310) branches on byte 0x89.
//     field_68 itself is read by nothing, so its 0x64 is witnessed by exclusion rather than
//     by a load: Keys is measured at 0x68, the base's last member is at 0x61, and a 4-byte
//     member declared between them can only sit at 0x64 -- which is where the sibling
//     TLShotZoom's first float is measured to be.
//   Two-sided: three siblings of TLShotZoomComponent whose first derived member is
//     8-aligned put it at 0x68, not 0x64, so the tail padding is reused only when the
//     member fits it and the 0x64 finding is not an artefact of the census --
//     ls::TLCameraDoF::Clone (0x3098e20) iterates Channels as buf 0x68 / size 0x74 with
//     nothing at 0x64; ls::TLSplatter::Clone (0x3098d80) does the same and calls
//     0x3099750 on each element; ls::TLVoice::Clone (0x7464730) copies two Guids at 0x68
//     and 0x78. All four named by their vtables' GetTypeName globals (TLFS::strTLCameraDoF,
//     strTLSplatter, strTLVoice, strTLShotZoom).
//   No sizeof assert on TLBaseComponent or TLKeyBaseComponent: neither binary has a
//     ctor or a sized allocation for them here, and the derived classes start inside the
//     base's padding, so nothing in the disassembly states either size.
static_assert(offsetof(Component, ID) == 0x8);
static_assert(offsetof(Component, TrackGroupIndex) == 0x20);
static_assert(offsetof(Component, StartTime) == 0x24);
static_assert(offsetof(Component, EndTime) == 0x28);

static_assert(offsetof(TLActor, UUID) == 0x8);
static_assert(offsetof(TLActor, PeanutOverride) == 0x18);
static_assert(sizeof(TLActor) == 0x20);

static_assert(offsetof(TLBaseComponent, Actor) == 0x30);
static_assert(offsetof(TLBaseComponent, field_68x) == 0x50);
static_assert(offsetof(TLBaseComponent, TypeId) == 0x54);
static_assert(offsetof(TLBaseComponent, PhaseIndex) == 0x58);
static_assert(offsetof(TLBaseComponent, IsSnappedToEnd) == 0x60);
static_assert(offsetof(TLBaseComponent, IsMimicry) == 0x61);

static_assert(offsetof(TLKeyBaseComponent<keys::KeyBase>, field_68) == 0x64);
static_assert(offsetof(TLKeyBaseComponent<keys::KeyBase>, Keys) == 0x68);
static_assert(offsetof(TLKeyBaseComponent<keys::KeyBase>, HasTargetKey) == 0x78);
static_assert(offsetof(TLKeyBaseComponent<keys::KeyBase>, TargetKey) == 0x80);
static_assert(offsetof(TLKeyBaseComponent<keys::KeyBase>, NeedsKeyUpdate) == 0x88);
static_assert(offsetof(TLKeyBaseComponent<keys::KeyBase>, field_91) == 0x89);
static_assert(offsetof(TLKeyBaseComponent<keys::KeyBase>, Random) == 0x8c);

static_assert(offsetof(TLShotZoomComponent, StartOffset) == 0x64);
static_assert(offsetof(TLShotZoomComponent, EndOffset) == 0x68);
#endif

#if defined(BG3SE_LINUX)
// PORT (linux): the nine TL key-component leaf classes, measured against bin/bg3 (c8da62b1).
// These are the classes whose *immediate* base is a second-level template instantiation
// (TLKeyBaseComponent<Key> / TLEventKeyComponent<Key> / TLInterpolationKeyComponent<Key>), so
// layouts.py's --by-base groups them apart from the rest of the family.  All of them
// put their first derived member at 0x94 -- the four bytes of tail padding TLKeyBaseComponent
// leaves after Random (0x8c..0x94, sizeof 0x98) -- except the one negative control below.
//   Named the way the family always is: the vtable's GetTypeName slot is a two-instruction
//   `lea rax, [rip+X]; ret` whose X is a TLFS::str<Name> global, which names 81 runs in this
//   binary outright (the ls:: block runs 0x7ac3500..0x7ac8498, the ecl:: one 0x79ff9f0..0x7a01558).
//   The measurement then comes from the per-class allocating site -- `mov edi, <sizeof>;
//   call operator new` followed by the vtable store and the derived defaults -- with the class's
//   own Clone, Visit, PostUpdateInternal or destructor to type each offset.
//
//   TLSpringsComponent = ls::TLSprings, vtable 0x7ac7248.  Allocated 0x98 at bin/bg3 0x309f013
//     (inside the big TL factory 0x309cbc0, which allocates 63 of these classes in one function);
//     the vtable store is followed by `mov dword [rbx+0x94], 0xffffffff`, a non-zero value store.
//     ls::TLSprings::Clone 0x3098ab0 copies exactly the dword at 0x94 and nothing else, and
//     ls::TLSprings::Visit 0x3096bb0 hands &this->0x94 to the visitor's uint32 slot with -1 as the
//     default.  So VisualFlag is a uint32 at 0x94 and the record ends at 0x98: upstream's
//     `field_b4` was MSVC trailing padding (removed above).  MSVC would put VisualFlag at 0x98
//     and make the record 0xa0, so both the offset and the size discriminate.
//   TLMaterialKeyComponent = ls::TLMaterialKeyComponent, vtable 0x7ac56f0.  Allocated 0x98 at
//     0x3094e85 (ls::TLMaterial's ctor); ls::TLMaterialKeyComponent::Clone 0x3098d50 copies the
//     float at 0x94 with movss -- which types it -- and nothing else.  MSVC: 0x98 / sizeof 0xa0.
//   TLAttitudeEventComponent = ls::TLAttitudeEvent, vtable 0x7ac3fb0.  Allocated 0x98 at
//     0x309daf5, which calls the ctor 0x309f080; the ctor sets [0x94] to -1, then acquires a
//     FixedString through ls::gst::Acquire and stores its index to 0x94 (0x309f144), and releases
//     the old one through ls::gst::Map::Release when it was not -1 (0x309f166).  The
//     acquire/release discipline is what types 0x94 as a FixedString.  MSVC: 0x98 / sizeof 0xa0.
//   TimelineActorPropertiesReflectionKeyComponent, vtable 0x7ac36c0.  Allocated 0x98 at 0x3ba9877
//     (TimelineActorPropertiesReflectionComponent::Parameter's ctor 0x3ba9840), and its
//     PostUpdateInternal 0x2792a20 interpolates four Key values through
//     ls::interpolate<float>(ESplineType, float*, float) and stores the return with
//     `movss [rbx+0x94]` -- a value store, and the float type with it.  MSVC: 0x98 / sizeof 0xa0.
//   TLCameraDoFChannelComponent = ls::TLCameraDoFChannel, vtable 0x7ac4270.  ls::TLCameraDoF's
//     ctor 0x3093bb0 builds seven of them, each `mov edi, 0xa0; call operator new`, and every arm
//     ends `mov dword [r15+0x94], <default>; mov byte [r15+0x98], <channel>` with the channel
//     running 0 through 6 and two of the defaults non-zero (1.0f at 0x3093eb9, 3.0f at 0x3094211).
//     Seven value stores at each offset.  Here sizeof is NOT an ABI witness -- MSVC lays this one
//     out at 0xa0 as well, because the byte at 0x98 absorbs the shift -- so only the two offsets
//     discriminate (MSVC: 0x98 and 0x9c).  field_b5 and field_b6 are never written and carry no
//     assert.
//   TLCameraExposureChannelComponent = ls::TLCameraExposureChannel, vtable 0x7ac44a8, adds
//     nothing: its ctor 0x3dd7f80 builds four with `mov edi, 0xa0` and the identical
//     0x94 / 0x98 pair, channels 0..3.
//   TLTransformChannelFloatComponent = ls::TLTransformChannel<float>, vtable 0x7ac7910 (named
//     outright: vtable slot 27 is the symtab's ls::TLTransformChannel<float>::PostUpdateInternal
//     0x2792f10).  ls::TLTransform's ctor 0x309f3e0 builds four of them at `mov edi, 0xa8`, and
//     the fourth (0x309f673, the scale channel) stores `mov dword [r14+0x94], 0x3f800000` -- 1.0f,
//     a value store -- before zeroing 0x98..0xa8.  PostUpdateInternal takes the array object as
//     `lea rax, [rdi+0x98]` and checks its capacity as `cmp dword [rdi+0xa0], esi` (0x279305a,
//     0x279306a) before growing it, and writes ls::interpolate<float>'s return back with
//     `movss [rax+0x94]` (0x2793189); the destructor 0x2d2f780 frees [0x98] and zeroes the dwords
//     at 0xa0 and 0xa4.  That is Array<T>'s buf_/capacity_/size_ at 0x98/0xa0/0xa4, so the array
//     is at 0x98 and the record ends at 0xa8 -- upstream's `field_b4` was MSVC padding (removed
//     above).  MSVC: 0x98 and 0xa0, sizeof 0xb0.
//   TLTransformChannelQuatComponent = ls::TLTransformChannel<Quaternionf>, vtable 0x7ac7a50 (slot
//     27 is ls::TLTransformChannel<Quaternionf>::PostUpdateInternal 0x2792a80).  Allocated 0xb8 at
//     0x309f5d9; xmm0 is zeroed and then `movhps xmm0, qword [0x1b74ea0]` loads {0.0f, 1.0f} into
//     its high half before `movups [r14+0x94], xmm0` -- the identity quaternion (0,0,0,1), a value
//     store whose non-zero lane fixes the block at 0x94..0xa4.  The array follows at 0xa8, zeroed
//     by `movups [r14+0xa8], xmm0` and confirmed by the destructor 0x2d2f5b0 (`mov rdi, [rbx+0xa8]`
//     freed, `cmp dword [rdi+0xb0], 0` the capacity).  Already right; MSVC would be 0x98 / 0xa8,
//     sizeof 0xb8 -- same size, so here the offsets alone discriminate.
//   TLTransformChannelFrameOfReferenceComponent = ls::TLTransformChannel<FrameOfReference>, vtable
//     0x7ac7b90, is the family's NEGATIVE CONTROL: its first derived member is 8-aligned, so it
//     does *not* move into the base's tail padding and the itanium rule leaves it unflagged.
//     Allocated 0xc0 at 0x309f6ff; the ctor builds the struct on the stack (ls::Guid::s_Empty, a
//     FixedString of -1, then 0x100 as two bytes) and copies it to **0x98**, not 0x94 --
//     `movups [r14+0x98], xmm0` for the Guid, `mov dword [r14+0xa8], ebp` (-1) for the
//     FixedString, `mov word [r14+0xac], ax` for the two bools -- then zeroes the array at 0xb0.
//     The destructor 0x2d2f860 corroborates all of it: FixedString::~FixedString on [rbx+0xa8],
//     the array freed from [rbx+0xb0] with its capacity at 0xb8.  So the 0x94 finding above is not
//     an artefact of only looking at classes that would show it.
//   No assert on any field_b5 / field_b6 / field_91-style filler: nothing writes them.
static_assert(offsetof(TLSpringsComponent, VisualFlag) == 0x94);
static_assert(sizeof(TLSpringsComponent) == 0x98);

static_assert(offsetof(TLAttitudeEventComponent, field_b0) == 0x94);
static_assert(sizeof(TLAttitudeEventComponent) == 0x98);

static_assert(offsetof(channels::TLMaterialKeyComponent, defaultVal) == 0x94);
static_assert(sizeof(channels::TLMaterialKeyComponent) == 0x98);

static_assert(offsetof(channels::TimelineActorPropertiesReflectionKeyComponent, KeyDefault) == 0x94);
static_assert(sizeof(channels::TimelineActorPropertiesReflectionKeyComponent) == 0x98);

static_assert(offsetof(channels::TLCameraDoFChannelComponent, field_b0) == 0x94);
static_assert(offsetof(channels::TLCameraDoFChannelComponent, channelNum) == 0x98);
static_assert(sizeof(channels::TLCameraDoFChannelComponent) == 0xa0);
static_assert(sizeof(channels::TLCameraExposureChannelComponent) == 0xa0);

static_assert(offsetof(channels::TLTransformChannelFloatComponent, defaultVal) == 0x94);
static_assert(offsetof(channels::TLTransformChannelFloatComponent, field_b8) == 0x98);
static_assert(sizeof(channels::TLTransformChannelFloatComponent) == 0xa8);

static_assert(offsetof(channels::TLTransformChannelQuatComponent, defaultVal) == 0x94);
static_assert(offsetof(channels::TLTransformChannelQuatComponent, field_c0) == 0xa8);
static_assert(sizeof(channels::TLTransformChannelQuatComponent) == 0xb8);

// the negative control: 8-aligned, so 0x98 and not 0x94
static_assert(offsetof(channels::TLTransformChannelFrameOfReferenceComponent, defaultVal) == 0x98);
static_assert(offsetof(keys::TransformChannelFrameOfReferenceKey::FrameOfReference, targetBone) == 0x10);
static_assert(offsetof(keys::TransformChannelFrameOfReferenceKey::FrameOfReference, OneFrameOnly) == 0x14);
static_assert(offsetof(keys::TransformChannelFrameOfReferenceKey::FrameOfReference, KeepScale) == 0x15);
static_assert(sizeof(keys::TransformChannelFrameOfReferenceKey::FrameOfReference) == 0x18);
static_assert(offsetof(channels::TLTransformChannelFrameOfReferenceComponent, field_b8) == 0xb0);
static_assert(sizeof(channels::TLTransformChannelFrameOfReferenceComponent) == 0xc0);
#endif


struct BillboardComponent : public FxBaseComponent
{
    Transform Transform;
    RenderableObject* BillboardRenderable;
    FloatKeyFrameProperty* KeyframedOffsetProperty;
    FloatKeyFrameProperty* OrientationProperty;
    StringProperty* TextProperty;
    ColorARGBKeyFrameProperty* ColorProperty;
    FloatKeyFrameProperty* BrightnessProperty;
    FloatKeyFrameProperty* SizeProperty;
    FloatProperty* UniformScaleProperty;
    FloatKeyFrameProperty* DynamicValueProperty;
    FloatKeyFrameProperty* AlphaProperty;
    FloatProperty* ModifierXProperty;
    FloatProperty* ModifierYProperty;
    FloatProperty* ModifierZProperty;
    FloatProperty* ModifierWProperty;
    Vector3Property* OffsetProperty;
    float UniformScale;
    float SnapToTop;
    float SnapToBottom;
    float Brightness;
    bool SnapToGround;
    bool MaterialApplied;
    uint8_t Facing;
    CustomModules CustomModules;
    glm::vec3 field_178;
    glm::vec3 field_184;
    glm::vec3 Position;
    glm::vec3 KeyframedPosition;
    glm::aligned_highp_vec4 Color;
    RenderPropertyList PropertyList;
    [[bg3::hidden]] uint16_t field_1C0;
};

struct BoundingBoxComponent : public FxBaseComponent
{
    glm::vec3 Center;
    glm::vec3 Size;
    glm::vec3 Position;
    glm::vec3 RequiredSize;
    // RenderableObject* BoxVisual;
};

struct BoundingSphereComponent : public FxBaseComponent
{
    glm::vec3 Center;
    float Radius;
    glm::vec3 Position;
    float RequiredRadius;
    // RenderableObject* SphereVisual;
};

struct CameraShakeComponent : public FxBaseComponent
{
    aspk::FloatRangeProperty* FalloffStartEndProperty;
    aspk::FloatKeyFrameProperty* ShakeStrengthProperty;
    aspk::FloatKeyFrameProperty* ShakeVibrationFrequencyProperty;
    uint64_t field_B8;
    float ShakeStrength;
    float ShakeFrequency;
    float field_C8;
    float field_CC;
    float UpdateTime;
    uint8_t Flags;
};

struct DecalComponent : public FxBaseComponent
{
    ColorARGBKeyFrameProperty* ColorProperty;
    FloatKeyFrameProperty* AlphaProperty;
    FloatKeyFrameProperty* BrightnessProperty;
    FloatKeyFrameProperty* DimensionsProperty;
    FloatKeyFrameProperty* ProjectedOffsetProperty;
    FloatKeyFrameProperty* OffsetProperty;
    FloatKeyFrameProperty* RotationProperty;
    FloatProperty* UniformScaleProperty;
    FloatKeyFrameProperty* DynamicValueProperty;
    FloatProperty* ModifierXProperty;
    FloatProperty* ModifierYProperty;
    FloatProperty* ModifierZProperty;
    FloatProperty* ModifierWProperty;
    [[bg3::hidden]] void* field_108;
    Material* Material;
    EntityHandle DecalEntity;
    float Distance;
    float NormalBlendingFactor;
    float Brightness;
    float UniformScale;
    glm::vec3 Offset;
    glm::mat3x3 Axis;
    glm::vec3 Position;
    glm::vec3 KeyframedPosition;
    glm::aligned_highp_vec4 Color;
    CustomModules CustomModules;
    uint8_t CoordinateSpace;
    bool IgnoreParentRotation;
};

struct DeflectorComponent : public FxBaseComponent
{
    bool Visible;
    glm::vec3 Normal;
    glm::vec3 Position;
    FloatProperty* PositionModifierXProperty;
    FloatProperty* PositionModifierYProperty;
    FloatProperty* PositionModifierZProperty;
    // Only available in editor
    // RenderableObject* PlaneVisual1;
    // RenderableObject* PlaneVisual2;
};

struct DragForceComponent : public FxBaseComponent
{
    FloatKeyFrameProperty* InfluenceRadiusProperty;
    FloatKeyFrameProperty* KeyframedOffsetProperty;
    FloatKeyFrameProperty* DampingProperty;
    glm::vec3 Offset;
    bool InfluenceRadiusApplied;
    float InfluenceRadius;
    glm::vec3 Position;
    glm::vec3 KeyframedPosition;
    glm::vec3 field_E4;
};

struct GravityForceComponent : public FxBaseComponent
{
    FloatKeyFrameProperty* InfluenceRadiusProperty;
    FloatKeyFrameProperty* KeyframedOffsetProperty;
    FloatKeyFrameProperty* AccelerationProperty;
    glm::vec3 Offset;
    glm::vec3 Direction;
    bool InfluenceRadiusApplied;
    float InfluenceRadius;
    glm::vec3 Position;
    glm::vec3 KeyframedPosition;
};

struct LightComponent : public FxBaseComponent
{
    ColorARGBKeyFrameProperty* ColorProperty;
    FloatKeyFrameProperty* IntensityProperty;
    FloatKeyFrameProperty* RadiusProperty;
    FloatKeyFrameProperty* PositionProperty;
    FloatKeyFrameProperty* RotationProperty;
    FloatKeyFrameProperty* FlickerAmountProperty;
    FloatKeyFrameProperty* FlickerSpeedProperty;
    FloatKeyFrameProperty* InitialRotationProperty;
    FloatKeyFrameProperty* RotationLifeProperty;
    [[bg3::hidden]] void* field_E8;
    // Only available in editor
    // RenderableObject* PreviewVisual;
    EntityHandle LightEntity;
    float SnapToTop;
    float SnapToBottom;
    glm::vec3 CalculatedPosition;
    glm::vec3 LastPosition;
    glm::vec3 field_120;
    glm::vec3 Offset;
    glm::vec3 Position;
    glm::vec3 KeyframedPosition;
    glm::vec3 field_15C;
    glm::vec3 field_168;
    glm::quat Rotation;
    bool WasActive;
    bool IgnoreParentRotation;
    bool ModulateLightTemplateRadius;
    bool OverrideLightTemplateColor;
    bool OverrideLightTemplateFlickerSpeed;
    bool SnapToGround;
    bool field_18A;
    bool AnimateManually;
    uint8_t CoordinateSpace;
};

struct ModelComponent : public FxBaseComponent
{
    EntityHandle VisualEntity;
    CustomModules CustomModules;
    float field_C8;
    float FadeOutTime;
    float Alpha;
    float Brightness;
    float UniformScale;
    float SnapTop;
    float SnapBottom;
    bool LoopAnimation;
    bool Active;
    bool SnapToGround;
    bool HasVisual;
    FixedString Animation;
    FixedString field_EC;
    FixedString field_F0;
    aspk::FloatKeyFrameProperty* KeyframedOffsetProperty;
    aspk::FloatKeyFrameProperty* ScaleRotationDurationProperty;
    aspk::FloatKeyFrameProperty* InitialRotationProperty;
    aspk::FloatKeyFrameProperty* RotationLifeProperty;
    aspk::ColorARGBKeyFrameProperty* ColorProperty;
    aspk::FloatKeyFrameProperty* BrightnessProperty;
    aspk::FloatKeyFrameProperty* UniformScaleProperty;
    aspk::FloatKeyFrameProperty* AxisScaleProperty;
    aspk::FloatProperty* AnimationSpeedProperty;
    aspk::BooleanProperty* SyncAnimationResourceTimeProperty;
    aspk::FloatKeyFrameProperty* DynamicValueProperty;
    aspk::FloatKeyFrameProperty* ModelAlphaProperty;
    aspk::FloatProperty* ModelModifierXProperty;
    aspk::FloatProperty* ModelModifierYProperty;
    aspk::FloatProperty* ModelModifierZProperty;
    aspk::FloatProperty* ModelModifierWProperty;
    aspk::FloatProperty* ScaleModifierXProperty;
    aspk::FloatProperty* ScaleModifierYProperty;
    aspk::FloatProperty* ScaleModifierZProperty;
    aspk::FloatProperty* KeyframedPositionModifierXProperty;
    aspk::FloatProperty* KeyframedPositionModifierYProperty;
    aspk::FloatProperty* KeyframedPositionModifierZProperty;
    aspk::BooleanProperty* ShowSkeletonProperty;
    [[bg3::hidden]] __int64 field_1B0;
    [[bg3::hidden]] __int64 field_1B8;
    [[bg3::hidden]] __int64 field_1C0;
    [[bg3::hidden]] __int64 field_1C8;
    glm::vec3 AxisScale;
    glm::vec3 Position;
    glm::vec3 KeyframedPosition;
    glm::vec3 Rotation;
    glm::vec3 Offset;
    glm::vec3 field_20C;
    glm::vec3 field_218;
    glm::aligned_highp_vec4 Color;
    glm::quat PivotRotation;
    glm::quat PostRotation;
};

struct MovingLevelComponent : public FxBaseComponent
{
    STDString LevelTemplate;
    glm::vec3 Offset;
    glm::vec3 ScrollingDirection;
    float ScrollingSpeed;
    float ScrollingDistance;
    float UpdateTime;
};

struct OrbitForceComponent : public FxBaseComponent
{
    FloatKeyFrameProperty* KeyframedOffsetProperty;
    FloatKeyFrameProperty* OrbitRadiusProperty;
    glm::vec3 Offset;
    glm::vec3 Position;
    glm::vec3 KeyframedPosition;
    glm::vec3 field_D4;
};

struct OverlayMaterialComponent : public FxBaseComponent
{
    ColorARGBKeyFrameProperty* ColorProperty;
    FloatKeyFrameProperty* BrightnessProperty;
    FloatProperty* AnimationSpeedProperty;
    FloatKeyFrameProperty* DynamicValueProperty;
    FloatKeyFrameProperty* AlphaProperty;
    FloatProperty* ModifierXProperty;
    FloatProperty* ModifierYProperty;
    FloatProperty* ModifierZProperty;
    FloatProperty* ModifierWProperty;
    EntityHandle EffectEntity;
    FixedString Material;
    uint32_t ApplyMaps;
    CustomModules CustomModules;
    uint64_t Applied;
    uint32_t ApplyTo;
    uint32_t OverlayType;
    glm::aligned_highp_vec4 Color;
    glm::vec3 VisualPosition;
    float Brightness;
    int32_t OverlayPriority;
    float FadeTime;
    uint8_t field_150;
    uint8_t field_151;
    bool Active;
    bool HasCustomVectors;
    uint8_t field_154;
    uint8_t AlphaChannel;
    [[bg3::hidden]] uint64_t field_158;
    [[bg3::hidden]] uint64_t field_158x;
    glm::vec4 field_160;
    glm::vec4 field_170;
    // Only available in editor
    // bool HasPreview;
    // EntityHandle DebugVisualEntity;
    // FixedString DebugAnimation;
};

struct PackedUnitVector
{
    uint16_t X;
    uint16_t Y;
    uint16_t Z;
};

struct FxParticleData
{
    uint32_t Flags;
    uint32_t Count;
    Array<float> Lifespan;
    Array<float> Ages;
    Array<glm::vec3> Positions;
    Array<glm::vec3> Velocities;
    Array<uint16_t> Rotations;
    Array<PackedUnitVector> Normals;
    Array<PackedUnitVector> UpVectors;
    Array<uint16_t> FlipbookImageIndices;
    Array<EntityHandle> field_88;
    Array<uint16_t> InitialRotationSpeeds;
    Array<uint32_t> FixedColors;
    Array<uint16_t> FixedScalars;
    Array<float> DiscardValues;
    Array<float> UnitAges;
    [[bg3::hidden]] Array<glm::vec3> Scales;
    Array<glm::vec3> AppliedForces;
    [[bg3::hidden]] Array<void*> MoveInstructions;
    Array<uint32_t> DiedParticles;
};

struct ParticleRenderStyle1
{
    Vector3Property* AxisAlignedOrientation;
    uint8_t Flags;
};

struct ParticleFlipbookData
{
    uint16_t InitialFrame;
    uint16_t HorizontalImages;
    uint16_t VerticalImages;
    uint16_t FrameRate;
    uint16_t FrameRangeMin;
    uint16_t FrameRangeMax;
    FloatKeyFrameProperty* ImageController;
    uint8_t Flags;
};

struct ParticleSystemSnapBehavior
{
    glm::vec3 field_0;
    glm::vec3 field_C;
    glm::mat3x3 field_18;
    float field_3C;
    float Top;
    float Bottom;
};

struct ParticleDistanceScale
{
    float Min;
    float Max;
    FloatKeyFrameProperty* Scale;
};

struct ParticleSystemComponent : public FxBaseComponent
{
    ParticleRenderStyle1* RenderStyle1;
    ParticleFlipbookData* Flipbook;
    [[bg3::hidden]] void* Emitter;
    ParticleSystemSnapBehavior* SnapBehavior;
    FloatKeyFrameProperty* KeyframedOffsetProperty;
    ColorARGBKeyFrameProperty* ParticleColorProperty;
    FloatKeyFrameProperty* ParticleBrightnessProperty;
    FloatKeyFrameProperty* ParticleAlphaProperty;
    FloatKeyFrameProperty* ParticleScaleProperty;
    FloatKeyFrameProperty* ParticleLocalOriginProperty;
    FloatKeyFrameProperty* ParticleVelocityOffsetProperty;
    FloatKeyFrameProperty* ParticleInitialRotationProperty;
    FloatKeyFrameProperty* ParticleRotationProperty;
    FloatKeyFrameProperty* ParticleInitialRotationSpeedProperty;
    FloatKeyFrameProperty* ParticleRotationSpeedProperty;
    FloatKeyFrameProperty* ParticleAxisScaleProperty;
    FloatKeyFrameProperty* EmitRateProperty;
    FloatKeyFrameProperty* EmitStutterProperty;
    FloatKeyFrameProperty* LifespanProperty;
    FloatKeyFrameProperty* InitialVelocityProperty;
    FloatKeyFrameProperty* EmitVelocityAnglesProperty;
    FloatKeyFrameProperty* EmitVelocityAxisRotationProperty;
    ColorARGBKeyFrameProperty* ColorProperty;
    FloatKeyFrameProperty* BrightnessProperty;
    FloatKeyFrameProperty* UniformScaleProperty;
    FloatKeyFrameProperty* AxisScaleProperty;
    FloatKeyFrameProperty* MaterialDynamicValueProperty;
    FloatProperty* MaterialModifierXProperty;
    FloatProperty* MaterialModifierYProperty;
    FloatProperty* MaterialModifierZProperty;
    FloatProperty* MaterialModifierWProperty;
    FloatProperty* MaterialScaleModifierXProperty;
    FloatProperty* MaterialScaleModifierYProperty;
    FloatProperty* MaterialScaleModifierZProperty;
    FloatProperty* KeyframedPositionModifierXProperty;
    FloatProperty* KeyframedPositionModifierYProperty;
    FloatProperty* KeyframedPositionModifierZProperty;
    FloatProperty* EmitRateModifierProperty;
    FloatKeyFrameProperty* InheritVelocityPercentProperty;
    glm::vec3 Offset;
    glm::vec3 LastPosition;
    glm::vec3 EmitVelocityAxis;
    glm::vec3 EmitPosition;
    float WarmUpTime;
    float RemainingParticleCount;
    float RecycleRadius;
    float RotationRateOverLife;
    float InitialVelocity;
    float Brightness_;
    float MinLifespan;
    float MaxLifespan;
    float UniformScale;
    glm::vec3 AxisScale;
    glm::vec3 Position;
    glm::vec3 KeyframedPosition;
    float ColorOverEffectLife;
    float ScaleOverEffectLife;
    glm::vec3 ScaleOverEffectLifeAxis;
    glm::aligned_highp_vec4 Color;
    float PhysicalMaterialFriction;
    float PhysicalMaterialBounce;
    float PhysicalMaterialLifetimeLoss;
    float PhysicalMaterialRadiusScale;
    glm::mat3x3 PivotRotation;
    // int field_2C4;
    Material* Material;
    FixedString Visual;
    [[bg3::hidden]] void* field_2D8;
    [[bg3::hidden]] void* MeshRenderList;
    [[bg3::hidden]] void* RenderBatch;
    [[bg3::hidden]] uint64_t field_2F0;
    uint8_t BlendStateID;
    uint8_t field_2F9;
    uint16_t field_2FA;
    uint16_t MaximumParticleCount;
    uint16_t InitialParticleCountMin;
    uint16_t InitialParticleCountMax;
    uint16_t RecycleRadius_;
    uint8_t RenderStyle;
    uint8_t CoordinateSpace;
    uint8_t EmitterType;
    uint8_t RenderOrder;
    uint8_t AttachedFXOnly;
    uint8_t Flags;
    uint8_t Flags2;
    // Array<void*> DebugShapes;
    // void* RecycleRadiusDebugShape;
    glm::aligned_highp_vec4 FixedColor;
    glm::vec3 ExternalVelocity;
    glm::vec3 ScaleMultiplier;
    glm::aligned_highp_vec4 ColorMultiplier;
    [[bg3::hidden]] Array<void*> CustomFloatModules;
    [[bg3::hidden]] Array<void*> CustomFloat3Modules;
    ParticleDistanceScale AlphaByEffectDistance;
    ParticleDistanceScale AlphaByParticleDistance;
    ParticleDistanceScale ScaleByEffectDistance;
    ParticleDistanceScale ScaleByParticleDistance;
    bool ParticleDiscardByDistanceEnabled;
    float ParticleDiscardByDistanceMin;
    float ParticleDiscardByDistanceMax;
    FloatKeyFrameProperty* ParticleDiscardByDistanceScale;
    FxParticleData ParticleData;
    [[bg3::hidden]] void* ParticleDeathSpawnFx;
    [[bg3::hidden]] void* AttachedFx;
    [[bg3::hidden]] void* field_500;
};

struct PostProcessComponent : public FxBaseComponent
{
    [[bg3::hidden]] Array<void*> field_A0; // FxPostProcessObject*
};

struct PreRollComponent : public FxBaseComponent
{
};

struct RadialForceComponent : public FxBaseComponent
{
    FloatKeyFrameProperty* InfluenceRadiusProperty;
    FloatKeyFrameProperty* KeyframedOffsetProperty;
    FloatKeyFrameProperty* AccelerationProperty;
    FloatKeyFrameProperty* LockDistanceProperty;
    glm::vec3 Offset;
    bool InfluenceRadiusApplied;
    bool LockDistanceApplied;
    float InfluenceRadius;
    glm::vec3 Position;
    glm::vec3 KeyframedPosition;
};

struct RibbonParticleData
{
    int field_0;
    int Size;
    int ParticleCount;
    int NextParticle;
    Array<uint16_t> Particles;
};

struct RibbonParticleHolder
{
    RibbonParticleData* Data;
    int field_8;
};

struct Ribbon2Component : public FxBaseComponent
{
    [[bg3::hidden]] void* EffectManager;
    [[bg3::hidden]] void* ResourceManager;
    [[bg3::hidden]] void* MaterialSystem;
    uint32_t field_B8;
    uint32_t CoordinateSpace;
    uint32_t EmissionBehavior;
    uint32_t TexCoordType;
    [[bg3::hidden]] RibbonParticleData ParticleData;
    [[bg3::hidden]] uint64_t Unknown;
    uint32_t DynamicParametersType;
    FloatKeyFrameProperty* DynamicParameterValueProperty;
    FloatProperty* DynamicParameterModifierXProperty;
    FloatProperty* DynamicParameterModifierYProperty;
    FloatProperty* DynamicParameterModifierZProperty;
    FloatProperty* DynamicParameterModifierWProperty;
    glm::vec4 DynamicParameterVec;
    uint32_t ColorType;
    ColorARGBKeyFrameProperty* ColorProperty;
    FloatKeyFrameProperty* BrightnessProperty;
    FloatKeyFrameProperty* AlphaProperty;
    uint32_t ScaleType;
    FloatKeyFrameProperty* ScaleProperty;
    FloatProperty* ScaleModifierProperty;
    float field_160;
    uint32_t field_164;
    glm::vec3 InitialVelocity;
    bool UseObjectOrientation;
    glm::quat ExtraRotation;
    bool InfiniteParticleLife;
    float ParticleLife;
    EntityHandle VisualEntity;
    Visual* Visuals;
    RenderableObject* Effect;
    uint32_t MaxParticleCount;
    float EmitRate;
    float EmitDistance;
    float UpdateTime;
    float field_1B8;
    float UnitTime;
    bool InvertTexcoord;
    bool ConnectToEmitter;
    glm::vec3 field_1C4;
    glm::vec3 Velocity;
    glm::vec3 Translate;
    glm::quat Rotation;
    uint32_t Flags;
    [[bg3::hidden]] std::array<RibbonParticleHolder, 6> ParticleHolders;
    Array<Visual*> DebugShapes;
    Array<Visual*> DebugShapes2;
    glm::vec3 BoundMin;
    glm::vec3 BoundMax;
};

struct SoundComponent : public FxBaseComponent
{
    [[bg3::hidden]] void* App;
    [[bg3::hidden]] void* TransformSystem;
    [[bg3::hidden]] void* ResourceManager;
    [[bg3::hidden]] void* SoundManager;
    [[bg3::hidden]] void* SoundEvent;
    [[bg3::hidden]] void* SoundEvent2;
    [[bg3::hidden]] void* SoundEvent3;
    FixedString SoundEventName;
    FixedString SoundEventName2;
    FixedString SoundEventName3;
    int32_t PlayOn;
    uint8_t Flags;
    uint8_t Flags2;
};

struct SpinForceComponent : public FxBaseComponent
{
    FloatKeyFrameProperty* InfluenceRadiusProperty;
    FloatKeyFrameProperty* KeyframedOffsetProperty;
    FloatKeyFrameProperty* SpinVelocityProperty;
    glm::vec3 Offset;
    glm::vec3 SpinAxis;
    bool InfluenceRadiusApplied;
    float InfluenceRadius;
    glm::vec3 Position;
    glm::vec3 KeyframedPosition;
};

struct TurbulenceForceComponent : public FxBaseComponent
{
    FloatKeyFrameProperty* InfluenceRadiusProperty;
    FloatKeyFrameProperty* KeyframedOffsetProperty;
    FloatKeyFrameProperty* AccelerationProperty;
    glm::vec3 Offset;
    bool InfluenceRadiusApplied;
    float InfluenceRadius;
    glm::vec3 Position;
    glm::vec3 KeyframedPosition;
};

struct VortexForceComponent : public FxBaseComponent
{
    FloatKeyFrameProperty* InfluenceRadiusProperty;
    FloatKeyFrameProperty* KeyframedOffsetProperty;
    FloatKeyFrameProperty* SpinVelocityProperty;
    glm::vec3 Offset;
    glm::vec3 RotationAxis;
    bool InfluenceRadiusApplied;
    float InfluenceRadius;
    glm::vec3 Position;
    glm::vec3 KeyframedPosition;
};

struct WindForceComponent : public FxBaseComponent
{
    aspk::FloatKeyFrameProperty* SpeedMultiplier;
    float UpdateTime;
};


struct Timeline : public ProtectedGameObject<Timeline>
{
    virtual ~Timeline() = 0;
    virtual void Play() = 0;
    virtual bool IsStopped() const = 0;
    virtual void Clear() = 0;
    virtual void Reset(bool) = 0;
    virtual void PostUpdate() = 0;
    virtual void SetTargetVisual(uint64_t) = 0;
    virtual void ResetTargetVisual(uint64_t) = 0;
    virtual void Stop() = 0;
    virtual void InitializeInternal() = 0;
    virtual bool IsPhaseValid() const = 0;
    virtual void UpdateTargetVisual() const = 0;
    virtual void UpdateTargetVisual2() const = 0;
    virtual void SortComponents() const = 0;
    virtual void UpdateComponents(float) const = 0;
    virtual bool IsComponentInPhase(Component*, uint64_t) const = 0;
    virtual void ResetComponents() = 0;

    uint64_t PhaseIndex;
    TimelineHeader* Header;
    Array<Component*>* Components;
    uint8_t State;
    bool PlayToEnd;
    bool IsPaused;
    bool field_23;
    bool IsHoldingAtPhaseEnd;
    bool NeedsPostUpdate_M;
    float PlayingSpeed;
    float TimePlayed;
    float Duration;
    float RemainingPhaseTime;
    float JumpToTime;
    int JumpToPhase;
    int CurrentPlayCount;
    Array<Input*> Inputs;
};


struct EffectTimeline : public Timeline
{
    // This is already exposed in aspk::Timeline
    [[bg3::hidden]] TimelineHeader Header;
    // This is already exposed in aspk::Timeline
    [[bg3::hidden]] Array<Component*> Components;
};


#if defined(BG3SE_LINUX)
// PORT (linux): the 49 aspk timeline records that add no member of their own, measured
// against bin/bg3 (c8da62b1).  Nothing in them can move -- the whole claim is that the game
// allocates the base's 0x98 as well -- so each carries one sizeof assert and no offsets.
//
//   The two intermediates are ONE class each in the game.  Upstream models
//   TLEventKeyComponent<Key> and TLInterpolationKeyComponent<Key> as templates; bin/bg3 has a
//   single ls::TLEventKeyComponent and a single ls::TLInterpolationKeyComponent, because
//   TLKeyBaseComponent holds `Array<Key*> Keys` and its layout does not depend on Key.  Both
//   have a real vtable and a real constructor:
//     ls::TLEventKeyComponent         ctor 0x30a0220, vtable 0x7ac4cf0
//     ls::TLInterpolationKeyComponent ctor 0x3ba9520, vtable 0x7ac5230
//   Each ctor stores the TLKeyBaseComponent vtable 0x7ac5340 first, writes the TypeId dword at
//   0x54 from its argument, calls 0x77e88b0 (the RNG) and stores the result into the two dwords
//   at 0x8c and 0x90 -- `Random` -- then stores its own vtable and returns.  Neither writes
//   anything at or above 0x94.  That is what fixes the end of the base chain at 0x94 and
//   sizeof at 0x98; both vtables are named by the family's GetTypeName slot
//   (TLFS::strTLEventKeyComponent / strTLInterpolationKeyComponent).
//
//   The sixteen leaves are allocated 0x98 apiece, each `mov edi, 0x98; call operator new`
//   followed by the leaf's own vtable store and no store at or above 0x94.  Fifteen of the
//   sixteen sites are arms of the big TL factory bin/bg3 0x309cbc0; the vtable is named by its
//   GetTypeName slot:
//     TLAttachToEventComponent        ls::TLAttachToEvent        vtable 0x7ac3e30  0x309eeaa
//     TLCameraFoVComponent            ls::TLCameraFoV            vtable 0x7ac46f0  0x309eaad
//     TLCameraLookAtComponent         ls::TLCameraLookAt         vtable 0x7ac4840  0x309ecbe
//     TLEffectPhaseEventComponent     ls::TLEffectPhaseEvent     vtable 0x7ac4a20  0x309eb9f
//     TLEmotionEventComponent         ls::TLEmotionEvent         vtable 0x7ac4b70  0x309ce1c
//     TLGenomeTextEventComponent      ls::TLGenomeTextEvent      vtable 0x7ac4f90  0x309ee82
//     TLHandsIKComponent              ls::TLHandsIK              vtable 0x7ac50e0  0x309eb17
//     TLLookAtEventComponent          ls::TLLookAtEvent          vtable 0x7ac5490  0x309cd77
//     TLPhysicsComponent              ls::TLPhysics              vtable 0x7ac5e98  0x309d46e
//     TLPlayRateComponent             ls::TLPlayRate             vtable 0x7ac6138  0x309ee2a (and 0x66766eb)
//     TLShapeShiftComponent           ls::TLShapeShift           vtable 0x7ac62b8  0x309ec3d
//     TLShowPeanutsComponent          ls::TLShowPeanuts          vtable 0x7ac69d0  0x309ee57
//     TLShowVisualComponent           ls::TLShowVisual           vtable 0x7ac6b20  0x309dcfc
//     TLShowWeaponComponent           ls::TLShowWeapon           vtable 0x7ac6c70  0x309e05a
//     TLSteppingFadeComponent         ls::TLSteppingFade         vtable 0x7ac73a8  0x309ef90
//     TLSwitchLocationEventComponent  ls::TLSwitchLocationEvent  vtable 0x7ac74c8  0x309ebfe
//
//   sizeof is an ABI witness for none of these: an empty derived class has the base's size
//   under both ABIs, so 0x98 says the mirror is neither truncated nor padded, not that the
//   packing is Itanium's.  The ABI witness for the family is its base's, which is the 0x94
//   offsets of the nine leaves measured above.
//   The negative controls are in the group already: eight siblings of these sixteen DO add a
//   member and the game allocates more for each -- 0xa0 for ls::TLShowArmorChannel (0x30943bc)
//   and ls::TLSplatterChannel (0x3ba90a2), 0xa8 for ls::TLFloatRTPC (0x3098966),
//   ls::TLSoundEvent (0x309e154) and ls::TLSwitchStageEvent (0x309e966), 0xb0 for
//   ls::TLAtmosphereAndLightingChannel (0x3ba95eb).  So "the game allocates 0x98" is a
//   measurement of these sixteen and not of the instrument.
//
//   TLShowHUDComponent gets NO assert: there is no ls::TLShowHUD in this build at all -- see
//   keys::ShowHUDKey's row, where `TLShowHUD` occurs 0 times as a string in bin/bg3, in the
//   macOS oracle, in the Windows bg3.exe of the same build and in the toolkit's exports.  That it
//   is also unobtainable from Lua is a *consequence*: GameHelpers.cpp reaches the mirror only
//   through `componentType == "TLShowHUD"`, a compare against the game object's own
//   GetTypeName(), so the branch is dead.
static_assert(sizeof(TLEventKeyComponent<keys::KeyBase>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::EffectPhaseEventKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::EmotionEventKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::GenomeTextEventKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::HandsIKKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::LookAtEventKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::PhysicsKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::PlayEffectEventKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::ShapeShiftKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::ShowArmorChannelKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::ShowHUDKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::ShowPeanutsKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::ShowVisualKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::ShowWeaponKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::SoundEventKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::SpringsKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::SteppingFadeKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::SwitchLocationEventKey>) == 0x98);
static_assert(sizeof(TLEventKeyComponent<keys::SwitchStageEventKey>) == 0x98);

static_assert(sizeof(TLInterpolationKeyComponent<keys::KeyBase>) == 0x98);
static_assert(sizeof(TLInterpolationKeyComponent<keys::AtmosphereAndLightingChannelKey>) == 0x98);
static_assert(sizeof(TLInterpolationKeyComponent<keys::AttachToEventKey>) == 0x98);
static_assert(sizeof(TLInterpolationKeyComponent<keys::CameraDoFChannelKey>) == 0x98);
static_assert(sizeof(TLInterpolationKeyComponent<keys::CameraFoVKey>) == 0x98);
static_assert(sizeof(TLInterpolationKeyComponent<keys::CameraLookAtKey>) == 0x98);
static_assert(sizeof(TLInterpolationKeyComponent<keys::FloatRTPCKey>) == 0x98);
static_assert(sizeof(TLInterpolationKeyComponent<keys::MaterialTextureKeyKey>) == 0x98);
static_assert(sizeof(TLInterpolationKeyComponent<keys::SplatterChannelKey>) == 0x98);
static_assert(sizeof(TLInterpolationKeyComponent<keys::TransformChannelFloatKey>) == 0x98);
static_assert(sizeof(TLInterpolationKeyComponent<keys::TransformChannelFrameOfReferenceKey>) == 0x98);
static_assert(sizeof(TLInterpolationKeyComponent<keys::TransformChannelQuatKey>) == 0x98);
static_assert(sizeof(TLInterpolationKeyComponent<keys::ValueKey<float>>) == 0x98);

static_assert(sizeof(TLAttachToEventComponent) == 0x98);
static_assert(sizeof(TLCameraFoVComponent) == 0x98);
static_assert(sizeof(TLCameraLookAtComponent) == 0x98);
static_assert(sizeof(TLEffectPhaseEventComponent) == 0x98);
static_assert(sizeof(TLEmotionEventComponent) == 0x98);
static_assert(sizeof(TLGenomeTextEventComponent) == 0x98);
static_assert(sizeof(TLHandsIKComponent) == 0x98);
static_assert(sizeof(TLLookAtEventComponent) == 0x98);
static_assert(sizeof(TLPhysicsComponent) == 0x98);
static_assert(sizeof(TLPlayRateComponent) == 0x98);
static_assert(sizeof(TLShapeShiftComponent) == 0x98);
static_assert(sizeof(TLShowPeanutsComponent) == 0x98);
static_assert(sizeof(TLShowVisualComponent) == 0x98);
static_assert(sizeof(TLShowWeaponComponent) == 0x98);
static_assert(sizeof(TLSteppingFadeComponent) == 0x98);
static_assert(sizeof(TLSwitchLocationEventComponent) == 0x98);
#endif


#if defined(BG3SE_LINUX)
// PORT (linux): four of the eight aspk timeline records that DO add a member, measured against
// bin/bg3 (c8da62b1).  All four are affected-only (their first derived member is 8-aligned, so it
// starts at the base's 0x98 under both ABIs) and all four match upstream -- but each is
// measured on its own function rather than batched off the base.
//
//   TLFloatRTPCComponent = ls::TLFloatRTPC, vtable 0x7ac4e00, allocated 0xa8 twice
//     (bin/bg3 0x3098966 inside ls::TLSoundEvent::Visit, and 0x746333c).  Both allocators store
//     `movabs rcx, 0xffc0000000000000; mov [r14+0x98], rcx` -- a non-zero qword, so field_b0 is a
//     real 8-byte member (the high half is -NaN as a float) -- then `mov dword [r14+0xa0],
//     0xffffffff` (a FixedString's empty value), `mov word [r14+0xa4], 0` and
//     `mov byte [r14+0xa6], 1`.  ls::TLFloatRTPC::Clone 0x3094300 types the rest: it hands
//     [rsi+0xa0] to the FixedString acquire 0x3883660 -- which NAMES RTPCName as a FixedString --
//     and then copies three separate bytes, 0xa5, 0xa4 and 0xa6.  It does not copy 0x98, so
//     field_b0 is runtime state.  Note the Clone copies ONE byte at 0xa6 where the mirror
//     declares `__int16 field_be`: nothing moves either way (sizeof is 0xa8 both ways) and 0xa7
//     has no witness, so the declaration is left alone.
//
//   TLSoundEventComponent = ls::TLSoundEvent, vtable 0x7ac6dc0 (34 slots), allocated 0xa8 at
//     bin/bg3 0x309e154.  RTPCChannels at 0x98 is witnessed three ways: the allocator's
//     `movups [rbx+0x98], xmm0` zeroes the whole 16-byte Array; ls::TLSoundEvent::Clone 0x30995d0
//     reads the buffer as `qword [r14+0x98]`, indexes it `[rcx + r15*8]` and uses
//     `dword [rbx+0xa4]` as the count, which is Array<T*>'s buf/capacity/size at 0x98/0xa0/0xa4;
//     and vtable slot 33, 0x7463330, allocates a TLFloatRTPCComponent and pushes it with
//     `add rbx, 0x98; call 0x74633d0`, so the array's element type is named too.
//
//   TLSwitchStageEventComponent = ls::TLSwitchStageEvent, vtable 0x7ac7628 (the class carries
//     symbols: ls::TLSwitchStageEvent::UpdateInternal 0x2e67760, ::CreateKeyInternal 0x3afd700,
//     ::GetTypeName 0x7463dd0), allocated 0xa8 at bin/bg3 0x309e966.  UpdateInternal is the
//     witness: it loads the timeline owner's 16 bytes at `[rsi+0x478]` and saves them with
//     `movups [rdi+0x98], xmm0` (0x2e67827) before overwriting the owner with the key's Guid from
//     `[rcx+0x10]`.  So 0x98..0xa8 is one 16-byte Guid -- the previous stage id -- which is
//     exactly the extent of the mirror's field_b0 + field_b8 pair.  The split between the two
//     halves at 0xa0 has NO witness of its own and gets no assert.
//
//   channels::TLAtmosphereAndLightingChannelComponent = ls::TLAtmosphereAndLightingChannel,
//     vtable 0x7ac3bd8, allocated 0xb0 at bin/bg3 0x3ba95eb.  Vtable slot 30, 0x745f990, is the
//     whole derived block in six instructions: it copies the key's Guid (`[rax+0x10]`, the key is
//     0x28 bytes with its Guid at 0x10 -- CreateKeyInternal 0x3ba72b0) to `[rdi+0x98]` with
//     movups, the key's FadeTime float (`[rax+0x20]`) to `[rdi+0xa8]` with movss, and then
//     `mov byte [rdi+0xac], 1` -- a non-zero value store.  So the game's members are
//     {Guid, float, bool} at 0x98 / 0xa8 / 0xac where upstream declares three __int64; the extent
//     and sizeof are identical, so the mirror reads correctly as it is, but anyone exposing
//     field_b8 or field_c0 to Lua should read them as the Guid's high half and the float+flag pair.
//     field_b8 at 0xa0 is inside the Guid and gets no assert.
//
//   sizeof discriminates for none of the four: each one's first derived member is 8-aligned, so
//   MSVC lands on the same offsets and the same size.  The witness is the offsets.
static_assert(offsetof(TLFloatRTPCComponent, field_b0) == 0x98);
static_assert(offsetof(TLFloatRTPCComponent, RTPCName) == 0xa0);
static_assert(offsetof(TLFloatRTPCComponent, SoundType) == 0xa4);
static_assert(offsetof(TLFloatRTPCComponent, SoundObjectIndex) == 0xa5);
static_assert(offsetof(TLFloatRTPCComponent, field_be) == 0xa6);
static_assert(sizeof(TLFloatRTPCComponent) == 0xa8);

static_assert(offsetof(TLSoundEventComponent, RTPCChannels) == 0x98);
static_assert(sizeof(TLSoundEventComponent) == 0xa8);

static_assert(offsetof(TLSwitchStageEventComponent, field_b0) == 0x98);
static_assert(sizeof(TLSwitchStageEventComponent) == 0xa8);

static_assert(offsetof(channels::TLAtmosphereAndLightingChannelComponent, field_b0) == 0x98);
static_assert(offsetof(channels::TLAtmosphereAndLightingChannelComponent, field_c0) == 0xa8);
static_assert(sizeof(channels::TLAtmosphereAndLightingChannelComponent) == 0xb0);
#endif


#if defined(BG3SE_LINUX)
// PORT (linux): the other four of the eight aspk timeline records that add a member, measured
// against bin/bg3 (c8da62b1).  Three of them are mirror defects and are fixed above; the fourth
// is right and was simply unnamed by the family's GetTypeName scan.
//
//   channels::TLMaterialTextureKeyComponent = vtable 0x7ac5bb8, allocated 0xa8 at bin/bg3
//     0x7460f79 by the ctor of TLMaterialComponent::TextureParameter (0x7460f50: it writes the
//     FixedString -1 at 0x8, the qword at 0x10, its own vtable, the byte at 0x18, then allocates
//     the channel and stores it at [this+0x20]).  Its own GetTypeName slot returns
//     TLFS::strTLInterpolationKeyComponent, NOT a name of its own, which is why the family's
//     GetTypeName census does not name it -- worth carrying: a class that
//     does not override GetTypeName shows up under its base's name, and the way to find it is its
//     creator.  The derived block is one 16-byte Guid at 0x98: vtable slot 30 (0x7461030) copies
//     the key's 16 bytes from [rax+0x10] to [rdi+0x98] with movups, and slot 28
//     (CreateKeyInternal, 0x7461000) allocates 0x20 for the key and puts ls::Guid::s_Empty at its
//     +0x10 -- which is exactly keys::MaterialTextureKeyKey {KeyBase, Guid Value}.  So the mirror's
//     field_b0 + field_b8 are that Guid's two halves; the split at 0xa0 has no witness and gets no
//     assert.
//
//   channels::TLShowArmorChannelComponent -- FIXED.  vtable 0x7ac6798, allocated 0xa0 at bin/bg3
//     0x30943bc.  Vtable slot 30 (0x7462cd0) copies the key's Value from byte [rax+0xd] to
//     byte [rdi+0x94] and then stores `mov byte [rdi+0x95], 1`, a non-zero value store; the ctor
//     0x3094350 stores `mov dword [r14+0x98], r13d` with r13d = 1 doubling per channel over the
//     eleven channels the loop builds (`cmp ebp, 0xb` at 0x3094464), so 0x98 is a uint32 bit.
//     Upstream had one __int64 at 0x98 for all three.
//
//   channels::TLSplatterChannelComponent -- FIXED.  vtable 0x7ac70a8, allocated 0xa0 at bin/bg3
//     0x3ba90a2 and 0x3ba9340 (two arms of ls::TLSplatter's ctor 0x3ba9050).  Vtable slot 30
//     (0x7463930) writes `mov byte [rdi+0x9c], 1`, copies the key's SplatterChangeMode from
//     byte [rax+0x14] to byte [rdi+0x94] and the key's Value from [rax+0x10] to [rdi+0x98] with
//     movss.  ls::TLSplatterChannel::Clone 0x7463890 copies ONLY the byte at 0x9d, and ::Visit
//     0x74638c0 visits that byte through the visitor's uint8 slot [rax+0xb8] with its own
//     FixedString (0x7d249f0) -- so SplatterType is at 0x9d, not 0xa0, and the second arm's
//     `mov word [r15+0x9c], 0x100` (0x3ba9145) is a non-zero store landing on it.
//     Upstream had `__int64 field_b0` + SplatterType at 0xa0 and sizeof 0xa8.
//
//   TLPlayEffectEventComponent -- FIXED.  vtable 0x7ac5fe8 (GetTypeName 0x7462580 ->
//     TLFS::strTLPlayEffectEvent), allocated 0x98 at bin/bg3 0x309e0c9, i.e. the base's size: the
//     factory arm writes `mov word [rbx+0x94], r14w` after the vtable store and nothing else, so
//     the record's own members are two bytes at 0x94.  Upstream's `__int64 field_b0` at 0x98 read
//     eight bytes past the end of the allocation.  The class overrides no Clone, Visit or
//     key-apply slot (slot 23 is the 3-byte `xor eax,eax; ret` 0x668a0e0), so the two bytes get no
//     name.
//
//   sizeof discriminates for three of the four.  TLPlayEffectEventComponent: MSVC would put two
//   bytes at 0x98 and land on 0xa0, the game is 0x98.  TLShowArmorChannelComponent: MSVC would put
//   bool/bool/uint32 at 0x98/0x99/0x9c and land on 0xa0 too, so only the offsets discriminate
//   there.  TLSplatterChannelComponent: MSVC would land on 0xa8, the game is 0xa0.
//   TLMaterialTextureKeyComponent is 8-aligned at 0x98 and identical under both.
static_assert(offsetof(channels::TLMaterialTextureKeyComponent, field_b0) == 0x98);
static_assert(sizeof(channels::TLMaterialTextureKeyComponent) == 0xa8);

static_assert(offsetof(channels::TLShowArmorChannelComponent, defaultVal) == 0x94);
static_assert(offsetof(channels::TLShowArmorChannelComponent, field_ad) == 0x95);
static_assert(offsetof(channels::TLShowArmorChannelComponent, field_b0) == 0x98);
static_assert(sizeof(channels::TLShowArmorChannelComponent) == 0xa0);

static_assert(offsetof(channels::TLSplatterChannelComponent, defaultChangeMode) == 0x94);
static_assert(offsetof(channels::TLSplatterChannelComponent, defaultVal) == 0x98);
static_assert(offsetof(channels::TLSplatterChannelComponent, field_b4) == 0x9c);
static_assert(offsetof(channels::TLSplatterChannelComponent, SplatterType) == 0x9d);
static_assert(sizeof(channels::TLSplatterChannelComponent) == 0xa0);

static_assert(offsetof(TLPlayEffectEventComponent, field_b0) == 0x94);
static_assert(sizeof(TLPlayEffectEventComponent) == 0x98);
#endif

END_NS()

BEGIN_NS(lua)

LUA_POLYMORPHIC(aspk::Property);
LUA_POLYMORPHIC(aspk::KeyFrameData);
LUA_POLYMORPHIC(aspk::Component);
LUA_POLYMORPHIC(aspk::TLMaterialComponent::Parameter);

END_NS()
