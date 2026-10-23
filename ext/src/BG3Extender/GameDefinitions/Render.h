#pragma once

#include <GameDefinitions/AllSparkShared.h>
#include <GameDefinitions/MaterialParameters.h>
#include <GameDefinitions/Resources.h>

BEGIN_SE()

struct MoveableObject;

struct [[bg3::hidden]] Pose : ProtectedGameObject<Pose>
{
    glm::mat3x4* Pose1;
    glm::mat3x4* Pose2;
    uint16_t BoneCount;
};

struct MeshBinding : ProtectedGameObject<MeshBinding>
{
    [[bg3::hidden]] void* VMT;
    [[bg3::hidden]] void* field_8;
    [[bg3::hidden]] Pose Pose;
    MoveableObject* Object;
    glm::mat4 Transform;
    AABound Bound1;
    AABound Bound2;
};

struct LocalTransform
{
    Transform LocalTransform;
    AABound LocalBound;
};


// PORT (linux): the virtual list below is MSVC's and does NOT match this build's vtable past
// slot 5.  Left as upstream has it, because nothing in ext/ calls any of them --
// the three live callers are LuaSetWorldTranslate/Rotate/Scale at slots 3, 4 and 5, which agree.
// Read this before adding a caller or a subclass virtual.  bin/bg3's ls::MoveableObject vtable
// is the run at 0x7aba048, 21 slots; ls::RenderableObject's is 0x7aba370, 37 = 21 + its own 16,
// and all 21 base slots are matched to the macOS oracle's __ZTVN2ls16RenderableObjectE
// (the oracle's _ZTV run slot by slot, dumps/match/a187af943be2_af3f1c711ba2/matches.tsv).
// The game's order is:
//
//    0 GetRTTI                          11 SetLocalTransform(vec3, quat, vec3)
//    1 ~MoveableObject [D1]             12 SetLocalTransform(Transform const&)
//    2 ~MoveableObject [D0]             13 SetTextureLOD(u8, u8, ls::aio::Priority) const
//    3 SetWorldTranslate                14 GetCullFlags() const            -- no argument
//    4 SetWorldRotate                   15 SetVisibleForRenderView(GameRenderView const*, bool)
//    5 SetWorldScale                    16 SetVisibleForAllRenderViews(bool)
//    6 SetWorldTransform(vec3,quat,vec3) 17 AttachToScene(Scene*)
//    7 SetWorldTransform(Transform&)    18 DetachFromScene()
//    8 SetLocalTranslate                19 IsAttachedToScene() const
//    9 SetLocalRotate                   20 IsInitialized() const
//   10 SetLocalScale
//
// So: both SetWorldTransform and both SetLocalTransform overloads are swapped (MSVC groups
// overloads at the first declaration in reverse order; Itanium keeps declaration order), there
// is no CalculateTextureLOD slot at all, GetCullFlags takes nothing, SetSomeCullFlag and
// ToggleCullFlag600 are SetVisibleForRenderView and SetVisibleForAllRenderViews, and the
// GetDebugName/GetStrDebugName/GetDebugInfo tail does not exist here -- 24 declared against the
// game's 21, so every RenderableObject/Visual virtual is three slots late in this mirror.
struct MoveableObject : ProtectedGameObject<MoveableObject>
{
    static constexpr uint32_t StaticRTTI = 0x1;

    virtual uint32_t GetRTTI() = 0;
    virtual ~MoveableObject() = 0;

    virtual void SetWorldTranslate(glm::vec3 const& translate) = 0;
    virtual void SetWorldRotate(glm::quat const& rotate) = 0;
    virtual void SetWorldScale(glm::vec3 const& scale) = 0;
    virtual void SetWorldTransform(Transform const& transform) = 0;
    virtual void SetWorldTransform(glm::vec3 const& translate, glm::quat const& rotate, glm::vec3 const& scale) = 0;

    virtual void SetLocalTranslate(glm::vec3 const& translate) = 0;
    virtual void SetLocalRotate(glm::quat const& rotate) = 0;
    virtual void SetLocalScale(glm::vec3 const& scale) = 0;
    virtual void SetLocalTransform(Transform const& mat) = 0;
    virtual void SetLocalTransform(glm::vec3 const& translate, glm::quat const& rotate, glm::vec3 const& scale) = 0;

    virtual uint8_t CalculateTextureLOD(float*, glm::vec3 const& translate, float, uint8_t) = 0;
    virtual void SetTextureLOD(uint8_t, uint8_t, uint64_t) = 0;
    virtual uint16_t GetCullFlags(uint16_t&) = 0;
    virtual void SetSomeCullFlag(void*, bool) = 0;
    virtual void ToggleCullFlag600(bool cull) = 0;
    virtual bool AttachToScene(Scene* scene) = 0;
    virtual bool DetachFromScene() = 0;
    virtual bool IsAttachedToScene() = 0;
    virtual char const* GetDebugName() const = 0;
    virtual StringView GetStrDebugName() = 0;
    virtual void GetDebugInfo(ScratchString &) const = 0;

    Transform WorldTransform;
    LocalTransform* LocalTransform;
    [[bg3::hidden]] Scene* Scene;
    AABound WorldBound;
    AABound BaseBound;
    float MinLODDistance;
    float MaxLODDistance;
    [[bg3::readonly]] int SceneIndex;

    // Lua call helpers
    //# P_FUN(SetWorldTranslate, MoveableObject::LuaSetWorldTranslate)
    //# P_FUN(SetWorldRotate, MoveableObject::LuaSetWorldRotate)
    //# P_FUN(SetWorldScale, MoveableObject::LuaSetWorldScale)
    void LuaSetWorldTranslate(glm::vec3 const& translate);
    void LuaSetWorldRotate(glm::quat const& rotate);
    void LuaSetWorldScale(glm::vec3 const& scale);
};

struct SceneObjectBounds
{
    Array<float> BoundMinX;
    Array<float> BoundMinY;
    Array<float> BoundMinZ;
    Array<float> BoundMaxX;
    Array<float> BoundMaxY;
    Array<float> BoundMaxZ;
};

struct SceneObjects
{
    SceneObjectBounds WorldBound;
    Array<float> BoundRadius;
    Array<float> MinLODDistanceSquared;
    Array<float> MaxLODDistanceSquared;
    SceneObjectBounds RootBound;
    SceneObjectBounds HLODBound;
    Array<bool> field_150;
    Array<MoveableObject*> Objects;
    Array<uint32_t> CullFlags;
    Array<RenderPass> RenderPasses;
};


struct Scene : public ProtectedGameObject<Scene>
{
    EntityHandle field_0;
    [[bg3::readonly]] bool Active;
    // Editor only
    // [[bg3::hidden]] void* SceneViewMode;
    [[bg3::hidden]] CRITICAL_SECTION DirtyListCS;
    [[bg3::hidden]] Array<MoveableObject*> DirtyList;
    [[bg3::hidden]] LevelBase* Level;
    [[bg3::hidden]] bool CullRegions;
    // PORT (linux): the game's ls::SRWLock, not Win32's 8-byte one (shim/windows.h).
    [[bg3::hidden]] LS_SRWLOCK SRWLock;
    [[bg3::hidden]] std::array<SceneObjects, 11> ObjectsByType;
    [[bg3::hidden]] Array<MoveableObject*> DistantLightProbes;
    [[bg3::hidden]] HashSet<MoveableObject*> field_11A8;
};

#if defined(BG3SE_LINUX)
// PORT (linux): witnessed by the game's own component registry. ls::Scene is a proxy
// component, so ValidateMappedComponentSizes() compares sizeof(SceneComponent) against the
// registry's TotalSize - InlineSize; with SRWLock at Win32's 8 bytes it says
// "'ls::Scene' OOB size mismatch: local 4568, ECS 4616".
// Giving that one member the game's 56-byte lock adds exactly the missing 48 and nothing else
// changes, which is why the number is evidence rather than a fit: 4568 + (0x38 - 8) = 4616.
// Derived from sizeof(LS_SRWLOCK) rather than hand-counted.
static_assert(sizeof(Scene) == 4616, "ls::Scene is 4616 bytes (the game's registry)");
#endif

struct ShaderParamBinding
{
    int8_t DxVsIndex{ -1 };
    int8_t DxPsIndex{ -1 };
    int8_t VkDescriptorSet{ -1 };
    int8_t VkBindingIndex{ -1 };
};

struct MaterialShaderDecalParameters
{
    int16_t DecalWorld;
    int16_t DecalDimensions;
    int16_t DecalTilingAndOffset;
    int16_t DecalDeferredProperties;
    int16_t InvWorldMatrix;
    int16_t NormalMatrix;
};

struct MaterialShaderDesc
{
    MaterialShaderDescFlags EngineParamFlags;
    uint16_t EngineCBSize;
    uint16_t MaterialCBSize;
    ShaderParamBinding EngineCBBinding;
    ShaderParamBinding MaterialCBBinding;
    int16_t WorldMatrixCurr;
    int16_t WorldMatrixPrev;
    int16_t WorldCurr;
    int16_t WorldPrev;
    int16_t TransformedVerticesOffsetCurr;
    int16_t TransformedVerticesOffsetPrev;
    int16_t MeshRandom;
    int16_t WorldBoundsMinSubVisual;
    int16_t WorldBoundsMinVisual;
    int16_t WorldBoundsMinParent;
    int16_t WorldBoundsMaxSubVisual;
    int16_t WorldBoundsMaxVisual;
    int16_t WorldBoundsMaxParent;
    int16_t LocalBoundsMin;
    int16_t LocalBoundsMax;
    int16_t ReceiveDecal;
    int16_t FadeOpacity;
    int16_t LightChannel;
    int16_t DiffusionProfileIndex;
    int16_t VirtualTexture_TilesetDataIndex;
    int16_t VirtualTexture_CB_Texture;
    MaterialShaderDecalParameters* DecalParameters;
};

struct UniformBindingData
{
    FixedString UniformName;
    std::array<int16_t, 15> PerShaderCBOffsets{ -1 };
};

struct TextureBindingData
{
    FixedString UniformName;
    std::array<ShaderParamBinding, 15> PerShaderBindings;
};

struct VirtualTextureShaderParamBinding
{
    ShaderParamBinding Cache;
    ShaderParamBinding Cache1;
    ShaderParamBinding Cache2;
    ShaderParamBinding Cache3;
};
    
struct VirtualTextureBindingData
{
    FixedString UniformName;
    std::array<ShaderParamBinding, 15> PerShaderBindings;
    std::array<VirtualTextureShaderParamBinding, 15> PerShaderVTBindings;
};

struct ScalarParameterInstance : public material::ScalarResourceParameter
{
    [[bg3::hidden]] UniformBindingData Binding;
};

struct Vector2ParameterInstance : public material::Vector2ResourceParameter
{
    [[bg3::hidden]] UniformBindingData Binding;
};

struct Vector3ParameterInstance : public material::Vector3ResourceParameter
{
    [[bg3::hidden]] UniformBindingData Binding;
};

struct Vector4ParameterInstance : public material::Vector4ResourceParameter
{
    [[bg3::hidden]] UniformBindingData Binding;
    [[bg3::hidden]] uint64_t _Padding3;
};

struct Texture2DParameterInstance : public material::Texture2DResourceParameter
{
    [[bg3::hidden]] TextureBindingData Binding;
};

struct SamplerStateParameterInstance : public material::ResourceParameter
{
    uint8_t TextureFilterOverride;
    uint8_t TextureAddressMode;
    [[bg3::hidden]] TextureBindingData Binding;
};

struct VirtualTextureParameterInstance : public material::VirtualTextureResourceParameter
{
    [[bg3::hidden]] VirtualTextureBindingData Binding;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the seven material parameter instances.  The game calls them
// ls::Material<Kind>Parameter (upstream's "Instance" suffix is upstream's), and two of them
// are flagged: SamplerStateParameterInstance puts TextureFilterOverride at 5, inside
// material::ResourceParameter's tail padding (ParameterName + Enabled end at 5, sizeof 8),
// and Vector4ParameterInstance puts Binding at 0x34, inside
// material::Vector4ResourceParameter's (IsColor ends at 0x31, sizeof 0x40).
//
// sizeof IS the ABI witness for both, and was checked to be one before it was used (the
// stats::Functor rule): MSVC would make the sampler 0x4c or 0x50 rather than 0x48, and the
// vector4 0x70 rather than 0x60.  It also pins every offset, because each record is a base
// plus one fixed-size binding block plus at most two bytes.  The five siblings that are NOT
// flagged are the family's built-in calibration -- the same instrument read on them has to
// come back with the mirror's numbers, and does, seven for seven.
//
// Witnesses:
//   ls::MaterialParameters::AddVector4Parameter(ls::MaterialVector4Parameter const&),
//     bin/bg3 0x27a5400 (matched) -- `shl rsi, 5; lea rsi, [rsi+rsi*2]`, i.e. index * 0x60,
//     then the element written field by field: dword -1 at +0 (ParameterName), a byte at +4
//     (Enabled), `movaps [.. + 0x10]` and `movaps [.. + 0x20]` (Value and BaseValue, and the
//     ALIGNED move is why glm::aligned_highp is right), a byte at +0x30 (IsColor), and at
//     +0x34 a dword set to -1 and copied from the source -- the Binding's leading
//     FixedString.  The copy loop at 0x27a5503..0x27a5570 repeats all of it.  Measured on
//     the Linux binary itself.
//   ls::LEGACY_Set<ls::MaterialXParameter, ...>::Realloc, macOS oracle -- `mov ecx, <size>;
//     mul rcx` over the element count, for all seven: Scalar 0x34 (0x106ee5f10), Vector2
//     0x3c (0x106ee64d0), Vector3 0x48 (0x106ee6ac0), Vector4 0x60 (0x106ee70f0), Texture2D
//     0x4c (0x106ee7660), SamplerState 0x48 (0x106ee7c40), VirtualTexture 0x140
//     (0x106ee82e0).  Every one equals this mirror's sizeof.
//   ls::Material::Visit, macOS 0x106ed9a20, the sampler arm at 0x106edba8c -- on an element
//     at (rbx + r12 - 8) it writes `dword [-8] = -1` (ParameterName), `byte [-4] = 0`
//     (Enabled), `word [-3] = 0` (the two bytes at +5 and +6), then `add rbx, r12` and four
//     `movdqu` of all-ones at +0, +0x10, +0x20, +0x30 from there, i.e. 0x40 bytes of Binding
//     starting at element+8 and ending at element+0x47.  The serialiser below it visits
//     [-8], [-4], [-3] and [-2] under four separate keys.
static_assert(sizeof(material::ResourceParameter) == 0x8, "ParameterName + Enabled, dsize 5");
static_assert(offsetof(material::ResourceParameter, Enabled) == 0x4, "Visit's `byte [-4] = 0`");
static_assert(offsetof(SamplerStateParameterInstance, TextureFilterOverride) == 0x5, "`word [-3] = 0`");
static_assert(offsetof(SamplerStateParameterInstance, TextureAddressMode) == 0x6, "the second byte of it");
static_assert(sizeof(TextureBindingData) == 0x40, "the four movdqu of all-ones");
static_assert(offsetof(SamplerStateParameterInstance, Binding) == 0x8, "they start at element+8");
static_assert(sizeof(SamplerStateParameterInstance) == 0x48, "LEGACY_Set Realloc, 0x106ee7c40");
static_assert(offsetof(material::Vector4ResourceParameter, Value) == 0x10, "movaps, bin/bg3 0x27a54b0");
static_assert(offsetof(material::Vector4ResourceParameter, BaseValue) == 0x20, "movaps, 0x27a54b6");
static_assert(offsetof(material::Vector4ResourceParameter, IsColor) == 0x30, "the byte at 0x27a54bc");
static_assert(sizeof(UniformBindingData) == 0x24, "Binding runs 0x34..0x58");
static_assert(offsetof(Vector4ParameterInstance, Binding) == 0x34, "the -1 dword, bin/bg3 0x27a54a3 path");
static_assert(offsetof(Vector4ParameterInstance, _Padding3) == 0x58, "so the record ends at 0x60");
static_assert(sizeof(Vector4ParameterInstance) == 0x60, "the element stride, bin/bg3 0x27a5431");
// the five unflagged siblings, the family's calibration
static_assert(sizeof(ScalarParameterInstance) == 0x34, "LEGACY_Set Realloc 0x106ee5f10");
static_assert(sizeof(Vector2ParameterInstance) == 0x3c, "LEGACY_Set Realloc 0x106ee64d0");
static_assert(sizeof(Vector3ParameterInstance) == 0x48, "LEGACY_Set Realloc 0x106ee6ac0");
static_assert(sizeof(Texture2DParameterInstance) == 0x4c, "LEGACY_Set Realloc 0x106ee7660");
static_assert(sizeof(VirtualTextureParameterInstance) == 0x140, "LEGACY_Set Realloc 0x106ee82e0");
#endif

struct MaterialParameters
{
    Material* Material;
    TrackedCompactSet<ScalarParameterInstance> ScalarParameters;
    TrackedCompactSet<Vector2ParameterInstance> Vector2Parameters;
    TrackedCompactSet<Vector3ParameterInstance> Vector3Parameters;
    TrackedCompactSet<Vector4ParameterInstance> VectorParameters;
    TrackedCompactSet<Texture2DParameterInstance> Texture2DParameters;
    TrackedCompactSet<SamplerStateParameterInstance> SamplerStateParameters;
    Array<VirtualTextureParameterInstance> VirtualTextureParameters;
};

struct Material : public ProtectedGameObject<Material>
{
    struct MaterialCB
    {
        [[bg3::hidden]] void* MaterialCB;
        uint64_t MaterialCBSize;
    };

    [[bg3::hidden]] void* VMT;
    FixedString Name;
    [[bg3::hidden]] std::array<MaterialShaderDesc, 15> ShaderDescriptions;
    // PORT (linux): not a kernel rw-lock -- the game's 4-byte userspace spin word
    // (SpinRWLock, Base/Lock.h), which is the one SRW primitive Win32's 8-byte
    // SRWLOCK sizes correctly, so nothing behind it moves.  The type changes
    // because the *operations* have to: the Linux shim's SRWLOCK is a lazily
    // created pthread_rwlock_t*, so AcquireSRWLockShared() here would have written
    // a pointer into a word the game spins on.  Witness block below the struct.
    [[bg3::hidden]] SpinRWLock MaterialCBLock;
    [[bg3::hidden]] std::array<MaterialCB, 15> MaterialCBs;
    std::array<FixedString, 15> Shaders;
    uint32_t ShadingModel;
    uint32_t ForwardLightingMode;
    uint8_t BlendStateID;
    uint8_t RasterizerStateID;
    uint8_t DepthStateID;
    uint32_t StencilRef;
    uint32_t UVCount;
    MaterialFlags Flags;
    MaterialUsedWithFlags UsedWithFlags;
    uint8_t MaterialPassHint;
    MaterialType MaterialType;
    RenderChannel RenderChannel;
    uint8_t MaterialUsage;
    FixedString DiffusionProfileUUID;
    MaterialParameters Parameters;
    Material* Parent;
    // Editor only
    // void* MaterialEditorHelperInternal;
    uint64_t Version64;

    //# P_FUN(SetScalar, Material::SetScalar)
    //# P_FUN(SetVector2, Material::SetVector2)
    //# P_FUN(SetVector3, Material::SetVector3)
    //# P_FUN(SetVector4, Material::SetVector4)
    bool SetScalar(FixedString const& param, float value);
    bool SetVector2(FixedString const& param, glm::vec2 value);
    bool SetVector3(FixedString const& param, glm::vec3 value);
    bool SetVector4(FixedString const& param, glm::vec4 value);

    template <class T>
    void SetUniformParam(UniformBindingData const& binding, T value);

    void* GetOrCreateConstantBuffer(uint8_t shaderIndex);
};

#if defined(BG3SE_LINUX)
// PORT (linux): ls::Material measured on bin/bg3 itself, not on the macOS oracle -- and the
// oracle would have been wrong.  Its ls::Material carries a 4-byte member at +0x504 that the
// Linux build does not have, so every offset from ShadingModel on is 4 higher there and
// Parameters/Parent/Version64 8 higher (macOS ls::Material::Visit 0x106ed9a20 puts UVCount at
// +0x518 and DiffusionProfileUUID at +0x528; the Linux constructor puts them at +0x514 and
// +0x524).  Absolute offsets are asserted here rather than differences because the lock in
// this class is the same size on both platforms -- the "oracle's absolute offsets stop at
// the first lock" caveat is about the kernel locks, and this one is not one.
//
// Witnesses, both read with tools/disasm.py on the build-id c8da62b1 binary:
//   ls::Material::Material(FixedString const&), bin/bg3 0x27a4c30 -- stores the vtable at +0,
//     ls::gst::Acquire(esi) then the FixedString at +8, memset(this + 0x10, 0, 0x3c0) over
//     ShaderDescriptions, memset(this + 0x3d8, 0, 0xf0) over MaterialCBs, four -1 stores
//     covering +0x4c8..+0x504 (Shaders[15]), a zero qword at +0x504, `word [+0x50c] = 0x107`
//     and `byte [+0x50e] = 5` (BlendStateID 7, RasterizerStateID 1, DepthStateID 5), 16 bytes
//     at +0x510 (StencilRef, UVCount, Flags, UsedWithFlags), `qword [+0x520] =
//     0xffffffff00000001` (MaterialPassHint 1 .. MaterialUsage, then a null
//     DiffusionProfileUUID at +0x524), `qword [+0x528] = this` (Parameters.Material, the
//     self-pointer), 0x70 of zeroes over the rest of Parameters up to +0x5a0, Parent = 0 at
//     +0x5a0 and Version64 = 0x80000000000000 at +0x5a8.
//   ls::Material::ClearConstantBufferData, bin/bg3 0x27a5db0 -- takes the spin lock at +0x3d0
//     and frees MaterialCBs[0..14] at +0x3d8 + 0x10*i, the last at +0x4b8.
static_assert(offsetof(Material, ShaderDescriptions) == 0x10, "ls::Material::ShaderDescriptions is at 0x10");
static_assert(sizeof(MaterialShaderDesc) == 0x40, "the ctor memsets 15 of these over 0x3c0 bytes");
static_assert(offsetof(Material, MaterialCBLock) == 0x3d0, "the spin word ClearConstantBufferData takes");
static_assert(offsetof(Material, MaterialCBs) == 0x3d8, "ls::Material::MaterialCBs is at 0x3d8");
static_assert(sizeof(Material::MaterialCB) == 0x10, "MaterialCBs[i] is freed at 0x3d8 + 0x10*i");
static_assert(offsetof(Material, Shaders) == 0x4c8, "ls::Material::Shaders is at 0x4c8");
static_assert(offsetof(Material, ShadingModel) == 0x504, "the zero qword behind Shaders[15]");
static_assert(offsetof(Material, BlendStateID) == 0x50c, "BlendStateID 7 / RasterizerStateID 1 at 0x50c");
static_assert(offsetof(Material, StencilRef) == 0x510, "the 16-byte store at 0x510 starts here");
static_assert(offsetof(Material, MaterialPassHint) == 0x520, "MaterialPassHint 1 at 0x520");
static_assert(offsetof(Material, DiffusionProfileUUID) == 0x524, "the null FixedString at 0x524");
static_assert(offsetof(Material, Parameters) == 0x528, "the ctor's self-pointer store");
static_assert(sizeof(MaterialParameters) == 0x78, "Parameters runs 0x528..0x5a0");
static_assert(offsetof(Material, Parent) == 0x5a0, "ls::Material::Parent is at 0x5a0");
static_assert(offsetof(Material, Version64) == 0x5a8, "Version64 = 0x80000000000000 at 0x5a8");
static_assert(sizeof(Material) == 0x5b0, "ls::Material is 0x5b0 bytes on Linux");
#endif

struct SBBinding
{
    uint8_t field_0;
    uint8_t DxVsIndex;
    uint8_t field_2;
    uint8_t field_3;
    uint8_t field_4;
    uint8_t DxPsIndex;
    uint8_t field_6;
    uint8_t VkBindingIndex;
    uint8_t VkDescriptorSet;
};

struct MaterialRenderingData
{
    EntityHandle Handle;
    EntityHandle VertexFormat;
    uint64_t InstancingHash;
    uint8_t ShaderIndex;
    uint8_t field_19;
    uint16_t EngineCBSize;
    uint16_t MaterialCBSize;
    SBBinding EngineBinding;
    SBBinding MaterialBinding;
    [[bg3::hidden]] void* MaterialCB;
    [[bg3::hidden]] uint64_t MaterialCBBufferSize;

    template <class T>
    std::optional<T> GetUniformParam(Material& instance, UniformBindingData const& binding);
    template <class T>
    void SetUniformParam(Material& instance, UniformBindingData const& binding, T value);
    bool CheckConstantBuffer(Material& instance);
};


struct PrimaryMaterialRenderingData : public MaterialRenderingData
{
    __int64 field_40;
    __int64 field_48;
    __int64 field_50;
    __int64 field_58;
};

struct [[bg3::hidden]] QueuedRawTextures
{
    FixedString Param;
    void* Texture; // rf::Texture*
};

struct AppliedMaterialQueuedParametersContainer : public MaterialParameters
{
    [[bg3::hidden]] Array<QueuedRawTextures> QueuedTextures;
};


struct AppliedMaterial
{
    struct Texture2DParam
    {
        [[bg3::hidden]] void* TextureResource;
        // Not here in dx11 build
        [[bg3::hidden]] void* VkTextureResource;
        FixedString Name;
        int16_t Index;
        bool IsStub;
        [[bg3::hidden]] uint8_t field_17;
    };

    struct VirtualTextureParam
    {
        FixedString ParameterName;
        FixedString ID;
        [[bg3::hidden]] void* VirtualTextureResource;
        int16_t Index;
    };


    [[bg3::hidden]] PrimaryMaterialRenderingData* PrimaryRenderingData;
    [[bg3::hidden]] std::array<MaterialRenderingData, 5> RenderingData;
    [[bg3::hidden]] std::array<void*, 8> PipelineStates;
    [[bg3::hidden]] Array<Texture2DParam> Texture2DParams;
    Array<VirtualTextureParam> VirtualTextureParams;
    RenderableObject* RenderableObject;
    [[bg3::legacy(MaterialInstance)]] Material* Material;
    [[bg3::hidden]] void* RenderStage;
    [[bg3::hidden]] void* ShaderCallback;
    uint64_t Hash;
    [[bg3::hidden]] void* DiffusionProfile;
    AppliedMaterialDirtyFlags DirtyFlags;
    uint8_t BlendStateID;
    uint8_t RasterizerStateID;
    uint8_t DebugMaterial;
    float OverlayOffset;
    FixedString MaterialName;
    AppliedMaterialFlags Flags;
    bool Initialized;
    uint8_t AlphaChannel;
    glm::vec4 MeshVertexColor;
    glm::vec4 DynamicParameter;
    AppliedMaterialQueuedParametersContainer* QueuedParameters;

    //# P_FUN(GetScalar, AppliedMaterial::GetScalar)
    //# P_FUN(GetVector2, AppliedMaterial::GetVector2)
    //# P_FUN(GetVector3, AppliedMaterial::GetVector3)
    //# P_FUN(GetVector4, AppliedMaterial::GetVector4)
    std::optional<float> GetScalar(FixedString const& param);
    std::optional<glm::vec2> GetVector2(FixedString const& param);
    std::optional<glm::vec3> GetVector3(FixedString const& param);
    std::optional<glm::vec4> GetVector4(FixedString const& param);

    //# P_FUN(SetScalar, AppliedMaterial::SetScalar)
    //# P_FUN(SetVector2, AppliedMaterial::SetVector2)
    //# P_FUN(SetVector3, AppliedMaterial::SetVector3)
    //# P_FUN(SetVector4, AppliedMaterial::SetVector4)
    //# P_FUN(SetTexture2D, AppliedMaterial::SetTexture2D)
    //# P_FUN(SetVirtualTexture, AppliedMaterial::SetVirtualTexture)
    bool SetScalar(FixedString const& param, float value);
    bool SetVector2(FixedString const& param, glm::vec2 value);
    bool SetVector3(FixedString const& param, glm::vec3 value);
    bool SetVector4(FixedString const& param, glm::vec4 value);
    bool SetTexture2D(FixedString const& param, FixedString const& texture);
    bool SetVirtualTexture(FixedString const& param, FixedString const& texture);

    template <class T>
    std::optional<T> GetUniformParam(UniformBindingData const& binding);
    template <class T>
    void SetUniformParam(UniformBindingData const& binding, T value);
};

// PORT (linux): witnessed off ls::AppliedMaterial::TryOverrideTexture2DParameter, bin/bg3
// 0x2942580 (manifest row) -- the only function in this build that reads all
// three of these members, and the reason the mirror's dx11 note matters here: this build is
// Vulkan, so Texture2DParam does carry VkTextureResource and the stride is 0x18, not 0x10.
static_assert(sizeof(AppliedMaterial::Texture2DParam) == 0x18, "bin/bg3 0x29425c4 advances the Texture2DParams cursor by 0x18 per entry");
static_assert(offsetof(AppliedMaterial::Texture2DParam, Name) == 0x10, "bin/bg3 0x29425b5/0x29425b9 compare the param index against dword [entry+0x10]");
static_assert(offsetof(AppliedMaterial, Texture2DParams) == 0x188, "bin/bg3 0x29425ab loads the Array's buf_ from [this+0x188] and 0x294259d its size_ from [this+0x194], which is buf_+0xc");
static_assert(offsetof(AppliedMaterial, Flags) == 0x1E4, "bin/bg3 0x294258e opens `test byte [this+0x1e4], 1` -- AppliedMaterialFlags::Queued selecting the queued path");
static_assert(offsetof(AppliedMaterial, QueuedParameters) == 0x208, "bin/bg3 0x294261c loads [this+0x208] into rdi for the queued {param, texture} push");

// PORT (linux): witnessed off ls::AppliedMaterial::ApplyLoadedShaders, bin/bg3 0x7417a30 -- the one
// caller of LoadVirtualTexture (manifest row 0x24f9c20) that *builds* a
// parameter rather than overwriting one, so it states the element's shape and the array's place
// together. It assembles {name, id, resource, index} on the stack at [rsp+0x40]/+0x44/+0x48/+0x50
// and pushes it into the array at [this+0x198]: capacity from [this+0x1a0], size from [this+0x1a4],
// and the write at 0x7418a07 indexes with `lea rcx,[rcx+rcx*2]` then `[rax+rcx*8]` = stride 0x18.
// ls::AppliedMaterial::TryOverrideVirtualTextureParameter 0x2942b00 scans the same array with
// `add r12, 0x18` and compares [entry] against the param and [entry+4] against the ID. macOS names
// the container ls::DynamicArray<ls::AppliedMaterialVirtualTextureParameter, ...> (Reallocate
// 0x28a0c80), which is the element class's own name.
static_assert(sizeof(AppliedMaterial::VirtualTextureParam) == 0x18, "bin/bg3 0x7418a03/0x7418a07 index the array by rcx*24, and 0x2942b78 advances the scan cursor by 0x18");
static_assert(offsetof(AppliedMaterial::VirtualTextureParam, ID) == 4, "bin/bg3 0x2942b59 compares dword [entry+4] against the wanted ID, having compared [entry] against the parameter name");
static_assert(offsetof(AppliedMaterial::VirtualTextureParam, VirtualTextureResource) == 8, "bin/bg3 0x74189c9 stores LoadVirtualTexture's result into the +8 slot of the element being built ([rsp+0x48], read back at 0x7418a31 as qword [r14+4] off &id)");
static_assert(offsetof(AppliedMaterial::VirtualTextureParam, Index) == 0x10, "bin/bg3 0x74189bb writes r15w into the +0x10 slot ([rsp+0x50]), read back at 0x7418a27 as word [r14+0xc] off &id");
static_assert(offsetof(AppliedMaterial, VirtualTextureParams) == 0x198, "bin/bg3 0x7418a07 loads the Array's buf_ from [this+0x198], 0x74189ce its capacity from [this+0x1a0] and 0x74189d4 its size_ from [this+0x1a4]");


struct Visual : public MoveableObject
{
    static constexpr uint32_t StaticRTTI = 0x1001;

    virtual bool AddObject(RenderableObject*, uint8_t flags) = 0;
    virtual void SetLODDistances(RenderableObject*) const = 0;
    virtual void Pick(void const* ray, void* result) = 0;
    virtual void UpdateCullFlags() = 0;

    struct ObjectDesc
    {
        RenderableObject* Renderable;
        [[bg3::legacy(field_8)]] uint8_t LOD;
        VisualObjectType Flags;
    };


    struct Attachment
    {
        Visual* Visual;
        [[bg3::legacy(BoneIndex1)]] int SourceIndex;
        [[bg3::legacy(BoneIndex2)]] int AttachIndex;
        [[bg3::legacy(Bone1)]] FixedString Attach;
        [[bg3::legacy(Bone2)]] FixedString Source;
        VisualAttachmentFlags Flags;
        [[bg3::legacy(BoneType1)]] VisualAttachType SourceType;
        [[bg3::legacy(BoneType2)]] VisualAttachType TargetType;
        FixedString field_20;
        [[bg3::legacy(field_24)]] glm::vec3 Offset;
    };


    struct SkeletonSlot
    {
        struct Remap
        {
            int16_t MapFrom;
            int16_t MapTo;
        };

        FixedString RemapperSlot;
        [[bg3::legacy(field_4)]] FixedString RemappedSlot;
        Skeleton* Skeleton;
        MiniCompactSet<Remap> Remaps;
    };

    [[bg3::hidden]] void* RenderPrecisionHack;
    MiniCompactSet<SkeletonSlot*> SkeletonSlots;
    Array<ObjectDesc> ObjectDescs;
    Array<float> LODDistances;
    Array<Attachment> Attachments;
    Visual* Parent;
    resource::VisualResource* VisualResource;
    [[bg3::hidden]] void* GameObject;
    [[bg3::hidden]] void* PhysicsScene;
    [[bg3::hidden]] void* ClothScene;
    VisualFlags VisualFlags;
    VisualCullFlags CullFlags;
    VisualPhysicsFlags PhysicsFlags;
    LightChannelValue LightChannel;
    bool HasValidPose;
    bool HasAnimatedObjects;
    int16_t WrinkleBaseBoneIndex;
    int16_t Wrinkle01BoneIndex;
    int16_t Wrinkle02BoneIndex;
    int16_t Wrinkle03BoneIndex;
    int16_t BlendShapeBase;
    ecs::EntityRef VisualEntity;
};


struct Effect : public Visual
{
    static constexpr uint32_t StaticRTTI = 0x3001;

    [[bg3::hidden]] Array<void*> PostProcess;
    float CullingDistance;
};

struct BasicModel : ProtectedGameObject<BasicModel>
{
    static constexpr uint32_t StaticRTTI = 0x1000000;

    virtual uint32_t GetRTTI() = 0;
#if defined(BG3SE_LINUX)
    // PORT (linux): upstream's `Destroy` is MSVC's one-slot scalar deleting destructor. Itanium
    // gives a virtual destructor two slots (D1, D0), so a plain one-slot virtual here puts every
    // Model virtual one slot early: `IsBlendShape()` calls slot 2, which is D0, and the one
    // caller, MakePolymorphicRef(BasicModel*) (Render.inl), runs it on every `.Model` read of a
    // model that is not skinned -- destroying and freeing a model the renderer still draws,
    // which crashes the game. Witness, the
    // rf::TransformingVertexModel vtable 0x7aeaed0: slot 0 `mov eax, 0x3000000; ret` (GetRTTI,
    // Model::StaticRTTI); slot 1 0x2dc43d0 stores this vtable into [rdi] (D1); slot 2 0x2dc43b0
    // calls slot 1 and tail-jumps to operator delete (D0); slots 3/4/5 are `cmp qword [rdi+X], 0;
    // setne al; ret` on 0x90/0x88/0x98, i.e. BlendShapeModelData/SkinnedModelData/ClothModelData
    // = IsBlendShape/IsSkinned/IsCloth. The game's own render worker calls IsBlendShape through
    // [vtbl+0x18] (bin/bg3 0x2455ff0), the same slot 3. Same declaration as MoveableObject's.
    virtual ~BasicModel() = 0;
#else
    virtual void Destroy() = 0;
#endif

    FixedString Name;
    FixedString Id;
    FixedString LinkId;
    AABound Bound;
};

struct [[bg3::hidden]] PickingData
{
    float* Vertices;
    uint32_t* Indices;
    int IndexCount;
    int VertexCount;
};

struct FormatDesc
{
    uint8_t Stream;
    uint8_t Usage;
    uint8_t UsageIndex;
    uint8_t RefType;
    uint8_t Format;
    uint8_t Size;
};

struct Model : BasicModel
{
    static constexpr uint32_t StaticRTTI = 0x3000000;

    virtual bool IsBlendShape() = 0;
    virtual bool IsSkinned() = 0;
    virtual bool IsCloth() = 0;

    int IndexCount;
    int IndexSize;
    int VertexCount;
    int VertexSize;
    Array<FormatDesc> Attributes;
    [[bg3::hidden]] void* VertexBuffer;
    [[bg3::hidden]] void* IndexBuffer;
    uint8_t Topology;
    [[bg3::hidden]] PickingData Picking;
    bool HasUVs;
    uint32_t UVCount;
};

struct ModelProxy : BasicModel
{
    static constexpr uint32_t StaticRTTI = 0x5000000;

    Array<glm::vec3> Vertices;
    Array<int> Indices;
};

struct TransformingVertexModel;

#if defined(BG3SE_LINUX)
// PORT (linux): the {begin, end} pointer pair slots 3 and 4 of GrBlendShapeModelData return
// in rax:rdx.  At namespace scope because a nested record breaks the P() property macro.
struct [[bg3::hidden]] BlendShapeRange { void* Begin; void* End; };
#endif

struct GrBlendShapeModelData : ProtectedGameObject<GrBlendShapeModelData>
{
#if defined(BG3SE_LINUX)
    // PORT (linux): read off the macOS oracle af3f1c71, whose only implementation of this
    // interface keeps its symbols -- __ZTVN12_GLOBAL__N_122GrannyBlendShapeLoaderE at
    // 0x1094ae768, an 8-slot run, with all six accessors disassembled.
    // Upstream's seven declarations were the MSVC reading and are
    // wrong here in four ways:
    //
    //   0 ~GrBlendShapeModelData [D1]   0x106948f30   Itanium spends TWO slots on a virtual
    //   1 ~GrBlendShapeModelData [D0]   0x106948f50   destructor where MSVC has the single
    //                                                 scalar-deleting one upstream spells
    //                                                 Destroy(), so every slot below was one
    //                                                 early -- GetVertexCount landed on D0.
    //   2 GetVertexCount(int) const     0x106948ff0   this=rdi, index=esi, uint32 in eax
    //   3 GetIndexRemapper(int) const   0x106949010   NOT a second GetVertexCount overload:
    //                                                 a distinct name, so there is no MSVC
    //                                                 overload group to reverse and the order
    //                                                 upstream has is the game's.
    //   4 GetModifiedVertexArray(int) const 0x106949050
    //   5 GetMeshName(int) const        0x106949080   this=rdi, returns [rdi+0x10] + i*4,
    //                                                 i.e. a FixedString* -- upstream's name
    //                                                 for it is GetMorphMeshName.
    //   6 GetMeshCount() const          0x1069490a0   takes NO argument (upstream's
    //                                                 GetMorphTargetCount(int) passed one)
    //   7 GetMaxVertDisplacement() const 0x1069490b0  takes NO argument either; float in xmm0
    //
    // And neither span-returning slot has a hidden return pointer: `this` is in rdi in all
    // six, and slots 3 and 4 return a {begin, end} POINTER PAIR in rax:rdx (0x106949028-30
    // loads [rcx+0x40] and the count at [rcx+0x3c], then `lea rdx, [rax+rcx*2]`; 0x106949068-74
    // the same with a 16-byte stride).  A 16-byte trivially-copyable record goes back in two
    // registers on Itanium, so upstream's `T* F(T& out, int)` spelling -- which would put the
    // out-parameter in rsi and `this` in rdi -- describes neither end.  BlendShapeRange above
    // spells the pair out rather than std::span<void*>, whose second word is a COUNT, not an end.
    virtual ~GrBlendShapeModelData() = 0;
    virtual uint32_t GetVertexCount(int) const = 0;
    virtual BlendShapeRange GetIndexRemapper(int) const = 0;
    virtual BlendShapeRange GetModifiedVertexArray(int) const = 0;
    virtual FixedString* GetMeshName(int) const = 0;
    virtual uint32_t GetMeshCount() const = 0;
    virtual float GetMaxVertDisplacement() const = 0;
#else
    virtual void Destroy() = 0;
    virtual uint32_t GetVertexCount(int) = 0;
    virtual std::span<void*>* GetVertexCount(std::span<void*>&, int) = 0;
    virtual std::span<void*>* GetVertices(std::span<void*>&, int) = 0;
    virtual FixedString* GetMorphMeshName(int) = 0;
    virtual uint32_t GetMorphTargetCount(int) = 0;
    virtual float GetMaxVertDisplacement(int) = 0;
#endif

    [[bg3::hidden]] void* Mesh;
    Array<FixedString> MorphTargetDrivers;
};

struct BlendShapeModelData : ProtectedGameObject<BlendShapeModelData>
{
    GrBlendShapeModelData* Data;
    TransformingVertexModel* TransformingModel;
    [[bg3::hidden]] void* VertexBuffer;
    uint32_t NumVertices;
    [[bg3::hidden]] void* Buffer1;
    [[bg3::hidden]] void* Buffer2;
    [[bg3::hidden]] void* MorphVBView[2];
    [[bg3::hidden]] void* MorphIBView[2];
};

struct TransformingVertexModel : Model
{
    [[bg3::hidden]] void* SkinnedModelData;
    BlendShapeModelData* BlendShapeModelData;
    [[bg3::hidden]] void* ClothModelData;
    bool HasClothInstance;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the rf::BasicModel / rf::Model / rf::TransformingVertexModel chain, measured
// on the macOS oracle -- bin/bg3 keeps no symbol for any of them and match.py resolves none
// of their methods, but the objects hold no lock, so the oracle's absolute offsets transfer.
// Model is the flagged record: IndexCount sits at 0x2c, inside
// BasicModel's tail padding (Bound ends there, sizeof(BasicModel) is 0x30), which is what
// MSVC would never do.  Note that sizeof is NOT the ABI witness for any of these -- the four
// ints re-align at 0x40 either way, so both layouts are 0x88 -- the argument store is.
//
// Witnesses, all on dumps/oracles/bg3-macos-x86_64 (Mach-O, same Itanium ABI):
//   rf::Model::SetIndices(int, unsigned char, void const*, EResourceUsage) 0x107386790 --
//     `mov [rbx+0x58], rax` (the buffer it just created), then `mov dword [rbx+0x2c], r14d`
//     with r14d = esi = the int argument, and `mov dword [rbx+0x30], r15d` with r15d = dl =
//     the index-size argument.  An ARGUMENT store at 0x2c: IndexCount and IndexSize name
//     themselves, with no match score, string or inference in the chain.
//   rf::Model::Model(FixedString const&, FixedString const&) 0x1073866e0 -- the BasicModel
//     vtable at +0, gst::Acquire + store for the three FixedStrings at +8/+0xc/+0x10, a
//     16-byte constant at +0x14 and a qword at +0x24 (Bound, 24 bytes, ending at 0x2c), the
//     Model vtable, then zero fills at +0x2c (16 bytes, the four ints, ending exactly where
//     Attributes' 8-alignment gap begins), +0x40 (Attributes), +0x50 (the two buffers),
//     `byte [+0x60] = 1` (Topology, a value store), +0x68 and +0x71 (Picking, 24 bytes, and
//     HasUVs at 0x80), and `dword [+0x84] = 0`.  Its one call site 0x106945098 opens
//     `mov edi, 0x88` / malloc.
//   rf::Model::SetPickingData(PickingData&&) 0x107387410 -- `lea r12, [rdi+0x68]` and
//     `cmp dword [rdi+0x7c], 0`, i.e. Picking at 0x68 with its VertexCount at 0x7c.
//   rf::TransformingVertexModel::TransformingVertexModel(EnumFlags, FixedString const&,
//     FixedString const&) 0x1073c33e0 -- Model's ctor inlined with the identical store
//     pattern, then the three pointer stores `[rbx+0x88] = r15` (0x1073c3537),
//     `[rbx+0x90] = r15` (0x1073c361b), `[rbx+0x98] = r14` (0x1073c36eb) and
//     `byte [rbx+0xa0] = 0`.  Its one call site 0x1069450f7 opens `mov edi, 0xa8`.
static_assert(offsetof(BasicModel, Name) == 0x8, "the first FixedString the ctor Acquires");
static_assert(offsetof(BasicModel, Bound) == 0x14, "the 16+8 byte constant block at 0x14");
static_assert(sizeof(AABound) == 0x18, "Bound runs 0x14..0x2c, which is BasicModel's dsize");
static_assert(sizeof(BasicModel) == 0x30, "so 0x2c..0x30 is tail padding Itanium reuses");
static_assert(offsetof(Model, IndexCount) == 0x2c, "SetIndices' int argument store at 0x2c");
static_assert(offsetof(Model, IndexSize) == 0x30, "SetIndices' unsigned char argument at 0x30");
static_assert(offsetof(Model, Attributes) == 0x40, "the ctor's second zero fill");
static_assert(offsetof(Model, VertexBuffer) == 0x50, "the ctor's third zero fill");
static_assert(offsetof(Model, IndexBuffer) == 0x58, "SetIndices stores the buffer at 0x58");
static_assert(offsetof(Model, Topology) == 0x60, "the ctor's `byte [+0x60] = 1`");
static_assert(offsetof(Model, Picking) == 0x68, "SetPickingData's `lea r12, [rdi+0x68]`");
static_assert(offsetof(Model, HasUVs) == 0x80, "the 0x71 store ends at 0x80, inclusive");
static_assert(offsetof(Model, UVCount) == 0x84, "the ctor's `dword [+0x84] = 0`");
static_assert(sizeof(Model) == 0x88, "the ctor's call site allocates 0x88 (not an ABI witness)");
static_assert(offsetof(TransformingVertexModel, SkinnedModelData) == 0x88, "ctor 0x1073c3537");
static_assert(offsetof(TransformingVertexModel, BlendShapeModelData) == 0x90, "ctor 0x1073c361b");
static_assert(offsetof(TransformingVertexModel, ClothModelData) == 0x98, "ctor 0x1073c36eb");
static_assert(offsetof(TransformingVertexModel, HasClothInstance) == 0xa0, "ctor 0x1073c36f2");
static_assert(sizeof(TransformingVertexModel) == 0xa8, "its call site allocates 0xa8");
#endif

struct [[bg3::hidden]] BufferBindingInfo
{
    __int64 field_0;
    __int64 field_8;
};

struct BlendShapeWeightOverride
{
    int32_t BlendShapeIndex;
    float Weight;
};

struct BlendShapeWeights
{
    Array<float> Weights;
#if 0
    // Editor only
    // Array<WeightOverride> WeightOverrides;
#endif
};

struct [[bg3::hidden]] BlendShapeObjectDataDX11 : ProtectedGameObject<BlendShapeObjectDataDX11>
{
    void* VertexBuffer;
    void* Buffer;
    BufferBindingInfo BufferBinding;
    BlendShapeWeights Weights;
};

struct [[bg3::hidden]] BlendShapeObjectDataVK : ProtectedGameObject<BlendShapeObjectDataVK>
{
    void* VertexBuffer;
// PORT (linux): upstream has a single `void* Buffer;` here, which put Weights at 0x40 -- one
// BufferBindingInfo short of where the native build keeps it.  The native (Vulkan) build has
// three buffers, one per binding, so Weights is at 0x50 and the object is 0x60 bytes.  Reading
// 0x40 handed the validator the third BufferBindingInfo: its buffer pointer as buf_, its binding
// index as capacity_, and the four bytes of padding behind that index as size_ -- which is why
// the refusal came and went with whatever the allocator left there.
    void* Buffer[3];
    BufferBindingInfo BufferBinding[3];
    BlendShapeWeights Weights;
};



struct RenderableObject : public MoveableObject
{
    static constexpr uint32_t StaticRTTI = 0x3;

    virtual void Render(void const* RenderObjectData, void const* RenderContext, void const* InstancingDrawData) const = 0;
    virtual void NotifyTextureIDAdded(void const* Texture, bool) = 0;
    virtual void NotifyTextureIDRemoved(void const* Texture, bool) = 0;
    virtual void* SetupMaterialParameters(void const* RenderObjectData, void const* RenderContext) const = 0;
    virtual glm::mat4 GetRenderingTransform() const = 0;
    virtual void SetupTransformedVerticesOffset(void* TempReflectedCB, void const* MaterialShaderDesc, void const* RenderContext) const = 0;
    virtual bool GetAnimatableTransform(glm::mat3x4 const*&, uint64_t&) const = 0;
    virtual void CalculateRenderPass() = 0;
    virtual void BuildVertexFormatDesc() = 0;
    virtual void BuildVelocityVertexFormatDesc() = 0;
    virtual bool Ret0_2() const = 0;
    virtual bool IsBlendShape() const = 0;
    virtual bool IsSkinned() const = 0;
    virtual bool Ret0_4() const = 0;
    virtual bool Ret0_5() const = 0;
    virtual bool CanUseOverlayBoneTransforms() const = 0;

    BasicModel* Model;
    RenderPropertyList PropertyList;
    Visual* Parent;
    // either BlendShapeObjectDataDX11 or BlendShapeObjectDataVK
    [[bg3::hidden]] void* BlendShape;
    glm::vec4 MeshRandomData;
    Array<AppliedMaterial*> AppliedMaterials;
    AppliedMaterial* ActiveMaterial;
    Array<AppliedMaterial*> AppliedOverlayMaterials;
    uint8_t DirtyFlags;
    uint8_t LOD;

    // Lua helpers
    BlendShapeWeights* LuaGetBlendShape() const;
    bool SetBlendShapeWeight(FixedString const& param, std::optional<float> weight);
    bool ClearBlendShapeWeights(FixedString const& param);

    //# P_GETTER(BlendShape, LuaGetBlendShape)
    //# P_FUN(SetBlendShapeWeight, RenderableObject::SetBlendShapeWeight)
    //# P_FUN(ClearBlendShapeWeights, RenderableObject::ClearBlendShapeWeights)
};

#if defined(BG3SE_LINUX)
// ls::RenderableObject::SetupBlendShapeObjectData() is bin/bg3 0x24e38a0 on build a187af94 --
// the callee both the matched ls::RenderableObject::~RenderableObject 0x2397940 and the matched
// ls::RenderableObject::RenderableObject(rf::Model const*) 0x24e36e0 reach, and the same
// function as the macOS oracle's _ZN2ls16RenderableObject25SetupBlendShapeObjectDataEv
// 0x106fc73d0.  It both builds and tears down the object, so one function witnesses every
// member:
//   `mov edi, 0x60` / operator new at 0x24e3904, then `[rbx+0x98] = rax` at 0x24e3931 -- the
//     object is 0x60 bytes and hangs off RenderableObject+0x98.
//   0x00 is the vertex buffer: created at 0x24e3ce4 and stored by `mov qword [rcx], rax`
//     0x24e3cf3; macOS frees `[r14]` through rf::metal::GPUDevice::DestroyVertexBuffer at
//     0x106fc740a.
//   0x08, 0x10, 0x18 are three buffers, each created by the same callee 0x34bf530 and stored at
//     0x24e3a3b, 0x24e3b3a and 0x24e3c35.
//   0x20, 0x30, 0x40 are their three BufferBindingInfo, written {buffer, dword index} right
//     after each buffer (0x24e3abb/0x24e3abf, 0x24e3bba/0x24e3bbe, 0x24e3cb5/0x24e3cb9) and
//     reset in the teardown branch to {rf::NO_BUFFER_VIEW, -1} from the .rodata pair
//     0x1e950c0/0x1e950c8 (0x24e3f2d/0x24e3f2a, 0x24e3ff0/0x24e3fed, 0x24e40b3/0x24e40b0).
//   0x50 is the Array<float>: zeroed as `movups xmmword [rax+0x50], xmm0` 0x24e392d, its size_
//     written 0 at the end of setup (`dword [rcx+0x5c] = 0`, 0x24e3cf6), and released in the
//     teardown branch as Array<T>::release() spelled out -- `cmp dword [r14+0x58], 0` 0x24e40bc
//     (capacity_), `cmp dword [r14+0x5c], 0` 0x24e40cc with `= 0` at 0x24e41c5 (size_), free of
//     `[r14+0x50]` 0x24e40d2 then `[r14+0x50] = 0` / `[r14+0x58] = 0`, and only then
//     `operator delete(this)` 0x24e4111.  It is the one non-trivial member and nothing in the
//     function touches any other offset, so 0x50..0x60 is Weights and 0x60 is the whole object.
//
// The toolkit settles the member set and its order, because it is a one-binding (DX11) build of
// the same class: GameEngine.dll's ls::RenderableObject::SetupBlendShapeObjectData 0x1801a6b00
// allocates 0x40, constructs the binding at +0x10 and two Arrays at +0x20 and +0x30
// ({qword 0, qword 0, dword 0} each -- buf_, capacity_, size_), and
// ls::RenderableObject::AddBlendShapeWeightOverride(int, float) 0x1801a3020 and
// RemoveAllBlendShapeWeightOverrides() 0x1801a57c0 both work the array at +0x30 with 8-byte
// elements keyed on a leading int -- upstream's BlendShapeWeightOverride, the member upstream
// fences off as editor-only.  So Weights is the Array<float> immediately before it, and the
// toolkit's ls::RenderableObject::GetBlendShapeObjectData() 0x1800a7730 is
// `mov rax, [rcx+0x98]; ret`, the same BlendShape offset as here.
//
// macOS (metal, Weights at 0x20, malloc(0x30) at 0x106fc750e) and the toolkit (DX11, 0x40) both
// have one buffer and one binding, which is exactly upstream's BlendShapeObjectDataDX11 -- left
// alone.  NOT measured: whether the shipped Windows *Vulkan* build has one buffer or three, so
// whether upstream's VK numbers are wrong there too is open.
static_assert(offsetof(RenderableObject, BlendShape) == 0x98,
    "the pointer SetupBlendShapeObjectData stores, bin/bg3 0x24e3931; the toolkit's "
    "ls::RenderableObject::GetBlendShapeObjectData is `mov rax, [rcx+0x98]; ret`, "
    "GameEngine.dll 0x1800a7730");
static_assert(sizeof(BufferBindingInfo) == 0x10,
    "the stride between the three bindings written at bin/bg3 0x24e3abb, 0x24e3bba and "
    "0x24e3cb5, each {buffer, dword index}");
static_assert(offsetof(BlendShapeObjectDataVK, Buffer) == 0x08,
    "the three buffers stored at bin/bg3 0x24e3a3b, 0x24e3b3a and 0x24e3c35");
static_assert(offsetof(BlendShapeObjectDataVK, BufferBinding) == 0x20,
    "the binding reset to {rf::NO_BUFFER_VIEW, -1}, bin/bg3 0x24e3f2d");
static_assert(offsetof(BlendShapeObjectDataVK, Weights) == 0x50,
    "Array<float>::release() spelled out over 0x50/0x58/0x5c, bin/bg3 0x24e40bc..0x24e40f3");
static_assert(sizeof(BlendShapeObjectDataVK) == 0x60,
    "`mov edi, 0x60` before the object's only operator new, bin/bg3 0x24e3904");
static_assert(sizeof(BlendShapeObjectDataDX11) == 0x30,
    "macOS malloc(0x30) at 0x106fc750e -- the one-binding build, not this one's");
#endif

struct AnimatableObject : public RenderableObject
{
    static constexpr uint32_t StaticRTTI = 0x7;

    MeshBinding* Mesh;
    bool IsRigid;
    bool HasStaticBounds;
    uint64_t OverrideTransformCount;
    glm::mat3x4* OverrideTransforms_Mat3x4;
    uint64_t SavedOverrideTransformCount;
    glm::mat3x4* SavedOverrideTransforms_Mat3x4;
    std::array<int, 2> TransformedVerticesOffset;
    std::array<int, 2> TransformedVerticesOffset2;
    std::array<int, 2> TransformedFrameCount;
};

struct Shape : public RenderableObject
{
    static constexpr uint32_t StaticRTTI = 0x103;

    Skeleton* SkeletonActor;
    float Lifetime;
    uint8_t field_F4;
    bool UseShapeRender;
// PORT (linux): upstream's `uint32_t Preset;` is not in the native build, and carrying it
// across put TriangulationFailed six bytes too high -- which is why the validator refused it
// and disabled proxying of the whole class.  The derived block ends at the bool: every
// construction site is `operator new(0xf0)` and writes exactly qword 0xe0, dword 0xe8, byte
// 0xec, byte 0xed, byte 0xee and nothing else, so there is no four-byte slot to put it in.
// The slot is real on MSVC -- the toolkit's GameEngine.dll exports
// `ls::Shape::SetLifeTime(float)` writing [this+0xf0] and `ls::Shape::GetTriangulationFailed()`
// reading [this+0xfc], twelve bytes apart where the native build has two -- so upstream's
// Win32 offsets (field_F4 = 0xf4 = 0xec+8) are right and the difference is in the member set,
// not in a misreading.  Whether the shipped Windows game has the slot was not measured; the
// toolkit is an editor build.
    bool TriangulationFailed;
};

#if defined(BG3SE_LINUX)
// The Linux ctor is bin/bg3 0x741f6b0 (build a187af94): it calls
// ls::RenderableObject::RenderableObject(rf::Model const*) 0x24e36e0, stores the 37-slot
// vtable run at 0x7aba580 (slot 0 is `mov eax, 0x103; ret`, this class's StaticRTTI), and
// then writes the derived block one member at a time.
static_assert(offsetof(Shape, SkeletonActor) == 0xE0,
    "the ctor's `qword [rbx+0xe0] = 0`, bin/bg3 0x741f6e2");
static_assert(offsetof(Shape, Lifetime) == 0xE8,
    "the ctor's `dword [rbx+0xe8] = 0`, bin/bg3 0x741f6ed; a float by the toolkit's "
    "ls::Shape::SetLifeTime, GameEngine.dll 0x1801e0590 `movss [rcx+0xf0], xmm1`");
static_assert(offsetof(Shape, field_F4) == 0xEC,
    "the ctor's `byte [rbx+0xec] = 0`, bin/bg3 0x741f6f7; macOS ls::Shape::UpdateLine "
    "0x107043256 passes byte [this+0xec] as BuildLine's unsigned char argument");
static_assert(offsetof(Shape, UseShapeRender) == 0xED,
    "ls::Shape::Render (vtable slot 21) opens `cmp byte [rdi+0xed], 0`, bin/bg3 0x43c5760; "
    "macOS 0x107035228 is the same instruction");
static_assert(offsetof(Shape, TriangulationFailed) == 0xEE,
    "`call ls::Triangulator::TriangulatePolygon` / `xor al, 1` / `mov byte [r12+0xee], al`, "
    "bin/bg3 0x343c26e; macOS 0x10703ce1b is the same three");
static_assert(sizeof(Shape) == 0xF0,
    "every construction site allocates 0xf0: bin/bg3 0x75042fe, 0x7504914, 0x7504af9, "
    "0x7504fd9, 0x75053d7, 0x7505c2e");
#endif

struct DecalObject : public RenderableObject
{
    static constexpr uint32_t StaticRTTI = 0xB;

    FixedString Decal;
    FixedString Visual;
    glm::vec2 TilingUV;
    glm::vec2 OffsetUV;
    float NormalBlendingFactor;
    float AngleCutoff;
    int32_t Layer;
    float Opacity;
    bool IsGameDecal;
    glm::vec3 Dimensions;
    uint16_t CullFlags;
};

#if defined(BG3SE_LINUX)
// PORT (linux): ls::DecalObject measured on bin/bg3 itself, and named there without the
// oracle.  Its vtable is the 37-slot run at 0x7add768: slot 0 is `mov eax, 0xb; ret`, which
// is GetRTTI() returning this class's own StaticRTTI, and slots 3..13 are matched
// ls::RenderableObject methods.  Decal at 0xdc is the flagged offset -- RenderableObject's
// last member (LOD) ends at 0xda, so a 4-byte FixedString lands at 0xdc, inside the base's
// tail padding, where MSVC would put it at 0xe0 and shift the whole record by 4.
//
// Witnesses, all read with tools/disasm.py on the build-id c8da62b1 binary:
//   the factory that stores the vtable, bin/bg3 0x24e42c0 (its only `lea` referrer, found by
//     the .text rip-relative scan) -- `mov edi, 0x120` / operator new, then the matched
//     ls::RenderableObject::RenderableObject(rf::Model const*) 0x24e36a0, the vtable, and the
//     derived block one store at a time: `[+0xdc] = ebp` (the FixedString argument, just
//     gst::Acquire'd -- an argument store), `[+0xe0] = -1` (a null FixedString),
//     `movsd [+0xe4]` of a .rodata constant (TilingUV), a 16-byte zero at +0xec bounded by
//     those two value stores, `qword [+0xfc] = 0x3f80000000000000` (Layer 0, Opacity 1.0f),
//     `byte [+0x104] = 0`, `qword [+0x108] = 0` and `dword [+0x110] = 0` later reloaded with
//     0x3f800000 at 0x24e4477 (Dimensions.z = 1.0f), and `word [+0x114] = 0x641`.
//   ls::DecalObject::GetCullFlags(), bin/bg3 0x751c740 -- an 8-byte function,
//     `movzx eax, word [rdi+0x114]; ret`, sitting in slot 14 of the vtable above.  It is the
//     ONLY occurrence of those seven bytes in the binary, and the 0x118 form MSVC would need
//     does not occur at all.
//   ls::DecalObject::~DecalObject(), bin/bg3 0x24e4560 (vtable slot 2's callee) --
//     `cmp dword [rdi+0xe0], -1` and `lea r14, [rbx+0xe0]` release Visual, `mov eax,
//     [rbx+0xdc]` releases Decal, `mov rsi, [rbx+0x80]` goes to the matched
//     ls::TrackedContentManager<rf::Model>::Release (RenderableObject::Model), and it tail-
//     jumps to the matched ls::RenderableObject::~RenderableObject 0x2397940.
//   The macOS oracle agrees offset for offset -- ls::DecalObject::GetCullFlags 0x106c297f0
//     reads word [rdi+0x114], ls::DecalObject::Render 0x106c29940 reads the floats at 0xf4,
//     0xf8, 0x100 and the vec3 at 0x108/0x10c/0x110 with movss, and the same factory at
//     0x106c29300 allocates 0x120.
//
// NOT asserted: sizeof.  The game allocates 0x120 where this mirror computes 0x118, and the
// 8 bytes are alignment, not a missing member -- nothing in either binary touches an offset
// above 0x116.  ls::MoveableObject is 16-byte aligned in the game (the matched
// ls::RenderableObject ctor 0x24e36a0 uses `movaps xmmword ptr [rdi+0x30]` and again at
// +0x90/+0xa0/+0xb0/+0xc0, which is only legal if `this` is 16-aligned), so the game rounds
// 0x116 up to 0x120 while glm's 8-aligned vec4 rounds it to 0x118.  Every member offset is
// unaffected and the extender never allocates one, so the mirror is left alone.
static_assert(offsetof(RenderableObject, Model) == 0x80, "the dtor's TrackedContentManager release");
static_assert(offsetof(DecalObject, Decal) == 0xdc, "the factory's FixedString argument store");
static_assert(offsetof(DecalObject, Visual) == 0xe0, "the null FixedString the dtor releases");
static_assert(offsetof(DecalObject, TilingUV) == 0xe4, "the factory's movsd of a rodata constant");
static_assert(offsetof(DecalObject, OffsetUV) == 0xec, "the zero block between 0xe4 and 0xfc");
static_assert(offsetof(DecalObject, NormalBlendingFactor) == 0xf4, "Render 0x106c29f42 movss");
static_assert(offsetof(DecalObject, AngleCutoff) == 0xf8, "Render 0x106c29f4b movss");
static_assert(offsetof(DecalObject, Layer) == 0xfc, "the low half of the 0xfc qword store");
static_assert(offsetof(DecalObject, Opacity) == 0x100, "its high half, 1.0f");
static_assert(offsetof(DecalObject, IsGameDecal) == 0x104, "the factory's `byte [+0x104] = 0`");
static_assert(offsetof(DecalObject, Dimensions) == 0x108, "z = 1.0f at 0x110, bin/bg3 0x24e4477");
static_assert(offsetof(DecalObject, CullFlags) == 0x114, "GetCullFlags, bin/bg3 0x751c740");
#endif

struct PhysicsClothInfo
{
    phx::PhysicsSoftShape* PhysicsShape;
    bool IsSimulated;
    bool IsCopyCloth;
    bool IsHiResProxy;
    [[bg3::hidden]] void* VertexModel; // rf::TransformingVertexModel*
};

struct RenderableObjectPhysicsInfo
{
    RenderableObjectPhysicsInfo* Parent;
    PhysicsClothInfo* ClothInfo;
};

struct AnimatablePhysicsObject : public AnimatableObject
{
    RenderableObjectPhysicsInfo Physics;
};

struct CullableInstance : public MoveableObject
{
    static constexpr uint32_t StaticRTTI = 0x40001;

    struct InstancingObject* Parent;
    uint32_t InstanceIndex;
};

struct InstancingRenderableObject : public RenderableObject
{
    static constexpr uint32_t StaticRTTI = 0x203;
};

struct InstancingObject : public RenderableObject
{
    static constexpr uint32_t StaticRTTI = 0x403;

    Array<CullableInstance*> Instances;
};

struct LightDesc
{
    FixedString LightCookieTexture;
    glm::vec3 Position;
    glm::vec3 Color;
    glm::vec3 Rotation;
    float SpotLightInnerAngle;
    int field_2C;
    glm::vec3 DirectionLightDimensions;
    float DirectionLightAttenuationStart;
    float DirectionLightAttenuationEnd;
    float DirectionLightAttenuationSide;
    float Kelvin;
    float Radius;
    float Intensity;
    float ScatteringIntensityScale;
    float Gain;
    float Speed;
    float Amount;
    float MovementSpeed;
    float MovementAmount;
    bool UseTemperature;
    bool IsFlickering;
    bool IsMoving;
    bool Shadow;
    bool VolumetricShadow;
    bool Enabled;
    bool PreExpose;
    bool FlatFalloff;
    LightType LightType;
    uint8_t LightChannelFlag;
    uint8_t DirectionLightAttenuationFunction;
};


struct [[bg3::component]] LightComponent : public MoveableObject
{
    static constexpr uint32_t StaticRTTI = 0x10001;

    DEFINE_PROXY_COMPONENT(Light, "ls::LightComponent")

    EntityHandle field_80;
    glm::vec3 Color;
    float SpotLightInnerAngle;
    float SpotLightOuterAngle;
    glm::vec3 DirectionLightDimensions;
    float DirectionLightAttenuationSide;
    float DirectionLightAttenuationEnd;
    float DirectionLightAttenuationSide2;
    float Kelvin;
    float Radius;
    float Intensity;
    [[bg3::legacy(field_C0)]] float IntensityOffset;
    float ScatteringIntensityScale;
    float Gain;
    float EdgeSharpening;
    uint8_t DirectionLightAttenuationFunction;
    LightFlags Flags;
    uint8_t LightChannelFlag;
    LightType LightType;
    glm::vec3 Blackbody;
    [[bg3::legacy(field_E0)]] glm::vec3 PositionOffset;
    uint8_t OwnerFlags;
    [[bg3::hidden]] void* LightCookieTexture;
    LightTemplate* Template;
    EntityHandle AssociatedScene;
    uint16_t CullFlags;
    FixedString UUID;
};

struct LightUpdateFadeRequest
{
    float field_0;
    float field_4;
    float Max;
    float Current;
};

struct LightSystem : public BaseSystem
{
    DEFINE_SYSTEM(Light, "ls::LightSystem")

    HashMap<EntityHandle, LightUpdateFadeRequest> FadeEdgeSharpening;
    HashMap<EntityHandle, LightUpdateFadeRequest> FadeRadius;
    HashMap<EntityHandle, LightUpdateFadeRequest> FadeIntensity;
    HashMap<EntityHandle, LightUpdateFadeRequest> FadeIntensityOffset;
    HashSet<EntityHandle> ExplicitUpdateTransform;
    // Editor only
    // HashSet<EntityHandle> CheckFlickering;
    // HashSet<EntityHandle> CheckMoving;
};

END_SE()

BEGIN_NS(lua)

LUA_POLYMORPHIC(MoveableObject);
LUA_POLYMORPHIC(RenderableObject);
LUA_POLYMORPHIC(AnimatableObject);
LUA_POLYMORPHIC(Visual);

LUA_POLYMORPHIC(BasicModel);
LUA_POLYMORPHIC(Model);

END_NS()
