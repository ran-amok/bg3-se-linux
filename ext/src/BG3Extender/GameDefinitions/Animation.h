#pragma once

#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/EntitySystem.h>

BEGIN_NS(gn)

struct GenomeTypeManager;

using SequenceIndex = uint16_t;
using NodeIndex = uint16_t;
using VariableIndex = uint16_t;
using EGenomeExecutionResult = uint32_t;

struct GenomeEventTypeIndex
{
    uint8_t Type;
    uint16_t Index;
};

struct GenomeEventArgs
{
    virtual ~GenomeEventArgs();
};

struct GenomeParametrizedEventArgs : public GenomeEventArgs
{
    virtual ~GenomeParametrizedEventArgs();

    std::array<GenomeVariant, 22> Args;
};

struct GenomeTimelineAnimation
{
    double StartTime;
    double EndTime;
    float PlayStartOffset;
    float PlayRate;
    FixedString Source;
    FixedString Animation;
    FixedString AdditiveLoopingAnimation;
    uint32_t field_24;
    float AdditiveFadeIn;
    bool IsContinuous;
    bool HoldAnimation;
    bool Voice;
    bool EnableRootMotion;
    bool IsMirrored;
    bool PerformanceDriftType;
    Guid BoneGroupId;
    glm::vec4 HeadCorrection;
};

struct GenomeTimelineAnimationPair
{
    GenomeTimelineAnimation AnimA;
    GenomeTimelineAnimation AnimB;

    Guid field_0;
    uint8_t field_10;
    uint8_t field_11;
    float EndTimeWithFade;
    float EndTimeWithFadeOffset;
    float StartTimeWithFade;
    float StartTimeWithFadeOffset;
    int field_D4;
};

struct GenomeTimelineActiveAnimations : public GenomeTimelineAnimation
{
    Array<GenomeTimelineAnimationPair> Animations;
    Array<GenomeTimelineAnimationPair> LayeredAnimations;
    Array<GenomeTimelineAnimationPair> AdditiveAnimations;
};

struct GenomeTimelineData
{
    uint64_t field_0;
    double field_8;
    double field_10;
    uint8_t field_18;
    uint8_t field_19;
    Array<GenomeTimelineActiveAnimations> Animations;
};

// Unmapped compiler internals
class GenomeGlobalCompilationContext;
class GenomeSequenceCompilationContext;
class GenomeCompilerSequence;
class GenomeCompilerNode;
class GenomeCompilerLink;
class GenomeCompilerSocket;
class GenomeCompilerFunctionDataSocket;
class GenomeNodeSocketPrototype;
class GenomeVisitor;
class GenomeForceTime;
class GenomeDebugNode;
class GenomeDebugSequence;
class GenomeInstanceDebugContext;
struct GenomeExecutionContext;

struct GenomeSequenceTemplate : public ProtectedGameObject<GenomeSequenceTemplate>
{
    virtual ~GenomeSequenceTemplate() = 0;
    virtual FixedString GetTypeID() const = 0;
    virtual bool PreprocessSequence(GenomeSequenceCompilationContext&, GenomeCompilerSequence const&) = 0;
    virtual void SetIndex(SequenceIndex const&) = 0;
    virtual void RegisterNodeForEvent(NodeIndex const&, FixedString const& event) = 0;
    virtual void RegisterSequenceForEvent(SequenceIndex const&, FixedString const&) = 0;
    virtual bool Visit(GenomeVisitor*) = 0;

    [[bg3::readonly]] SequenceIndex Index;
    Array<VariableIndex> InputVariables;
    Array<VariableIndex> OutputVariables;
    LegacyRefMap<FixedString, Array<NodeIndex>> NodeEvents;
};

struct GenomeNodeTemplate : public ProtectedGameObject<GenomeNodeTemplate>
{
    virtual ~GenomeNodeTemplate() = 0;
    virtual void Create(NodeIndex const&) = 0;
    virtual bool PreprocessNode(GenomeSequenceCompilationContext&, GenomeCompilerNode const&) = 0;
    virtual bool DependencyResolve(GenomeSequenceCompilationContext&, GenomeCompilerNode const&) = 0;
    virtual bool FlowResolve(GenomeSequenceCompilationContext&, GenomeCompilerNode const&, GenomeCompilerLink const*) = 0;
    virtual FixedString const& GetTypeID() const = 0;
    virtual bool IsEntryPoint(GenomeSequenceCompilationContext const&) const = 0;
    virtual bool ShouldResetOnParentReset() const = 0;
    virtual VariableIndex const* GetInputSocketVariables(int, int*) const = 0;
    virtual VariableIndex const* GetOutputSocketVariables(int, int*) const = 0;
    virtual GenomeNodeSocketPrototype const* GetInputSocketPrototypeTypes(uint64_t&) const = 0;
    virtual GenomeNodeSocketPrototype const* GetOutputSocketPrototypeTypes(uint64_t&) const = 0;
    virtual bool Visit(GenomeVisitor*) = 0;
    virtual uint64_t GetProcessType()const = 0;
    virtual uint64_t GetEntryTrigger(GenomeSequenceCompilationContext const&) const = 0;
    virtual void AddSocketLinkDependencies(GenomeSequenceCompilationContext&, GenomeCompilerNode const&, GenomeCompilerSocket const&, bool) = 0;
    virtual bool CreateAndLinkOutputSockets(GenomeSequenceCompilationContext&, GenomeCompilerNode const&) = 0;
    virtual bool CreateAndLinkInputSockets(GenomeSequenceCompilationContext&, GenomeCompilerNode const&) = 0;
    virtual bool ValidateAnyTypeSocket(GenomeSequenceCompilationContext&, GenomeCompilerNode const&, GenomeCompilerFunctionDataSocket const&, bool) = 0;
    virtual VariableIndex& GetSocketVariable(VariableIndex&, GenomeSequenceCompilationContext&, GenomeCompilerNode const&, GenomeCompilerFunctionDataSocket const&, int, bool) = 0;
    virtual VariableIndex& GetOutputSocketPassthrough(VariableIndex&, GenomeSequenceCompilationContext&, GenomeCompilerNode const&, GenomeCompilerFunctionDataSocket const&, int) = 0;
    virtual bool ResolveDataInputs(GenomeSequenceCompilationContext&, GenomeCompilerNode const&) = 0;
    virtual bool HandleResolveDataInput(GenomeSequenceCompilationContext&, FixedString const&, CompactSet<GenomeCompilerSocket*> const&) = 0;
    virtual bool HandleResolveDataSocket(GenomeSequenceCompilationContext&, FixedString const&, GenomeCompilerSocket const*, uint64_t) = 0;
    virtual bool ResolveFlow(GenomeSequenceCompilationContext&, GenomeCompilerNode const&) = 0;
    virtual bool HandleResolveFlow(GenomeSequenceCompilationContext&, FixedString const&, CompactSet<GenomeCompilerSocket*> const&) = 0;

    [[bg3::readonly]] NodeIndex Index;
};

struct GenomeVariableTemplate : public ProtectedGameObject<GenomeVariableTemplate>
{
    [[bg3::readonly]] VariableIndex Index;
    [[bg3::readonly]] GenomeVariableType Type;
    GenomeVariant Value;
    Guid ID;
    STDString Name;
};

struct GenomeExternalGroupTemplate : public ProtectedGameObject<GenomeExternalGroupTemplate>
{
    Array<VariableIndex> Variables;
};

struct GenomeBlueprint : public ProtectedGameObject<GenomeBlueprint>
{
    Array<GenomeSequenceTemplate*> Sequences;
    Array<GenomeNodeTemplate*> Nodes;
    Array<GenomeVariableTemplate*> Variables;
    Array<GenomeVariableTemplate*> StackVariables;
    LegacyRefMap<FixedString, GenomeExternalGroupTemplate*> ExternalGroups;
    [[bg3::hidden]] void* GenomePluginDataWrapper;
    LegacyRefMap<FixedString, GenomeEventTypeIndex> Events;
    LegacyRefMap<Guid, VariableIndex> VariableIndices;
    // Editor only
#if 0
    // [[bg3::hidden]] void* DebugContext;
    // STDString Path;
    // [[bg3::hidden]] __int64 field_98;
#endif
    [[bg3::hidden]] GenomeManager* GenomeManager;
    Version Version;
    bool IsValid;
    Array<Array<GenomeVariant>*> Stacks;
    [[bg3::hidden]] CRITICAL_SECTION StackLock;
};

struct GenomeSequence : public ProtectedGameObject<GenomeSequence>
{
    [[bg3::hidden]] void* VMT;
    GenomeSequenceTemplate* Template;
    int Clock;
};

struct GenomeNode : public ProtectedGameObject<GenomeNode>
{
    virtual ~GenomeNode() = 0;
    virtual void OnGenomeEvent(GenomeExecutionContext&, FixedString const& evt, GenomeEventArgs const& args) = 0;
    virtual void Reset(GenomeExecutionContext&) = 0;
    virtual void Create(GenomeBlueprintInstance*, GenomeNodeTemplate const*) = 0;
    virtual void PreExecute(GenomeExecutionContext&) = 0;
    virtual EGenomeExecutionResult Execute(GenomeExecutionContext&) = 0;
    virtual void ForceClock(GenomeExecutionContext&, GenomeForceTime const&) = 0;
};

// Unmapped
struct GenomePluginDataDesc {};

struct GenomeBlueprintInstance : public ProtectedGameObject<GenomeBlueprintInstance>
{
    [[bg3::hidden]] Array<GenomePluginDataDesc*>* PluginData;
    [[bg3::hidden]] void* Listener;
    Array<GenomeSequence*> Sequences;
    Array<GenomeNode*> Nodes;
    [[bg3::legacy(VariablesForEdit)]] Array<GenomeVariant> Variables;
    GenomeBlueprint* Parent;
    [[bg3::hidden]] GenomeManager* GenomeManager;
    [[bg3::hidden]] void* GenomeInstanceVariableOverrider;
    ecs::EntityRef Entity;
    uint32_t CreationFlags;
    // Editor only
    // [[bg3::hidden]] void* DebugContext;
    // [[bg3::hidden]] bool DumpStateOnNextRun;

    // Lua call helpers
    //# P_FUN(GetNode, gn::GenomeBlueprintInstance::LuaGetNode)
    UserReturn LuaGetNode(lua_State* L, uint32_t nodeIdx);
};

struct BlueprintInstanceData : public ProtectedGameObject<BlueprintInstanceData>
{
    GenomeBlueprint* Blueprint;
    [[bg3::readonly]] uint32_t Flags;
    [[bg3::readonly]] uint64_t ThreadEvent;
};

struct [[bg3::hidden]] GenomeNodeTypeDesc : public ProtectedGameObject<GenomeNodeTypeDesc>
{
    using CreateNodeTemplateProc = GenomeNodeTemplate* ();
    using CreateNodeProc = GenomeNode* ();
    using CreateCompilerNodeProc = GenomeCompilerNode* ();
    using NodeFunctionProc = uint64_t (GenomeExecutionContext&);
    using CreateDebugNodeProc = GenomeDebugNode* ();
    using GetSocketProc = GenomeNodeSocketPrototype* (uint64_t);

    CreateNodeTemplateProc* CreateNodeTemplate;
    CreateNodeProc* CreateNode;
    CreateCompilerNodeProc* CreateCompilerNode;
    uint32_t TypeHash;
    CreateDebugNodeProc* CreateDebugNode;
    GetSocketProc* GetInputSockets;
    GetSocketProc* GetOutputSockets;
};

struct [[bg3::hidden]] GenomeSequenceTypeDesc : public ProtectedGameObject<GenomeSequenceTypeDesc>
{
    using CreateTemplateProc = GenomeSequenceTemplate* ();
    using CreateInstanceProc = GenomeSequence* ();
    using CreateCompilationContextProc = GenomeSequenceCompilationContext* (GenomeGlobalCompilationContext*, GenomeSequenceTemplate*);
    using CreateCompilerSequenceProc = GenomeCompilerSequence* ();
    using CreateDebugSequenceProc = GenomeDebugSequence* ();

    CreateTemplateProc* CreateTemplate;
    CreateInstanceProc* CreateInstance;
    CreateCompilationContextProc* CreateCompilationContext;
    CreateCompilerSequenceProc* CreateCompilerSequence;
    CreateDebugSequenceProc* CreateDebugSequence;
};

struct [[bg3::hidden]] GenomeSocketTypeDesc {
    // FIXME
};
struct [[bg3::hidden]] GenomeLinkTypeDesc {};

struct [[bg3::hidden]] GenomeExternalGroupDesc
{
    Guid* Guid;
    uint32_t Index;
    FixedString Name;
};


struct [[bg3::hidden]] GenomeTypeManager : public ProtectedGameObject<GenomeTypeManager>
{
    LegacyRefMap<uint32_t, GenomeVarTypeDesc*> VarTypesByHash;
    LegacyRefMap<FixedString, GenomeVarTypeDesc> VarTypes;
    LegacyRefMap<uint32_t, GenomeNodeTypeDesc*> NodesById;
    LegacyRefMap<FixedString, GenomeNodeTypeDesc> NodesByName;
    LegacyRefMap<FixedString, GenomeSequenceTypeDesc> Sequences;
    LegacyRefMap<FixedString, GenomeSocketTypeDesc> Sockets;
    LegacyRefMap<FixedString, GenomeLinkTypeDesc> LinkTypes;
    LegacyRefMap<FixedString, GenomeExternalGroupDesc> ExternalGroups;
    [[bg3::hidden]] Array<void*> field_80;
    [[bg3::hidden]] Array<void*> BlueprintPluginDataList;
    [[bg3::hidden]] Array<void*> BlueprintCompilerPluginDataList_Arr_GenomePluginDataDesc;
    [[bg3::hidden]] Array<void*> BlueprintManagerPluginDataList;
    [[bg3::hidden]] Array<void*> CompilerSavePlugins;
    [[bg3::hidden]] Array<void*> BlueprintLoadPlugins;
    [[bg3::hidden]] Array<void*> BlueprintInstanceInitPlugins_Arr_func;
    [[bg3::hidden]] Array<void*> BlueprintInstancePreExePlugins;
    [[bg3::hidden]] Array<void*> BlueprintInstancePostExePlugins;
    [[bg3::hidden]] Array<void*> BlueprintSequencePreExePlugins;
    [[bg3::hidden]] Array<void*> BlueprintSequencePostExePlugins;
    LegacyRefMap<FixedString, uint32_t> VarTypesFromSource;
    LegacyRefMap<FixedString, FixedString> NodeTypesFromSource;
    LegacyRefMap<FixedString, FixedString> SequenceTypesFromSource;
    LegacyRefMap<FixedString, FixedString> SocketTypesFromSource;
    LegacyRefMap<FixedString, FixedString> LinkTypesFromSource;
};

struct [[bg3::hidden]] GenomeVariableRange
{
    uint16_t Start;
    uint16_t End;
};

struct [[bg3::hidden]] GenomeExecutionContext
{
    GameTime GameTime;
    GenomeBlueprintInstance* Instance;
    GenomeSequence* qword20;
    GenomeSequence* qword28;
    NodeIndex NodeIndex;
    GenomeVariableRange* InputRangesStart;
    GenomeVariableRange* InputRangesEnd;
    VariableIndex* InputsStart;
    VariableIndex* InputsEnd;
    GenomeVariableRange* OutputRangesStart;
    GenomeVariableRange* OutputRangesEnd;
    VariableIndex* OutputsStart;
    VariableIndex* OutputsEnd;
    Array<GenomeVariant>* Variables;
    GenomeVariant* StackStart;
    GenomeVariant* StackEnd;
    GenomeInstanceDebugContext* DebugContext;
};

struct [[bg3::hidden]] GenomeManager : public ProtectedGameObject<GenomeManager>
{
    GenomeTypeManager* TypeManager;
    void* PluginData;
    Array<void*> Listeners_pListener;
    CRITICAL_SECTION ListenerLock;
    LegacyRefMap<GenomeBlueprint*, Array<GenomeBlueprintInstance*>> BlueprintInstances;
    CRITICAL_SECTION InstanceLock;
    LegacyRefMap<STDString, BlueprintInstanceData> LoadedBlueprints;
    LegacyRefMap<GenomeBlueprint*, STDString> BlueprintsToPath;
#if defined(BG3SE_LINUX)
    // PORT (linux): these three members are ls::SRWLock (Base/Lock.h) hand-expanded by
    // upstream -- the primitive, its owning thread and its write recursion count.  On this
    // build the primitive is a bare 56-byte pthread_rwlock_t and the owner is a pthread_t,
    // so the block is 0x48 bytes where Win32's is 0x10; witnesses below the struct.
    LS_SRWLOCK BlueprintInstantiationRWLock;
    uint64_t LockThreadID;
    uint32_t LockWriteEnterCount;
#else
    SRWLOCK BlueprintInstantiationRWLock;
    int LockThreadID;
    int LockWriteEnterCount;
#endif
    // Editor only?
    // __int64 field_B0;
    Array<void*> SourceReaderPlugins;
    StringView CompiledPathRoot;
    StringView DebugPathRoot;
    Version CodeVersion;
    Version MinVersion;
    SynchronizedMPMCQueueBounded<GenomeBlueprintInstance*> UnloadInstanceQueue;
    SynchronizedMPMCQueueBounded<GenomeBlueprint*> UnloadQueue;
    void* UnloadBatch;
    void* UnloadBatchJob;
    CRITICAL_SECTION PendingBlueprintsLock;
    Array<std::pair<bool, Path>> PendingBlueprints;
    double SyncedTime;
    double LastSyncTime;
    bool IsMasterTime;
};

#if defined(BG3SE_LINUX)
// PORT (linux): measured on gn::GenomeManager itself, off two matched Linux methods that take
// its locks -- gn::GenomeManager::UnloadBlueprint (bin/bg3 0x3b39200, matched r5:2) does the
// whole of upstream's ReadLock/WriteLock/WriteUnlock inline, and
// gn::GenomeManager::GetCompiledPath (0x2d78a50, matched r1:3) reads the tail.  No constructor
// symbol exists for this class in either binary, so the method is the instrument.
//   BlueprintInstantiationRWLock 0xb0, and it is the bare primitive: `lea r13, [rdi + 0xb0]`
//     is the only address ever passed to the shared read acquire (0x218f6b0, a
//     pthread_rwlock_tryrdlock spin that tail-jumps pthread_rwlock_rdlock@plt), to
//     pthread_rwlock_trywrlock@plt (0x77ea330) and to pthread_rwlock_unlock@plt (0x77e8310).
//   LockThreadID 0xe8 and 8 bytes wide, not 4: `mov r14, qword [rdi + 0xe8]` / `cmp r14, -1`
//     is the recursion test, and on taking the write side the function stores the *return of
//     pthread_self@plt* (0x77e8f00) there with a qword move, then `mov qword [rbx + 0xe8],
//     0xffffffffffffffff` on release.  0xe8 - 0xb0 = 0x38 = sizeof(pthread_rwlock_t), so there
//     is nothing between the primitive and the owner.
//   LockWriteEnterCount 0xf0: `inc dword [rbx + 0xf0]` after the write acquire,
//     `dec dword [rbx + 0xf0]` before the release, and the -1 store happens only when it
//     reaches zero.
//   SourceReaderPlugins 0xf8, so the block ends there and the whole of it is 0x48: the u32
//     count's own 4 bytes of tail padding are the last of it.
// The same function confirms the prefix, which is upstream's, all of it behind
// 48-byte ls::CriticalSections: BlueprintInstances 0x50 (`mov esi, dword [rbx + 0x54]` is its
// HashSize), InstanceLock 0x60 (taken with pthread_mutex_trylock@plt (0x77e8ef0) on
// `rbx + 0x60`, with `inc dword [rbx + 0x88]` = the recursion count at lock+0x28),
// LoadedBlueprints 0x90 and BlueprintsToPath 0xa0 -- the latter two witnessed by what the
// lookup does with them, in the mirror's own declared key types: divide the blueprint pointer
// by `[rbx + 0xa4]` and index `[rbx + 0xa8]`, then hash the STDString found in that node
// through (anonymous namespace)::Hash(ls::_StringView<char>) (0x2515450), divide by
// `[rbx + 0x94]` and index `[rbx + 0x98]`.
// And the tail: CompiledPathRoot 0x108, read as `{qword [rsi + 0x108], dword [rsi + 0x110]}`
// by GetCompiledPath, which appends it to the path it returns; UnloadQueue 0x240 (its own
// lock, read-acquired on `rbx + 0x240`, then Queue.Capacity 0x280, Queue.Slots 0x288 and
// Queue.WriteIndex 0x2c0 in a textbook bounded-queue Emplace with a 64-byte slot stride and
// the value at slot+8); UnloadBatch 0x340 and UnloadBatchJob 0x348.  UnloadInstanceQueue is
// therefore at 0x140, i.e. the two 8-byte Versions at 0x128/0x130 are followed by 8 bytes of
// padding for the queue's alignas(64).
static_assert(offsetof(GenomeManager, BlueprintInstances) == 0x50);
static_assert(offsetof(GenomeManager, InstanceLock) == 0x60);
static_assert(offsetof(GenomeManager, LoadedBlueprints) == 0x90);
static_assert(offsetof(GenomeManager, BlueprintsToPath) == 0xa0);
static_assert(offsetof(GenomeManager, BlueprintInstantiationRWLock) == 0xb0);
static_assert(offsetof(GenomeManager, LockThreadID) == 0xe8);
static_assert(offsetof(GenomeManager, LockWriteEnterCount) == 0xf0);
static_assert(offsetof(GenomeManager, SourceReaderPlugins) == 0xf8);
// The lock block's own size, derived from the primitive rather than hand-counted: the owner
// and the count add 16 bytes to it, padding included.
static_assert(offsetof(GenomeManager, SourceReaderPlugins)
              - offsetof(GenomeManager, BlueprintInstantiationRWLock) - sizeof(LS_SRWLOCK) == 16,
              "ls::SRWLock hand-expanded is the bare primitive, a pthread_t owner and a u32");
static_assert(offsetof(GenomeManager, CompiledPathRoot) == 0x108);
static_assert(offsetof(GenomeManager, UnloadInstanceQueue) == 0x140);
static_assert(offsetof(GenomeManager, UnloadQueue) == 0x240);
static_assert(offsetof(GenomeManager, UnloadBatch) == 0x340);
static_assert(offsetof(GenomeManager, UnloadBatchJob) == 0x348);
#endif


END_NS()

BEGIN_SE()

struct TextKeyTypeProperties : public ProtectedGameObject<TextKeyTypeProperties>
{
    virtual ~TextKeyTypeProperties() = 0;
    virtual bool Visit(ObjectVisitor* visitor) = 0;
    virtual TextKeyType GetType() = 0;
    virtual void Init(TextKeyTypeProperties* props) = 0;
    virtual void Prepare(void* prepareData) = 0;
    virtual void Unprepare() = 0;
};

struct TextKeySoundTypeProperties : public TextKeyTypeProperties
{
    uint8_t SoundObjectIndex;
    FixedString Resource;
};

struct TextKeyEffectTypeProperties : public TextKeyTypeProperties
{
    FixedString Effect;
    FixedString Bone;
    FixedString field_10;
    bool HardAttach;
    bool InterruptLoopAtEnd;
    int16_t BoneId;
};

struct TextKeyFootStepTypeProperties : public TextKeyTypeProperties
{
    uint8_t FootID;
    bool Slide;
    bool PlaySound;
    bool PlayScuff;
    bool PlayStepFX;
    bool PlaySlideFX;
    bool PlayHearingFX;
};

struct TextKeyAttachTypeProperties : public TextKeyTypeProperties
{
    FixedString Bone;
    FixedString AttachBone;
    bool Detach;
};

struct TextKeyWeaponEffectTypeProperties : public TextKeyTypeProperties
{
    uint32_t Weapon;
    uint32_t EffectType;
};

struct TextKeyGenomeTypeProperties : public TextKeyTypeProperties
{
    // Empty
};

struct TextKeyAttackTypeProperties : public TextKeyTypeProperties
{
    // Empty
};

struct TextKeyRagdollTypeProperties : public TextKeyTypeProperties
{
    float AngularVelocityModifier;
    float LinearVelocityModifier;
    float StiffnessMultiplier;
    float DampingMultiplier;
    float DurationHardLimit;
    float DurationSoftLimit;
    float HitImpulseThreshold;
    bool HoldLastFrame;
    FixedString AnimationResourceID;
    bool LockLimits;
};

struct TextKeyVisualCullFlagTypeProperties : public TextKeyTypeProperties
{
    uint32_t VisualFlag;
    bool Enabled;
};

struct TextKeyFloatDataTypeProperties : public TextKeyTypeProperties
{
    float Data;
};

struct TextKeyFoleyTypeProperties : public TextKeySoundTypeProperties
{
    uint8_t FoleyType;
    uint8_t FoleyIntensity;
};

struct TextKeyVocalTypeProperties : public TextKeyTypeProperties
{
    uint8_t VocalType;
};

struct TextKeyFootMoveTypeProperties : public TextKeyTypeProperties
{
    uint8_t Data;
};

struct TextKeyReactTypeProperties : public TextKeyTypeProperties
{
    // Empty
};

struct TextKeyEvent
{
    FixedString ID;
    float Time;
    float Length;
    uint8_t Track;
    TextKeyTypeProperties* Properties;
};

struct AnimationReceivedGameplayEvent
{
    EntityHandle Entity;
    FixedString Event;
    gn::GenomeParametrizedEventArgs Args;
};

struct AnimationReceivedTextKeyEvent
{
    EntityHandle Entity;
    TextKeyEvent* Event;
    [[bg3::hidden]] void* Skeleton;
};

struct AnimationReceivedRootMotionEvent
{
    EntityHandle Entity;
    std::array<float, 7> LocalPose;
};

struct [[bg3::hidden]] AnimationBlueprintSystem : public BaseSystem
{
    DEFINE_SYSTEM(AnimationBlueprint, "ls::AnimationBlueprintSystem")

    void* VMT_IBlueprintInstanceListener;
    void* VMT_IGenomeManagerCallback;
    UnknownSignal StopUpdateSignal;
    UnknownSignal AnimationReceivedTextKeyEventSignal;
    UnknownSignal AnimationReceivedGameplayEventSignal;
    UnknownSignal BlueprintCreatedSignal;
    UnknownSignal BlueprintDestroyedSignal;
    HashMap<EntityHandle, void*> AnimationUpdates; /* UpdateJob* */
    Queue<AnimationReceivedGameplayEvent> GameplayEventsToGenome;
    CRITICAL_SECTION GameplayEventsToGenomeLock;
    // Editor only 
    // Queue<AnimationReceivedTextKeyEvent> DebugTextKeyEventsToGenome;
    // CRITICAL_SECTION DebugTextKeyEventsToGenomeLock;
    HashSet<EntityHandle> LoadedBlueprints;
    GameTime Time;
    bool IsInterestedInVisuals;
    void* Batch1;
    void* Batch2;
    void* ThreadTicket;
    Queue<AnimationReceivedRootMotionEvent> RootMotion;
    CRITICAL_SECTION RootMotionLock;
    Queue<AnimationReceivedGameplayEvent> GameplayEventsFromGenome;
    CRITICAL_SECTION GameplayEventsFromGenomeLock;
    Queue<AnimationReceivedTextKeyEvent> TextKeyEventsFromGenome;
    CRITICAL_SECTION TextKeyEventsFromGenomeLock;
    Queue<Visual*> DelayedUpdateAttachments;
    CRITICAL_SECTION DelayedUpdateAttachmentsLock;
    Pool UpdateJobPool;
    void* ThreadDispatcher;
    gn::GenomeManager* GenomeManager;
    int32_t NextInstanceId;
    HashMap<EntityHandle, Array<AnimationInstanceChangeType>> AnimationInstanceChanges;
    Array<EntityHandle> LinkedEntities;
    // Editor only 
    // void* AnimationDebugWorker;
    // void* AnimationDebugThread;

    void LuaQueueGameplayEventToGenome(EntityHandle entity, FixedString const& event, Array<gn::GenomeVariant> args);
    void LuaQueueGameplayEventFromGenome(EntityHandle entity, FixedString const& event, Array<gn::GenomeVariant> args);
};

#if defined(BG3SE_LINUX)
// This mirror is +120 against the macOS oracle's operator new(0x2a0):
// five locks, 48 bytes each here and 24 on macOS. bin/bg3's own TriggerGameplayEvent (0x2381110,
// matched r4:6) is the witness that the mirror is right, not the oracle.
static_assert(offsetof(AnimationBlueprintSystem, GameplayEventsToGenome) == 0xD8,
    "bin/bg3 TriggerGameplayEvent 0x2381198 reads the queue's buffer at this+0xd8");
static_assert(offsetof(AnimationBlueprintSystem, GameplayEventsToGenomeLock) == 0xF0,
    "bin/bg3 TriggerGameplayEvent 0x2381144 locks this+0xf0 and ++'s its count at +0x118");
#endif


END_SE()

BEGIN_NS(lua)

LUA_POLYMORPHIC(TextKeyTypeProperties);

END_NS()
