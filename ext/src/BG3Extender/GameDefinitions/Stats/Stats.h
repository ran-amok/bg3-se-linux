#pragma once

#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/Stats/Common.h>
#include <GameDefinitions/Stats/Functors.h>
#include <GameDefinitions/Stats/Entries.h>

BEGIN_NS(stats)

enum class RPGEnumerationType
{
    Int,
    Int64,
    Float,
    FixedString,
    Enumeration,
    Flags,
    GUID,
    StatsFunctors,
    Conditions,
    RollConditions,
    Requirements,
    // Legacy DOS2 type, unused
    MemorizationRequirements,
    TranslatedString,
    Unknown
};

struct RPGEnumeration : public ProtectedGameObject<RPGEnumeration>
{
    FixedString Name;
    LegacyMap<FixedString, int32_t> Values;

    static bool IsFlagType(FixedString const& typeName);
    RPGEnumerationType GetPropertyType() const;

    inline FixedString const& GetElementName() const
    {
        return Name;
    }
};

struct Modifier : public Noncopyable<Modifier>
{
    int32_t EnumerationIndex{ -1 };
    int32_t LevelMapIndex{ -1 };
    int32_t UnknownZero{ 0 };
    FixedString Name;

    inline FixedString const& GetElementName() const
    {
        return Name;
    }
};

struct ModifierList : public Noncopyable<ModifierList>
{
    // PORT (linux): the game *derives* this from CNamedElementManager<Modifier> rather than
    // holding one, so Itanium puts Name in the manager's tail padding at 0x5C and the whole
    // object is 0x60 -- where a plain member gives 0x60/0x68 and every read of Name lands four
    // bytes past the object (it read `gPastels` for BLESS on the server and a garbage FixedString
    // id, 0x85c6b058, on the client, which faulted ls::gst::Acquire).  MSVC never reuses tail
    // padding, so upstream cannot tell the two apart
    // and its layout is right on Windows either way.  [[no_unique_address]] buys the base-class
    // layout without renaming the member at thirteen call sites or moving Attributes out of the
    // Lua property map.  MSVC ignores the standard spelling (it wants [[msvc::no_unique_address]]),
    // which is exactly the behaviour upstream wants.
    [[no_unique_address]] CNamedElementManager<Modifier> Attributes;
    FixedString Name;

    Modifier* GetAttributeInfo(FixedString const& name, int * attributeIndex) const;

    inline FixedString const& GetElementName() const
    {
        return Name;
    }
};

struct ItemTypeManager : public CNamedElementManager<ItemType>
{
    uint64_t Unknown;
};

struct ItemProgressionManager : ProtectedGameObject<ItemProgressionManager>
{
    LegacyMap<FixedString, ItemGroup*> ItemGroups;
    LegacyMap<FixedString, NameGroup*> NameGroups;
    ItemGroup* CurrentItemGroup;
    LevelGroup* CurrentLevelGroup;
    RootGroup* CurrentRootGroup;
    NameGroup* CurrentNameCool;
};

struct ItemCombinationManager : public CNamedElementManager<ItemCombination>
{
    ItemCombination* CurrentCombination;
    ItemCombinationPreviewData* CurrentPreviewData;
    ItemCombinationProperty* CurrentProperty;
    LegacyRefMap<FixedString, ItemCombinationPreviewData*> PreviewData;
    LegacyRefMap<FixedString, ItemCombinationProperty*> Properties;
};

struct RNG
{
    uint32_t* Numbers;
    int field_8;
    int NumSteps;
    int field_10;
    int field_14;
    int field_18;
    int Min;
    int field_20;
    bool Initialized;
};

struct PreParsedDataBuffer
{
    ScratchString Buf;
    FixedString Type;
    void* field_28;
};

struct ItemColor
{
    uint32_t Color[3];
};

struct BloodType
{
    FixedString Info[9];
};

struct RPGStats : public ProtectedGameObject<RPGStats>
{
    struct VMTMappings
    {
        bool VMTsMapped{ false };
        void* ObjectVMT{ nullptr };

        void Update();
    };

    static VMTMappings sVMTMappings;

    CNamedElementManager<RPGEnumeration> ModifierValueLists;
    CNamedElementManager<ModifierList> ModifierLists;
    CNamedElementManager<Object> Objects;
    CNamedElementManager<TreasureCategory> TreasureCategories;
    CNamedElementManager<TreasureTable> TreasureTables;
    ItemTypeManager ItemTypes;
    LegacyMap<FixedString, Functors*> StatsFunctors;
    RPGEnumeration* CurrentParsedValueList;
    Modifier* CurrentParsedType;
    TreasureTable* CurrentTreasureTable;
    TreasureCategory* CurrentTreasureCategory;
    // PORT (linux): a heap block the game frees in RPGStats::Destroy (macOS 0x1024fd760 frees
    // [this+0x280] and zeroes it) that upstream's mirror does not have; without it every member
    // from ExtraData on sat 8 bytes short of the game's
    void* field_280;
    uint64_t RNG[5];
    HashMap<FixedString, float>* ExtraData;
    LegacyMap<FixedString, FixedString> CategoryMappings;
    LegacyMap<FixedString, int32_t> WeaponCounters;
    LegacyMap<FixedString, int32_t> SpellbookCounters;
    LegacyMap<FixedString, int32_t> ArmorCounters;
    LegacyRefMap<FixedString, void*> field_2F0;
    FixedString TreasureRarities[7];
    TrackedCompactSet<FixedString> FixedStrings;
    Array<int64_t*> Int64s;
    Array<Guid> GUIDs;
    Array<float> Floats;
    Array<TranslatedString> TranslatedStrings;
    CNamedElementManager<EquipmentSet>* EquipmentSetManager;
    CNamedElementManager<SpellSet>* SpellSetManager;
    ItemProgressionManager* ItemProgressionManager;
    ItemCombinationManager* ItemCombinationManager;
    void* CurrentDataBuffer;
    FixedString CurrentDataBufferPath;
    LegacyMap<FixedString, int32_t> PreParsedDataBufferMap;
    Array<PreParsedDataBuffer*> PreParsedDataBuffers;
    LegacyMap<FixedString, BloodType*> BloodTypes;
    bool field_3E0;
    bool IsLoading;
    bool Loaded;
    uint32_t FastLock;
    Array<STDString> Conditions;

    Modifier * GetModifierInfo(FixedString const& modifierListName, FixedString const& modifierName);
    ModifierList * GetTypeInfo(Object * object);

    bool ObjectExists(FixedString const& statsId, FixedString const& type);
    std::optional<Object*> CreateObject(FixedString const& name, FixedString const& type);
    std::optional<Object*> CreateObject(FixedString const& name, int32_t modifierListIndex);
    Functors* ConstructFunctorSet(FixedString const& propertyName);
    Functor* ConstructFunctor(FunctorId action);
    void SyncWithPrototypeManager(Object* object);

    std::optional<FixedString*> GetFixedString(int stringId);
    FixedString* GetOrCreateFixedString(int& stringId);
    std::optional<int64_t*> GetInt64(int int64Id);
    int64_t* GetOrCreateInt64(int& int64Id);
    std::optional<float*> GetFloat(int floatId);
    float* GetOrCreateFloat(int& floatId);
    std::optional<Guid*> GetGuid(int guidId);
    Guid* GetOrCreateGuid(int& guidId);
    std::optional<TranslatedString*> GetTranslatedString(int tsId);
    TranslatedString* GetOrCreateTranslatedString(int& tsId);
    std::optional<STDString*> GetConditions(int conditionsId);
    int GetOrCreateConditions(STDString const& conditions);

    std::optional<int> EnumLabelToIndex(FixedString const& enumName, char const* enumLabel);
    FixedString EnumIndexToLabel(FixedString const& enumName, int index);
};

// PORT (linux): witnessed on bin/bg3 c8da62b1 and the macOS oracle. The mirror agrees
// with the game up to the parser cursors (0x2fd1cf0 writes [this+0x260], 0x2fd6120 zeroes [this+0x270]);
// the game then keeps a pointer at +0x280 (freed by macOS RPGStats::Destroy 0x1024fd760) before the RNG
// state, and ExtraData is the pointer at +0x2b0 (esv::VisualSystem::Update 0x238a664 hands it to the
// ExtraData lookup; Destroy frees it). In RPGStats::Load 0x3b3c120 the four managers are allocated back
// to back into +0x398/+0x3a0/+0x3a8/+0x3b0 (0x68, 0x68, 0x50, 0x98 bytes), the last being what
// CItemCombinationManager::LoadText(paths, RPGStats&) 0x2fd8240 gets; Loaded is the byte Load xchg's to 1
// at its end and Destroy xchg's to 0 at its start; FastLock is the spin word Load takes with lock xadd
// 0x100000 (the macOS RPGStats::Start 0x10250b750 opens on it). Load also sets [this+0x408] for its
// duration, one byte below what upstream names IsLoading; not asserted.
static_assert(offsetof(RPGStats, ExtraData) == 0x2B0, "bin/bg3 0x238a664 loads [stats+0x2b0] for the ExtraData lookup");
static_assert(offsetof(RPGStats, EquipmentSetManager) == 0x398, "bin/bg3 0x3b3c120 stores new(0x68) at [stats+0x398]");
static_assert(offsetof(RPGStats, ItemCombinationManager) == 0x3B0, "bin/bg3 0x3b3c120 stores new(0x98) at [stats+0x3b0]");
// The two the stats-load hook walks (StatLoadOrderHelper::OnStatFileOpened), from the macOS
// RPGStats::InsertDataBuffer 0x1025077c0: div [this+0x3c8], + [this+0x3d0], inc [this+0x3d8] is the
// map; [this+0x3e0]/[this+0x3e8]/[this+0x3ec] the buffer array. No lock sits before them, so the
// oracle's offsets are absolute here -- and both have a Linux witness of their own as well:
// RPGStats::ParseDataBuffers 0x27ddb00 opens on mov rcx,[rdi+0x3e0] / movsxd
// rax,[rdi+0x3ec] / lea rax,[rcx+rax*8], the buffer array and its size, and
// FinishAndCleanCurrentParsedType 0x3b3cb00 reads dword [rbx+0x3c8] as the map's hash size before
// walking [rbx+0x3d0].
static_assert(offsetof(RPGStats, PreParsedDataBufferMap) == 0x3C8, "bin/bg3 0x3b3cb00 reads [stats+0x3c8] as the map's hash size; macOS RPGStats::InsertDataBuffer 0x1025077c0 hashes into it");
static_assert(offsetof(RPGStats, PreParsedDataBuffers) == 0x3E0, "bin/bg3 0x27ddb00 loads [stats+0x3e0] with [stats+0x3ec] as its size; macOS RPGStats::InsertDataBuffer 0x1025077c0 pushes into it");
static_assert(offsetof(RPGStats, Loaded) == 0x40A, "bin/bg3 0x3b3c120 xchg's [stats+0x40a] to 1 at its end");
static_assert(offsetof(RPGStats, FastLock) == 0x40C, "bin/bg3 0x3b3c120 lock xadd's [stats+0x40c]");

// PORT (linux): the element managers and the modifier list, read end to end. Every
// offset below has a witness in bin/bg3 as well as in the oracle, and none of these
// objects holds a lock, so the offsets are absolute.
//
// The manager: macOS CNamedElementManager<CRPGStats_Modifier_List,int,0,-1>::GetEntry(size_t)
// 0x101f95370 is four instructions -- mov eax,[rdi+0x14]; cmp rax,rsi; jbe; mov rax,[rdi+8];
// mov rax,[rax+rsi*8] -- so Values.buf_ is +8 and Values.size_ +0x14. ::Insert 0x101f94f30 reads
// the capacity at [rbx+0x10] and post-increments NextHandle at [rbx+0x58]; ::UpdateHandles
// 0x101f953b0 passes [rdi+0x18] as the HashMap. Linux corroborates NextHandle: RPGStats
// ParseStructureFolder 0x2fd2782 zeroes the qword at [new+0x58] in the inlined constructor.
static_assert(offsetof(CNamedElementManager<Modifier>, NextHandle) == 0x58, "macOS CNamedElementManager<CRPGStats_Modifier_List>::Insert 0x101f94f7c post-increments dword [this+0x58] as the new handle; bin/bg3 0x2fd2782 zeroes it");
static_assert(sizeof(CNamedElementManager<Modifier>) == 0x60, "macOS ::Insert 0x101f94f30 ends its fields at NextHandle (+0x58), so the manager is 0x60 with four bytes of tail padding");
//
// The modifier list *derives* from that manager (see the note on the member above), so its own
// FixedString sits in the tail padding. bin/bg3 RPGStats::ParseStructureFolder 0x2fd1cf0 is the
// witness on this binary: 0x2fd2354 is mov edi,0x60; call operator new, the inlined constructor
// zeroes 8..0x60 and stores the vtable at +0, then 0x2fd27a6 initialises dword [new+0x5c] to -1
// (an empty FixedString) and 0x2fd27c0/0x2fd27d0 read it back, compare it with -1 and store the
// parsed name's index there. The oracle agrees twice over: macOS
// CNamedElementManager<CRPGStats_Modifier_List>::UpdateHandles 0x101f953e8 hands the map
// Values[i]+0x5c as GetElementName(), and ::Insert 0x101f94f44 does lea r15,[rsi+0x5c].
static_assert(offsetof(ModifierList, Name) == 0x5C, "bin/bg3 0x2fd27a6 sets dword [list+0x5c] to -1 in the new(0x60) at 0x2fd2354 and 0x2fd27d0 stores the parsed name index there; macOS 0x101f953e8 and 0x101f94f44 take it as GetElementName()");
static_assert(sizeof(ModifierList) == 0x60, "bin/bg3 0x2fd2354 allocates the modifier list with mov edi,0x60");
static_assert(offsetof(Modifier, Name) == 0x0C, "macOS CNamedElementManager<CRPGStats_Modifier>::UpdateHandles 0x102500338 adds 0xc to Values[i] for GetElementName()");
//
// stats::Object needed no change; these record what the same reading established, because a
// garbage ModifierList name was first blamed on this object. bin/bg3 eoc::GetGoldValue(
// StatsObject const*) 0x27e0c30 (matched s2:0.85 to macOS 0x101e9ad60) is the whole chain in
// thirty instructions: 0x27e0c53 mov eax,[obj+0xe4] is ModifierListIndex; it loads gRPGStats
// (0x7bbd418, manifest row), bound-checks the index against dword [stats+0xb8] and indexes
// qword [stats+0x68] -- ModifierLists.NextHandle and .Values.buf_, so ModifierLists is at 0x60 --
// and 0x27e0cb0 reads IndexedProperties' begin at [obj+8] with a dword stride. macOS
// StatsObject::SetType(int) 0x1023c34d0 is the same shape on the write side (cmp [rdi+0xe4],esi
// ... mov [rbx+0xe4],r15d) and CNamedElementManager<StatsObject>::UpdateHandles 0x1024fbb88 adds
// 0x20 for the name. The same function's [stats+0x368]/[stats+0x374] with shl 4 is GUIDs and its
// size, which is the mirror's 0x368 unchanged.
static_assert(offsetof(RPGStats, ModifierLists) == 0x60, "bin/bg3 0x27e0c80 indexes qword [gRPGStats+0x68] for the modifier list and bound-checks dword [gRPGStats+0xb8] (Values.buf_ and NextHandle of a manager at 0x60)");
static_assert(offsetof(RPGStats, GUIDs) == 0x368, "bin/bg3 0x27e0ce2 adds qword [gRPGStats+0x368] to index<<4 after bound-checking dword [gRPGStats+0x374]");
static_assert(offsetof(Object, IndexedProperties) == 0x08, "bin/bg3 0x27e0cb0 loads [obj+8] and reads it at a dword stride");
static_assert(offsetof(Object, Name) == 0x20, "macOS CNamedElementManager<StatsObject>::UpdateHandles 0x1024fbb88 adds 0x20 to Values[i] for GetElementName()");
static_assert(offsetof(Object, ModifierListIndex) == 0xE4, "bin/bg3 0x27e0c53 loads dword [obj+0xe4] as the modifier list handle; macOS StatsObject::SetType 0x1023c34e1/0x1023c3531 compares and stores it");

Object * StatFindObject(char const * name, bool warnOnError = true);
Object * StatFindObject(int index);

END_NS()
