#pragma once

#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/AllSparkShared.h>
#include <GameDefinitions/Camera.h>
#include <GameDefinitions/MaterialParameters.h>

BEGIN_SE()

#define FOR_EACH_NONGUID_RESOURCE_TYPE() \
    FOR_RESOURCE_TYPE(Visual) \
    FOR_RESOURCE_TYPE(VisualSet) \
    FOR_RESOURCE_TYPE(Animation) \
    FOR_RESOURCE_TYPE(AnimationSet) \
    FOR_RESOURCE_TYPE(Texture) \
    FOR_RESOURCE_TYPE(Material) \
    FOR_RESOURCE_TYPE(Physics) \
    FOR_RESOURCE_TYPE(Effect) \
    FOR_RESOURCE_TYPE(Script) \
    FOR_RESOURCE_TYPE(Sound) \
    FOR_RESOURCE_TYPE(Lighting) \
    FOR_RESOURCE_TYPE(Atmosphere) \
    FOR_RESOURCE_TYPE(AnimationBlueprint) \
    FOR_RESOURCE_TYPE(MeshProxy) \
    FOR_RESOURCE_TYPE(MaterialSet) \
    FOR_RESOURCE_TYPE(BlendSpace) \
    FOR_RESOURCE_TYPE(FCurve) \
    FOR_RESOURCE_TYPE(Timeline) \
    FOR_RESOURCE_TYPE(Dialog) \
    FOR_RESOURCE_TYPE(VoiceBark) \
    FOR_RESOURCE_TYPE(TileSet) \
    FOR_RESOURCE_TYPE(IKRig) \
    FOR_RESOURCE_TYPE(Skeleton) \
    FOR_RESOURCE_TYPE(VirtualTexture) \
    FOR_RESOURCE_TYPE(TerrainBrush) \
    FOR_RESOURCE_TYPE(ColorList) \
    FOR_RESOURCE_TYPE(CharacterVisual) \
    FOR_RESOURCE_TYPE(MaterialPreset) \
    FOR_RESOURCE_TYPE(SkinPreset) \
    FOR_RESOURCE_TYPE(ClothCollider) \
    FOR_RESOURCE_TYPE(DiffusionProfile) \
    FOR_RESOURCE_TYPE(LightCookie) \
    FOR_RESOURCE_TYPE(TimelineScene) \
    FOR_RESOURCE_TYPE(SkeletonMirrorTable)

struct TextureLayerConfigId
{
    uint32_t TextureLayerConfig;
    uint8_t Flag;

    inline bool operator == (TextureLayerConfigId const& o) const
    {
        return o.TextureLayerConfig == TextureLayerConfig
            && o.Flag == Flag;
    }
};

inline constexpr uint64_t Hash(TextureLayerConfigId const& v)
{
    return v.Flag ^ (v.TextureLayerConfig * 397);
}

struct [[bg3::hidden]] VirtualTextureManagerBase : public ProtectedGameObject<VirtualTextureManagerBase>
{
    struct [[bg3::hidden]] GTSInfo
    {
        int32_t GTSHandle;
        int32_t RefCount;
    };

    struct [[bg3::hidden]] LayerConfig
    {
        uint64_t A;
        uint64_t B;
        uint64_t C;
    };

    virtual ~VirtualTextureManagerBase() = 0;
    virtual void MakePackedTilesResident(void*, int, int, int) = 0;
    virtual void MakePackedTilesResident2(void*, int) = 0;
    virtual void Update() = 0;
    virtual void UpdateGraniteDevice() = 0;
    virtual void BeginFrame() = 0;
    virtual void VMT_30() = 0;
    virtual bool Init() = 0;
    virtual void* KeepWarm(int32_t gtsHandle, FixedString const& gtexFileName, uint8_t r9_0) = 0;
    virtual bool Cooldown(int32_t gtsHandle, void* graphineTexture) = 0;
    virtual void FillTextureMap(void*, void*) = 0;
    virtual bool IsPartialUpdate() = 0;
    virtual void VMT_60() = 0;
    virtual void VMT_68() = 0;
    virtual void GetDataLoaderBufferBindingInfo() = 0;
    virtual void UpdateCacheSizes() = 0;
    virtual void VMT_88() = 0;
    virtual void PrefetchOneImmediately(void*, uint32_t) = 0;
    virtual void Prefetch(void*, uint32_t, void*) = 0;
    virtual void ProcessFeedbackData(void*, void*) = 0;
    virtual void ResolveFeedbackData(void*, void*) = 0;
    virtual void SuspendWarmTextures(uint8_t) = 0;
    virtual void ResumeWarmTextures(uint8_t) = 0;
    virtual int32_t OpenVirtualTextureFile(uint32_t textureLayerConfig, STDString const& path, uint8_t textureSetId, Array<LayerConfig>& layerConfigs) = 0;
    virtual void CloseVirtualTextureFile(uint32_t textureLayerConfig, int32_t gtsHandle, bool releaseTexSet0, bool releaseTexSet1) = 0;

    bool UseChunkedVirtualTextures;
#if defined(BG3SE_LINUX)
    // PORT (linux): the game's SRW lock, a bare 56-byte pthread_rwlock_t, not Win32's
    // 8-byte handle (ext/linux/shim/windows.h). Every field below was 48 low, and so was
    // the whole of VirtualTextureManager behind this base.
    LS_SRWLOCK GTSLock;
#else
    SRWLOCK GTSLock;
#endif
    LegacyRefMap<STDString, GTSInfo> LoadedGTS;
    HashMap<TextureLayerConfigId, Array<STDString>> field_28;
    HashMap<Guid, STDString> TileSets;
};

struct [[bg3::hidden]] VirtualTexturePrefetcher
{
    void* VMT;
    void* field_8;
    VirtualTextureManagerBase* ThisPtr;
    CRITICAL_SECTION CriticalSection;
    Array<void*> Arr_pGranitePrefetchWorker_BatchedPrefetchJob;
};


struct [[bg3::hidden]] VirtualTextureManager : public VirtualTextureManagerBase
{
    struct [[bg3::hidden]] SomeVal
    {
        __int64 field_0;
        int field_8;
    };

    void* Renderer;
    void* GraphineContext;
    void* GraphineDevice;
    void* GraphineTranscoder;
    SparseArray<void*> TexSet0;
    SparseArray<void*> TexSet1;
#if defined(BG3SE_LINUX)
    // PORT (linux): both of this struct's SRW locks are the game's bare 56-byte
    // pthread_rwlock_t, and field_188/field_1b8 are 56 bytes the mirror did not have --
    // see the witness block below the struct.
    LS_SRWLOCK Lock2;
    void* DataLoaderBuffer;
    void* DataLoaderBufferBindingInfo[2];
    SomeVal field_188[3];
    int field_1b8;
    LS_SRWLOCK PerThreadLocks[64];
#else
    SRWLOCK Lock2;
    void* DataLoaderBuffer;
    void* DataLoaderBufferBindingInfo[2];
    SRWLOCK PerThreadLocks[64];
#endif
    Array<void*> PerThreadPrefetches[64]; // GranitePrefetch*
    Array<void*> PerThreadWStrings[64]; // GranitePrefetch*
    SparseArray<void*> CpuCaches;
    SparseArray<void*> GpuCaches;
    int field_388;
    int field_38C;
    int field_390;
    int field_394;
    int field_398;
    int field_39C;
    int field_3A0;
    int field_3A4;
    VirtualTexturePrefetcher Prefetcher;
    std::optional<int> field_3F8;
    bool WasShutdown;
    bool PackedTilesResident;
    bool UpdateFinished;
    bool PartialUpdate;
    int field_404;
    int field_408;
    int field_40C;
    int GPUCachesSize;
    bool RequestResizeGPUCaches;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the whole of VirtualTextureManager, read off its own Linux constructor.
// App::CreateVirtualTextureManager (bin/bg3 0x50abea0, matched r2:2) is
// `operator new(0x1898)` followed by a call to the inlined constructor at 0x50abef0, so
// the game's sizeof is stated outright; the constructor then names every lock by calling
// pthread_rwlock_init@plt (0x77ea020) on it, 65 times in all.
//   Renderer 0xd8 and GraphineDevice 0xe8: `movups [rbx+0xd8], xmm0` and
//     `movups [rbx+0xe8], xmm0` zero exactly the four pointers this struct opens with, so
//     VirtualTextureManagerBase ends at 0xd8 -- which is the base's GTSLock at 56 bytes,
//     not 8.  That is this base's witness and it is the only one it has.
//   TexSet0 0xf8 and TexSet1 0x118: the two SparseArray constructor calls (0x50ad2b0).
//   Lock2 0x138: the first pthread_rwlock_init.
//   field_188 0x188 and field_1b8 0x1b8: three {qword 0, dword -1} pairs at a 16-byte
//     stride and then a zeroed dword, which is upstream's own SomeVal shape -- declared
//     inside this struct and otherwise unused.  56 bytes the mirror did not have; kept
//     under BG3SE_LINUX because the witness is the Linux binary and the Windows mirror
//     has not been re-measured.
//   PerThreadLocks 0x1c0: the remaining 64 pthread_rwlock_init calls, at a stride of
//     0x38 from 0x1c0 to 0xf88 -- 64 bare 56-byte locks, 0xe00 in all where MSVC has
//     0x200.  PerThreadPrefetches follows that run at 0xfc0.
//   sizeof 0x1898: operator new's argument.  It is the end-to-end check on all of the
//     above plus VirtualTexturePrefetcher's 48-byte CRITICAL_SECTION: drop any one of
//     the lock widths back to Win32's and the mirror misses it.
static_assert(sizeof(VirtualTextureManagerBase) == 0xd8);
static_assert(offsetof(VirtualTextureManager, Renderer) == 0xd8);
static_assert(offsetof(VirtualTextureManager, GraphineDevice) == 0xe8);
static_assert(offsetof(VirtualTextureManager, TexSet0) == 0xf8);
static_assert(offsetof(VirtualTextureManager, TexSet1) == 0x118);
static_assert(offsetof(VirtualTextureManager, Lock2) == 0x138);
static_assert(offsetof(VirtualTextureManager, field_188) == 0x188);
static_assert(offsetof(VirtualTextureManager, field_1b8) == 0x1b8);
static_assert(offsetof(VirtualTextureManager, PerThreadLocks) == 0x1c0);
static_assert(offsetof(VirtualTextureManager, PerThreadPrefetches) == 0xfc0);
static_assert(sizeof(VirtualTextureManager) == 0x1898);
#endif

struct [[bg3::hidden]] ViewDescriptor
{
    uint16_t Format;
    uint8_t BaseMip;
    uint8_t MipCount;
    uint16_t BaseSlice;
    uint16_t SliceCount;
    uint16_t unk;
};

struct [[bg3::hidden]] ImageView
{
    uint64_t unk;
    uint64_t View;
};

struct [[bg3::hidden]] Image
{
    Array<ViewDescriptor> ViewDescriptors;
    uint32_t Width;
    uint32_t Height;
    uint32_t Depth;
    uint16_t unk;
    uint16_t MipCount;
    uint16_t ArraySize;
    uint16_t Usage;
    uint8_t Dimension;
    uint8_t Format;
    uint8_t SampleCount;
    uint8_t Access;
    uint8_t NamedViews;
};

#if defined(BG3SE_LINUX)
// PORT (linux): rf::vk::GPUDevice::GetViewIndex(rf::Texture*, rf::TextureViewDescriptor const&),
// bin/bg3 0x2586f00 (matched to the metal one, s1:0.89), on the texture in rdi: it walks
// `[tex + 0]` in 10-byte steps for `[tex + 0xc]` entries -- ViewDescriptors' buffer and Size,
// sizeof(ViewDescriptor) 10 -- comparing the descriptor's bytes at +0..+9, and reads the u16s at
// +0x1e (MipCount) and +0x20 (ArraySize) and a byte at +0x29 the mirror does not name.
// Views (0x30) and ImageView::View (+8) are witnessed in game by the IMGUI texture path:
// RegisterTexture reads `views 1` off three descriptors the game's LoadTextureHelper hands
// back, ImGui binds `Views[0]->View` as the VkImageView, and the frame shows the two icons and
// the 1024x1024 normal map -- a wrong offset there draws nothing or garbage, and the sizes it
// reads (2048x2048 and 1024x1024) agree with the atlas UVs and the resource's Width/Height.
static_assert(sizeof(ViewDescriptor) == 10);
static_assert(offsetof(Image, ViewDescriptors) == 0x00);
static_assert(offsetof(Image, Width) == 0x10);
static_assert(offsetof(Image, MipCount) == 0x1e);
static_assert(offsetof(Image, ArraySize) == 0x20);
static_assert(offsetof(ImageView, View) == 0x08);
#endif

struct [[bg3::hidden]] ResourceView
{
    uint64_t unk;
    ID3D11ShaderResourceView* View;
};

struct [[bg3::hidden]] TextureDescriptorDX11
{
    ID3D11Resource* Resource;
    Array<void *> unkArr;
    uint32_t Width;
    uint32_t Height;
    uint32_t Depth;
    uint8_t unk[16];
    Array<ResourceView> Views;
};

struct [[bg3::hidden]] TextureDescriptorVulkan
{
    Image ImageData;
    Array<ImageView*> Views;
};

union [[bg3::hidden]] TextureDescriptor
{
    ~TextureDescriptor() {}

    TextureDescriptorDX11 DX11;
    TextureDescriptorVulkan Vulkan;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the in-game half of the witness block above Image -- Views at 0x30, read as
// `views 1` off three descriptors and drawn.
static_assert(offsetof(TextureDescriptorVulkan, Views) == 0x30);
#endif

struct [[bg3::hidden]] TrackedTexture
{
    TextureDescriptor* Descriptor;
    uint32_t RefCount;
    uint32_t WaitingForLoadRefCount;
    uint8_t State;
    STDString Path;
    void* EventObject;
    CRITICAL_SECTION CriticalSection;
    Array<void*> LoadCallbacks;
    uint8_t field_70;
    uint8_t Flags;
    uint32_t CriticalSectionRefCount;
    CRITICAL_SECTION* TextureStreamerCS;
};

struct [[bg3::hidden]] TextureManager
{
#if defined(BG3SE_LINUX)
    // PORT (linux): the game's SRW lock, a bare 56-byte pthread_rwlock_t, not Win32's 8-byte
    // handle -- so both HashMaps were 48 bytes low (witness block below).
    LS_SRWLOCK Lock;
#else
    SRWLOCK Lock;
#endif
    HashMap<TextureDescriptor*, FixedString> Names;
    HashMap<FixedString, TrackedTexture*> Textures;
};

#if defined(BG3SE_LINUX)
// PORT (linux): read off bin/bg3 0x270d1f0, which matches
// ls::ResourceManager::InstantiateTrackedTexture(rf::Texture*) (s4:0.83) but is in fact the
// *TextureManager's* method -- its caller 0x270c0a0 loads ls__gGlobalResourceManager
// (0x7d473a8), reads `[rax + 0xa8]` and passes that as rdi, which also witnesses
// ResourceManager::TextureManager at 0xa8.  With rbx = this:
//   Lock 0x00: rdi goes into the shared read acquire (0x218f6b0) unchanged, and the exit path
//     calls pthread_rwlock_unlock@plt (0x77e8310) on rbx itself.  The first member read is at
//     0x38 = sizeof(pthread_rwlock_t) past it, so the primitive is bare.
//   Names 0x38: a HashMap by hand -- divide the rf::Texture* by the u32 at 0x40, index the
//     int32 buckets at 0x38, compare the *pointer* keys at 0x58 and read a FixedString out of
//     the values at 0x68 (`shl r15, 2` -- a 4-byte element).
//   Textures 0x78: the same shape 0x40 later -- buckets 0x78 / count 0x80, NextIds 0x88, the
//     FixedString keys at 0x98 compared as dwords, and the values at 0xa8 read as pointers,
//     followed by `lock inc dword [value + 8]` = TrackedTexture::RefCount at +8, which is
//     where this mirror has it.
static_assert(sizeof(HashMap<FixedString, TrackedTexture*>) == 0x40);
static_assert(offsetof(TextureManager, Names) == 0x38);
static_assert(offsetof(TextureManager, Textures) == 0x78);
static_assert(sizeof(TextureManager) == 0xb8);
static_assert(offsetof(TrackedTexture, RefCount) == 0x08);
#endif

struct UVValues
{
    glm::vec2 UV0;
    glm::vec2 UV1;
};

struct TextureAtlas : public ProtectedGameObject<TextureAtlas>
{
    [[bg3::hidden]] void* VMT;
    LegacyMap<FixedString, UVValues*> Icons;
    STDString Path;
    STDString TexturePath;
    uint32_t IconWidth;
    uint32_t IconHeight;
    uint32_t TextureWidth;
    uint32_t TextureHeight;
    FixedString TextureUuid;
    resource::TextureResource* Texture;
};

struct TextureAtlasMap : public ProtectedGameObject<TextureAtlasMap>
{
    [[bg3::hidden]] void* VMT;
    LegacyRefMap<STDString, TextureAtlas*> AtlasMap;
    LegacyMap<FixedString, TextureAtlas*> IconMap;
    [[bg3::hidden]] uint32_t Unknown;
};

#if defined(BG3SE_LINUX)
// PORT (linux): both read off the Linux binary (build-id c8da62b1); the mirrors already land
// here under this ABI (a 16-byte STDString), so these pin rather than fix.
// ls::TextureAtlas::Visit, bin/bg3 0x3fde990 (matched r4:6), names every member against its
// offset: the map visitor 0x3fdecd0 on [this + 8] (Icons), ls::Path::GetModPath on
// [this + 0x20] (Path) and ls::Path::Visit on [this + 0x30] under GEFS::strTextureAtlasPath,
// GEFS::strWidth/strHeight into +0x40/+0x44 under strTextureAtlasIconSize and into
// +0x48/+0x4c under strTextureAtlasTextureSize, and FFFS::strUUID into +0x50.
// ui::BrushManager::CreateBrushFromAtlasByIconId 0x39b0540 reads TextureUuid at 0x50 a
// second time (`add rdx, 0x50` on the atlas GetTextureEntry handed back).
// ls::TextureAtlasManager::GetTextureEntry 0x74226d0 (matched r2:2) reads the manager:
// AtlasMap.ItemCount at +8, IconMap.HashSize / HashTable / ItemCount at +0x18 / +0x20 / +0x28,
// and the atlas's Icons.HashSize / HashTable at +8 / +0x10 (nodes: next, key +8, value +0x10).
static_assert(offsetof(TextureAtlas, Icons) == 0x08);
static_assert(offsetof(TextureAtlas, Path) == 0x20);
static_assert(offsetof(TextureAtlas, TexturePath) == 0x30);
static_assert(offsetof(TextureAtlas, IconWidth) == 0x40);
static_assert(offsetof(TextureAtlas, IconHeight) == 0x44);
static_assert(offsetof(TextureAtlas, TextureWidth) == 0x48);
static_assert(offsetof(TextureAtlas, TextureHeight) == 0x4c);
static_assert(offsetof(TextureAtlas, TextureUuid) == 0x50);
static_assert(sizeof(LegacyMap<FixedString, UVValues*>) == 0x18);
static_assert(offsetof(TextureAtlasMap, AtlasMap) == 0x08);
static_assert(offsetof(TextureAtlasMap, IconMap) == 0x18);
static_assert(offsetof(TextureAtlasMap, Unknown) == 0x30);
#endif

struct [[bg3::hidden]] Bank : public ProtectedGameObject<Bank>
{
    void* VMT;
    LegacyMap<FixedString, resource::Resource*> Resources;
#if defined(BG3SE_LINUX)
    // PORT (linux): the game's SRW lock, a bare 56-byte pthread_rwlock_t, not Win32's 8-byte
    // handle (ext/linux/shim/windows.h) -- so BankTypeId and sizeof were 48 bytes low.
    // Resources, the only member the extender reads, sits in front of it and did not move.
    // Witnesses below.
    LS_SRWLOCK SRWLock;
#else
    SRWLOCK SRWLock;
#endif
    ResourceBankType BankTypeId;
};

#if defined(BG3SE_LINUX)
// PORT (linux): read off the class's own Linux constructor, ls::Bank::Bank(ResourceBankType),
// bin/bg3 0x322fd50 (build-id c8da62b1) -- a class with no symbol in either binary, found as
// the one callee of the ResourceContainer constructor below that takes a small integer.
// It says every member outright, in order:
//   VMT 0x00: `mov qword [rdi], rax` with rax the .data.rel.ro address 0x7acd2e8, the base
//     Bank vtable that the container then overwrites per resource type.
//   Resources 0x08: `mov dword [rdi + 8], 0x869` is LegacyMap::HashSize (2153 buckets),
//     `mov qword [r14 + 0x10], rax` its HashTable -- the buffer from a matching
//     `mov edi, 0x4348; call` allocation, and 0x4348 == 0x869 * 8 -- and
//     `mov dword [rdi + 0x18], 0` its ItemCount, so the map ends at 0x20.
//   SRWLock 0x20, and it is the bare primitive: `lea rdi, [r14 + 0x20]; xor esi, esi; call
//     0x77ea020` is pthread_rwlock_init@plt on it, and nothing else in the constructor is
//     touched between it and BankTypeId -- no owner, no recursion count.
//   BankTypeId 0x58: `mov dword [r14 + 0x58], ebx`, ebx being the constructor's own
//     ResourceBankType argument, which the container passes as 0..33 in order.
//   sizeof 0x60, stated by the container's allocation: `mov edi, 0x60` before each of the
//     34 calls to this constructor.
// The macOS oracle agrees on the structure and, as always, not on the offsets past the lock:
// ls::ConcreteBank<ls::IKRigResource>::~ConcreteBank (0x106fd44f0) semaphore_destroys the two
// halves of a 12-byte ls::SRWKernelLock at +0x20/+0x24 and tail-jumps
// ls::FixedStringMap<ls::Resource*>::~FixedStringMap on this + 8, and the macOS container
// constructor (0x106fcbea0) mallocs 0x38 per bank and writes the type at +0x30.
// Nothing in the extender reads BankTypeId or takes this lock; Resources is in front of it,
// which is why Ext.StaticData and the virtual-texture probes do not depend on them.
static_assert(offsetof(Bank, Resources) == 0x08);
static_assert(offsetof(Bank, SRWLock) == 0x20);
static_assert(offsetof(Bank, BankTypeId) == 0x58);
static_assert(sizeof(Bank) == 0x60);
// Derived from the lock's own size rather than hand-counted: the
// constructor initialises the lock at 0x20 and writes the type immediately behind it.
static_assert(offsetof(Bank, BankTypeId) - sizeof(LS_SRWLOCK) == 0x20,
              "the game's rw-lock is bare here -- no owner and no recursion count behind it");
#endif

struct [[bg3::hidden]] ResourceContainer : public ProtectedGameObject<ResourceContainer>
{
    void* VMT;
    std::array<Bank*, (unsigned)ResourceBankType::Sentinel> Banks;
};

#if defined(BG3SE_LINUX)
// PORT (linux): ls::ResourceContainer::ResourceContainer, bin/bg3 0x322f6f0 -- the unrolled
// constructor that allocates the 34 banks.  `lea r14, [rdi + 8]` then a memset of 0x110 bytes
// there is Banks[34], and the last slot written is `[rbx + 0x110]`, so the array starts at
// 0x08 and the object ends at 0x118.  The mirror matches; the assert is here
// because sizeof(Bank) moving would otherwise go unnoticed -- Banks holds pointers.
static_assert(offsetof(ResourceContainer, Banks) == 0x08);
static_assert(sizeof(ResourceContainer) == 0x118);
#endif

struct [[bg3::hidden]] ResourcePackage;

struct [[bg3::hidden]] ResourceBank
{
    void* VMT;
    ResourceContainer Container;
    LegacyMap<FixedString, ResourcePackage*> Packages;
    void* LoadHelper;
    FixedString ModName;

    resource::Resource* GetResource(ResourceBankType type, FixedString const& resource);
};

struct [[bg3::hidden]] ResourcePackage
{
    void* VMT;
    ResourceContainer Container;
    ResourceBank* Bank;
    FixedString PackageName;
    STDString Path;
    bool field_38;
};

struct [[bg3::hidden]] ResourceManager
{
#if defined(BG3SE_LINUX)
    // PORT (linux): where the Windows mirror has a vtable pointer, the Linux game has this
    // class's SRW lock -- a bare 56-byte pthread_rwlock_t, the constructor's first act and
    // the destructor's last (witness block below the struct).  Everything after it was 48
    // bytes low, which put ResourceBanks inside the lock.
    LS_SRWLOCK Lock;
#else
    void* VMT;
#endif
    LegacyMap<FixedString, resource::Resource*> PreviewResources;
    std::array<ResourceBank*, 2> ResourceBanks;
    void* VisualFactory;
    void* EffectDependenciesVMT;
    Array<FixedString> EffectDependencies;
    void* EffectManager;
    void* VisualTemplateManager;
    void* ModelManager;
    void* ModelProxyManager;
    void* ModelProxyTemplateManager;
    TextureManager* TextureManager;
    void* ShaderManager;
    WwiseManager* SoundManager;
    void* VideoManager;
    void* VideoManager2;
    void* GameAnalytics;
    VirtualTextureManager* VirtualTextureManager;
    CRITICAL_SECTION CriticalSection;
    LegacyRefMap<STDString, void*> ResourceDependencies;
    LegacyMap<FixedString, Path*> Sources;
    Array<void*> VisualLoaders;
    LegacyMap<FixedString, void*> GenomeAnimationManagers;
    gn::GenomeTypeManager* GenomeTypeManager;
    ui::GameUI* UI;
    ui::GameUI* UISwap;
#if defined(BG3SE_LINUX)
    // PORT (linux): 8 bytes the mirror did not have -- a pointer the constructor fills last
    // and the destructor deletes first (witness block below).
    void* field_178;
#endif
};

#if defined(BG3SE_LINUX)
// PORT (linux): the whole of ls::ResourceManager, read off its own Linux constructor and
// destructor.  ls::ResourceManager::Create (bin/bg3 0x3f51260, the only function that stores
// into ls__gGlobalResourceManager) is `operator new(0x180)` around a call to the constructor
// at 0x3f512b0, so the game states this struct's sizeof outright; the destructor is 0x7482e30.
//   Lock 0x00, 56 bytes: the constructor's first call is pthread_rwlock_init@plt (0x77ea020)
//     on `rdi` itself with a null attr, and the destructor's last act is a tail
//     `jmp pthread_rwlock_destroy@plt` (0x77ea030) with rdi = this.  So the object *begins*
//     with the bare 56-byte primitive -- no owner and no recursion count behind it, as in
//     resource::VirtualTextureResource -- and the Windows mirror's leading `void* VMT` is
//     that lock seen as Win32's 8-byte SRWLOCK handle.  The constructor stores no vtable
//     pointer anywhere, which is the same reading from the other side.
//   PreviewResources 0x38: `mov dword [rbx+0x38], 0x25` is HashSize = 37 and the
//     ls::MemoryManager::Allocate(0x128) two instructions later is 37 * 8 for HashTable at
//     0x40, with ItemCount cleared at 0x48; the destructor walks that table and calls
//     ls::gst::Map::Release on each node's `+8`, so the key is a FixedString.
//   ResourceBanks 0x50: the constructor's last store is `add rbx, 0x50` /
//     `movups [rbx], xmm0`, zeroing exactly the two pointers; ls::ResourceManager::LoadSound
//     (0x2d14840, matched r2:2) then reads `[rdi + sel*8 + 0x50]` -- sel is a thread-local
//     byte -- and calls a virtual at `[bank+8]`'s vtable + 0x30 with esi = 9 =
//     ResourceBankType::Sound, i.e. ResourceBank::Container at +8 as the mirror has it.
//   CriticalSection 0xe0, 48 bytes: `lea r15, [rbx+0xe0]` before the member's constructor
//     call in the ctor, `lea rdi, [rbx+0xe0]` before pthread_mutex_destroy@plt (0x77e8ee0)
//     in the dtor, which also clears the SpinCount dword at 0x10c (= 0xe0 + 0x2c, the last
//     dword of the 48-byte lock).  ls::ResourceManager::RemoveSource (0x279bdc0, matched
//     r3:2) takes it: pthread_mutex_trylock@plt on `rdi+0xe0`, then `inc dword [+0x108]`.
//   ResourceDependencies 0x110, 16 bytes: constructed at 0x110, and RemoveSource hashes an
//     STDString key through (anonymous namespace)::Hash(StringView) (0x2515450), divides by
//     the u32 at 0x114 and indexes the table at 0x118 -- RefMapInternals' {ItemCount,
//     HashSize, HashTable} exactly.
//   Sources 0x120: `mov dword [rbx+0x120], 0x191` is HashSize = 401 with an Allocate(0xc88)
//     = 401 * 8 for HashTable at 0x128; RemoveSource divides by 0x120 and indexes 0x128,
//     comparing a FixedString index against each node's `+8` and reading a string out of
//     `+0x10`.
//   VisualLoaders 0x138: the buffer of three 24-byte elements is stored at 0x138 and
//     `mov qword [rbx+0x140], 3` is capacity 3 with size 0.
//   GenomeAnimationManagers 0x148: HashSize = 3 at 0x148, Allocate(0x18) = 3 * 8 at 0x150,
//     ItemCount cleared at 0x158.
//   GenomeTypeManager 0x160, UI 0x168, UISwap 0x170: the three pointers the constructor
//     zeroes between the last map and field_178 (`movups [rbx+0x160]`, `[rbx+0x170] = 0`);
//     the census over every site that loads the global reads 0x168 58 times, the most of
//     any field past the lock, and always from ecl::UISystem's update.
//   field_178: `lea rdi, [rbx+0x178]` / call 0x3f51e20, which is `operator new(0xa0)` and a
//     constructor stored back into `[rdi]`; the destructor deletes it first, through the
//     vtable slot at +8, and App::Update (0x2d1b960) reads it.  sizeof is its end.
//   TextureManager 0xa8: the caller of the TextureManager method at 0x270d1f0 (0x270c0a0)
//     loads this global and reads `[rax + 0xa8]` as the object it calls it on -- so the whole
//     run of pointers from VisualFactory 0x60 to VirtualTextureManager 0xd8 is right as
//     declared, the CriticalSection at 0xe0 being its other end.
static_assert(offsetof(ResourceManager, PreviewResources) - sizeof(LS_SRWLOCK) == 0,
    "PreviewResources starts where the game's rw-lock ends");
static_assert(offsetof(ResourceManager, ResourceBanks) == 0x50);
static_assert(offsetof(ResourceManager, EffectManager) == 0x80);
static_assert(offsetof(ResourceManager, TextureManager) == 0xa8);
static_assert(offsetof(ResourceManager, SoundManager) == 0xb8);
static_assert(offsetof(ResourceManager, VirtualTextureManager) == 0xd8);
static_assert(offsetof(ResourceManager, CriticalSection) == 0xe0);
static_assert(offsetof(ResourceManager, ResourceDependencies) == 0x110);
static_assert(offsetof(ResourceManager, Sources) == 0x120);
static_assert(offsetof(ResourceManager, VisualLoaders) == 0x138);
static_assert(offsetof(ResourceManager, GenomeAnimationManagers) == 0x148);
static_assert(offsetof(ResourceManager, UI) == 0x168);
static_assert(offsetof(ResourceManager, field_178) == 0x178);
static_assert(sizeof(ResourceManager) == 0x180);
#endif

END_SE()

BEGIN_NS(resource)

struct Resource : public ProtectedGameObject<Resource>
{
    virtual ~Resource() = 0;
    virtual void* GetLogInfo(void*) = 0;
    virtual STDString* ToLogString(STDString&) = 0;
    virtual void SetUUID(FixedString const&) = 0;
    virtual void VMT18(FixedString const&) = 0;
    virtual void ForceUnload(ResourceManager* mgr) = 0;
    virtual bool AcquireRef(ResourceManager* mgr) = 0;
    virtual bool ReleaseRef(ResourceManager* mgr) = 0;
    virtual bool IsLoaded() = 0;
    virtual bool IsLoadFailed() = 0;
    virtual uint32_t GetType() = 0;
    virtual Resource* Clone() = 0;
    virtual bool Visit(ObjectVisitor& visitor) = 0;

    Path SourceFile;
    [[bg3::readonly]] FixedString Guid;
    [[bg3::readonly]] bool IsModded;
};

struct TwoStepLoadableResource : public Resource
{
    [[bg3::hidden]] void* TwoStepLoadableResource_VMT;
    __int64 field_30;
#if defined(BG3SE_LINUX)
    // PORT (linux): the game's SRW lock, a bare 56-byte pthread_rwlock_t, not Win32's 8-byte
    // handle (ext/linux/shim/windows.h).  This base is the widest-reaching of the lock fixes:
    // twelve resource classes derive from it and every member of all twelve was 48 bytes low,
    // including the FixedStrings that Ext.Resource proxies to Lua.  Witnesses below.
    [[bg3::hidden]] LS_SRWLOCK lock;
#else
    [[bg3::hidden]] SRWLOCK lock;
#endif
    [[bg3::hidden]] void* LoadJob;
};

#if defined(BG3SE_LINUX)
// PORT (linux): read off ls::TwoStepLoadableResource's own Linux destructor, bin/bg3 0x35f3b40
// -- reached as the tail jump of ls::BlendSpaceResource::~BlendSpaceResource (0x7482100,
// matched r2:2) and ls::AnimationBlueprintResource::~AnimationBlueprintResource (0x74822f0,
// matched r4:2), which is how a class with no symbol of its own was named.  It says the whole
// block outright:
//   TwoStepLoadableResource_VMT 0x20: `mov qword [rdi + 0x20], rax` with rax a .data.rel.ro
//     address is the base's own vtable store, the second vptr of the object.
//   lock 0x30, and it is the bare primitive: `lea r14, [rdi + 0x30]` is passed to the shared
//     read acquire (0x218f6b0, a pthread_rwlock_tryrdlock spin that tail-jumps
//     pthread_rwlock_rdlock@plt), then to pthread_rwlock_unlock@plt (0x77e8310) and finally to
//     **pthread_rwlock_destroy@plt** (0x77ea030).  No owner or recursion count follows it.
//   LoadJob 0x68: `mov rcx, qword [rbx + 0x68]` between the acquire and the release is the job
//     the destructor waits on, so it is the member the lock guards.  0x68 - 0x30 = 0x38.
//   sizeof 0x70, from the two derived destructors above: BlendSpaceResource destroys its first
//     member at `this + 0x70` and AnimationBlueprintResource its LegacyRefMap at `this + 0x88`
//     with the Array buffer before it at `this + 0x78`, i.e. VMT3 0x70 / Blueprints 0x78 /
//     Params 0x88 with nothing between the base and them.
// The same destructor re-establishes Resource: `mov qword [rbx], rax` the primary vtable,
// `[rbx + 0x18]` the Guid released through ls::gst::Map::Release, and the SSO byte at
// `[rbx + 0x17]` guarding the free of `[rbx + 8]` -- SourceFile 0x08, Guid 0x18, so Resource
// ends at 0x20 as the mirror has it.
static_assert(offsetof(TwoStepLoadableResource, TwoStepLoadableResource_VMT) == 0x20);
static_assert(offsetof(TwoStepLoadableResource, lock) == 0x30);
static_assert(offsetof(TwoStepLoadableResource, LoadJob) == 0x68);
static_assert(sizeof(TwoStepLoadableResource) == 0x70);
// Derived from the lock's own size rather than hand-counted: LoadJob
// sits immediately behind a lock that begins at 0x30.
static_assert(offsetof(TwoStepLoadableResource, LoadJob) - sizeof(LS_SRWLOCK) == 0x30,
              "the game's rw-lock is bare here -- no owner and no recursion count behind it");
#endif

struct LoadableResource : public Resource
{
    using LoadProc = bool (LoadableResource* self, ResourceManager* mgr);
    using UnloadProc = bool (LoadableResource* self, ResourceManager* mgr);
    using TranscodeProc = __int64 (void* self, void* transcodeData, void* source, __int64 sourceSize, int width, int height, void* pSaveMip, void* paramBlock, int a9, void* destination, int expectedBCFormat);

    struct [[bg3::hidden]] VMT
    {
        void* Dtor;
        void* GetMetadata;
        void* DebugDump;
        void* SetGuid;
        void* VMT18;
        void* ForceUnload;
        void* IncRef;
        void* DecRef;
        void* IsInState2;
        void* IsInState3;
        void* GetType;
        void* Clone;
        void* Visit;
        LoadProc* Load;
        UnloadProc* Unload;
    };

    virtual bool Load(ResourceManager* mgr) = 0;
    virtual bool Unload(ResourceManager* mgr) = 0;

    [[bg3::readonly]] uint16_t RefCount;
    [[bg3::readonly]] uint8_t State;
};

struct PresetData
{
    struct Mapped
    {
        FixedString GroupName;
        FixedString MaterialPresetResource;
        bool ForcePresetValues;
        [[bg3::hidden]] int8_t field_9;
        [[bg3::hidden]] int16_t field_a;
    };

    Array<material::ScalarParameterPreset> ScalarParameters;
    Array<material::Vector2ParameterPreset> Vector2Parameters;
    Array<material::Vector3ParameterPreset> Vector3Parameters;
    Array<material::VectorParameterPreset> VectorParameters;
    Array<material::Texture2DParameterPreset> Texture2DParameters;
    Array<material::VirtualTextureParameterPreset> VirtualTextureParameters;
    FixedString MaterialResource;
    [[bg3::hidden]] __int32 field_64;

    HashMap<FixedString, Mapped> MaterialPresets;
};

struct AnimationResource : public TwoStepLoadableResource
{
    struct Event
    {
        FixedString ID;
        float Time;
        float Length;
        uint8_t Track;

        TextKeyTypeProperties* Properties;
    };

    Array<Event*> Events;
    FixedString Template;
    FixedString AdditiveLoopingAnimationID;
    FixedString LeftTransitionAnimation;
    FixedString RightTransitionAnimation;
    // FixedString PreviewVisualResource;
    FixedString SkeletonResource;
    float TimeStep;
    float Duration;
    float Offset;
    bool Looping;
    // bool IsPoseBank;
    uint8_t AnchorHand;
    uint8_t SupportingLeg;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the strongest of the witnesses for the 0x70 base, because ls::ObjectVisitor
// names each member.  ls::AnimationResource::Visit (bin/bg3 0x243b500, matched r2:2) visits
// ls::FFFS::strTemplate on `rbx + 0x80`, three floats on 0x94 / 0x98 / 0x9c through the visitor
// slot at vtable+0xf0 and a bool on 0xa0 through vtable+0xc0; the Events array's buffer is
// `[rbx + 0x70]` with its capacity at 0x78 and its size at 0x7c, and the Event it allocates is
// `operator new(0x18)` = the mirror's 24-byte Event.  Five FixedStrings fit exactly between
// Template 0x80 and TimeStep 0x94.
static_assert(offsetof(AnimationResource, Events) == 0x70);
static_assert(offsetof(AnimationResource, Template) == 0x80);
static_assert(offsetof(AnimationResource, TimeStep) == 0x94);
static_assert(offsetof(AnimationResource, Duration) == 0x98);
static_assert(offsetof(AnimationResource, Offset) == 0x9c);
static_assert(offsetof(AnimationResource, Looping) == 0xa0);
static_assert(sizeof(AnimationResource::Event) == 0x18);
#endif

struct AnimationBlueprintResource : public TwoStepLoadableResource
{
    [[bg3::hidden]] void* VMT3;
    Array<gn::GenomeBlueprint*> Blueprints;
    LegacyRefMap<bg3se::Guid, gn::GenomeVariant> Params;
    // Editor only
    // Guid PreviewVisualResourceID;
};

#if defined(BG3SE_LINUX)
// PORT (linux): ls::AnimationBlueprintResource::~AnimationBlueprintResource (bin/bg3 0x74822f0,
// matched r4:2) destroys Params at `this + 0x88` and frees the Blueprints buffer at
// `[this + 0x78]` through ls::DynamicArray<>::~DynamicArray before tail-jumping to the base
// destructor -- so VMT3 sits at 0x70, immediately after a 0x70-byte base.
static_assert(offsetof(AnimationBlueprintResource, VMT3) == 0x70);
static_assert(offsetof(AnimationBlueprintResource, Blueprints) == 0x78);
static_assert(offsetof(AnimationBlueprintResource, Params) == 0x88);
#endif

struct AnimationDesc
{
    FixedString ID;
    AnimationSetAnimationFlags flags;
};

struct AnimationSubSet
{
    LegacyRefMap<FixedString, AnimationDesc> Animation;
    FixedString FallBackSubSet;
};

struct AnimationSet
{
    LegacyRefMap<FixedString, AnimationSubSet> AnimationSubSets;
};

struct AnimationSetResource : public LoadableResource
{
    AnimationSet* AnimationBank;
};

struct Atmosphere
{
    [[bg3::hidden]] void* VMT;
    FixedString GUID;
    uint32_t InheritanceFlags;
    PostProcessSetting PostProcess;
    float WindDirection;
    float WindSpeed;
    float ClothWindSpeed;
    float ClothWindVariance;
    float ClothMainWindSpeed;
    float ClothMainWindFrequency;
    float ClothWindDirectionOffsetFrequency;
    float ClothMaxWindDirectionOffset;
    float NearPlane;
    float FarPlane;
    bool EnvironmentEffectGlobalEnabled;
    std::array<bool, 4> EnvironmentEffectEnabled;
    std::array<bool, 4> EnvironmentEffectEnabledForTimeline;
    std::array<FixedString, 4> EnvironmentEffect;
    std::array<float, 4> EnvironmentEffectOffset;
    bool LocalLightSourceEnabled;
    bool LocalLightSourceOverrideSettings;
    glm::fvec3 LocalLightSourceColor;
    float LocalLightSourceIntensity;
    Guid TimelineAutomaticLightingDefaultSetup;
    bool TimelineAutomaticLightingDisableFlip;
    Guid Atmosphere;
};

struct AtmosphereResource : public LoadableResource
{
    Atmosphere* Atmosphere;
    Array<FixedString> Labels;
};

struct BlendSpaceResource : public TwoStepLoadableResource
{
    struct [[bg3::hidden]] BlendSpaceInternals;

    struct Input
    {
        glm::vec2 Position;
        FixedString AnimationID;
        FixedString ShortNameID;
    };

    struct InputInfo
    {
        [[bg3::hidden]] void* ContainingInternals; // Points to XAxisName
        glm::vec2 Position;
        FixedString AnimationID;
        FixedString ShortNameID;
    };

#if defined(BG3SE_LINUX)
    // PORT (linux): ls::BlendSpaceResource::~BlendSpaceResource (bin/bg3 0x7482100, matched
    // r2:2) destroys this class's own part at `this + 0x70` and then tail-jumps to the base
    // destructor, so the first member is at 0x70.  The assert is on
    // the member itself, below the struct.
#endif
    STDString XAxisName;
    STDString YAxisName;
    int32_t XElements;
    int32_t YElements;
    float MinXValue;
    float MaxXValue;
    float MinYValue;
    float MaxYValue;

    struct Adjustment
    {
        glm::fvec3 Adjustment;
        std::array<uint16_t, 3> indexes;
        uint16_t index;
    };

    struct FieldB8Entry
    {
        glm::fvec2 field_0;
        glm::fvec2 field_8;
        glm::fvec2 field_10;
        glm::fvec2 field_18;
        __int32 field_20;
        __int32 field_24;
        __int32 field_28;
        glm::fvec2 field_2c;
        __int32 field_34;
        __int32 field_38;
        __int32 field_3c;
        glm::fvec2 field_40;
        __int32 field_48;
        __int32 field_4c;
        __int32 field_50;
        glm::fvec2 field_54;
        __int32 field_5c;
    };

    Array<Adjustment> Adjustments;

    Array<InputInfo*> Inputs;
    bool NeedsAdjustmentCalculation;
    [[bg3::hidden]] __int8 field_b1;
    [[bg3::hidden]] __int16 field_b2;
    [[bg3::hidden]] __int32 field_b4;
    Array<FieldB8Entry> field_b8;
};

#if defined(BG3SE_LINUX)
// PORT (linux): see the comment on XAxisName -- the derived destructor's `add rdi, 0x70` names
// where this class's own part begins.
static_assert(offsetof(BlendSpaceResource, XAxisName) == 0x70);
#endif

struct VisualSet
{
    struct Slot
    {
        FixedString Slot;
        FixedString VisualResource;
        FixedString Bone;
    };

    struct LocatorAttachment
    {
        FixedString LocatorId;
        FixedString VisualResource;
    };

    FixedString BodySetVisual;
    Array<Slot> Slots;
    Array<LocatorAttachment> LocatorAttachments;
    PresetData MaterialOverrides;
    LegacyMap<FixedString, PresetData> Materials;
    HashMap<FixedString, FixedString> RealMaterialOverrides;
    [[bg3::legacy(field_158)]] HashMap<FixedString, FixedString> MaterialRemaps;
    FixedString ID;
    bool ShowEquipmentVisuals;
};

struct CharacterVisualResource : public LoadableResource
{
    VisualSet VisualSet;
    FixedString BaseVisual;
};

struct ClothColliderResource : public LoadableResource
{
    struct Sphere
    {
        glm::fvec3 Position;
        float Radius;
        FixedString Name;
        [[bg3::hidden]] __int32 field_14;
        Array<FixedString> Links;
        FixedString AttachedName;
        [[bg3::hidden]] __int32 field_2c;
    };

    Array<Sphere> Spheres;
};

struct ColorListResource : public LoadableResource
{
    [[bg3::hidden]] __int64 field_30; // Dynamic array vtable
    Array<uint32_t> Colors;
    [[bg3::hidden]] __int64 field_48; // Dynamic array other info

    [[bg3::hidden]] __int64 field_50; // Dynamic array vtable
    Array<int32_t> Ids;
    [[bg3::hidden]] __int64 field_68; // Dynamic array other info

    int32_t IdGenerator;
    [[bg3::hidden]] __int32 field_74;
};

struct DialogResource : public LoadableResource
{
    Array<bg3se::Guid> ChildResources;
    Array<uint8_t> SpeakerSlotsWithLines;
    STDString Name;
    uint8_t TimelineSplitScreenPriority;
    uint8_t Flags;
    uint8_t Flags2;
};

struct DiffusionProfileResource : public LoadableResource
{
    glm::fvec3 ScatterColor;
    float ScatterDistance;
    glm::fvec3 TransmissionTint;
    float ThicknessRemapMin;
    float ThicknessRemapMax;
    uint8_t ThickObjectTransmission; // Sometimes filled with junk values, unknown why
    float DualSpecularRoughnessA;
    float DualSpecularRoughnessB;
    float DualSpecularMix;
};

#if defined(BG3SE_LINUX)
// PORT (linux): read off ls::DiffusionProfileResource::Visit, bin/bg3 0x3ed3440
// (_ZN2ls24DiffusionProfileResource5VisitEPNS_13ObjectVisitorE, matched s1:1.00 to macOS
// 0x106c3bdf0).  The class has no Linux constructor -- LTO inlined it -- but the visitor names
// every member at once: it tail-calls ls::Resource::Visit (0x243ba80) for the base and then
// makes one visitor call per derived member with `lea rdx, [r14 + <offset>]`, so each offset is
// an argument rather than a lane of a merged store.  The visitor *slot* carries the type, and
// the slot-to-type mapping is consistent across the whole family (the same three slots in
// ls::VirtualTextureResource::Visit 0x3352860 below):
//     +0x120  glm::fvec3, with rcx a pointer to the 12-byte default
//     +0xf0   float, with the default in xmm0
//     +0xc0   uint8_t/bool, with the default in ecx
//   0x3ed348e  lea rdx, [r14+0x24]  slot 0x120, default (1,1,1) at 0x1e94448  ScatterColor
//   0x3ed34b4  lea rdx, [r14+0x30]  slot 0xf0,  default 20                    ScatterDistance
//   0x3ed34cb  lea rdx, [r14+0x34]  slot 0x120, default (1,1,1) at 0x1e94454  TransmissionTint
//   0x3ed34e9  lea rdx, [r14+0x40]  slot 0xf0,  default 0 (xorps)             ThicknessRemapMin
//   0x3ed350b  lea rdx, [r14+0x44]  slot 0xf0,  default 20                    ThicknessRemapMax
//   0x3ed3522  lea rdx, [r14+0x48]  slot 0xc0,  default 0 (xor ecx)           ThickObjectTransmission
//   0x3ed3543  lea rdx, [r14+0x4c]  slot 0xf0,  default 0.75                  DualSpecularRoughnessA
//   0x3ed3562  lea rdx, [r14+0x50]  slot 0xf0,  default 1.3                   DualSpecularRoughnessB
//   0x3ed3581  add r14, 0x54        slot 0xf0,  default 0.85                  DualSpecularMix
// The two 12-byte defaults sit 12 bytes apart in .rodata, which is what says slot 0x120 takes a
// vec3 and not a float -- nine members, nine calls, three slots, no member unaccounted for.
// sizeof: ls::DiffusionProfileResource::CreateClone 0x748e130 (matched r2:2) does
// `mov edi, 0x58; call operator new` and then copies the derived block as three 16-byte moves
// from [r15+0x24], [r15+0x34], [r15+0x44] plus a dword at [r15+0x54] -- 0x24 to 0x58 exactly,
// which confirms ScatterColor's 0x24 a second way.  And here sizeof *is* an ABI witness: MSVC
// would put ScatterColor at 0x28 and make the record 0x60.
static_assert(offsetof(DiffusionProfileResource, ScatterColor) == 0x24);
static_assert(offsetof(DiffusionProfileResource, ScatterDistance) == 0x30);
static_assert(offsetof(DiffusionProfileResource, TransmissionTint) == 0x34);
static_assert(offsetof(DiffusionProfileResource, ThicknessRemapMin) == 0x40);
static_assert(offsetof(DiffusionProfileResource, ThicknessRemapMax) == 0x44);
static_assert(offsetof(DiffusionProfileResource, ThickObjectTransmission) == 0x48);
static_assert(offsetof(DiffusionProfileResource, DualSpecularRoughnessA) == 0x4c);
static_assert(offsetof(DiffusionProfileResource, DualSpecularRoughnessB) == 0x50);
static_assert(offsetof(DiffusionProfileResource, DualSpecularMix) == 0x54);
static_assert(sizeof(DiffusionProfileResource) == 0x58);
#endif

struct TimelineConstructor
{
    aspk::TimelineHeader Header;
    Array<aspk::Input*> Inputs;
    Array<aspk::Component*> EffectComponents;
    bool Loaded;
    FixedString EffectName;
};

struct EffectResource : public TwoStepLoadableResource
{
    FixedString EffectName;
    Array<aspk::Component*> EffectComponents;
    Array<FixedString> Dependencies;
    TimelineConstructor* Constructor;
    [[bg3::hidden]] void* TimelineContent;
    glm::fvec3 BoundsMin;
    glm::fvec3 BoundsMax;
    float CullingDistance;
    float Duration;
    bool Looping;
    bool UseSoundOcclusion;
    uint32_t InterruptionMode;
    // Editor only
    // bool PreRollMode;
    // [[bg3::hidden]] Array<void*> TimelineDependencies;
};

struct FCurveResource : public TwoStepLoadableResource
{
    struct CurveKey
    {
        [[bg3::hidden]] void* ContainingInternals; // Pointer to the CurveKeys of containing FCurveResource
        glm::fvec2 CurveKeyPosition;
        float CurveKeyLeftTangent;
        float CurveKeyRightTangent;
        int32_t CurveKeyInterpolationType;
        int32_t CurveKeyConstraintType;
    };

    struct CurveLimit
    {
        float MinX;
        float MinY;
        float MaxX;
        float MaxY;
    };

    Array<CurveKey*> CurveKeys;

    CurveLimit CurveLimits;

    struct ProcessedCurveKey
    {
        glm::fvec2 CurveKeyPosition;
        float Tan_CurveKeyLeftTangent;
        float Tan_CurveKeyRightTangent;
        int32_t CurveKeyInterpolationType;
    };

    Array<ProcessedCurveKey> ProcessedCurveKeys;
};

struct IKRigResource : public LoadableResource
{
    struct BoneProperty
    {
        bool Enabled;
        bool DoFIncludesX;
        bool DoFIncludesY;
        bool DoFIncludesZ;
        bool LimitEnabled;
        uint8_t LimitAxis;
        [[bg3::hidden]] __int16 field_6;
        glm::fvec3 Weight;
        glm::fvec3 Min;
        glm::fvec3 Max;
    };

    struct IKTask
    {
        FixedString field_0;
        FixedString Bone;
        int32_t IKBoneType;
        FixedString IKBoneName;
        FixedString IKBoneCachedName;
        bool PositionEnabled;
        bool RotationEnabled; // RotationEnabled?
        bool PositionDoFIncludesX;
        bool PositionDoFIncludesY;
        bool PositionDoFIncludesZ;
        bool RotationDoFIncludesX;
        bool RotationDoFIncludesY;
        bool RotationDoFIncludesZ;
        int32_t PositionDepth;
        int32_t RotationDepth;
        float PositionWeight;
        float RotationWeight;
        glm::fvec3 LookAtAxis;
        bool IsPoleObject;
        __int8 field_39;
        bool IsParentTask;
        bool LookAtLocalSpace;
        bool LookAt;
        [[bg3::hidden]] __int8 field_3d;
        [[bg3::hidden]] __int16 field_3e;
    };

    LegacyRefMap<FixedString, BoneProperty> BoneProperties;

    [[bg3::hidden]] void* field_40; // DynamicArray VMT
    Array<IKTask> IKTasks;
    [[bg3::hidden]] __int64 field_58; // DynamicArray extra data

    bool SolverRoot;
    [[bg3::hidden]] __int8 field_61;
    [[bg3::hidden]] __int16 field_62;
    [[bg3::hidden]] __int32 field_64;
    HashMap<bg3se::Guid, FixedString> BoneCategories;
};

struct LightCookieResource : public TwoStepLoadableResource
{
    BitArray<uint32_t, 0x80> Data;
};

struct LightingResource : public LoadableResource
{
    Lighting* Lighting;
};

struct MaterialResource : public TwoStepLoadableResource
{
    Material* Instance;
    TrackedCompactSet<material::ScalarResourceParameter> ScalarParameters;
    TrackedCompactSet<material::Vector2ResourceParameter> Vector2Parameters;
    TrackedCompactSet<material::Vector3ResourceParameter> Vector3Parameters;
    TrackedCompactSet<material::Vector4ResourceParameter> VectorParameters;
    TrackedCompactSet<material::Texture2DResourceParameter> Texture2DParameters;
    TrackedCompactSet<material::VirtualTextureResourceParameter> VirtualTextureParameters;
    FixedString DiffusionProfileUUID;
    MaterialType MaterialType;
    RenderChannel RenderChannel;
};

struct MaterialPresetResource : public LoadableResource
{
    PresetData Presets;
};

// I think this is unimplemented and std::terminates on construction
struct MaterialSetResource : public TwoStepLoadableResource
{
};

struct MeshProxyResource : public TwoStepLoadableResource
{
    __int64 field_48; // Size 0x30
    FixedString Template;
    [[bg3::hidden]] __int32 field_54;
};

struct PhysicsResource : public LoadableResource
{
    struct ObjectTemplate
    {
        struct PhysicsObject
        {
            virtual ~PhysicsObject() = 0;
            virtual const FixedString& GetType() = 0;
        };

        struct PhysicsBox : public PhysicsObject
        {
            [[bg3::hidden]] void* VMT;
            bool Kinematic;
            [[bg3::hidden]] __int8 field_9;
            [[bg3::hidden]] __int16 field_a;
            float Mass;
            FixedString UUID;
            [[bg3::hidden]] __int32 field_14;
            glm::fvec3 Extends;
            [[bg3::hidden]] __int32 field_24;
        };

        struct PhysicsCapsule : public PhysicsObject
        {
            [[bg3::hidden]] void* VMT;
            bool Kinematic;
            [[bg3::hidden]] __int8 field_9;
            [[bg3::hidden]] __int16 field_a;
            float Mass;
            FixedString UUID;
            [[bg3::hidden]] __int32 field_14;
            float Height;
            float Radius;
        };

        FixedString UUID;
        [[bg3::hidden]] __int32 field_4;
        Array<PhysicsObject*> PhysicsObjects;
    };

    Array<ObjectTemplate*> ObjectTemplates;
    Array<FixedString> IgnoreColliderList;
    FixedString Template;
    bool field_54;
    [[bg3::hidden]] __int8 field_55;
    [[bg3::hidden]] __int16 field_56;
};

struct ScriptResource : public LoadableResource
{
    struct Parameter
    {
        int32_t Type;
        [[bg3::hidden]] __int32 field_4;
        STDString Value;
    };

    LegacyMap<FixedString, Parameter*> Parameters;
    // PORT (linux): added.  The game's record is 0x48 bytes, not 0x40 -- one byte past the
    // map, which ends at 0x40.  ls::ScriptResource::CreateClone bin/bg3 0x748c400 does
    // `mov edi, 0x48; call operator new` and then `mov byte [r14+0x40], bl` with bl zero
    // (0x748c4c3); the macOS twin 0x107024b50 and the bank that reads one off disk,
    // ls::ConcreteBank<ls::ScriptResource>::VisitResourceReadMapItem 0x106fd9610, both
    // allocate 0x48 as well.  Not serialised -- ls::ScriptResource::Visit (macOS 0x107025040)
    // visits only FFFS::strParameters at +0x28 -- so it is runtime state and stays hidden.
    // Truncation, not an ABI shift: nothing in front of it moves.
    [[bg3::hidden]] uint8_t field_48;
};

struct SkeletonResource : public TwoStepLoadableResource
{
    struct Socket
    {
        FixedString Name;
        FixedString Bone;
        FixedString BoneConstraint;
        uint16_t ConstraintsPos;
        uint16_t ConstraintsRot;
        glm::fvec4 Rotation;
        glm::fvec3 Position;
    };

    struct Bone
    {
        struct BoneGroup
        {
            int32_t IKBoneType;
            FixedString IKBoneName;
            FixedString IKBoneCachedName;
        };
        Array<BoneGroup> BoneGroups;
        glm::fvec3 Position;
        glm::fvec4 Rotation;
        float SoundObjectActivationRange;
        uint8_t SoundObjectIndex;
        bool ShouldLOD;
        [[bg3::hidden]] __int16 field_32;
        [[bg3::hidden]] __int32 field_34;
    };

    LegacyRefMap<FixedString, Bone> Bones;
    [[bg3::hidden]] void* field_58; // DynamicArray VMT
    Array<Socket> Sockets;
    [[bg3::hidden]] __int64 field_70; // DynamicArray extra data
    FixedString RagdollResourceID;
    FixedString SpringResourceID;
    FixedString DynamicPhysicsResourceID;
    FixedString SoftbodyResourceID;
    FixedString Template;
    FixedString IKRigResourceID;
    FixedString ClothColliderResourceID;
    FixedString MirrorTableResourceID;
    // Editor build only
    // uint64_t field_98;
    // FixedString PreviewVisualResource;
    // FixedString PreviewAnimationResource;
};

struct SkeletonMirrorTableResource : public TwoStepLoadableResource
{
    struct Entry
    {
        FixedString LeftBone;
        FixedString RightBone;
    };

    Array<Entry> Entries;
};

struct SkinPresetResource : public LoadableResource
{
    PresetData Presets;
};

struct SoundResource : public Resource
{
    FixedString SoundEvent;
    uint32_t SoundEventID;
    float Duration;
    float MaxDistance;
    AudioCodec SoundCodec;
    uint8_t Flags;
};

struct TerrainBrushResource : public LoadableResource
{
    FixedString BaseColorMap;
    FixedString NormalMap;
    FixedString PhysicalMap;
    FixedString DetailNormalMap;
    float HeightScale;
    float HeightOffset;
    float HeightContrast;
    float AlphaThreshold_maybe;
    float Tiling;
    float DetailNormalScale;
    float DetailNormalTiling;
    float GradientHeightBottom;
    float GradientHeightMidPoint;
    float GradientHeightTop;
    float GradientRoughnessReach;
    float GradientRoughnessPower;
    glm::fvec3 GradientBottomHeightColor;
    glm::fvec3 GradientMidHeightColor;
    bool UseDetailNormalMap;
    bool UseHeightGradient_maybe;
    bool IsBlackBrush;
    MaterialType MaterialType;
    [[bg3::hidden]] __int32 field_8c;
};

#if defined(BG3SE_LINUX)
// PORT (linux): read off ls::TerrainBrushResource's own Linux constructor, bin/bg3 0x38448b0
// (_ZN2ls20TerrainBrushResourceC1Ev, matched s3:1.00 to macOS 0x1070f75b0).  Every derived
// member gets a non-default default, so the whole record is value stores rather than a zero
// fill.  The four FixedStrings are each acquired from a global through ls::gst::Acquire
// (0x237a5a0) and then stored, and one of those globals is named by the match --
// GEFS::strEngine_Default_PM_UUID at 0x7d47a60 lands at +0x2c, which is PhysicalMap, and the
// same global (0x7d47dc8) feeds +0x28 and +0x30, which is one default normal map shared by
// NormalMap and DetailNormalMap.  That names the class a second way, independent of the match.
//   0x3844914  mov [rbx+0x24], ebp   BaseColorMap      (gst global 0x7d47da0)
//   0x3844929  mov [rbx+0x28], ebp   NormalMap         (gst global 0x7d47dc8)
//   0x384493e  mov [rbx+0x2c], ebp   PhysicalMap       (GEFS::strEngine_Default_PM_UUID)
//   0x3844968  mov [rbx+0x30], ebp   DetailNormalMap   (gst global 0x7d47dc8 again)
// then five SSE stores of .rodata constants, whose lanes are the twelve floats and two vec3s in
// declaration order -- a lane that is zero is a zero *inside* a value-bearing 16-byte store, so
// the block's start and the neighbouring non-zero lanes fix every boundary:
//   0x384496b  movups [rbx+0x34], 0x1b6de80 = 3.5, 0, 1, 0
//                HeightScale 0x34, HeightOffset 0x38, HeightContrast 0x3c, AlphaThreshold 0x40
//   0x384496f  movups [rbx+0x44], 0x1b70610 = 0.25, 0.6, 0.0278, -0.25
//                Tiling 0x44, DetailNormalScale 0x48, DetailNormalTiling 0x4c,
//                GradientHeightBottom 0x50
//   0x3844973  movups [rbx+0x54], 0x1b701a0 = 0.1, 10.5, 0.4, 5
//                GradientHeightMidPoint 0x54, GradientHeightTop 0x58,
//                GradientRoughnessReach 0x5c, GradientRoughnessPower 0x60
//   0x3844986  movups [rbx+0x64], 0x1b73460 = 0.139, 0.154, 0.111, 0.639
//                GradientBottomHeightColor 0x64, and the .x of GradientMidHeightColor at 0x70
//   0x384498a  movsd  [rbx+0x74], 0x1b746f0 = 0.494, 0.28   its .y and .z
//   0x384498f  mov dword [rbx+0x7c], 1      UseDetailNormalMap 0x7c
// The three bools at 0x7d/0x7e/0x7f and field_8c at 0x80 are the zero bytes of that last dword
// and beyond it, so they get no assert.  sizeof is no witness here either: MSVC would make the
// record 0x88 as well, because the shift of the FixedString into the base's tail padding is
// absorbed by field_8c's alignment.
static_assert(offsetof(TerrainBrushResource, BaseColorMap) == 0x24);
static_assert(offsetof(TerrainBrushResource, NormalMap) == 0x28);
static_assert(offsetof(TerrainBrushResource, PhysicalMap) == 0x2c);
static_assert(offsetof(TerrainBrushResource, DetailNormalMap) == 0x30);
static_assert(offsetof(TerrainBrushResource, HeightScale) == 0x34);
static_assert(offsetof(TerrainBrushResource, HeightOffset) == 0x38);
static_assert(offsetof(TerrainBrushResource, HeightContrast) == 0x3c);
static_assert(offsetof(TerrainBrushResource, AlphaThreshold_maybe) == 0x40);
static_assert(offsetof(TerrainBrushResource, Tiling) == 0x44);
static_assert(offsetof(TerrainBrushResource, DetailNormalScale) == 0x48);
static_assert(offsetof(TerrainBrushResource, DetailNormalTiling) == 0x4c);
static_assert(offsetof(TerrainBrushResource, GradientHeightBottom) == 0x50);
static_assert(offsetof(TerrainBrushResource, GradientHeightMidPoint) == 0x54);
static_assert(offsetof(TerrainBrushResource, GradientHeightTop) == 0x58);
static_assert(offsetof(TerrainBrushResource, GradientRoughnessReach) == 0x5c);
static_assert(offsetof(TerrainBrushResource, GradientRoughnessPower) == 0x60);
static_assert(offsetof(TerrainBrushResource, GradientBottomHeightColor) == 0x64);
static_assert(offsetof(TerrainBrushResource, GradientMidHeightColor) == 0x70);
static_assert(offsetof(TerrainBrushResource, UseDetailNormalMap) == 0x7c);
#endif

struct TextureResource : public LoadableResource
{
    [[bg3::hidden]] void* RfTexture;
    FixedString Template;
    bool Streaming;
    bool SRGB;
    TextureType Type;
    int32_t Width;
    int32_t Height;
    int32_t Depth;
};

#if defined(BG3SE_LINUX)
// PORT (linux): LoadTextureHelper, bin/bg3 0x32c8b00 (the manifest's
// ls__AppliedMaterial__LoadTexture), on the resource it takes out of the Texture bank:
// `cmp byte [res + 0x36], 8` is Type against TextureType::T2DRenderTarget, whose branch reads
// the u16 halves of Width/Height at +0x38/+0x3c; `mov byte [res + 0x20], 2` (3 on failure) is
// State; the SSO byte at +0x17 guards SourceFile (+8) and Guid is +0x18 (AddTexture's two
// arguments). RefCount is the u16 two bytes before State, in Resource's tail padding: the
// Linux LoadVirtualTexture 0x24f9c20 increments `word [res + 0x1e]` on a VirtualTextureResource
// (a LoadableResource, below) exactly where the Windows one increments `word [rbx + 0x28]`
// before `byte [rbx + 0x2a] = 2`. The mirror packs it there on its own because Resource is
// non-POD.
// The base has a name, and both members have a Linux witness of their own rather than a
// comparison against the Windows build.  Upstream's `resource::LoadableResource`
// is the game's **ls::NonDeferredLoadableResource** -- ls::VirtualTextureResource's constructor
// (0x2449080) unwinds through ls::NonDeferredLoadableResource::~NonDeferredLoadableResource
// (0x7482550), so that is literally its base -- and four of its methods keep their macOS symbol
// and their Linux match:
//   Load 0x7482840 (_ZN2ls27NonDeferredLoadableResource4LoadEPNS_15ResourceManagerENS_3aio8PriorityE)
//     is upstream's AcquireRef: `movzx eax, word [rdi+0x1e]` / `inc eax` / `mov word [rdi+0x1e], ax`
//     -- a 16-bit read-modify-write at 0x1e -- and on the zero-to-one edge it calls vtable +0x70
//     and then `mov byte [rbx+0x20], 2` on success, 3 on failure.
//   Release 0x7482880 is ReleaseRef: `dec ax` on the same word, vtable +0x78, `mov byte [r14+0x20], 0`.
//   Destroy 0x7482820: `cmp word [rdi+0x1e], 0` then `mov word [rdi+0x1e], 1`, a non-zero constant.
//   IsLoaded 0x74818a0 `cmp byte [rdi+0x20], 2` and IsLoadFailed 0x74818b0 `cmp byte [rdi+0x20], 3`
//     are upstream's IsInState2/IsInState3, which is what the two State values above mean.
// Those vtable slots are also the Itanium check on the mirror's virtual list: upstream's VMT
// struct counts the destructor once (MSVC) and so puts Load at +0x68, while the game calls +0x70
// -- the one extra slot is Itanium's second destructor entry (D1 and D0).  Nothing indexes that
// struct, and a virtual call through the mirror is right anyway because clang applies the same
// rule to the mirror's own declaration; it would only bite a hand-written slot index.
static_assert(offsetof(LoadableResource, RefCount) == 0x1e);
static_assert(offsetof(LoadableResource, State) == 0x20);
static_assert(offsetof(TextureResource, Type) == 0x36);
static_assert(offsetof(TextureResource, Width) == 0x38);
static_assert(offsetof(TextureResource, Height) == 0x3c);
#endif

struct TileSetResource : public LoadableResource
{
    struct Tile
    {
        FixedString VisualGUID;
        FixedString PhysicsGUID;
        FixedString InvisibleClimbingHelperPhysicsGUID;
        [[bg3::hidden]] __int32 field_10;
    };

    struct ExtendedRefMap
    {
        LegacyRefMap<bg3se::Guid, Tile> Tiles;
        bool IsRoof;
        [[bg3::hidden]] __int8 field_11;
        [[bg3::hidden]] __int16 field_12;
        [[bg3::hidden]] __int32 field_14;
    };

    ExtendedRefMap* TileSet;
};

struct TimelineResource : public LoadableResource
{
    bg3se::Guid DialogResourceId;
    bool FadeOutOnEnd;
    [[bg3::hidden]] __int8 field_41;
    [[bg3::hidden]] __int16 field_42;
    float FadeOutDuration;
    int32_t BodyPartMocap;
    [[bg3::hidden]] __int32 field_4c;
};

struct TimelineSceneResource : public LoadableResource
{
    Array<FixedString> Labels;
    uint8_t SceneType;
    [[bg3::hidden]] __int8 field_41;
    [[bg3::hidden]] __int16 field_42;
    int32_t AdditionalSpeakerCount;
};

struct VirtualTextureResource : public LoadableResource
{
    [[bg3::readonly]] uint8_t GTSIndex;
    [[bg3::readonly]] uint32_t VirtualTextureLayerConfig;
    [[bg3::readonly]] uint32_t LoadedVirtualTextureLayerConfig;
    STDString RootPath;
#if defined(BG3SE_LINUX)
    // PORT (linux): the game's SRW lock, a bare 56-byte pthread_rwlock_t, not Win32's
    // 8-byte handle (ext/linux/shim/windows.h). Every field below was 48 low.
    [[bg3::hidden]] LS_SRWLOCK Lock;
#else
    [[bg3::hidden]] SRWLOCK Lock;
#endif
    FixedString TileSetFileName;
    FixedString GTexFileName;
    [[bg3::hidden]] void* GraphineTextureData;
    bool Prefetch;
    int8_t PrefetchMipLevel;
    [[bg3::readonly]] uint8_t IsEditor;
    [[bg3::readonly]] uint8_t IsValid;
    uint32_t ReferencedColorSpaces;
};

#if defined(BG3SE_LINUX)
// PORT (linux): read off ls::VirtualTextureResource's own Linux constructor and destructor
// -- bin/bg3 0x2449080 (_ZN2ls22VirtualTextureResourceC2Ev, matched s3:1.00) and 0x74862b0
// (_ZN2ls22VirtualTextureResourceD1Ev).  Both name the lock outright: the ctor calls
// pthread_rwlock_init@plt (0x77ea020) on `rbx + 0x40` with a null attr, the dtor calls
// pthread_rwlock_destroy@plt (0x77ea030) on the same address.  So the lock is the bare
// 56-byte primitive with no owner or recursion count behind it -- unlike ls::SRWLock, which
// is that primitive plus a pthread_t and a u32 (Base/Lock.h).
//   RootPath 0x30: the dtor's `cmp byte [rbx+0x3f], 0` / `js` long-string test frees
//     [rbx+0x30] through ls::StringAllocatorInternal::Free, so the 16-byte STDString ends
//     exactly where the lock begins.
//   TileSetFileName 0x78 and GTexFileName 0x7c: the ctor's `mov qword [rbx+0x78], -1` sets
//     both FixedStrings to Null in one store, and the dtor runs ~FixedString on 0x7c then
//     0x78 (reverse declaration order).
//   GraphineTextureData 0x80, and the ctor's `mov word [rbx+0x88], 0xff00` is
//     Prefetch=false at 0x88 with PrefetchMipLevel=-1 at 0x89; `mov byte [rbx+0x8a], 0` is
//     IsEditor, IsValid follows at 0x8b uninitialised, and `mov dword [rbx+0x8c], 0` is
//     ReferencedColorSpaces.  sizeof 0x90 is not an inference from that last u32's end:
//     ls::VirtualTextureResource::CreateClone 0x75477e0 (matched r5:2) does `mov edi, 0x90;
//     call operator new` and then the constructor above, and here the size *is* an ABI witness
//     -- MSVC would lay the same members out to 0x98.  The same
//     function copies the clone's dword at 0x24 and 0x8c and assigns the FixedStrings at 0x7c
//     and 0x78, which re-establishes four of the offsets below.
// The same ctor confirms the prefix, which is upstream's: `mov dword [rbx+0x18], -1`
// is Resource::Guid, `mov byte [rbx+0x1c], 0` IsModded, `mov dword [rbx+0x1e], 0xff000000`
// is LoadableResource::RefCount/State at 0x1e/0x20 with GTSIndex at 0x21 in the base's tail
// padding, and `mov qword [rbx+0x24], 0` clears both layer-config u32s at 0x24 and 0x28.
static_assert(offsetof(VirtualTextureResource, GTSIndex) == 0x21);
static_assert(offsetof(VirtualTextureResource, VirtualTextureLayerConfig) == 0x24);
static_assert(offsetof(VirtualTextureResource, RootPath) == 0x30);
static_assert(offsetof(VirtualTextureResource, Lock) == 0x40);
static_assert(offsetof(VirtualTextureResource, TileSetFileName) == 0x78);
static_assert(offsetof(VirtualTextureResource, GTexFileName) == 0x7c);
static_assert(offsetof(VirtualTextureResource, GraphineTextureData) == 0x80);
static_assert(offsetof(VirtualTextureResource, Prefetch) == 0x88);
static_assert(offsetof(VirtualTextureResource, ReferencedColorSpaces) == 0x8c);
static_assert(sizeof(VirtualTextureResource) == 0x90);
#endif

struct VisualResource : public TwoStepLoadableResource
{
    struct Object
    {
        FixedString ObjectID;
        FixedString MaterialID;
        uint8_t LOD;
        [[bg3::hidden]] __int8 field_9;
        [[bg3::hidden]] __int16 field_10;
    };

    struct Attachment
    {
        FixedString UUID;
        FixedString Name;
    };

    struct ClothParam
    {
        FixedString UUID;
        float LinearStiffness;
        float BendingStiffness;
        float CompressionLimit;
        float StretchLimit;
        float StiffnessMultiplier;
        glm::fvec3 LinearDrag;
        glm::fvec3 AngularDrag;
        glm::fvec3 LinearInertia;
        glm::fvec3 AngularInertia;
        glm::fvec3 CentrifugalInertia;
        glm::fvec3 Damping;
        float CollisionMassScale;
        float DragCoefficient;
        float FluidDensity;
        float Friction;
        float LiftCoefficient;
        float TetherConstraintScale;
        float TetherConstraintStiffness;
        float SolverFrequency;
        float StiffnessFrequency;
        float FrontalWindSpeed;
        float FrontalWindVariance;
        float AnimDriveSpringForce;
        float AnimDriveDampingForce;
        float ClothWindSpeed;
        float ClothMainWindVariance;
        float ClothMainWindSpeed;
        float ClothMainWindFrequency;
        float ClothWindDirectionOffsetFrequency;
        float ClothMaxWindDirectionOffset;
        float SelfCollisionDistance;
        float SelfCollisionStiffness;
        bool EnableVirtualParticles;
        bool EnableCCD;
        bool AtmosphericWindEnabled;
        bool OverrideAtmosphericWind;
        float AnimInfluenceMultiplier;
    };

    struct AnimationSetOverride
    {
        bg3se::Guid PairElement1;
        FixedString PairElement2;
        int32_t strAnimationSetOverrideType;
    };

    struct Bone
    {
        glm::fvec3 Position;
        glm::fvec4 Rotation;
    };

    struct ClothHighResInfo
    {
        FixedString Name;
        [[bg3::hidden]] Array<uint16_t> ClosestVertices;
        [[bg3::hidden]] void* CompressedVertices;
        [[bg3::hidden]] uint32_t CompressedSize;
        [[bg3::hidden]] uint32_t NbClosestVertices;
    };

    struct ClothData
    {
        HashMap<FixedString, Array<ClothHighResInfo>> Mappings;
        TrackedCompactSet<ClothParam> Params;
        FixedString ClothColliderResourceID;
    };

    [[bg3::hidden]] void* field_48; // DynamicArray VMT
    TrackedCompactSet<FixedString> Tags;
    [[bg3::hidden]] __int64 field_60; // DynamicArray extra data
    TrackedCompactSet<Object> Objects;
    TrackedCompactSet<Attachment> Attachments;
    ClothData* Cloth;
    Array<AnimationSetOverride> AnimationSetOverrides;
    Array<FixedString> AnimationWaterfall;
    LegacyRefMap<FixedString, Bone> Bones;
    [[bg3::hidden]] void* field_c8; // DynamicArray VMT
    TrackedCompactSet<FixedString> VertexColorMaskSlots;
    [[bg3::hidden]] __int64 field_e0; // DynamicArray extra data
    FixedString SkeletonResource;
    FixedString Template;
    FixedString SkeletonSlot;
    FixedString SoftbodyResourceID;
    FixedString HairPresetResourceId;
    FixedString ScalpMaterialId;
    FixedString Slot;
    FixedString RemapperSlotId;
    FixedString AttachBone;
    FixedString BlueprintInstanceResourceID;

    glm::fvec3 BoundsMin;
    glm::fvec3 BoundsMax;
    bool NeedsSkeletonRemap;
    bool SupportsVertexColorMask;
    MaterialType MaterialType;
    uint8_t HairType;
    // Editor only
    // bool ExcludeFromHLOD;
};

#if defined(BG3SE_LINUX)
// PORT (linux): ls::VisualResource::DoUnload (bin/bg3 0x2798d10, matched s3:0.84) reads five of
// these with r15 = this, every one of them 48 bytes above where the Win32 lock size put it:
// Objects' buffer at `[r15 + 0x90]` with its size at 0x9c and a 12-byte element stride
// (`lea rax, [rax + rax*2]` then `[rbp + rax*4]`, i.e. the mirror's 12-byte Object),
// Attachments' size at `[r15 + 0xac]`, the Cloth pointer at `[r15 + 0xb0]`, and two
// FixedStrings compared with -1 at `[r15 + 0x108]` and `[r15 + 0x11c]` -- SkeletonResource and
// ScalpMaterialId, six FixedStrings apart in the mirror and 0x14 apart here.  The same function
// reads Guid at `[r15 + 0x18]` and ResourceBanks at `[mgr + sel*8 + 0x50]`, both already known.
static_assert(offsetof(VisualResource, Objects) == 0x90);
static_assert(offsetof(VisualResource, Attachments) == 0xa0);
static_assert(offsetof(VisualResource, Cloth) == 0xb0);
static_assert(offsetof(VisualResource, SkeletonResource) == 0x108);
static_assert(offsetof(VisualResource, ScalpMaterialId) == 0x11c);
static_assert(sizeof(VisualResource::Object) == 12);
#endif

struct VisualSetResource : public LoadableResource
{
};

struct VoiceBarkResource : public LoadableResource
{
};

#if defined(BG3SE_LINUX)
// PORT (linux): the eighteen LoadableResource subclasses that add members but put none of them
// in the base's tail padding -- the whole "affected, not flagged" shape, measured rather than
// assumed.  None of them is an ABI question in its own right: the
// derived block starts at the base's sizeof 0x28 under both ABIs (LoadableResource is 0x28
// either way, see its own witness block), so nothing here moves and these asserts pin the
// mirrors against the game rather than fixing them.  They are worth taking anyway, because the
// same functions say whether upstream got the members right at all -- and for ScriptResource it
// had not (see the member comment there).
//
// The instrument, and it names the whole family at once: every ls::*Resource carries
// ls::Resource::GetLogInfo (bin/bg3 0x7482590, matched s1:0.82 to macOS 0x106fc97b0) at slot 2,
// so the 38 vtable runs in .data.rel.ro that hold it are the resource family.  Slot 11 is
// GetType() and is `mov eax, <ResourceType>; ret`, which is the same constant the macOS oracle's
// named ls::X::GetType returns -- so the constant maps each stripped Linux run to a class name.
// Slot 12 is CreateClone, which is the class's own allocator-and-copy: `mov edi, <sizeof>;
// call operator new` followed by the derived members, one copy per member.  Three independent
// confirmations of that run-to-class map: the GetType constant, the vtable the class's own
// CreateClone/ctor stores back into [this], and two runs whose slot 12 the register already
// knew (VirtualTextureResource's CreateClone 0x75477e0, DiffusionProfileResource's 0x748e130).
// Its negative control is built in: ls::VisualSetResource is the one class with no CreateClone
// of its own in either binary, and its Linux slot 12 is the shared base function 0x74827c0.
static_assert(offsetof(AnimationSetResource, AnimationBank) == 0x28);
static_assert(sizeof(AnimationSetResource) == 0x30);
// CreateClone 0x7482280: `mov edi, 0x30`, then `mov rdi, [r14+0x28]` / `mov rsi, [r15+0x28]`
// handed to the ls::AnimationSet copy 0x747e3e0 -- the member is the pointer at 0x28.

static_assert(offsetof(AtmosphereResource, Atmosphere) == 0x28);
static_assert(offsetof(AtmosphereResource, Labels) == 0x30);
static_assert(sizeof(AtmosphereResource) == 0x40);
// ctor 0x2feffc0 (it stores this class's vtable 0x7ace0a8): `mov edi, 0x2f0; call operator new`
// for the ls::Atmosphere and `mov qword [rbx+0x28], r14` at 0x2ff00dc -- a pointer value store --
// then `movdqu [rbx+0x30], xmm0` over the Labels Array.  CreateClone 0x748cc00 `mov edi, 0x40`.

static_assert(offsetof(CharacterVisualResource, VisualSet) == 0x28);
static_assert(offsetof(CharacterVisualResource, BaseVisual) == 0x198);
static_assert(sizeof(CharacterVisualResource) == 0x1a0);
// CreateClone 0x748d810: `mov edi, 0x1a0`, `lea rsi, [r15+0x28]` / `lea rdi, [r14+0x28]` into
// the VisualSet copy 0x2ed7d70, then `mov esi, [r15+0x198]` with `add rdi, 0x198` into the
// FixedString acquire 0x3883660 -- the last member is a FixedString at 0x198, by the callee.

static_assert(offsetof(ClothColliderResource, Spheres) == 0x28);
static_assert(sizeof(ClothColliderResource) == 0x38);
// CreateClone 0x748dc90: `mov edi, 0x38`, vtable 0x7ace760, `movups [rbx+0x28], xmm0` over the
// Array, then the source's buf at [rax+0x28] and its size at [rax+0x34] -- Array{buf, capacity,
// size} at 0x28, the size member where it belongs.

static_assert(offsetof(ColorListResource, field_30) == 0x28);
static_assert(offsetof(ColorListResource, Colors) == 0x30);
static_assert(offsetof(ColorListResource, field_48) == 0x40);
static_assert(offsetof(ColorListResource, field_50) == 0x48);
static_assert(offsetof(ColorListResource, Ids) == 0x50);
static_assert(offsetof(ColorListResource, IdGenerator) == 0x68);
static_assert(sizeof(ColorListResource) == 0x70);
// CreateClone 0x748d1e0: `mov edi, 0x70`, the class vtable 0x7ace480 into [r14], and then value
// stores all the way down -- `mov qword [r14+0x28], rdx` with rdx the .data.rel.ro run 0x7ace510
// and `mov qword [r14+0x48], rcx` with rcx 0x7ace550, so upstream's field_30 and field_50 are
// two more vtable pointers; `mov qword [r14+0x40], 0x100000000`; the two Arrays zeroed at
// [r14+0x30] and [r14+0x50]; and `mov eax, [r15+0x68]` / `mov [r14+0x68], eax`, a lone dword
// copy, for IdGenerator.  field_68 at 0x60 and field_74 at 0x6c get no assert: only zero lands
// in them (`mov dword [r14+0x60], 0`, and the 1 of `mov qword [r14+0x64], 1` falls inside
// field_68, which is why that member is 8 bytes wide here).

static_assert(offsetof(DialogResource, ChildResources) == 0x28);
static_assert(offsetof(DialogResource, SpeakerSlotsWithLines) == 0x38);
static_assert(offsetof(DialogResource, TimelineSplitScreenPriority) == 0x58);
static_assert(offsetof(DialogResource, Flags) == 0x59);
static_assert(offsetof(DialogResource, Flags2) == 0x5a);
static_assert(sizeof(DialogResource) == 0x60);
// CreateClone 0x748cfd0: `mov edi, 0x60`; `lea rsi, [r15+0x38]` / `lea rdi, [r14+0x38]` into
// 0x748d0a0 and `lea rsi, [r15+0x28]` with rdi = r14+0x28 into the Array<Guid> copy 0x3881830;
// `mov al, [r15+0x58]` / `mov [r14+0x58], al`; and three read-modify-writes of the *word* at
// [r14+0x59] under the masks 0x197, 0x20 and 0x48, which is what says the two flag bytes are at
// 0x59 and 0x5a and not one 16-bit member.  Name (0x48) is not touched here and gets no assert;
// it is bounded by the Array ending at 0x48 and the byte at 0x58, an STDString's 0x10 apart.

static_assert(offsetof(IKRigResource, BoneProperties) == 0x28);
static_assert(offsetof(IKRigResource, field_40) == 0x38);
static_assert(offsetof(IKRigResource, IKTasks) == 0x40);
static_assert(offsetof(IKRigResource, SolverRoot) == 0x58);
static_assert(sizeof(IKRigResource) == 0xa0);
// CreateClone 0x7481130: `mov edi, 0xa0` and then `memset(this, 0, 0xa0)` -- so no zero store in
// it witnesses anything -- followed by the copies.  It walks the source's map as HashSize
// [r12+0x2c] and HashTable [r12+0x30], which is RefMapInternals{ItemCount, HashSize, HashTable}
// at 0x28; treats 0x38 as the head of one object (`add r15, 0x38`, then 0x7480fd0 and the
// reserve 0x3514b10) whose element count it takes from [r12+0x4c], i.e. the Array at 0x40 with
// its size at 0x4c; and copies a byte `[r12+0x58]` -> `[rcx+0x58]`.  BoneCategories (0x60) is
// not reached by this function and gets no assert.

static_assert(offsetof(LightingResource, Lighting) == 0x28);
static_assert(sizeof(LightingResource) == 0x30);
// CreateClone 0x748c980: `mov edi, 0x30`, the class vtable 0x7ace018, `mov qword [r14+0x28], 0`
// and then `mov rsi, [r15+0x28]` tested for null and handed to 0x748ca10 -- a pointer read at
// 0x28, and sizeof 0x30 leaves nowhere else for it to be.

static_assert(offsetof(MaterialPresetResource, Presets) == 0x28);
static_assert(sizeof(MaterialPresetResource) == 0xd0);
static_assert(offsetof(SkinPresetResource, Presets) == 0x28);
static_assert(sizeof(SkinPresetResource) == 0xd0);
// The two PresetData holders, and they witness each other: CreateClone 0x748d9d0 and 0x748dae0
// both `mov edi, 0xd0` and both call the *same* PresetData copy 0x2edac20 with rdi = this+0x28
// and rsi = src+0x28.  0x748dae0 also zero-fills 0x28..0xd0 with `mov dword [r14+0x88],
// 0xffffffff` inside it, and the material one's destructor 0x748da40 frees at +0x28, +0x68,
// +0x78, +0x88 and +0x90 -- a 0xa8-byte PresetData at 0x28 in both.

static_assert(offsetof(PhysicsResource, ObjectTemplates) == 0x28);
static_assert(offsetof(PhysicsResource, Template) == 0x48);
static_assert(offsetof(PhysicsResource, field_54) == 0x4c);
static_assert(sizeof(PhysicsResource) == 0x50);
// CreateClone 0x748b380: `mov edi, 0x50`; `mov esi, [r15+0x48]` / `lea rdi, [r12+0x48]` into the
// FixedString acquire 0x3883660, so Template is a FixedString at 0x48 by the callee; `mov al,
// [r15+0x4c]` / `mov [rbx+0x4c], al`; and the Array at 0x28 read as buf [rbx+0x28], capacity
// [rbx+0x30], size [rbx+0x34].  IgnoreColliderList (0x38) is not touched and gets no assert.

static_assert(offsetof(ScriptResource, Parameters) == 0x28);
static_assert(offsetof(ScriptResource, field_48) == 0x40);
static_assert(sizeof(ScriptResource) == 0x48);
// CreateClone 0x748c400: `mov edi, 0x48`, the class vtable 0x7acde88, then the map built in
// place -- `mov dword [r14+0x28], 0xb` (HashSize, a value), the table from 0x218f1e0 into
// `mov qword [r14+0x30], rax`, `mov dword [r14+0x38], 0` -- which is MapInternals{HashSize,
// HashTable, ItemCount} at 0x28, and the macOS twin names it: ls::ScriptResource::Visit
// 0x107025040 visits FFFS::strParameters at this+0x28 as ls::FixedStringMap<ls::ScriptVar*>*.
// Then `mov byte [r14+0x40], bl` -- the member upstream was missing; see its comment above.

static_assert(offsetof(TextureResource, Template) == 0x30);
static_assert(offsetof(TextureResource, Streaming) == 0x34);
static_assert(offsetof(TextureResource, SRGB) == 0x35);
static_assert(offsetof(TextureResource, Type) == 0x36);
static_assert(offsetof(TextureResource, Width) == 0x38);
static_assert(offsetof(TextureResource, Height) == 0x3c);
static_assert(offsetof(TextureResource, Depth) == 0x40);
static_assert(sizeof(TextureResource) == 0x48);
// CreateClone 0x7481a70 is the densest of the eighteen: `mov edi, 0x48`, the FixedString acquire
// 0x3883660 on this+0x30, then byte copies at 0x34, 0x36 and 0x35 one by one, a *qword* copy at
// 0x38 and a dword copy at 0x40.  Width and Height are the two halves of that qword and Depth
// closes the block at 0x40, so 0x3c is the only place Height can be.  RfTexture (0x28) is
// runtime state, is not copied, and gets no assert.

static_assert(offsetof(TileSetResource, TileSet) == 0x28);
static_assert(sizeof(TileSetResource) == 0x30);
// CreateClone 0x7540180: `mov edi, 0x30`, then `mov rdi, [r14+0x28]` / `mov rsi, [r15+0x28]`
// into 0x75402b0 -- the member is the pointer at 0x28, dereferenced as an object.

static_assert(offsetof(TimelineResource, DialogResourceId) == 0x28);
static_assert(offsetof(TimelineResource, FadeOutOnEnd) == 0x38);
static_assert(offsetof(TimelineResource, FadeOutDuration) == 0x3c);
static_assert(offsetof(TimelineResource, BodyPartMocap) == 0x40);
static_assert(sizeof(TimelineResource) == 0x48);
// CreateClone 0x748ce40: `mov edi, 0x48`; a 16-byte `movups` copy of [r15+0x28] (the Guid);
// a qword copy at 0x40; a byte copy at 0x38; and `movss` at 0x3c -- the instruction kind is what
// says 0x3c is the float and 0x40 the int32 pair, not the other way round.

static_assert(offsetof(TimelineSceneResource, Labels) == 0x28);
static_assert(offsetof(TimelineSceneResource, AdditionalSpeakerCount) == 0x3c);
static_assert(sizeof(TimelineSceneResource) == 0x40);
// CreateClone 0x748def0: `mov edi, 0x40`, the class vtable 0x7ace850, `movups [r14+0x28], xmm0`
// over the Labels Array and `mov dword [r14+0x3c], 0xffffffff` -- a value store, the default -1.
// SceneType (0x38) gets no assert: `mov byte [r14+0x38], 0` is a bare zero fill.

static_assert(sizeof(VisualSetResource) == 0x28);
static_assert(sizeof(VoiceBarkResource) == 0x28);
// The two that add nothing, and the only claim to make about them is that the game adds nothing
// either.  VoiceBarkResource's CreateClone 0x748e080 does `mov edi, 0x28` and zero-fills exactly
// [0, 0x28) (`movaps [rax]`, `movaps [rax+0x10]`, `mov qword [rax+0x20], 0`) before storing its
// vtable 0x7acea08.  VisualSetResource has no CreateClone of its own, so the witness is its two
// vtable referrers in .text, the bank's read pair 0x7488620 and 0x7488730: each allocates 0x28
// through the tagged allocator 0x2452590 and stores 0x7acd358 into [this], and neither writes
// anything above 0x28.
#endif

END_NS()

BEGIN_NS(lua)

LUA_POLYMORPHIC(resource::PhysicsResource::ObjectTemplate::PhysicsObject);

END_NS()
