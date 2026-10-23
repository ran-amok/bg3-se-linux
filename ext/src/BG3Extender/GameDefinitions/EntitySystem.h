#pragma once

#include <GameDefinitions/Base/Base.h>

BEGIN_SE()

// Base class for all component data
struct BaseComponent
{
};

// Base class for component data that uses external storage
struct BaseProxyComponent : public BaseComponent
{
};

// Base class for all systems
struct BaseSystem : ProtectedGameObject<BaseSystem>
{
    virtual ~BaseSystem() = 0;
    virtual bool Initialize() = 0;
    virtual void SystemLoad() = 0;
    virtual void SystemUnload() = 0;
    virtual void OnActivated() = 0;
    virtual void OnDeactivated() = 0;
    virtual void EnsureBeforeSave() = 0;

    [[bg3::hidden]] ecs::EntityWorld* EntityWorld;
};

template <class T>
concept IsComponentType = requires(T t)
{
    t.ComponentType;
    t.ComponentName;
    t.EngineClass;
};

template <class T>
concept IsProxyComponentType = IsComponentType<T> && 
    (std::derived_from<T, BaseProxyComponent> || requires(T t) { t.ForceProxy; });

template <class T>
concept IsSystemType = std::derived_from<T, BaseSystem> &&
    requires(T t) { t.SystemType; };

END_SE()

BEGIN_NS(ecs)

struct EntityWorld;
struct EntityStorageData;

constexpr unsigned ComponentMapSize = 0x880;
constexpr unsigned OneFrameComponentMapSize = 0x280;
constexpr unsigned QueryMapSize = 0xB00;

using ComponentTypeMask = BitArray<uint64_t, ComponentMapSize/64>;
using OneFrameComponentTypeMask = BitArray<uint64_t, OneFrameComponentMapSize /64>;
using QueryMask = BitArray<uint64_t, QueryMapSize/64>;
using StorageComponentTypeMask = BitArray<uint64_t, 4>;

// Component type index, registered statically during game startup
using TComponentTypeIndex = uint16_t;
enum class ComponentTypeIndex : TComponentTypeIndex {};

// Replication component type index, registered statically during game startup
using TReplicationTypeIndex = uint16_t;
enum class ReplicationTypeIndex : TReplicationTypeIndex {};

using TSystemTypeIndex = int32_t;
enum class SystemTypeIndex : TSystemTypeIndex {};

using TQueryIndex = uint16_t;
enum class QueryIndex : TQueryIndex {};

static constexpr ReplicationTypeIndex UndefinedReplicationComponent{ 0xffff };
static constexpr ComponentTypeIndex UndefinedComponent{ 0xffff };
static constexpr SystemTypeIndex UndefinedSystem{ -1 };
static constexpr QueryIndex UndefinedQuery{ 0xffff };

inline constexpr bool IsOneFrame(ComponentTypeIndex idx)
{
    return (TComponentTypeIndex(idx) & 0x8000) == 0x8000;
}

END_NS()

BEGIN_SE()

template <>
inline uint64_t SparseHashMapHash<ecs::ComponentTypeIndex>(ecs::ComponentTypeIndex const& v)
{
    return (ecs::TComponentTypeIndex(v) & 0x7FFF) + (ecs::TComponentTypeIndex(v) >> 15) * ecs::ComponentMapSize;
}

template <>
inline uint64_t HashMapHash<ecs::ComponentTypeIndex>(ecs::ComponentTypeIndex const& v)
{
    auto h0 = ((uint64_t)v & 0x7FFF) + ((uint64_t)v >> 15) * ecs::ComponentMapSize;
    return h0 | (h0 << 16);
}

template <class T>
struct alignas(64) MPMCQueueSlot
{
    std::atomic<uint64_t> NextAndFlags;
    T Value;
};

template <class T>
struct alignas(64) MPMCQueueBounded : public ProtectedGameObject<MPMCQueueBounded<T>>
{
    uint64_t Capacity;
    MPMCQueueSlot<T>* Slots;
    struct alignas(64) {
        uint64_t WriteIndex;
    };
    struct alignas(64) {
        uint64_t ReadIndex;
    };
};


template <class T>
struct alignas(64) SynchronizedMPMCQueueBounded : public ProtectedGameObject<SynchronizedMPMCQueueBounded<T>>
{
#if defined(BG3SE_LINUX)
    // PORT (linux): the game's SRW lock is a bare 56-byte pthread_rwlock_t, not Win32's 8-byte
    // handle (ext/linux/shim/windows.h).  Unusually for this port the widening moves nothing:
    // Queue is alignas(64), so it sits at 0x40 either way and sizeof stays 0x100 -- the asserts
    // below are that invariant, and no struct holding one of these queues shifts.
    LS_SRWLOCK Lock;
#else
    SRWLOCK Lock;
#endif
    MPMCQueueBounded<T> Queue;
};

#if defined(BG3SE_LINUX)
// PORT (linux): witnessed on gn::GenomeManager::UnloadBlueprint (bin/bg3 0x3b39200, matched
// r5:2), which pushes onto GenomeManager::UnloadQueue at +0x240: the address read-acquired
// through the shared pthread_rwlock helper (0x218f6b0) and released with
// pthread_rwlock_unlock@plt is the queue's own base, and the Emplace that follows reads
// Queue.Capacity at +0x40 from it, Queue.Slots at +0x48 and Queue.WriteIndex at +0x80, with a
// 64-byte slot stride and the value stored at slot+8.  UnloadBatch at +0x340 puts the end of
// that queue at 0x340, so sizeof is 0x100.  sizeof(MPMCQueueBounded<T>) does not depend on T
// (the slots are out of line), so one instantiation stands for all six in this repo.
static_assert(offsetof(SynchronizedMPMCQueueBounded<void*>, Queue) == 0x40,
              "the game's 56-byte rwlock fits inside the alignas(64) padding before Queue");
static_assert(sizeof(SynchronizedMPMCQueueBounded<void*>) == 0x100);
#endif

END_SE()

BEGIN_NS(ecs)

inline void* DereferenceProxyComponent(void* component)
{
    return *static_cast<void**>(component);
}

struct alignas(64) FrameAllocator : public ProtectedGameObject<FrameAllocator>
{
    struct FrameBuffer
    {
        volatile int64_t Offset;
    };

    FrameBuffer* CurrentPage;
    uint64_t PageSize;
    CRITICAL_SECTION CS;
    Array<void*> FrameBuffers[2];

    void* Allocate(uint16_t size);
    FrameBuffer* AllocPage();
    void Free(void* ptr);
};

struct ECBFrameAllocator
{
    inline ECBFrameAllocator(FrameAllocator* allocator)
        : Allocator(allocator)
    {}
    
    inline ECBFrameAllocator(ECBFrameAllocator const& allocator)
        : Allocator(allocator.Allocator)
    {}

    FrameAllocator* Allocator;

    inline void* alloc(std::size_t size)
    {
        se_assert(size <= 0xFFC0);
        return Allocator->Allocate((uint16_t)size);
    }

    template <class T, class ...Args>
    T* allocArray(std::size_t n, Args... args)
    {
        se_assert(n * sizeof(T) <= 0xFFC0);
        auto ptr = static_cast<T*>(Allocator->Allocate((uint16_t)(n * sizeof(T))));
        for (auto i = 0; i < n; i++) {
            new (ptr + i) T(args...);
        }
        return ptr;
    }

    inline void free(void* ptr)
    {
        Allocator->Free(ptr);
    }
};

struct ComponentTypeEntry : public ProtectedGameObject<ComponentTypeEntry>
{
    ComponentTypeIndex TypeId;
    int UncheckedId;
    bool Replicated;
    bool OneFrame;
    bool field_A;
    bool QueryFlags[4];
    uint16_t InlineSize;
    uint16_t TotalSize;
    void* DtorProc;
#if 0
    void* CountProc;
    void* ModifiedCountProc;
#endif
    Array<ComponentTypeIndex> DependentComponentIndices;
    Array<ComponentTypeIndex> DependencyComponentIndices;
#if 0
    LSStringView Name;
#endif
};

struct StorageComponentMap : public ProtectedGameObject<StorageComponentMap>
{
    StorageComponentTypeMask WriteMask;
    union {
        uint8_t* ComponentIndices;
        std::array<uint8_t, 8> InlineComponentIndices;
    };
    uint8_t Optionals;
    uint8_t OneFrame;
    uint8_t OneFrameOptionals;
    uint8_t FilterOr;
    uint8_t FilterAnd;
    uint8_t NumComponents;
    EntityStorageData* Storage;
    uint16_t StorageId;

    uint8_t GetComponentIndex(uint8_t idx) const
    {
        se_assert(idx < NumComponents);
        if (NumComponents > 8) {
            return ComponentIndices[idx];
        }
        else {
            return InlineComponentIndices[idx];
        }
    }

    uint8_t const* GetComponentIndexPtr(uint8_t idx) const
    {
        se_assert(idx < NumComponents);
        if (NumComponents > 8) {
            return ComponentIndices + idx;
        } else {
            return &InlineComponentIndices[idx];
        }
    }

    inline std::span<uint8_t const> GetComponents() const
    {
        return std::span(GetComponentIndexPtr(0), GetComponentIndexPtr(Optionals));
    }

    inline std::span<uint8_t const> GetOptionals() const
    {
        return std::span(GetComponentIndexPtr(Optionals), GetComponentIndexPtr(OneFrame));
    }

    inline std::span<uint8_t const> GetOneFrameComponents() const
    {
        return std::span(GetComponentIndexPtr(OneFrame), GetComponentIndexPtr(OneFrameOptionals));
    }

    inline std::span<uint8_t const> GetOneFrameOptionals() const
    {
        return std::span(GetComponentIndexPtr(OneFrameOptionals), GetComponentIndexPtr(FilterOr));
    }

    inline std::span<uint8_t const> GetFilterOr() const
    {
        return std::span(GetComponentIndexPtr(FilterOr), GetComponentIndexPtr(FilterAnd));
    }

    inline std::span<uint8_t const> GetFilterAnd() const
    {
        return std::span(GetComponentIndexPtr(FilterAnd), GetComponentIndexPtr(NumComponents));
    }
};

enum class QueryFlags : uint32_t
{
    Modified = 1,
    Added = 2,
    Removed = 4
};

struct QueryDescription : public ProtectedGameObject<QueryDescription>
{
    // TypeList template parameters to ecs::query::spec::Spec<...>
    HashMap<int32_t, StorageComponentMap> EntityStorages;
#if 0
    STDString Name;
#endif
    ComponentTypeIndex* ComponentIndices;
    QueryFlags Flags;
    uint8_t IncludeAny;
    uint8_t Excludes;
    uint8_t AddOrs;
    uint8_t AddAnds;
    uint8_t OneFrames;
    uint8_t OptionalOneFrames;
    uint8_t Optionals;
    uint8_t Immediates;
    uint8_t Writes;
    uint8_t WriteEnd;

    inline std::span<ComponentTypeIndex const> GetIncludes() const
    {
        return std::span(ComponentIndices, ComponentIndices + IncludeAny);
    }

    inline std::span<ComponentTypeIndex const> GetIncludeAny() const
    {
        return std::span(ComponentIndices + IncludeAny, ComponentIndices + Excludes);
    }

    inline std::span<ComponentTypeIndex const> GetExcludes() const
    {
        return std::span(ComponentIndices + Excludes, ComponentIndices + AddOrs);
    }

    inline std::span<ComponentTypeIndex const> GetAddOrs() const
    {
        return std::span(ComponentIndices + AddOrs, ComponentIndices + AddAnds);
    }

    inline std::span<ComponentTypeIndex const> GetAddAnds() const
    {
        return std::span(ComponentIndices + AddAnds, ComponentIndices + OneFrames);
    }

    inline std::span<ComponentTypeIndex const> GetOneFrames() const
    {
        return std::span(ComponentIndices + OneFrames, ComponentIndices + OptionalOneFrames);
    }

    inline std::span<ComponentTypeIndex const> GetOptionalOneFrames() const
    {
        return std::span(ComponentIndices + OptionalOneFrames, ComponentIndices + Optionals);
    }

    inline std::span<ComponentTypeIndex const> GetOptionals() const
    {
        return std::span(ComponentIndices + Optionals, ComponentIndices + Immediates);
    }

    inline std::span<ComponentTypeIndex const> GetImmediates() const
    {
        return std::span(ComponentIndices + Immediates, ComponentIndices + Writes);
    }

    inline std::span<ComponentTypeIndex const> GetWrites() const
    {
        return std::span(ComponentIndices + Writes, ComponentIndices + WriteEnd);
    }

    void* GetFirstMatchingComponent(std::size_t componentSize);
    void DebugPrint(QueryIndex index, EntitySystemHelpersBase& eh) const;
};


struct QueryRegistry : public ProtectedGameObject<QueryRegistry>
{
    Array<QueryDescription> Queries;
    Array<QueryIndex> PersistentQueries;
    Array<QueryIndex> AliveQueries;
    Array<QueryIndex> RemovedQueries;
    Array<QueryIndex> DeadQueries;
    Array<QueryIndex> DeadOneFrameQueries;
    Array<QueryIndex> FrameData;
};

struct ComponentRegistry : public ProtectedGameObject<ComponentRegistry>
{
    BitSet<> Bitmask;
    StaticArray<ComponentTypeEntry> Types;

    ComponentTypeEntry const* Get(ComponentTypeIndex index) const;
};

struct SystemTypeEntry : public ProtectedGameObject<SystemTypeEntry>
{
    using UpdateProcType = void (BaseSystem*, EntityWorld&, GameTime const&);

    BaseSystem* System;
    SystemTypeIndex SystemIndex0;
    SystemTypeIndex SystemIndex1;
    __int16 field_10;
    bool Activated;
    UpdateProcType* UpdateProc;
    void* SomeProc2;
    void* SomeProc3;
    void* ECBFlushJob;
    HashSet<SystemTypeIndex> DependencySystems;
    HashSet<SystemTypeIndex> DependentSystems;
    HashSet<uint32_t> HandleMappings2;
    HashSet<uint32_t> HandleMappings;
#if 0
    StringView Name;
    uint64_t field_108;
    QueryMask SomeBitfield;
    QueryMask SomeBitfield2;
#endif
};

struct SystemRegistry : public ProtectedGameObject<SystemRegistry>
{
    void* VMT;
    Array<SystemTypeEntry> Systems;
    uint32_t Unknown;
    uint32_t GrowSize;
};

struct SyncBuffers : public ProtectedGameObject<SyncBuffers>
{
    Array<HashMap<EntityHandle, BitSet<>>> ComponentPools;
    bool Dirty;
};

struct FieldTracker
{
    struct FieldChangeData
    {
        uint64_t Value; // Changes based on field type
        EntityHandle Entity;
        void* Component;
    };

    virtual ~FieldTracker();
#if defined(BG3SE_LINUX)
    // PORT (linux): the game's class is ecs::sync::_private::FieldTracker<ls::FieldMeta<...>> and
    // its three virtuals are Track / Dispatch(entity) / Dispatch(), not Add / FireEvents / Remove.
    // Read off the macOS oracle, which keeps the symbols, and confirmed against bin/bg3
    // :
    //   oracle _ZTVN3ecs4sync8_private12FieldTrackerI...m_IsVisualChangedEEEEEEE at 0x109312448 is
    //   { ~D1 0x10131e440, ~D0 0x10131e4f0, Track 0x10131e620, Dispatch(ID const&) 0x10131e860,
    //     Dispatch() 0x10131e9a0 }, i.e. +0x10 / +0x18 / +0x20 off the vptr.
    // The slot *positions* are the same as upstream's, so the names above line up one for one --
    // but the meanings do not: the per-frame flush that fires ChangeEvent is Dispatch() at +0x20
    // (upstream's "Remove"), not the +0x18 slot ("FireEvents"). +0x18 is a per-entity dispatch and
    // bin/bg3 only calls it from ComponentDeserializer<T>::DestroyComponent. Keeping upstream's
    // names meant a subscription was accepted and no handler ever ran.
    //   bin/bg3 0x36d3420 ComponentDeserializer<eoc::HealthComponent>::DeserializeDirty -- six
    //     `call [rax+0x10]`, one per replicated field (the oracle's 0x101d2f850 has the same six).
    //   bin/bg3 0x36d24d0 ...::DestroyComponent -- one `call [rax+0x18]`.
    //   bin/bg3 0x5dded00 ...::DispatchChanges -- one `call [rax+0x20]`.
    // Track takes the entity by *const reference* (the oracle's 0x10131e620 passes rsi straight to
    // ls::HashTable<ls::ID<...>>::Ensure<ls::ID<...> const&>), so upstream's by-value EntityHandle
    // would have been a pointer to the handle, not the handle.
    virtual void Track(EntityHandle const& entity, void* component) = 0;
    virtual void Dispatch(EntityHandle const& entity) = 0;
    virtual void Dispatch() = 0;
#else
    virtual void Add(EntityHandle entity, void* component) = 0;
    virtual void FireEvents() = 0;
    virtual void Remove(EntityHandle entity) = 0;
#endif

    Signal<FieldChangeData*> ChangeEvent;
    HashMap<EntityHandle, FieldChangeData> Changes;
};

#if defined(BG3SE_LINUX)
// PORT (linux): witnesses for the two data members, both from the oracle's Track/Dispatch() bodies
// for the m_IsVisualChanged tracker:
//   0x10131e9a0 Dispatch() loads the connection buffer from [this+0x10] and the connection count
//     from [this+0x1c], walking it with stride 0x48 -- that is Signal::Connections, so the Signal
//     (NextRegistrantId first) starts at +8.
//   0x10131e620 Track calls HashTable::Ensure on this+0x20 and grows a parallel value array at
//     [this+0x50] with stride 0x18 -- that is HashMap<EntityHandle, FieldChangeData>, 24-byte value.
static_assert(offsetof(FieldTracker, ChangeEvent) == 8);
static_assert(offsetof(FieldTracker, Changes) == 0x20);
static_assert(sizeof(FieldTracker::FieldChangeData) == 0x18);
#endif

struct ComponentDeserializer : public ProtectedGameObject<ComponentDeserializer>
{
    virtual ~ComponentDeserializer() = 0;
    // Other functions umapped

    SparseArray<FieldTracker*> FieldTrackers;
};


struct EntityReplicationPeer : public ProtectedGameObject<EntityReplicationPeer>
{
    ecs::EntityWorld* World;
    SyncBuffers** Buffers;
    net::GameClient* Client;
    HashMap<NetId, EntityHandle> NetIdToEntity;
    HashMap<EntityHandle, NetId> EntityToNetId;
    SparseArray<ComponentDeserializer*> Deserializers;
};


struct EntityHandleGenerator : public ProtectedGameObject<EntityHandleGenerator>
{
    struct alignas(64) ThreadState : public ProtectedGameObject<ThreadState>
    {
        struct Entry
        {
            uint32_t Index;
            uint32_t Salt;
        };

        PagedArray<Entry> Entries;
        uint32_t NextFreeSlotIndex;
        uint32_t LastFreeSlotIndex;
        uint32_t NumFreeSlots;

        Entry* Add();
    };

    std::array<ThreadState, 0x40> ThreadStates;

    EntityHandle Create();
    bool IsEntityAlive(EntityHandle entity) const;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the handle generator is the one mirror every EntityHandle validation walks
// (WasValidEntity in LuaObjectProxies.cpp), and it had no assert at all. Read off two
// disassemblies:
//   bin/bg3 0x25c3960  ecs::EntityRef::IsAlive(EntityHandle rdi, EntityWorld* rsi) --
//     [rsi+0x208] is the generator, `shl ecx, 6` on the 6-bit thread index is the stride,
//     `movzx ecx, byte [state+8]` is BitsPerPage, `cmp dword [state+0xc], edi` is size_,
//     `mov rax, [state]` is pages_, `[page + slot*8]` is the 8-byte Entry, and the final
//     `test rax, 0x3fffff00000000` is the 22-bit salt at bits 32..53.
//   oracle 0x1072b9a50  ecs::EntityHandleGenerator::Create() -- `movzx eax, word
//     [state+0xa]` shifted left by BitsPerPage is capacity(), so NumPages is at +0xa;
//     [state+0x10] is the free-list head and [state+0x18] the free count (compared with
//     bucket_size()/2, then decremented).
// Nothing here differs from upstream's Win32 layout; the value of the asserts is that the
// generator is now ruled out by construction when a handle fails to validate.
static_assert(sizeof(EntityHandleGenerator::ThreadState) == 0x40);
static_assert(sizeof(EntityHandleGenerator::ThreadState::Entry) == 8);
static_assert(offsetof(EntityHandleGenerator::ThreadState, NextFreeSlotIndex) == 0x10);
static_assert(offsetof(EntityHandleGenerator::ThreadState, NumFreeSlots) == 0x18);
// PagedArray<Entry> is the first member, so its own members are at the state's offsets:
static_assert(sizeof(PagedArray<EntityHandleGenerator::ThreadState::Entry>) == 0x10);
static_assert(offsetof(PageLayout, BitsPerPage) == 0);
static_assert(offsetof(PageLayout, NumPages) == 2);
// And the one that was actually wrong: ThreadStates has to start at the generator, because
// `shl ecx, 6` at bin/bg3 0x25c3981 is the whole of the game's indexing -- no base offset.
// It did not, until ProtectedGameObject<T> stopped sharing an empty base on Linux (see
// CoreLib/Base/BaseUtilities.h).
static_assert(offsetof(EntityHandleGenerator, ThreadStates) == 0);
static_assert(sizeof(EntityHandleGenerator) == 0x40 * 0x40);
#endif

struct EntityStorageComponentPage
{
    struct ComponentInfo
    {
        void* ComponentBuffer;
        std::atomic<uint64_t> ModifiedEntities;
    };

    std::array<ComponentInfo, 256> Components;
};

struct ComponentFrameStorageIndex
{
    static constexpr uint16_t InvalidIndex = 0xffffu;

    uint16_t PageIndex{ InvalidIndex };
    uint16_t EntryIndex{ InvalidIndex };

    inline operator bool() const
    {
        return PageIndex != InvalidIndex;
    }
};
        
struct EntityStorageData : public ProtectedGameObject<EntityStorageData>
{
    static constexpr std::size_t PageSize = 64;

    struct ComponentEntry
    {
        uint16_t Index;
        ComponentTypeIndex ComponentTypeId;
        bool QueryFlag1;
        bool QueryFlag3;
        void* DtorProc;
    };

    struct HandlePage
    {
        EntityHandle Pool[PageSize];
    };

    struct PageInfo
    {
        uint64_t CapacityMask;
        uint64_t UsedMask;
    };


    ComponentTypeMask ComponentsInClass;
    uint64_t GroupMask;
    uint16_t* ComponentSizes;
    ComponentEntry* ComponentDtors;
    uint16_t StorageIndex;
    uint16_t TotalSize;
    uint16_t ComponentIndexListSize;
    bool SomeQueryFlag;
    bool AggregateQueryFlag3;
    bool RegisteredForQueries;
    bool RegisteredForQueries2;
    Array<EntityStorageComponentPage*> Components;
    Array<HandlePage*> Handles;
    Array<PageInfo> PageInfos;
    int16_t field_148;
    int16_t SmallPageIndex;
    Array<uint16_t> PageToComponentPoolIndex;
    HashMap<ComponentTypeIndex, uint8_t> ComponentTypeToIndex;
    HashMap<EntityHandle, ComponentFrameStorageIndex> InstanceToPageMap;
    HashMap<uint64_t, uint16_t> AddedComponentFrameStorageIDs;
    HashMap<uint64_t, uint16_t> RemovedComponentFrameStorageIDs2;
    Array<Array<EntityHandle>> ComponentAddedEntityMap;
    Array<Array<EntityHandle>> ComponentRemovedEntityMap;
    HashMap<ComponentTypeIndex, HashMap<EntityHandle, void*>> OneFrameComponents;
    bool HasOneFrameComponents;
    __int64 field_2C8;
    StorageComponentTypeMask ModifiedComponents; // Index of components that were changed
    Array<QueryIndex> RegisteredQueries;
    Array<QueryIndex> AddComponentQueries;
    QueryMask AddComponentQueryMap;
    Array<QueryIndex> RemoveComponentQueries;
    QueryMask RemoveComponentQueryMap;

    void* GetComponent(EntityHandle entityHandle, ComponentTypeIndex type, std::size_t componentSize) const;
    void* GetOneFrameComponent(EntityHandle entityHandle, ComponentTypeIndex type) const;
    void* GetComponent(ComponentFrameStorageIndex const& entityPtr, ComponentTypeIndex type, std::size_t componentSize) const;
    void* GetComponent(ComponentFrameStorageIndex const& entityPtr, uint8_t componentSlot, std::size_t componentSize) const;
    bool MarkComponentAsChanged(EntityHandle entity, ComponentTypeIndex component);
    bool WasComponentChanged(EntityHandle entity, ComponentTypeIndex component) const;
    bool WasComponentChanged(ComponentFrameStorageIndex storageIndex, uint8_t componentSlot) const;

    inline bool HasComponent(ComponentTypeIndex type) const
    {
        return ComponentsInClass[(uint16_t)type];
    }
};


struct EntityStorageContainer : public ProtectedGameObject<EntityStorageContainer>
{
    struct TypeSalt
    {
        int32_t Salt;
        uint16_t StorageIndex;
    };

    struct ThreadSalts : public ProtectedGameObject<ThreadSalts>
    {
        std::array<PagedArray<TypeSalt>, 0x40> Buckets;
        uint32_t Size;
    };

    Array<EntityStorageData*> Storages;
    HashMap<uint64_t, uint16_t> TypeHashToEntityTypeIndex;
    ThreadSalts Salts;
    HashMap<uint64_t, uint64_t> field_458;
    BitSet<> UsedFrameDataStorages;
    ComponentRegistry* ComponentRegistry;
    QueryRegistry* Queries;

    std::optional<uint16_t> GetEntityStorageIndex(EntityHandle entityHandle) const;
    bool IsEntityStorageDirty(uint16_t storageIndex) const;
    EntityStorageData* GetEntityStorage(uint16_t storageIndex) const;
    EntityStorageData* GetEntityStorage(EntityHandle entityHandle) const;
};

struct ComponentCallbacks;

struct ComponentOps : public ProtectedGameObject<ComponentOps>
{
    virtual ~ComponentOps() = 0;
    virtual void FireConstructCallbacks(void*, void*) = 0;
    virtual void FireDestructCallbacks(void*, void*) = 0;
    virtual void DefaultConstructComponents() = 0;
    virtual void AddImmediateDefaultComponent(uint64_t entity, int retryCount) = 0;

    ComponentRegistry* Registry;
    ComponentCallbacks* Callbacks;
    EntityStorageContainer* Storage;
    EntityWorld* World;
    ComponentTypeIndex TypeId;
};

struct ComponentPool : public ProtectedGameObject<ComponentPool>
{
    Array<void*> Pages;
    void* GrowProc;
    uint64_t field_18;
    FrameAllocator* Allocator;
    uint32_t NextFreeIndex;
    uint16_t ComponentSize;
    ComponentTypeIndex ComponentTypeId;
    void* DtorProc;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the game's EntityRef argument is {Handle, World} in two registers, not a pointer
// to one. Read off the component-callback invocations in bin/bg3 0x3e0d460 (both loops, construct
// at 0x3e0d5a7 and destroy at 0x3e0d636): rdi the functor storage, rsi the handle, rdx the world
// ([cache + 0x250]), rcx the component. MSVC passes the 16-byte struct by hidden pointer, hence
// upstream's EntityRef*; our EntityRef cannot stand in by value either, because TypedHandle's
// user-provided copy constructor makes Itanium pass it by hidden pointer too.
using ComponentSignal = Signal<uint64_t, EntityWorld*, void*>;
#else
using ComponentSignal = Signal<EntityRef*, void*>;
#endif

struct ComponentCallbacks : public ProtectedGameObject<ComponentCallbacks>
{
    void* VMT;
    ComponentSignal OnConstruct;
    ComponentSignal OnDestroy;
};

struct ComponentCallbackRegistry : public ProtectedGameObject<ComponentCallbackRegistry>
{
    Array<ComponentCallbacks*> Callbacks;

    ComponentCallbacks* Get(ComponentTypeIndex index);
};

struct ECBEntityComponentChange
{
    // -1 = removed
    ComponentFrameStorageIndex Index;
    uint16_t field_4{ 0 };
    ComponentTypeIndex ComponentTypeId;
};

struct ECBEntityChangeSet
{
    inline ECBEntityChangeSet(FrameAllocator* allocator)
        : Store(4, ECBFrameAllocator(allocator))
    {}

    PagedArray<ECBEntityComponentChange, ECBFrameAllocator> Store;
    uint64_t AddGroupMask{ 0 };
    uint64_t RemoveGroupMask{ 0 };
    EntityChangeFlags Flags{ 0 };
    int16_t field_2A{ -1 };
};

struct ComponentFrameStorage
{
    inline ComponentFrameStorage(FrameAllocator* allocator)
        : Pages(3, allocator)
    {}

    PagedArray<void*, ECBFrameAllocator> Pages;
    uint32_t NumComponents{ 0 };
    uint16_t ComponentSizeInBytes{ 0 };
    ComponentTypeIndex ComponentTypeId{ 0 };
    void* DestructorProc{ nullptr };

    void* Allocate(ComponentFrameStorageIndex& index);
    uint32_t Capacity() const;

    inline void* GetComponent(ComponentFrameStorageIndex const& index) const
    {
        se_assert(index.PageIndex < Pages.size());
        auto page = Pages[index.PageIndex];
        return static_cast<uint8_t*>(page) + (index.EntryIndex * ComponentSizeInBytes);
    }

private:
    void Grow();

    uint32_t ComponentSizeBucket() const;
    uint16_t ComponentsOnPage(uint32_t page) const;
};

struct ImmediateWorldCache : public ProtectedGameObject<ImmediateWorldCache>
{
    struct ComponentChange
    {
        void* Ptr{ nullptr };
        ComponentFrameStorageIndex StorageIndex;
    };

    struct ComponentChanges
    {
        inline ComponentChanges(FrameAllocator* allocator)
            : Components(5, allocator),
            FrameStorage(allocator)
        {}

        PagedHashMap<EntityHandle, ComponentChange, ECBFrameAllocator> Components;
        ComponentFrameStorage FrameStorage;
        void* field_70{ nullptr };
        void* field_78{ nullptr };
    };

    struct Changes
    {
        ComponentTypeMask AvailableComponentTypes;
        ComponentChanges* ComponentsByType;
        uint64_t Unknown;

        void* GetChange(EntityHandle entityHandle, ComponentTypeIndex type) const;
        ComponentChanges* GetOrAddComponentChanges(ComponentTypeEntry const* type, FrameAllocator* allocator);
    };

    Changes WriteChanges;
    Changes ReadChanges;
    ComponentCallbackRegistry* Callbacks;
    FrameAllocator* Allocator;
    EntityWorld* EntityWorld;
    EntityHandleGenerator* HandleGenerator;
    __int64 field_158;

    ComponentChanges* GetOrAddComponentChanges(ComponentTypeIndex type);
    bool RemoveComponent(EntityHandle entity, ComponentTypeIndex type);
    bool PrepareAddComponent(EntityHandle entity, ComponentTypeIndex type, void*& component);
    void FinalizeAddComponent(EntityHandle entity, ComponentTypeIndex type, void* component);
};

struct ECBData : public ProtectedGameObject<ECBData>
{
    PagedHashMap<EntityHandle, ECBEntityChangeSet, ECBFrameAllocator> EntityChanges;
    DoubleIndexedPagedArray<ComponentTypeIndex, ComponentFrameStorage, ECBFrameAllocator> ComponentPools;
    bool field_A0;

    ECBEntityChangeSet const* GetEntityChange(EntityHandle const& entity) const;
    ECBEntityChangeSet* GetOrAddEntityChange(EntityHandle const& entity);
};

struct EntityCommandBuffer : public ProtectedGameObject<EntityCommandBuffer>
{
public:
    EntityHandleGenerator* HandleGenerator;
    FrameAllocator* Allocator;
    ECBData Data;
    uint32_t ThreadId;

    EntityHandle CreateEntity();
    EntityHandle CreateEntityImmediate();
    bool DestroyEntity(EntityHandle entity);
    void* GetComponentChange(ComponentTypeIndex type, ComponentFrameStorageIndex const& index) const;
    void* GetComponentChange(EntityHandle entity, ComponentTypeIndex type) const;
    ComponentFrameStorage* GetStorage(ComponentTypeIndex type, uint16_t componentSize, void* dtor);
    void* CreateComponentRaw(EntityHandle entity, ComponentTypeIndex type, uint16_t componentSize, ComponentFrameStorageIndex& index, void* dtor);
    void RemoveComponent(EntityHandle entity, ComponentTypeIndex type, uint16_t componentSize, void* dtor);
};

struct GroupAllocator : public ProtectedGameObject<GroupAllocator>
{
    CriticalSection CS;
#if 0
    uint64_t PendingAcquire;
    uint64_t PendingRelease;
#endif
};

struct EntityWorldSettings
{
    int JobPriority;
    int UpdateJobPriority;
#if 0
    // FIXME - check if correct?
    STDString Path;
    __int64 field_20;
    uint8_t field_28;
    uint8_t field_30;
#endif
};

struct ComponentOpsRegistry
{
    Array<ComponentOps*> Ops;

    ComponentOps* Get(ComponentTypeIndex id) const;
};

struct EntityStorageChangeBatch
{
    PagedHashMap<EntityHandle, ECBEntityChangeSet, ECBFrameAllocator> EntityChanges;
    EntityStorageContainer* Storage;
    FrameAllocator* FrameAllocator;
    SparseArray<uint16_t> OneFrameComponentTypes;
    __int64 field_78;
    ComponentTypeMask QueryFlag0Mask;
    ComponentTypeMask QueryFlag1Mask;
    BitSet<> field_280;
};


struct ECBExecutor
{
    EntityHandleGenerator* HandleGenerator;
    EntityStorageContainer* Storage;
    ComponentRegistry* ComponentRegistry;
    ComponentOpsRegistry* ComponentOps;
    ComponentTypeMask RemovedComponentsMask;
    ComponentTypeMask AddedComponentsMask;
    OneFrameComponentTypeMask AddedOneFrameComponentsMask;
    Array<ComponentPool> ComponentPools;
    PagedHashMap<EntityHandle, ECBEntityChangeSet, ECBFrameAllocator> EntityChanges;
    __int64 field_2B8;
    EntityStorageChangeBatch ChangeBatch;
    FrameAllocator* FrameAllocator;
    void* ScratchMemory;
};


struct EntityWorld : public ProtectedGameObject<EntityWorld>
{
    SyncBuffers* Replication;
    ComponentRegistry ComponentRegistry_;
    SystemRegistry Systems;
    Array<SystemTypeEntry*> ActiveSystems;
    Array<void*> ECBFlushJobs;
    void* SystemDependencyExecutor;
    MPMCQueueBounded<void*> DependencyExecutorQueue;
    GameTime Time;
    void* ECSUpdateBatch;
    void* field_160x;
    int field_160;
    Array<EntityCommandBuffer> CommandBuffers;
    ComponentCallbackRegistry ComponentCallbacks;
    bool RegisterPhaseEnded;
    bool Active;
    bool NeedsOptimize;
    bool PerformingECSUpdate;
    QueryRegistry Queries;
    EntityHandleGenerator* HandleGenerator;
    EntityStorageContainer* Storage;
    FrameAllocator ComponentData;
    Array<void*> SystemDependencyExecutors;
    ComponentOpsRegistry ComponentOps;
    ScratchString field_2E0;
    ECBExecutor* Executor;
    GroupAllocator GroupAllocator;
    ImmediateWorldCache* Cache;
    EntityWorldSettings Settings;
#if 0
    uint32_t DebugUpdateFrame;
    void* LegacyWorldViewChecker;
    CRITICAL_SECTION CS2;
#endif

    void* GetRawComponent(EntityHandle entityHandle, ComponentTypeIndex type, std::size_t componentSize);
    void* GetAndDereferenceRawComponent(EntityHandle entityHandle, ComponentTypeIndex type, std::size_t componentSize);
    void* GetCommittedComponent(EntityHandle entityHandle, ComponentTypeIndex type, std::size_t componentSize);
    void* GetImmediateComponent(EntityHandle entityHandle, ComponentTypeIndex type);
    void* GetECBComponent(EntityHandle entityHandle, ComponentTypeIndex type);
    bool MarkComponentAsChanged(EntityHandle entity, ComponentTypeIndex component);
    bool WasComponentChanged(EntityHandle entity, ComponentTypeIndex component);

    EntityStorageData* GetEntityStorage(EntityHandle entityHandle) const;
    bool IsValid(EntityHandle entityHandle) const;
    EntityCommandBuffer* Deferred();
};

#if defined(BG3SE_LINUX)
// PORT (linux): ecs::EntityWorld's layout in the Linux build, measured against
// bin/bg3 c8da62b1... and against both live worlds (server EoCServer+0x288, client
// EoCClient+0x1A0) read out of the process. Every row below is read two ways -- a census of the
// offsets the game itself reads off the world pointer (both globals,
// 2633 + 1960 reads) or a hand-read function, and a value in the running game -- and the
// five cross-object identities that ValidateEntityWorld() (EntitySystem.cpp) re-checks at
// bind time hold in both worlds.
//
// Only Cache differs from upstream, by 8, because GroupAllocator's lock is 8 wider here
// (see CRITICAL_SECTION in ext/linux/shim/windows.h); everything else is upstream's.
static_assert(offsetof(EntityWorld, ComponentRegistry_) == 0x008);   // Storage->ComponentRegistry points here
static_assert(offsetof(EntityWorld, Systems) == 0x028);              // VMT; Systems.Systems at +0x30
static_assert(offsetof(EntityWorld, ActiveSystems) == 0x048);
static_assert(offsetof(EntityWorld, Time) == 0x140);
static_assert(offsetof(EntityWorld, CommandBuffers) == 0x170);
static_assert(offsetof(EntityWorld, ComponentCallbacks) == 0x180);   // Cache->Callbacks points here
static_assert(offsetof(EntityWorld, RegisterPhaseEnded) == 0x190);
static_assert(offsetof(EntityWorld, PerformingECSUpdate) == 0x193);  // the game compares this byte
static_assert(offsetof(EntityWorld, Queries) == 0x198);              // Storage->Queries points here
static_assert(offsetof(EntityWorld, HandleGenerator) == 0x208);
static_assert(offsetof(EntityWorld, Storage) == 0x210);
static_assert(offsetof(EntityWorld, ComponentData) == 0x240);        // Cache->Allocator points here
static_assert(offsetof(EntityWorld, ComponentOps) == 0x2D0);
static_assert(offsetof(EntityWorld, Executor) == 0x300);
static_assert(offsetof(EntityWorld, GroupAllocator) == 0x308);
static_assert(offsetof(EntityWorld, Cache) == 0x348);                // Cache->EntityWorld points back
static_assert(offsetof(EntityWorld, Settings) == 0x350);
// Strides the game uses on the arrays those fields reach, each read off the disassembly and
// confirmed on the live world:
//   SystemTypeEntry   0xf8  `imul rcx, rcx, 0xf8` at bin/bg3 0x2270eb9, indexing [world+0x30]
//   ComponentTypeEntry 0x40  the 1854 populated entries of Types are exactly the 1854 set
//                            bits of Bitmask, at that stride
//   QueryDescription  0x58   the only stride at which 600/600 entries have a readable
//                            ComponentIndices pointer and plausible per-kind counts
static_assert(sizeof(SystemTypeEntry) == 0xf8);
static_assert(sizeof(ComponentTypeEntry) == 0x40);
static_assert(sizeof(QueryDescription) == 0x58);
static_assert(sizeof(ComponentRegistry) == 0x20);
static_assert(sizeof(QueryRegistry) == 0x70);
static_assert(sizeof(ImmediateWorldCache) == 0x268);
static_assert(sizeof(EntityWorld) == 0x380);    // align 64 swallows the 8 Cache moved by

// The five back-pointers are the strongest witnesses in this file: the game's own statements
// about where those members are, holding in both live worlds, and
// exactly what ValidateEntityWorld() re-checks before the world is handed out. Pin both ends
// of each identity -- the runtime check can only report a failure, this one refuses to build.
static_assert(offsetof(EntityStorageContainer, ComponentRegistry) == 0x4A8);
static_assert(offsetof(EntityStorageContainer, Queries) == 0x4B0);
static_assert(offsetof(ImmediateWorldCache, Callbacks) == 0x240);
static_assert(offsetof(ImmediateWorldCache, Allocator) == 0x248);
static_assert(offsetof(ImmediateWorldCache, EntityWorld) == 0x250);

// The BitSet's word order, at the two places the game was seen reading the +12 slot as its
// inline-vs-heap test (`cmp dword [obj+0x14], 0x41` at bin/bg3 0x26aee1a on the registry's
// Bitmask; the same shape at [storage+0x4a4] in 0x3591e30). Derived through the containing
// offsets, so either half moving breaks it.
static_assert(offsetof(EntityWorld, ComponentRegistry_) + offsetof(ComponentRegistry, Bitmask)
    + offsetof(BitSet<>, Capacity) == 0x14, "the registry Bitmask's Capacity is the [world+0x14] slot");
static_assert(offsetof(EntityStorageContainer, UsedFrameDataStorages) + offsetof(BitSet<>, Capacity) == 0x4A4,
    "UsedFrameDataStorages' Capacity is the [storage+0x4a4] slot");

// ComponentData's own tail. The lock ahead of FrameBuffers is the hazard that moved Cache, so
// size the assert from it: on the live world the {ptr, cap 8, size 1} pair reads at
// world+0x240+0x40 = +0x280 (upstream's +0x278, with the 40-byte lock).
static_assert(offsetof(FrameAllocator, PageSize) == 0x08);          // 0x100000 live
static_assert(offsetof(FrameAllocator, FrameBuffers) == 0x10 + sizeof(CRITICAL_SECTION));
static_assert(offsetof(EntityWorld, ComponentData) + offsetof(FrameAllocator, FrameBuffers) == 0x280,
    "ComponentData::FrameBuffers is at world+0x280 on Linux");
#endif

struct PeerReplicationData
{
    PeerId PeerID;
    HashSet<EntityHandle> ReplicatedToPeer;
    HashSet<EntityHandle> RequestedForReplication;
    HashSet<EntityHandle> RequestedToStopReplication;
    HashMap<EntityHandle, BitSet<>> Components;
    HashMap<EntityHandle, BitSet<>> Components2;
};

struct NetIdGenerator
{
    PagedArray<NetId> AssignedNetIds;
    NetId NextFreeNetId;
    NetId LastFreeNetId;
    int FreeSlots;
};

struct EntitiesDirtyFieldsBuffer
{
    Array<HashMap<EntityHandle, BitSet<>>> field_0;
    bool field_10;
};

struct EntitiesDirtyCollectionIndicesBuffer
{
    BitSet<> field_0;
    Array<void*> field_10;
};

struct EntityReplicationAuthority : public ProtectedGameObject<EntityReplicationAuthority>
{
    net::Host* Host;
    EntityWorld* World;
    EntitiesDirtyFieldsBuffer DirtyFields;
    EntitiesDirtyCollectionIndicesBuffer DirtyCollectionIndices;
    NetIdGenerator NetIdGenerator;
    SparseArray<void*> field_70_IComponentSerializer;
    HashMap<EntityHandle, NetId> EntityToNetId;
    HashMap<NetId, EntityHandle> NetIdToEntity;
    HashMap<EntityHandle, BitSet<>> field_110;
    HashSet<EntityHandle> ReplicateEntities;
    HashSet<EntityHandle> StopReplicateEntities;
    SparseArray<PeerReplicationData> ReplicationData;
    Array<PeerId> Peers;
    Array<PeerId> ConnectedPeers;
    Array<void*> field_1F0_pIComponentSubscription;
    HashMap<PeerId, void*> field_200_PeerID_pECSReplicationMessage;
};

struct WorldView
{
    EntityWorld* World;
};

END_NS()
