#pragma once

#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/EntitySystem.h>

BEGIN_NS(dlg)

struct Dialog;
struct DialogNode;
struct DialogManager;
struct DialogInstance;
struct DefaultDialogNode;
struct DialogVisitor;
struct IInputNode;
struct DialogueVisualTag;
struct Flag;
struct TagRuleGroup;
struct DialogEventListener;
struct IGameInterface;
struct INodeConstructor;
struct FlagParamInfo;


struct FlagDescription
{
    FixedString Name;
    bool HasFlagParamInfo;
    [[bg3::hidden]] FlagParamInfo* ParamInfo;
};


struct Flag : public ProtectedGameObject<Flag>
{
    virtual ~Flag() = 0;

    Guid Uuid;
    FixedString Type;
    bool Value;
    [[bg3::hidden]] uint32_t _Pad;
};


struct FlagCollection
{
    [[bg3::hidden]] DialogManager* DialogManager;
    LegacyRefMap<FixedString, Array<Flag*>> Flags;
};


struct ScriptFlag
{
    Guid Uuid;
    STDString Script;
    uint64_t KeywordConditionList;
#if 0
    //STDString Name;
    //STDString Description;
    //STDString Path;
    //STDString ModName;
#endif
};


struct SpeakerGroup
{
    Guid Uuid;
    Guid OverwriteSpeakerUuid;
#if 0
    //STDString SpeakerName;
    //STDString Description;
    //STDString Path;
    //STDString ModName;
#endif
};


struct Variable
{
    Guid Uuid;
    uint8_t Type;
    STDString DefaultValue;
#if 0
    //STDString Name;
    //STDString Description;
    //STDString Path;
    //STDString ModName;
#endif
};


struct TextLine
{
    Guid LineId;
    Guid CustomSequenceId;
    TranslatedFSString TagText;
    TranslatedFSString OldText;
    TrackedCompactSet<FixedString> DialogVariables;
    bool HasOverrideText;
    STDString Content;
    uint8_t field_70;
    bool Stub;
    TranslatedFSString ActualText;
};


struct TaggedText
{
    TagRuleGroup* RootRule;
    TrackedCompactSet<TextLine> Lines;
};


struct TagRuleEntry : public ProtectedGameObject<TagRuleGroup>
{
    virtual ~TagRuleEntry() = 0;
};


struct TagRule : public TagRuleEntry
{
    int SpeakerIndex;
    Array<Guid> CheckedTags;
    int CombineOp;
};


struct TagRuleGroup : public TagRuleEntry
{
    int Operator;
    Array<TagRuleEntry*> ChildRules;
};


struct LookAtInfo
{
    int16_t Speaker;
    int16_t Target;
};


struct DialogGameData : public ProtectedGameObject<DialogGameData>
{
    virtual ~DialogGameData() = 0;
    virtual DialogNode* GetParentNode() const = 0;
    virtual void SetParentNode(DialogNode&) = 0;
    virtual bool OnVisit(ObjectVisitor&) = 0;
    virtual void SerializeToBuffer(ScratchBuffer&) = 0;
    virtual void DeserializeFromBuffer(ScratchBuffer&) = 0;
    virtual void Unknown0() = 0;
    virtual void Unknown1() = 0;
    virtual bool ValidateNode(DialogInstance const&) = 0;
    virtual bool OnNodeStarted(DialogInstance&) = 0;
    virtual TranslatedFSString GetCustomDisplayText(DialogInstance const&, FixedString const&, unsigned int, TranslatedFSString&&) const = 0;
    virtual void* GetOverridenTag(DialogInstance const&, FixedString const&, unsigned int) const = 0;
    virtual void Clone(DialogGameData const*) = 0;

    DialogNode* ParentNode;
    TrackedCompactSet<FixedString> AiPersonality;
    Array<LookAtInfo> LookAt;
    STDString SoundEvent;
    std::array<STDString, 4> SoundPerMusicInstrument;
    FixedString OriginTheme;
    FixedString OriginOnly;
    bool OriginThemeAddInstrument;
    STDString CustomMovie;
    [[bg3::hidden]] IGameInterface* GameInterface;
    int CameraTarget;
    int ExtraWaitTime;
};


struct NodeData
{
    FixedString UUID;
    int field_4;
    int field_8;
    int field_C;
    Guid field_10;
};


struct Dialog : public ProtectedGameObject<Dialog>
{
    bool Automated;
    bool IsWorld;
    bool IsBehaviour;
    bool IsSFXDialog;
    bool AllowDeadSpeakers;
    bool IsAllowingJoinCombat;
    bool IsSubbedDialog;
    bool IsSubsAnonymous;
    bool IsPrivateDialog;
    LegacyRefMap<FixedString, DialogNode*> Nodes;
    [[bg3::hidden]] DialogManager* DialogManager;
    STDString Category;
    TrackedCompactSet<STDString> SpeakerGroups;
    TrackedCompactSet<STDString> SpeakerTags;
    Array<Guid> SpeakerMappingIds;
    HashMap<int, int> DefaultAddressedSpeakers;
    HashSet<int> PeanutSlots;
    Array<DialogNode*> RootNodes;
    STDString FileName;
    FixedString UUID;
    FixedString TimelineId;
    double UnloadedTimeStamp;
    int RefCount;
    int DefaultSpeakerIndex;
    __int64 field_128;
    __int64 field_130;
};



struct DialogNode : public ProtectedGameObject<DialogNode>
{
    virtual ~DialogNode() = 0;
    virtual Guid GetApprovalRatingID() const = 0;
    virtual void SetParentDialog(Dialog*) = 0;
    virtual bool NeedsUserInput() const = 0;
    virtual bool IsValid(DialogInstance const&, DialogNode const*) const = 0;
    virtual bool CanUserSelect(DialogInstance const&, EntityHandle const&) const = 0;
    virtual bool ReferencesNodes() const = 0;
    virtual void GetReferencedNodes(ObjectSet<FixedString>&) const = 0;
    virtual PrimitiveSet<TaggedText*> const* GetTaggedTexts() const = 0;
    virtual bool Visit(DialogVisitor&) = 0;
    virtual DialogGameData* GetGameData() const = 0;
    virtual void SetGameData(DialogGameData*) = 0;
    virtual bool IsPresentable() const = 0;
    virtual ObjectSet<Guid> const& GetTags() const = 0;
    virtual void ClearTags() = 0;
    virtual void AddTag(Guid const&) = 0;
    virtual void SerializeToBuffer(ScratchBuffer&) = 0;
    virtual void DeserializeFromBuffer(ScratchBuffer&, DialogManager*) = 0;
    virtual DefaultDialogNode const* ToDefaultDialogNode() const = 0;
    virtual DefaultDialogNode const* ToDefaultDialogNode2() const = 0;
    virtual IInputNode const* ToInputNode() const = 0;
    virtual IInputNode const* ToInputNode2() const = 0;
    virtual bool Start(DialogInstance&) = 0;
    virtual void Resume(DialogInstance&) = 0;
    virtual void OnNodeSelected(DialogInstance&, NodeData&) = 0;
    virtual void OnPresentedForSelection(DialogInstance const&, unsigned int&, bool, EntityHandle) = 0;
    virtual void CollectAllTextLines(ObjectSet<STDString>&) = 0;
    virtual void DoSerializeStateToBuffer(ScratchBuffer&, DialogInstance const&) const = 0;
    virtual void DoDeserializeStateFromBuffer(ScratchBuffer&, DialogInstance&) = 0;
    virtual bool SupportsEmptySpeaker() const = 0;
    virtual bool HasNarratorSpeaker() const = 0;
    virtual int GetSpeakerIndex() const = 0;
    virtual int GetAddressedSpeakerIndex() const = 0;
    virtual TranslatedFSString GetDisplayText(DialogInstance const&, unsigned int, bool, bool) const = 0;
    virtual TextLine* GetTextLine(DialogInstance const&, unsigned int) const = 0;
    virtual PrimitiveSet<TaggedText*> const* GetTaggedTexts2() const = 0;
    virtual Array<DialogueVisualTag> GetDialogueVisualTag(DialogInstance const&, unsigned int) const = 0;
    virtual bool SupportsEmptySpeaker2() const = 0;
    virtual void SetSpeakerIndex(int) = 0;
    virtual void SetAddressedSpeakerIndex(int) = 0;
    virtual bool SupportsEmptySpeaker3() const = 0;


    FixedString UUID;
    FixedString GroupID;
    int GroupIndex;
    FixedString ConstructorID;
    int TransitionMode;
    DialogNodeFlags Flags;
    FlagCollection SetFlags;
    FlagCollection CheckFlags;
    Dialog* ParentDialog;
    [[bg3::hidden]] void* GameData;
    TrackedCompactSet<FixedString> Children;
    float WaitTime;
    Array<Guid> Tags;
    Guid ApprovalRatingID;
};


struct JumpNode : public DialogNode
{
    FixedString JumpTarget;
    int JumpTargetPoint;
};


struct NestedDialogNode : public DialogNode
{
    Guid NestedDialogNodeUUID;
    HashMap<int, int> SpeakerLinking;
};


struct VisualStateNode : public DialogNode
{
};


struct DefaultDialogNode : public DialogNode
{
    int32_t Speaker;
    int32_t AddressedSpeaker;
#if 0
    // STDString Value;
#endif
    bool Stub;
};


struct CinematicTaggedNode : public DefaultDialogNode
{
    uint8_t BehaviourNodeType;
};


struct AliasNode : public DefaultDialogNode
{
    FixedString SourceNode;
    bool OverrideApprovalRatingID;
};


struct BaseTaggedNode : public DefaultDialogNode
{
    Array<TaggedText*> TaggedTexts;
};


struct FallibleQuestionResultNode : public DefaultDialogNode
{
    bool Result;
};


// PORT (linux): the dlg::DefaultDialogNode family against bin/bg3 c8da62b1.  The game's
// namespace is LSDialogs, not dlg -- grepping the macOS oracle for `N3dlg` returns nothing.
// DefaultDialogNode's own three members are pinned by four matched one-instruction
// accessors and by its matched serializer; the three derived classes then put their first
// member at 0xa1/0xa4, inside the base's seven bytes of tail padding (its nvsize ends at
// 0xa1), which is what the Itanium ABI does and MSVC never does -- so the flag on these
// three records is not a defect, the mirrors already match.
//   The base (matched by name, bin/bg3):
//     LSDialogs::DefaultDialogNode::GetSpeakerIndex          0x66328d0  mov eax, [rdi+0x98]
//     LSDialogs::DefaultDialogNode::SetSpeakerIndex          0x6634e10  mov [rdi+0x98], esi
//     LSDialogs::DefaultDialogNode::GetAddressedSpeakerIndex 0x66328e0  mov eax, [rdi+0x9c]
//     LSDialogs::DefaultDialogNode::SetAddressedSpeakerIndex 0x6632930  mov [rdi+0x9c], esi
//     LSDialogs::DefaultDialogNode::SerializeToBuffer        0x73de790  calls the base's,
//       then writes one byte from each of [+0x98], [+0x9c] and [+0xa0] (0x73de803) and stops.
//       Nothing is written between 0x9c and 0xa0, so upstream's commented-out `STDString
//       Value` really is absent here.
//   The three derived classes have no Linux symbol and no match.  Their vtable runs were
//   named by a two-sided census (tools/vtables.py runs whose slot 19 is
//   DefaultDialogNode::ToDefaultDialogNode 0x6632850, against every oracle _ZTV whose slot
//   19 is the same function, 0x1013b9ae0): 16 Linux runs, 15 oracle vtables, and for each of
//   the three exactly one oracle class disagrees on no matched slot.  AliasNode is decided
//   outright -- it is the only descendant on either side that overrides slot 32
//   (GetSpeakerIndex), one run and one vtable.
//     run 0x7ab50f8 = CinematicTaggedNode        (oracle _ZTV 0x1094b3b18, 32 slots agree, 0 disagree)
//     run 0x7ab4818 = FallibleQuestionResultNode (oracle _ZTV 0x1094b43d0, 29 agree, 0 disagree)
//     run 0x7ab5448 = AliasNode                  (oracle _ZTV 0x1094b3858)
//     run 0x7ab4338 = DefaultDialogNode          (its slot 17 is the matched 0x73de790)
//   and the witnesses off those runs, slots 17 and 18:
//     CinematicTaggedNode::SerializeToBuffer      0x73e5130  mov al, byte [r14+0xa1] (0x73e513f)
//     CinematicTaggedNode::DeserializeFromBuffer  0x73e5170  mov byte [rbx+0xa1], al (0x73e51af)
//     FallibleQuestionResultNode::SerializeToBuffer     0x73dff10  reads one byte at [r14+0xa1]
//     FallibleQuestionResultNode::DeserializeFromBuffer 0x73dff40  copies one byte into [rbx+0xa1]
//     AliasNode::SerializeToBuffer      0x73e5690  mov edi, dword [r14+0xa4] -> ls::gst::Get,
//       then one byte out of [r14+0xa8]
//     AliasNode::DeserializeFromBuffer  0x73e5700  lea rdi, [rbx+0xa4] ->
//       ls::FixedString::operator=(FixedString&&) 0x242e570, then one byte into [rbx+0xa8]
//   Negative control, and the family has a built-in one: BaseTaggedNode's first member is an
//   8-aligned Array, too wide for the seven bytes of padding, so it starts at 0xa8 under both
//   ABIs and is not flagged.  LSDialogs::BaseTaggedNode::SerializeToBuffer 0x73dd9e0 (matched
//   by name) reads buf_ at qword [r14+0xa8] and size_ at dword [r14+0xb4].
//   sizeof is no witness for any of these: MSVC would make DefaultDialogNode 0xa8 as well,
//   and Cinematic/Fallible 0xb0 against Itanium's 0xa8 -- a difference, but nothing here
//   allocates one by a constant this build keeps.
static_assert(offsetof(DefaultDialogNode, Speaker) == 0x98);
static_assert(offsetof(DefaultDialogNode, AddressedSpeaker) == 0x9c);
static_assert(offsetof(DefaultDialogNode, Stub) == 0xa0);

static_assert(offsetof(CinematicTaggedNode, BehaviourNodeType) == 0xa1);

static_assert(offsetof(AliasNode, SourceNode) == 0xa4);
static_assert(offsetof(AliasNode, OverrideApprovalRatingID) == 0xa8);

static_assert(offsetof(FallibleQuestionResultNode, Result) == 0xa1);

static_assert(offsetof(BaseTaggedNode, TaggedTexts) == 0xa8);


struct SelectSpeakerNode : public BaseTaggedNode
{
    int SpeakerId;
};


struct TaggedQuestionNode : public BaseTaggedNode
{
    [[bg3::hidden]] void* InputNodeVMT;
    int SpeakerId2;
    std::optional<FlagCollection> ValidatedFlags;
};


struct DialogRoll : public ProtectedGameObject<DialogRoll>
{
    [[bg3::hidden]] void* VMT;
    stats::RollType RollType;
    SkillId Skill;
    AbilityId Ability;
    uint8_t Advantage;
    Guid DifficultyClass;
    int RollTargetSpeaker;
    bool ExcludeCompanionsOptionalBonuses;
    bool ExcludeSpeakerOptionalBonuses;
    Array<Guid> ExcludedBonusesTags;
};


struct ActiveRollNode : public TaggedQuestionNode
{
    DialogRoll Roll;
    bool DetectThoughts;
    TranslatedString AdvantageReason;
    uint8_t Flags2;
    STDString AdvantageReasonReferenceString;
};


struct PassiveRollNode : public DefaultDialogNode
{
    DialogRoll Roll;
    bool IsFlatSkillCheck;
};


struct TradeNode : public DefaultDialogNode
{
    STDString ItemsTagFilter;
    int PlayerOverride;
    bool DefaultToBarter;
};


struct QueuedActor
{
    EntityHandle field_0;
    int SpeakerId;
    bool field_C;
    bool field_D;
    bool field_E;
    bool field_F;
};


struct VisitedNode
{
    int field_0;
    int field_4;
    __int64 field_8;
    __int64 field_10;
    int field_18;
    int field_1C;
    uint8_t NodePlayed;
    uint8_t field_21;
    uint8_t field_22;
};


struct PlayedNestedDialog
{
    FixedString DialogResourceUuid;
    FixedString NestedDialogResourceUuid;
};


struct NodeSelectionInfo
{
    DialogNode* Node;
    int32_t LineId;
    bool field_C;
};

struct EmptyTarget
{
};

struct DialogInstance : public ProtectedGameObject<DialogInstance>
{
    int DialogId;
    int TimeAccumulator;
    __int64 field_8;
    TrackedCompactSet<FixedString> PopLevels;
    Array<NodeSelectionInfo> NodeSelection;
    Array<EntityHandle> Speakers;
    TrackedCompactSet<VisitedNode> VisitedNodes;
    Array<QueuedActor> QueuedActors;
    HashMap<FixedString, uint32_t> NodeCustomData;
    HashSet<FixedString> PlayedDialogs;
    NodeData CurrentNode;
    [[bg3::hidden]] void* CustomNodeVisitor;
    [[bg3::hidden]] void* CustomNodeVisitData;
    Dialog* ActiveDialog;
    FixedString DialogResourceUUID;
    Dialog* OverriddenDialog;
    HashMap<FixedString, Dialog*> Dialogs;
    int32_t State;
    std::variant<EmptyTarget, Guid, FixedString> PartyTeleportOnEnd;
    bool WasActivated;
    bool AutomatedDialog;
    bool AllowDeadSpeakers;
    bool IsAllowingJoinCombat;
    bool IsWorld;
    bool IsBehaviour;
    bool IsPrivateDialog;
    bool IsTimelineEnabled;
    uint8_t LoadedSerializedState;
    bool StartPaused;
    bool UnloadRequested;
    bool ForceStopped;
    uint8_t GracefulExitState;
    bool SmoothExit;
    bool WorldHadTriggered;
    bool IsPlayerWatchingTimeline;
    bool TimelineConfirmed;
    bool IsPreview;
    int8_t LocalHighlightedAnswer;
    int8_t HostHighlightedAnswer;
    uint8_t Flags;
    int LastPlayerSpeakerIndex;
    bool CanAttack;
    bool UseSpellVocals;
    uint8_t LevelStartingDialog;
    float DialogWaitTime;
    float CullingDistance;
    float UnloadTimeout;
    Guid TimelineSceneTriggerId;
    bool IsPaused;
    bool IsOnlyPlayers;
    bool CachedIsOnlyPlayers;
    Array<PlayedNestedDialog> PlayedNestedDialogs;
    FixedString InitialDialogResourceUUID;
    int CharacterCreationUser;
    Array<HashMap<int, int>> SpeakerLinkings;
    Array<int> UniqueSpeakerLinkingIndices;
    int SpeakerLinkingIndex;
};


struct ActorRefCount
{
    uint16_t field_0;
    uint16_t field_2;
    uint16_t field_4;
    uint16_t field_8;
};


struct DialogManager : public ProtectedGameObject<DialogManager>
{
    [[bg3::hidden]] Array<DialogEventListener*> EventListeners;
    Array<int32_t> PendingInstanceLoads;
    LegacyRefMap<int, DialogInstance*> Dialogs;
    LegacyRefMap<FixedString, FlagDescription*> FlagDescriptions;
    [[bg3::hidden]] LegacyRefMap<FixedString, INodeConstructor*> NodeConstructorMap;
    [[bg3::hidden]] void* NodeLogic;
    uint8_t field_58;
    [[bg3::hidden]] IGameInterface* GameInterface;
    int NextInstanceId;
    [[bg3::hidden]] Array<INodeConstructor*> NodeConstructors;
    Array<Dialog*> CachedDialogs;
    LegacyRefMap<EntityHandle, ActorRefCount> ActorRefCounts;
    [[bg3::hidden]] FlagParamInfo* TagFlagParamInfo;
    [[bg3::hidden]] void* KeywordParser;
    [[bg3::hidden]] void* KeywordParseCallback;
    LegacyRefMap<Guid, ScriptFlag> ScriptFlags;
    LegacyRefMap<Guid, Variable> DialogVariables;
    LegacyRefMap<Guid, SpeakerGroup> SpeakerGroups;
    LegacyRefMap<int, Guid> DialogToVariable;
    Array<int32_t> field_F8;
    [[bg3::hidden]] CRITICAL_SECTION CriticalSection;
    [[bg3::hidden]] ModManager* ModManager;
};



struct [[bg3::hidden]] SharedDialogGameInterface : public ProtectedGameObject<SharedDialogGameInterface>
{
    using DialogPreloadCallbackProc = void (DialogInstance&, DialogManager&, bool);

    virtual ~SharedDialogGameInterface();
    virtual void fun00();
    virtual void fun01();
    virtual DialogGameData* CreateGameData();
    virtual DialogGameData* CloneGameData(DialogGameData const*);
    virtual void DestroyGameData(DialogGameData*);
    virtual void PreLoadDialog(DialogInstance&, DialogPreloadCallbackProc*, bool);
    virtual void fun02();
    virtual void OnManagerShutdown(DialogManager*);
    virtual void OnManagerStart(DialogManager*);
    virtual float GetDefaultNodeWaitTime() const;
    virtual void fun03();

    // PORT (linux): each IntrusiveMPSCQueueSmall<DialogIOLoadRequest> is 26 qwords in the Itanium
    // builds, not upstream's 27. The ctor (bin/bg3 0x3dd04e0, macOS 0x101fe6f40) points both queue
    // heads at the sentinel at +0x18 and zeroes 0xc0 bytes of it, so the first queue is +0x8..0xd8
    // and the second +0xd8..0x1a8, and everything after them sits 0x10 below upstream's offsets:
    // DialogManager is at +0x1b8 (OnManagerStart, bin/bg3 0x6561f80 / macOS 0x101fe3a40), where
    // upstream's 0x1c8 read 16 bytes past it.
    uint64_t DialogIOLoadRequests_MPSCQueue[26];
    uint64_t DialogIOLoadRequests_MPSCQueue2[26];
    Array<void*> DialogIOLoadRequests;
    DialogManager* DialogManager;
    Pool DialogGameData_Pool;
    ScratchString Scratch1;
    Array<void*> Syntax;
    Array<void*> field_220;
    // PORT (linux): upstream's field_230/field_238/field_240 are not in this class. The base ctor's
    // last store is into field_220 (+0x1f8..0x208) and the derived ctor's first own store is at
    // +0x208, so the base is 0x208 bytes; the 0x20 after it is the derived class's Scratch2.
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(SharedDialogGameInterface, DialogIOLoadRequests_MPSCQueue2) == 0xD8,
    "the bin/bg3 ctor stores the second queue's head at this+0xd8 (0x3dd051f; macOS 0x101fe6fbb)");
static_assert(offsetof(SharedDialogGameInterface, DialogIOLoadRequests) == 0x1A8,
    "the bin/bg3 ctor zeroes this+0x1a8..0x1c4 right after the second sentinel (0x3dd054f, 0x3dd0556)");
static_assert(offsetof(SharedDialogGameInterface, DialogManager) == 0x1B8,
    "eoc::SharedDialogGameInterface::OnManagerStart is mov [rdi+0x1b8], rsi (bin/bg3 0x6561f80; macOS 0x101fe3a40)");
static_assert(offsetof(SharedDialogGameInterface, DialogGameData_Pool) == 0x1C0,
    "CreateGameData lock-incs PoolSize at this+0x1c0 (bin/bg3 0x364aaa1)");
static_assert(offsetof(SharedDialogGameInterface, Scratch1) == 0x1C8,
    "the bin/bg3 ctor zeroes Buffer at this+0x1c8, writes -1 at +0x1d8 and Managed/CanGrow 0x0101 at +0x1e0 (0x3dd055d..0x3dd056f)");
static_assert(offsetof(SharedDialogGameInterface, Syntax) == 0x1E8,
    "the bin/bg3 ctor grows the array at this+0x1e8 and appends \"CONDITION IsFlag(#,$,#)\" (0x3dd0586..0x3dd05a6)");
static_assert(offsetof(SharedDialogGameInterface, field_220) == 0x1F8,
    "\"CONDITION IsFlag(SpeakerIndex, FlagName_Guid, Bool)\" goes into the array at this+0x1f8 (bin/bg3 0x3dd0844..0x3dd085a)");
static_assert(sizeof(SharedDialogGameInterface) == 0x208,
    "the esv::EoCDialogGameInterface ctor's first own store is at this+0x208 (bin/bg3 0x4eacf80; macOS 0x1054336ff)");
#endif


END_NS()

BEGIN_NS(esv)


struct [[bg3::hidden]] DialogNodeConstructor : public ProtectedGameObject<DialogNodeConstructor>
{
    void* VMT;
    FixedString Name;
};

struct [[bg3::hidden]] DialogGameInterface : public dlg::SharedDialogGameInterface
{
    // PORT (linux): a ScratchString where upstream has field_230..field_248. The ctor (bin/bg3
    // 0x4eacf60, macOS 0x1054336e0) writes the same pattern into it that the base ctor writes into
    // Scratch1: Buffer/Position/Capacity zero, -1 at +0x10, Managed/CanGrow 0x0101 at +0x18.
    ScratchString Scratch2;
    // An 8-byte object holding only a vtable (the ctor stores one at +0x228); every FlagDescription's
    // ParamInfo below is its address.
    void* FlagParamInfo;
    dlg::FlagDescription GlobalFlag;
    dlg::FlagDescription ObjectFlag;
    dlg::FlagDescription PartyFlag;
    dlg::FlagDescription DialogFlag;
    dlg::FlagDescription UserFlag;
    dlg::FlagDescription UserShapeFlag;
    dlg::FlagDescription QuestFlag;
    dlg::FlagDescription DialogInstanceFlag;
    DialogNodeConstructor PopNodeConstructor;
    DialogNodeConstructor ActiveRollNodeConstructor;
    DialogNodeConstructor PassiveRollNodeConstructor;
    DialogNodeConstructor RollResultNodeConstructor;
    DialogNodeConstructor TradeNodeConstructor;
    // PORT (linux): upstream's HashMap<EntityHandle, void*> field_328_EH_pVariableManager is not in
    // the Itanium builds. The ctor's last store is TradeNodeConstructor.Name at +0x2f8, and the
    // system ctor builds this object at system+0x30 and zeroes its own next member at system+0x330.
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(DialogGameInterface, Scratch2) == 0x208,
    "the bin/bg3 ctor zeroes this+0x208, writes -1 at +0x218 and 0x0101 at +0x220 (0x4eacf80..0x4eacf92; macOS 0x1054336ff)");
static_assert(offsetof(DialogGameInterface, FlagParamInfo) == 0x228,
    "the bin/bg3 ctor stores a vtable at this+0x228 and hands lea [this+0x228] to every FlagDescription (0x4eacf9b, 0x4eacfcc)");
static_assert(offsetof(DialogGameInterface, GlobalFlag) == 0x230,
    "esv::EoCDialogGameInterface::OnManagerStart hands &this+0x230 to the FlagDescription RefMap's GetOrAdd (bin/bg3 0x3e7fb43)");
static_assert(offsetof(DialogGameInterface, DialogInstanceFlag) == 0x2A0,
    "LSDFS::strDialogInstanceFlag is acquired into this+0x2a0 (bin/bg3 0x4ead0d0)");
static_assert(offsetof(DialogGameInterface, PopNodeConstructor) == 0x2B0,
    "a vtable at this+0x2b0 and EoCFS::strPop at +0x2b8 (bin/bg3 0x4ead0e4, 0x4ead104)");
static_assert(offsetof(DialogGameInterface, TradeNodeConstructor) == 0x2F0,
    "the bin/bg3 ctor's last store is Name at this+0x2f8 (0x4ead195); OnManagerStart registers &this+0x2e0/0x2c0/0x2d0 as node constructors (0x3e7fbbb)");
static_assert(sizeof(DialogGameInterface) == 0x300,
    "the esv::DialogSystem ctor builds it at system+0x30 and zeroes the system's next member at +0x330 (bin/bg3 0x4eacef8, 0x4eacf1c; macOS 0x10443baea, 0x10443bb00)");
#endif


struct [[bg3::hidden]] DialogSystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerDialog, "esv::DialogSystem")

    void* TimelineSystem;
    void* CombatLogSystem;
    void* RollSystem;
    void* DialogEventsSystem;
    DialogGameInterface GameInterface;
    void* DialogEventListener1;
    void* DialogEventListenerAdapter2;
    void* DialogEventListenerAdapter;
    void* RequestedRollCallbacks;
    void* field_3B8_arr_TradeInstance;
    // PORT (linux): a ScratchBuffer (0x58), not upstream's 0x20 ScratchString. The ctor (bin/bg3
    // 0x4eaced0, macOS 0x10443bac5) writes MemBufferMeta {Owned, Mutable, AllocatorTag 8,
    // Alignment 1} = 0x100080101 at +0x358 and {External, Mutable} at +0x370 (Write.Meta), zeroes
    // the rest to +0x3b0, and the macOS D2 (0x1053daed0) frees Write.Buffer at +0x378 and
    // Buffer.Buffer at +0x360 when their Origin is Owned.
    ScratchBuffer Scratch;
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(DialogSystem, GameInterface) == 0x30,
    "the bin/bg3 esv::DialogSystem ctor adds 0x30 to this before the interface ctor (0x4eacef8)");
static_assert(offsetof(DialogSystem, DialogEventListener1) == 0x330,
    "SystemUpdate<esv::DialogSystem> tests [this+0x330] before calling through it (bin/bg3 0x34a9093, 0x34a924d)");
static_assert(offsetof(DialogSystem, field_3B8_arr_TradeInstance) == 0x350,
    "zeroed on its own by the bin/bg3 ctor (0x4eacf0e); the macOS D2 deletes [this+0x350] after the two buffers (0x1053db015)");
static_assert(offsetof(DialogSystem, Scratch) == 0x358,
    "the bin/bg3 ctor writes 0x100080101 (Owned, Mutable, tag 8, align 1) at this+0x358 (0x4eacf23)");
static_assert(offsetof(DialogSystem, Scratch) + offsetof(ScratchBuffer, Write) == 0x370,
    "the bin/bg3 ctor writes {External, Mutable} at this+0x370 (0x4eacf31) and the macOS D2 frees [this+0x378] (0x1053daef0)");
static_assert(sizeof(DialogSystem) == 0x3B0,
    "operator new(0x3b0) in the bin/bg3 factory 0x4eace8f and in the macOS RegisterSystems 0x10443ba98");
#endif

END_NS()

BEGIN_NS(lua)

LUA_POLYMORPHIC(dlg::DialogNode);

END_NS()
