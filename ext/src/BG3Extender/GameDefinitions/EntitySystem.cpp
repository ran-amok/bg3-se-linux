#include "stdafx.h"

#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/Symbols.h>
#include <GameDefinitions/EntitySystem.h>
#include <GameDefinitions/GuidResources.h>
#include <GameDefinitions/Components/All.h>
#include <Extender/ScriptExtender.h>

#undef DEBUG_INDEX_MAPPINGS

#if defined(DEBUG_INDEX_MAPPINGS)
#define DEBUG_IDX(x) std::cout << x << std::endl
#else
#define DEBUG_IDX(x)
#endif

BEGIN_NS(ecs)

const unsigned ComponentSizeBuckets[65] = {
    0, 
    0, 1, 2, 2, 3, 3, 3, 3,
    4, 4, 4, 4, 5, 5, 5, 5,
    6, 6, 6, 6, 7, 7, 7, 7,
    8, 8, 8, 8, 9, 9, 9, 9,
    10, 10, 10, 10, 10, 10, 10, 10,
    11, 11, 11, 11, 11, 11, 11, 11,
    12, 12, 12, 12, 12, 12, 12, 12,
    13, 13, 13, 13, 13, 13, 13, 13
};

const unsigned FrameAllocatorComponentsPerPage[14][9] = {
    { 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40 },
    { 0x20, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40 },
    { 0x10, 0x20, 0x30, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40 },
    { 0x08, 0x10, 0x18, 0x28, 0x40, 0x40, 0x40, 0x40, 0x40 },
    { 0x05, 0x0A, 0x10, 0x1A, 0x2A, 0x40, 0x40, 0x40, 0x40 },
    { 0x04, 0x08, 0x0C, 0x14, 0x20, 0x34, 0x40, 0x40, 0x40 },
    { 0x03, 0x06, 0x09, 0x10, 0x19, 0x29, 0x40, 0x40, 0x40 },
    { 0x02, 0x05, 0x08, 0x0D, 0x15, 0x22, 0x38, 0x40, 0x40 },
    { 0x02, 0x04, 0x06, 0x0B, 0x12, 0x1D, 0x30, 0x40, 0x40 },
    { 0x02, 0x04, 0x06, 0x0A, 0x10, 0x1A, 0x2A, 0x40, 0x40 },
    { 0x01, 0x03, 0x04, 0x08, 0x0C, 0x14, 0x21, 0x36, 0x40 },
    { 0x01, 0x02, 0x04, 0x06, 0x0A, 0x11, 0x1C, 0x2D, 0x40 },
    { 0x01, 0x02, 0x03, 0x05, 0x09, 0x0E, 0x18, 0x26, 0x40 },
    { 0x01, 0x02, 0x03, 0x05, 0x08, 0x0D, 0x15, 0x22, 0x40 }
};

const unsigned FrameAllocatorComponentsAggregate[14][9] = {
    { 0x40, 0x80, 0xC0, 0x100, 0x140, 0x180, 0x1C0, 0x200, 0x240 },
    { 0x20, 0x60, 0xA0, 0xE0,  0x120, 0x160, 0x1A0, 0x1E0, 0x220 },
    { 0x10, 0x30, 0x60, 0xA0,  0xE0,  0x120, 0x160, 0x1A0, 0x1E0 },
    { 0x08, 0x18, 0x30, 0x58,  0x98,  0xD8,  0x118, 0x158, 0x198 },
    { 0x05, 0x0F, 0x1F, 0x39,  0x63,  0xA3,  0xE3,  0x123, 0x163 },
    { 0x04, 0x0C, 0x18, 0x2C,  0x4C,  0x80,  0xC0,  0x100, 0x140 },
    { 0x03, 0x09, 0x12, 0x22,  0x3B,  0x64,  0xA4,  0xE4,  0x124 },
    { 0x02, 0x07, 0x0F, 0x1C,  0x31,  0x53,  0x8B,  0xCB,  0x10B },
    { 0x02, 0x06, 0x0C, 0x17,  0x29,  0x46,  0x76,  0xB6,  0xF6 },
    { 0x02, 0x06, 0x0C, 0x16,  0x26,  0x40,  0x6A,  0xAA,  0xEA },
    { 0x01, 0x04, 0x08, 0x10,  0x1C,  0x30,  0x51,  0x87,  0xC7 },
    { 0x01, 0x03, 0x07, 0x0D,  0x17,  0x28,  0x44,  0x71,  0xB1 },
    { 0x01, 0x03, 0x06, 0x0B,  0x14,  0x22,  0x3A,  0x60,  0xA0 },
    { 0x01, 0x03, 0x06, 0x0B,  0x13,  0x20,  0x37,  0x57,  0x98 }
};

void* QueryDescription::GetFirstMatchingComponent(std::size_t componentSize)
{
    for (auto const& cls : EntityStorages.values()) {
        if (cls.Storage->InstanceToPageMap.size() > 0) {
            auto const& instPage = cls.Storage->InstanceToPageMap.values()[0];
            auto componentIdx = cls.GetComponentIndex(0);
            se_assert(cls.Storage->Components.size() >= 1);
            return cls.Storage->GetComponent(instPage, componentIdx, componentSize);
        }
    }

    return {};
}

void PrintRange(EntitySystemHelpersBase& eh, char const* name, std::span<ComponentTypeIndex const> inds)
{
    if (!inds.empty()) std::cout << "\t" << name << ": ";
    for (auto i : inds) {
        std::cout << " " << *eh.GetComponentName(i);
    }
    if (!inds.empty()) std::cout << std::endl;
}

void QueryDescription::DebugPrint(QueryIndex index, EntitySystemHelpersBase& eh) const
{
    auto const& queries = eh.GetEntityWorld()->Queries;

    std::cout << "Query ";
    if ((Flags & QueryFlags::Modified) == QueryFlags::Modified) std::cout << " Modified";
    if ((Flags & QueryFlags::Added) == QueryFlags::Added) std::cout << " Added";
    if ((Flags & QueryFlags::Removed) == QueryFlags::Removed) std::cout << " Removed";
    if (queries.PersistentQueries.find(index) != queries.PersistentQueries.end()) std::cout << " Persistent";
    if (queries.AliveQueries.find(index) != queries.AliveQueries.end()) std::cout << " Alive";
    if (queries.RemovedQueries.find(index) != queries.RemovedQueries.end()) std::cout << " Removed2";
    if (queries.DeadQueries.find(index) != queries.DeadQueries.end()) std::cout << " Dead";
    if (queries.DeadOneFrameQueries.find(index) != queries.DeadOneFrameQueries.end()) std::cout << " DeadOneFrame";
    std::cout << std::endl;

    PrintRange(eh, "Includes", GetIncludes());
    PrintRange(eh, "IncludeAny", GetIncludeAny());
    PrintRange(eh, "Excludes", GetExcludes());
    PrintRange(eh, "AddOrs", GetAddOrs());
    PrintRange(eh, "AddAnds", GetAddAnds());
    PrintRange(eh, "OneFrames", GetOneFrames());
    PrintRange(eh, "OptionalOneFrames", GetOptionalOneFrames());
    PrintRange(eh, "Optionals", GetOptionals());
    PrintRange(eh, "Immediates", GetImmediates());
    PrintRange(eh, "Writes", GetWrites());

    std::cout << std::endl;
}

ComponentTypeEntry const* ComponentRegistry::Get(ComponentTypeIndex index) const
{
    auto idx = (uint32_t)SparseHashMapHash(index);
    if (Bitmask[idx]) {
        return &Types[idx];
    } else {
        return nullptr;
    }
}

ComponentCallbacks* ComponentCallbackRegistry::Get(ComponentTypeIndex index)
{
    auto idx = (uint32_t)SparseHashMapHash(index);
    if (idx < Callbacks.size()) {
        return Callbacks[idx];
    } else {
        return nullptr;
    }
}


FieldTracker::~FieldTracker() {}


void* FrameAllocator::Allocate(uint16_t size)
{
    auto realSize = (uint16_t)(size + 63) & 0xFFC0u;

    auto curPage = CurrentPage;
    auto offset = (uint64_t)InterlockedExchangeAdd64(&curPage->Offset, realSize);
    while (offset + realSize > PageSize) {

        EnterCriticalSection(&CS);
        if (curPage == CurrentPage) {
            auto page = AllocPage();
            page->Offset = 0x40;
            CurrentPage = page;
        }
        LeaveCriticalSection(&CS);

        curPage = CurrentPage;
        offset = (uint64_t)InterlockedExchangeAdd64(&curPage->Offset, realSize);
    }

    return (uint8_t*)curPage + offset;
}

FrameAllocator::FrameBuffer* FrameAllocator::AllocPage()
{
    auto page = GameAllocRaw(PageSize);
    FrameBuffers[0].push_back(page);
    return static_cast<FrameBuffer*>(page);
}

void* ComponentFrameStorage::Allocate(ComponentFrameStorageIndex& index)
{
    auto capacity = Capacity();
    if (Capacity() <= NumComponents) {
        Grow();
        capacity = Capacity();
    }

    auto freeComponents = capacity - NumComponents;
    index.PageIndex = (uint16_t)(Pages.size() - 1);
    index.EntryIndex = ComponentsOnPage(Pages.size() - 1) - (uint16_t)freeComponents;
    NumComponents++;
    return (uint8_t*)Pages[index.PageIndex] + index.EntryIndex * ComponentSizeInBytes;
}

void ComponentFrameStorage::Grow()
{
    auto allocSize = ComponentsOnPage(Pages.size());
    auto page = Pages.allocator().alloc(ComponentSizeInBytes * allocSize);
    *Pages.add() = page;
}

uint32_t ComponentFrameStorage::ComponentSizeBucket() const
{
    auto cappedSize = std::min<uint16_t>(ComponentSizeInBytes, 0x40);
    return ComponentSizeBuckets[cappedSize];
}

uint16_t ComponentFrameStorage::ComponentsOnPage(uint32_t page) const
{
    auto sizeBucket = ComponentSizeBucket();
    auto cappedPages = std::min<uint32_t>(page, 8);
    return FrameAllocatorComponentsPerPage[sizeBucket][cappedPages];
}

uint32_t ComponentFrameStorage::Capacity() const
{
    if (Pages.size() > 0) {
        auto sizeBucket = ComponentSizeBucket();
        auto cappedPages = std::min<uint32_t>(Pages.size() - 1, 8);
        return FrameAllocatorComponentsAggregate[sizeBucket][cappedPages]
            + FrameAllocatorComponentsPerPage[sizeBucket][8] * (Pages.size() - cappedPages - 1);
    } else {
        return 0;
    }
}

void FrameAllocator::Free(void* ptr)
{
    // Per-frame data is batch released at the end of the frame
}

bool EntityHandleGenerator::IsEntityAlive(EntityHandle entity) const
{
    if (entity.GetThreadIndex() < ThreadStates.size()) {
        auto const& state = ThreadStates[entity.GetThreadIndex()];
        if (entity.GetIndex() < state.Entries.size()) {
            auto const& entry = state.Entries[entity.GetIndex()];
            return entry.Index == entity.GetIndex()
                && entry.Salt == entity.GetSalt();
        }
    }

    return false;
}

EntityHandle EntityHandleGenerator::Create()
{
    auto tid = ThreadRegistry::RequestThreadIndex();
    auto entry = ThreadStates[tid].Add();
    return EntityHandle(tid, entry->Index, entry->Salt);
}

EntityHandleGenerator::ThreadState::Entry* EntityHandleGenerator::ThreadState::Add()
{
    if (NumFreeSlots < Entries.bucket_size() / 2) {
        auto oldCapacity = Entries.capacity();
        if (Entries.capacity() < Entries.size() + Entries.bucket_size()) {
            Entries.resize(Entries.capacity() + Entries.bucket_size());
        }

        auto growSize = Entries.capacity() - oldCapacity;
        for (uint32_t i = 0; i < growSize; i++) {
            auto index = Entries.size();
            auto entry = Entries.add();
            *entry = Entry{
                .Index = index,
                .Salt = 1
            };
            if (NumFreeSlots > 0) {
                Entries[LastFreeSlotIndex].Index = index;
            } else {
                NextFreeSlotIndex = index;
            }

            LastFreeSlotIndex = index;
            NumFreeSlots++;
        }
    }

    auto index = NextFreeSlotIndex;
    NumFreeSlots--;
    auto entry = &Entries[index];
    NextFreeSlotIndex = entry->Index;
    entry->Index = index;
    return entry;
}

void* EntityStorageData::GetComponent(EntityHandle entityHandle, ComponentTypeIndex type, std::size_t componentSize) const
{
    auto ref = InstanceToPageMap.try_get(entityHandle);
    if (ref) {
        return GetComponent(*ref, type, componentSize);
    } else {
        return nullptr;
    }
}

void* EntityStorageData::GetOneFrameComponent(EntityHandle entityHandle, ComponentTypeIndex type) const
{
    auto pool = OneFrameComponents.try_get(type);
    if (pool) {
        return pool->get_or_default(entityHandle);
    }

    return nullptr;
}

void* EntityStorageData::GetComponent(ComponentFrameStorageIndex const& entityPtr, ComponentTypeIndex type, std::size_t componentSize) const
{
    auto compIndex = ComponentTypeToIndex.try_get(type);
    if (compIndex) {
        return GetComponent(entityPtr, *compIndex, componentSize);
    } else {
        return nullptr;
    }
}

void* EntityStorageData::GetComponent(ComponentFrameStorageIndex const& entityPtr, uint8_t componentSlot, std::size_t componentSize) const
{
    auto& page = Components[entityPtr.PageIndex]->Components[componentSlot];
    auto buf = (uint8_t*)page.ComponentBuffer;
    se_assert(buf != nullptr);
    return buf + componentSize * entityPtr.EntryIndex;
}

bool EntityStorageData::MarkComponentAsChanged(EntityHandle entity, ComponentTypeIndex component)
{
    auto componentSlot = ComponentTypeToIndex.try_get(component);
    if (componentSlot) {
        auto storageIndex = InstanceToPageMap.try_get(entity);
        if (storageIndex) {
            auto& page = Components[storageIndex->PageIndex]->Components[*componentSlot];
            uint64_t entryMask = 1ull << (storageIndex->EntryIndex & 0x3f);
            if (!(page.ModifiedEntities & entryMask)) {
                page.ModifiedEntities |= entryMask;

                if (!ModifiedComponents[*componentSlot]) {
                    ModifiedComponents.AtomicSet(*componentSlot);
                }
            }
            return true;
        }
    }

    return false;
}

bool EntityStorageData::WasComponentChanged(EntityHandle entity, ComponentTypeIndex component) const
{
    auto componentSlot = ComponentTypeToIndex.try_get(component);
    if (componentSlot && ModifiedComponents[*componentSlot]) {
        auto storageIndex = InstanceToPageMap.try_get(entity);
        if (storageIndex) {
            return WasComponentChanged(*storageIndex, *componentSlot);
        }
    }

    return false;
}

bool EntityStorageData::WasComponentChanged(ComponentFrameStorageIndex storageIndex, uint8_t componentSlot) const
{
    uint64_t entryMask = 1ull << (storageIndex.EntryIndex & 0x3f);
    auto const& changeMask = Components[storageIndex.PageIndex]->Components[componentSlot].ModifiedEntities;
    return (changeMask & entryMask) != 0;
}

void* ImmediateWorldCache::Changes::GetChange(EntityHandle entityHandle, ComponentTypeIndex type) const
{
    auto typeIdx = (uint16_t)type;
    if (AvailableComponentTypes[typeIdx]) {
        auto change = ComponentsByType[typeIdx].Components.find(entityHandle);
        if (change) {
            return change->Ptr;
        }
    }

    return nullptr;
}

ImmediateWorldCache::ComponentChanges* ImmediateWorldCache::Changes::GetOrAddComponentChanges(ComponentTypeEntry const* type, FrameAllocator* allocator)
{
    auto typeIdx = (uint16_t)type->TypeId;
    auto components = ComponentsByType + typeIdx;
    if (!AvailableComponentTypes[typeIdx]) {
        AvailableComponentTypes.AtomicSet(typeIdx);
        new (components) ComponentChanges(allocator);
        components->FrameStorage.ComponentTypeId = type->TypeId;
        components->FrameStorage.ComponentSizeInBytes = type->InlineSize;
        components->FrameStorage.DestructorProc = type->DtorProc;
    }

    return components;
}

ImmediateWorldCache::ComponentChanges* ImmediateWorldCache::GetOrAddComponentChanges(ComponentTypeIndex type)
{
    auto typeIdx = (uint16_t)type;
    auto typeInfo = EntityWorld->ComponentRegistry_.Get(type);
    return WriteChanges.GetOrAddComponentChanges(typeInfo, Allocator);
}

bool ImmediateWorldCache::RemoveComponent(EntityHandle entity, ComponentTypeIndex type)
{
    auto typeInfo = EntityWorld->ComponentRegistry_.Get(type);
    auto component = EntityWorld->GetCommittedComponent(entity, type, typeInfo->InlineSize);
    if (!component) {
        return false;
    }

    auto changes = GetOrAddComponentChanges(type);
    auto change = changes->Components.find(entity);

    if (!change) {
        // TODO - different behavior for proxy objects?
        auto& onDestroy = Callbacks->Get(type)->OnDestroy;
        #if defined(BG3SE_LINUX)
        // PORT (linux): the game's shape of the signal, split EntityRef (EntitySystem.h ComponentSignal).
        onDestroy.Invoke(entity.Handle, EntityWorld, component);
        #else
        EntityRef e{
            .Handle = entity,
            .World = EntityWorld
        };
        onDestroy.Invoke(&e, component);
        #endif

        // Default change means deletion
        change = changes->Components.add(entity, ComponentChange{});
        return true;
    } else {
        WARN("A change for component [%d] already exists on entity [%016llx]!", type, entity.Handle);
        return false;
    }
}

bool ImmediateWorldCache::PrepareAddComponent(EntityHandle entity, ComponentTypeIndex type, void*& component)
{
    auto typeInfo = EntityWorld->ComponentRegistry_.Get(type);
    if (EntityWorld->GetCommittedComponent(entity, type, typeInfo->InlineSize)) {
        return false;
    }

    auto changes = GetOrAddComponentChanges(type);
    auto change = changes->Components.find(entity);

    if (!change) {
        ComponentFrameStorageIndex index;
        component = changes->FrameStorage.Allocate(index);
        change = changes->Components.add(entity, ComponentChange{
            .Ptr = component,
            .StorageIndex = index
        });
        return true;
    } else {
        WARN("A change for component [%d] already exists on entity [%016llx]!", type, entity.Handle);
        return false;
    }
}

void ImmediateWorldCache::FinalizeAddComponent(EntityHandle entity, ComponentTypeIndex type, void* component)
{
    // TODO - different behavior for proxy objects?
    auto& onConstruct = Callbacks->Get(type)->OnConstruct;
    #if defined(BG3SE_LINUX)
    // PORT (linux): the game's shape of the signal, split EntityRef (EntitySystem.h ComponentSignal).
    onConstruct.Invoke(entity.Handle, EntityWorld, component);
    #else
    EntityRef e{
        .Handle = entity,
        .World = EntityWorld
    };
    onConstruct.Invoke(&e, component);
    #endif
}

ECSComponentDataMap::ECSComponentDataMap()
{
    nullComponentData_.Name = new STDString();
}

PerECSComponentData const& ECSComponentDataMap::Get(ComponentTypeIndex type) const
{
    auto idx = (uint32_t)SparseHashMapHash(type);
    if (idx < componentData_.size()) {
        return componentData_[idx];
    } else {
        return nullComponentData_;
    }
}

PerECSComponentData& ECSComponentDataMap::GetOrAdd(ComponentTypeIndex type)
{
    auto idx = (uint32_t)SparseHashMapHash(type);
    if (idx >= componentData_.size()) {
        componentData_.resize(idx + 1);
    }

    return componentData_[idx];
}

PerECSReplicationData const& ECSComponentDataMap::Get(ReplicationTypeIndex type) const
{
    auto idx = (unsigned)type;
    if (idx < replicationData_.size()) {
        return replicationData_[idx];
    } else {
        return nullReplicationData_;
    }
}

PerECSReplicationData& ECSComponentDataMap::GetOrAdd(ReplicationTypeIndex type)
{
    auto idx = (unsigned)type;
    if (idx >= replicationData_.size()) {
        replicationData_.resize(idx + 1);
    }

    return replicationData_[idx];
}

void ECSComponentDataMap::Clear()
{
    componentData_.clear();
    replicationData_.clear();
}

ComponentOps* ComponentOpsRegistry::Get(ComponentTypeIndex id) const
{
    auto idx = (uint32_t)SparseHashMapHash(id);
    return idx < Ops.size() ? Ops[idx] : nullptr;
}

ECBEntityChangeSet const* ECBData::GetEntityChange(EntityHandle const& entity) const
{
    return EntityChanges.find(entity);
}

ECBEntityChangeSet* ECBData::GetOrAddEntityChange(EntityHandle const& entity)
{
    auto changes = EntityChanges.find(entity);
    if (changes == nullptr) {
        changes = EntityChanges.add_uninitialized(entity);
        new (changes) ECBEntityChangeSet(EntityChanges.allocator().Allocator);
    }

    return changes;
}

EntityHandle EntityCommandBuffer::CreateEntity()
{
    auto entity = HandleGenerator->Create();
    auto change = Data.GetOrAddEntityChange(entity);
    change->Flags |= EntityChangeFlags::Create;
    return entity;
}

EntityHandle EntityCommandBuffer::CreateEntityImmediate()
{
    auto entity = HandleGenerator->Create();
    auto change = Data.GetOrAddEntityChange(entity);
    change->Flags |= EntityChangeFlags::Create | EntityChangeFlags::Immediate;
    return entity;
}

bool EntityCommandBuffer::DestroyEntity(EntityHandle entity)
{
    if (HandleGenerator->IsEntityAlive(entity)) {
        auto change = Data.GetOrAddEntityChange(entity);
        if ((change->Flags & EntityChangeFlags::Create) == EntityChangeFlags::Create) {
            ERR("Cannot create and destroy an entity in the same frame");
        } else {
            change->Flags |= EntityChangeFlags::Destroy;
            return true;
        }
    }

    return false;
}

void* EntityCommandBuffer::GetComponentChange(ComponentTypeIndex type, ComponentFrameStorageIndex const& index) const
{
    auto pool = Data.ComponentPools.Find(type);
    if (index && pool) {
        return pool->GetComponent(index);
    }

    return nullptr;
}

void* EntityCommandBuffer::GetComponentChange(EntityHandle entity, ComponentTypeIndex type) const
{
    auto entityChanges = Data.EntityChanges.find(entity);
    if (entityChanges) {
        // ECB doesnt keep a component hash map, so we need to iterate all changes
        for (unsigned i = 0; i < entityChanges->Store.size(); i++) {
            auto componentChange = entityChanges->Store[i];
            if (componentChange.ComponentTypeId == type) {
                // Null index == component deletion request
                if (componentChange.Index) {
                    return Data.ComponentPools.Find(type)->GetComponent(componentChange.Index);
                } else {
                    // Don't search further if we already found a deletion entry
                    return nullptr;
                }
            }
        }
    }

    return nullptr;
}

ComponentFrameStorage* EntityCommandBuffer::GetStorage(ComponentTypeIndex type, uint16_t componentSize, void* dtor)
{
    auto storage = Data.ComponentPools.Find(type);
    if (!storage) {
        storage = Data.ComponentPools.Add(type, Allocator);
        storage->ComponentSizeInBytes = componentSize;
        storage->ComponentTypeId = type;
        storage->DestructorProc = dtor;
    }

    return storage;
}

void* EntityCommandBuffer::CreateComponentRaw(EntityHandle entity, ComponentTypeIndex type, uint16_t componentSize, ComponentFrameStorageIndex& index, void* dtor)
{
    auto storage = GetStorage(type, componentSize, dtor);

    auto component = storage->Allocate(index);
    auto changes = Data.GetOrAddEntityChange(entity);
    auto change = changes->Store.add();
    change->Index = index;
    change->ComponentTypeId = type;

    return component;
}

void EntityCommandBuffer::RemoveComponent(EntityHandle entity, ComponentTypeIndex type, uint16_t componentSize, void* dtor)
{
    auto storage = GetStorage(type, componentSize, dtor);

    auto changes = Data.GetOrAddEntityChange(entity);
    auto change = changes->Store.add();
    change->Index = ComponentFrameStorageIndex{};
    change->ComponentTypeId = type;
}

void* EntityWorld::GetAndDereferenceRawComponent(EntityHandle entityHandle, ComponentTypeIndex type, std::size_t componentSize)
{
    auto component = GetCommittedComponent(entityHandle, type, componentSize);
    if (component) {
        return DereferenceProxyComponent(component);
    }

    component = GetImmediateComponent(entityHandle, type);
    if (component) {
        // NOTE: ImmediateWorldCache keeps a pointer to the external object, not to the proxy
        return component;
    }

    component = GetECBComponent(entityHandle, type);
    if (component) {
        return DereferenceProxyComponent(component);
    }

    return nullptr;
}

void* EntityWorld::GetRawComponent(EntityHandle entityHandle, ComponentTypeIndex type, std::size_t componentSize)
{
    auto component = GetCommittedComponent(entityHandle, type, componentSize);
    if (!component) {
        component = GetImmediateComponent(entityHandle, type);
        if (!component) {
            component = GetECBComponent(entityHandle, type);
        }
    }

    return component;
}

void* EntityWorld::GetCommittedComponent(EntityHandle entityHandle, ComponentTypeIndex type, std::size_t componentSize)
{
    auto storage = GetEntityStorage(entityHandle);
    if (IsOneFrame(type)) {
        if (storage != nullptr) {
            auto component = storage->GetOneFrameComponent(entityHandle, type);
            if (component != nullptr) {
                return component;
            }
        }
    } else {
        if (storage != nullptr) {
            auto component = storage->GetComponent(entityHandle, type, componentSize);
            if (component != nullptr) {
                return component;
            }
        }
    }

    return nullptr;
}

void* EntityWorld::GetImmediateComponent(EntityHandle entityHandle, ComponentTypeIndex type)
{
    auto change = Cache->WriteChanges.GetChange(entityHandle, type);
    if (change != nullptr) {
        return change;
    }

    change = Cache->ReadChanges.GetChange(entityHandle, type);
    if (change != nullptr) {
        return change;
    }

    return nullptr;
}

void* EntityWorld::GetECBComponent(EntityHandle entityHandle, ComponentTypeIndex type)
{
    return Deferred()->GetComponentChange(entityHandle, type);
}

bool EntityWorld::MarkComponentAsChanged(EntityHandle entity, ComponentTypeIndex component)
{
    auto storage = GetEntityStorage(entity);
    if (storage) {
        if (storage->MarkComponentAsChanged(entity, component)) {
            if (!Storage->UsedFrameDataStorages[storage->StorageIndex]) {
                Storage->UsedFrameDataStorages.Set(storage->StorageIndex);
            }
            return true;
        }
    }

    return false;
}

bool EntityWorld::WasComponentChanged(EntityHandle entity, ComponentTypeIndex component)
{
    auto storage = GetEntityStorage(entity);
    return storage
        && Storage->UsedFrameDataStorages[storage->StorageIndex]
        && storage->WasComponentChanged(entity, component);
}

bool EntityWorld::IsValid(EntityHandle entityHandle) const
{
    if (entityHandle.GetThreadIndex() < std::size(HandleGenerator->ThreadStates)) {
        auto& state = HandleGenerator->ThreadStates[entityHandle.GetThreadIndex()];
        if (entityHandle.GetIndex() < state.Entries.size()) {
            auto const& salt = state.Entries[entityHandle.GetIndex()];
            return salt.Salt == entityHandle.GetSalt() && salt.Index == entityHandle.GetIndex();
        }
    }

    return false;
}

EntityCommandBuffer* EntityWorld::Deferred()
{
    return &CommandBuffers[ThreadRegistry::RequestThreadIndex()];
}

std::optional<uint16_t> EntityStorageContainer::GetEntityStorageIndex(EntityHandle entityHandle) const
{
    if (entityHandle.GetThreadIndex() >= Salts.Buckets.size()) {
        return {};
    }

    auto& componentSalts = Salts.Buckets[entityHandle.GetThreadIndex()];
    if (entityHandle.GetIndex() < componentSalts.size()) {
        auto const& salt = componentSalts[entityHandle.GetIndex()];
        if (salt.Salt == entityHandle.GetSalt()) {
            return salt.StorageIndex;
        }
    }

    return {};
}

bool EntityStorageContainer::IsEntityStorageDirty(uint16_t storageIndex) const
{
    return UsedFrameDataStorages[storageIndex];
}

EntityStorageData* EntityStorageContainer::GetEntityStorage(uint16_t storageIndex) const
{
    return Storages[storageIndex];
}

EntityStorageData* EntityStorageContainer::GetEntityStorage(EntityHandle entityHandle) const
{
    auto storageIdx = GetEntityStorageIndex(entityHandle);
    if (storageIdx) {
        return GetEntityStorage(*storageIdx);
    } else {
        return nullptr;
    }
}

EntityStorageData* EntityWorld::GetEntityStorage(EntityHandle entityHandle) const
{
    return Storage->GetEntityStorage(entityHandle);
}


StringView ECSComponentLog::GetName() const
{
    auto name = GetCurrentExtensionState()->GetLua()->GetEntitySystemHelpers()->GetComponentName(ComponentType);
    return name ? *name : StringView{};
}

std::optional<ExtComponentType> ECSComponentLog::GetType() const
{
    return GetCurrentExtensionState()->GetLua()->GetEntitySystemHelpers()->GetComponentType(ComponentType);
}

void ECSChangeLog::Clear()
{
    Entities.clear();
}

void ECSChangeLog::AddEntityChange(EntityHandle entity, EntityChangeFlags flags)
{
    auto entry = Entities.get_or_add(entity);
    entry->Flags = entry->Flags | flags;
}

void ECSChangeLog::AddComponentChange(EntityWorld* world, EntityHandle entity, ComponentTypeIndex type, ComponentChangeFlags flags)
{
    auto componentInfo = world->ComponentRegistry_.Get(type);

    if (componentInfo->OneFrame) {
        flags |= ComponentChangeFlags::OneFrame;
    }
    
    if (componentInfo->Replicated) {
        flags |= ComponentChangeFlags::ReplicatedComponent;
    }

    auto entry = Entities.get_or_add(entity);
    auto componentEntry = entry->Components.get_or_add((uint16_t)type);

    componentEntry->ComponentType = type;
    componentEntry->Flags = componentEntry->Flags | flags;
}


#if defined(NDEBUG)
RuntimeCheckLevel EntitySystemHelpersBase::CheckLevel{ RuntimeCheckLevel::Once };
#else
RuntimeCheckLevel EntitySystemHelpersBase::CheckLevel{ RuntimeCheckLevel::FullECS };
#endif

EntitySystemHelpersBase::EntitySystemHelpersBase()
    : staticDataIndices_{ resource::UndefinedStaticDataType },
    systemIndices_{ UndefinedSystem },
    tracer_(this)
{}

std::optional<ComponentTypeIndex> EntitySystemHelpersBase::GetComponentIndex(ExtComponentType type) const
{
    auto idx = extComponentMap_[(unsigned)type].ComponentIndex;
    if (idx != UndefinedComponent) {
        return idx;
    } else {
        return {};
    }
}

STDString SimplifyComponentName(StringView name)
{
    STDString key{ name };
    if (key.length() > 52 && strncmp(key.c_str(), "class ls::_StringView<char> __cdecl ls::GetTypeName<", 52) == 0) {
        key = key.substr(52);
    }

    if (key.length() > 6 && strncmp(key.c_str(), "class ", 6) == 0) {
        key = key.substr(6);
    }
    else if (key.length() > 7 && strncmp(key.c_str(), "struct ", 7) == 0) {
        key = key.substr(7);
    }

    if (key.length() > 7 && strncmp(key.c_str() + key.size() - 7, ">(void)", 7) == 0) {
        key = key.substr(0, key.size() - 7);
    }

    return key;
}

void* EntitySystemHelpersBase::CreateComponentRaw(EntityHandle entity, ExtComponentType type)
{
    auto const& meta = GetComponentMeta(type);
    if (!meta.ComponentIndex
        || meta.Properties == nullptr
        || meta.Properties->Construct == nullptr) {
        return nullptr;
    }

    ComponentFrameStorageIndex index;
    auto ptr = GetEntityWorld()->Deferred()->CreateComponentRaw(entity, *meta.ComponentIndex, meta.InlineSize, index, (void*)meta.Properties->ProxyDestroy); // PATCH (clang): no implicit function-pointer -> void*

    if (meta.IsProxy) {
        auto external = GameAllocRaw(meta.ExternalSize);
        memset(external, 0, meta.ExternalSize);
        meta.Properties->Construct(external);
        *(void**)ptr = external;
        return external;
    } else {
        // Ensure we're using zeroed memory since not every component has proper default constructors
        // and could end up using leftover garbage from memory
        memset(ptr, 0, meta.InlineSize);
        meta.Properties->Construct(ptr);
        return ptr;
    }
}

void* EntitySystemHelpersBase::CreateComponentImmediateRaw(EntityHandle entity, ExtComponentType type)
{
    auto const& meta = GetComponentMeta(type);
    if (!meta.ComponentIndex
        || meta.Properties == nullptr
        || meta.Properties->Construct == nullptr) {
        return nullptr;
    }

    ComponentFrameStorageIndex index;
    void* ptr;
    auto iwc = GetEntityWorld()->Cache;
    if (iwc->PrepareAddComponent(entity, *meta.ComponentIndex, ptr)) {
        if (meta.IsProxy) {
            auto comp = (void**)ptr;
            *comp = GameAllocRaw(meta.ExternalSize);
            memset(*comp, 0, meta.ExternalSize);
            meta.Properties->Construct(*comp);
            return *comp;
        } else {
            // Ensure we're using zeroed memory since not every component has proper default constructors
            // and could end up using leftover garbage from memory
            memset(ptr, 0, meta.InlineSize);
            meta.Properties->Construct(ptr);
            return ptr;
        }

        iwc->FinalizeAddComponent(entity, *meta.ComponentIndex, ptr);
    }

    return nullptr;
}

bool EntitySystemHelpersBase::RemoveComponent(EntityHandle entity, ExtComponentType type)
{
    auto const& meta = GetComponentMeta(type);
    if (!meta.ComponentIndex
        || meta.Properties == nullptr) {
        return false;
    }

    GetEntityWorld()->Deferred()->RemoveComponent(entity, *meta.ComponentIndex, meta.InlineSize, (void*)meta.Properties->ProxyDestroy); // PATCH (clang): no implicit function-pointer -> void*
    return true;
}

BitSet<>* EntitySystemHelpersBase::GetReplicationFlags(EntityHandle const& entity, ExtComponentType type)
{
    auto replicationIdx = GetComponentMeta(type).ReplicationIndex;
    if (!replicationIdx) {
        return nullptr;
    }

    return GetReplicationFlags(entity, *replicationIdx);
}

BitSet<>* EntitySystemHelpersBase::GetReplicationFlags(EntityHandle const& entity, ReplicationTypeIndex replicationType)
{
    auto world = GetEntityWorld();
    if (!world || !world->Replication) return nullptr;

    auto& pools = world->Replication->ComponentPools;
    auto typeId = (uint16_t)replicationType;
    if (typeId >= (int)pools.Size()) {
        OsiError("Attempted to fetch replication list for component " << typeId << ", but replication pool size is " << pools.Size() << "!");
        return nullptr;
    }

    return pools[typeId].try_get(entity);
}

BitSet<>* EntitySystemHelpersBase::GetOrCreateReplicationFlags(EntityHandle const& entity, ExtComponentType type)
{
    auto replicationIdx = GetComponentMeta(type).ReplicationIndex;
    if (!replicationIdx) {
        return nullptr;
    }

    return GetOrCreateReplicationFlags(entity, *replicationIdx);
}

BitSet<>* EntitySystemHelpersBase::GetOrCreateReplicationFlags(EntityHandle const& entity, ReplicationTypeIndex replicationType)
{
    auto world = GetEntityWorld();
    if (!world || !world->Replication) return nullptr;

    auto& pools = world->Replication->ComponentPools;
    auto typeId = (uint16_t)replicationType;
    if (typeId >= (int)pools.Size()) {
        OsiError("Attempted to fetch replication list for component " << typeId << ", but replication pool size is " << pools.Size() << "!");
        return nullptr;
    }

    auto& pool = pools[typeId];
    auto syncFlags = pool.try_get(entity);
    if (syncFlags) {
        return syncFlags;
    } else {
        return pool.add_key(entity);
    }
}

void EntitySystemHelpersBase::NotifyReplicationFlagsDirtied()
{
    auto world = GetEntityWorld();
    if (!world || !world->Replication) return;

    world->Replication->Dirty = true;
}

void EntitySystemHelpersBase::BindSystem(StringView name, SystemTypeIndex systemId)
{
    auto it = systemIndexMappings_.insert(std::make_pair(name, systemId));
    if (systemTypeIdToName_.size() <= (unsigned)systemId) {
        systemTypeIdToName_.resize((unsigned)systemId + 1);
    }

    systemTypeIdToName_[(unsigned)systemId] = &it.first->first;
}

void EntitySystemHelpersBase::BindStaticData(StringView name, resource::StaticDataTypeIndex id)
{
    auto it = staticDataMappings_.insert(std::make_pair(name, id));
    if (staticDataIdToName_.size() <= (unsigned)id) {
        staticDataIdToName_.resize((unsigned)id + 1);
    }

    staticDataIdToName_[(unsigned)id] = &it.first->first;
}

void EntitySystemHelpersBase::BindComponent(StringView name, ComponentTypeIndex id)
{
    STDString const* pName;
    auto it = componentNameToIndexMappings_.find(STDString(name));
    if (it == componentNameToIndexMappings_.end()) {
        auto iit = componentNameToIndexMappings_.insert(std::make_pair(name, IndexMappings{id, UndefinedReplicationComponent}));
        pName = &iit.first->first;
    } else {
        it->second.ComponentIndex = id;
        pName = &it->first;
    }

    auto& binding = ecsComponentData_.GetOrAdd(id);
    binding.Name = pName;
}

void EntitySystemHelpersBase::BindReplication(StringView name, ReplicationTypeIndex id)
{
    auto it = componentNameToIndexMappings_.find(STDString(name));
    if (it == componentNameToIndexMappings_.end()) {
        componentNameToIndexMappings_.insert(std::make_pair(name, IndexMappings{UndefinedComponent, id}));
    } else {
        it->second.ReplicationIndex = id;
    }
}

void EntitySystemHelpersBase::UpdateComponentMappings()
{
    if (initialized_) return;

    componentNameToIndexMappings_.clear();
    ecsComponentData_.Clear();
    extComponentMap_.fill(PerComponentData{});
    staticDataIndices_.fill(resource::UndefinedStaticDataType);
    systemIndices_.fill(UndefinedSystem);
#if defined(BG3SE_LINUX)
    mapStats_ = {};
#endif

    std::unordered_map<int32_t*, TypeIdContext> contexts;
    for (auto const& context : GetStaticSymbols().IndexSymbolToContextMaps) {
        auto name = ecs::SimplifyComponentName(context.second);
        if (name == "ecs::OneFrameComponentTypeIdContext") {
            contexts.insert(std::make_pair(context.first, TypeIdContext::OneFrameComponent));
        } else if (name == "ecs::ComponentTypeIdContext") {
            contexts.insert(std::make_pair(context.first, TypeIdContext::Component));
        } else if (name == "ecs::EntityWorld::SystemsContext"
            // PORT (linux): the Linux build's .symtab spells the systems context
            // ecs::SystemsContext (ls::TypeContext<ecs::SystemsContext>::m_State, 934 TypeIds
            // under it on c8da62b1; tools/typeids.py), and that is the name the embedded
            // table carries. Nothing else in the build has the Windows spelling.
            || name == "ecs::SystemsContext") {
            contexts.insert(std::make_pair(context.first, TypeIdContext::System));
        } else if (name == "ecs::sync::ReplicatedTypeContext") {
            contexts.insert(std::make_pair(context.first, TypeIdContext::Replication));
        } else if (name == "ls::ImmutableDataHeadmaster") {
            contexts.insert(std::make_pair(context.first, TypeIdContext::ImmutableData));
        }
    }

    auto const& symbolMaps = GetStaticSymbols().IndexSymbolToNameMaps;
    for (auto const& mapping : symbolMaps) {
        auto s = STDString(mapping.second.name);
        auto name = SimplifyComponentName(mapping.second.name);
        if (name.starts_with("ecs::query::spec::Spec<")) {
            // Queries ignored
        } else {
            auto contextIt = contexts.find(mapping.second.context);
            if (contextIt != contexts.end()) {
                switch (contextIt->second) {
                case TypeIdContext::System:
                    BindSystem(name, SystemTypeIndex(*mapping.first));
                    break;

                case TypeIdContext::ImmutableData:
                    BindStaticData(name, resource::StaticDataTypeIndex(*mapping.first));
                    break;

                case TypeIdContext::Component:
                    BindComponent(name, ComponentTypeIndex(*mapping.first));
                    break;

                case TypeIdContext::OneFrameComponent:
                    BindComponent(name, ComponentTypeIndex(*mapping.first | 0x8000));
                    break;

                case TypeIdContext::Replication:
                    BindReplication(name, ReplicationTypeIndex(*mapping.first));
                    break;
                }
            }
        }
    }

#if defined(DEBUG_INDEX_MAPPINGS)
    DEBUG_IDX("COMPONENT MAPPINGS:");

    for (auto const& map : componentNameToIndexMappings_) {
        DEBUG_IDX("\t" << map.first << ": Handle " << map.second.HandleIndex << ", Component " << map.second.ComponentIndex);
    }

    DEBUG_IDX("-------------------------------------------------------");
#endif

    #define T(cls) MapComponentIndices(cls::EngineClass, cls::ComponentType, sizeof(cls), IsProxyComponentType<cls>, cls::OneFrame);
    #include <GameDefinitions/Components/AllComponentTypes.inl>
    #undef T

    #define T(cls) MapSystemIndex(cls::EngineClass, cls::SystemType);
    #include <GameDefinitions/Components/AllSystemTypes.inl>
    #undef T

#define FOR_RESOURCE_TYPE(cls) MapResourceManagerIndex(resource::cls::EngineClass, resource::cls::ResourceManagerType);
    FOR_EACH_GUID_RESOURCE_TYPE()
#undef FOR_RESOURCE_TYPE

#if defined(BG3SE_LINUX)
    // PORT (linux): the count that says whether the TypeId table did its job, in the log
    // where the per-name "Could not find index" warnings are. Names bound = distinct names
    // the maps yielded (a component name may come from the component, one-frame and
    // replication contexts alike); found/asked = the Map*Index() lookups above. An empty
    // table reads as 0 names and 0 found, not as silence.
    INFO("UpdateComponentMappings(): %zu component, %zu system, %zu static-data names bound; upstream's lookups: components %u/%u, systems %u/%u, resource managers %u/%u",
        componentNameToIndexMappings_.size(), systemIndexMappings_.size(), staticDataMappings_.size(),
        mapStats_.componentsFound, mapStats_.componentsAsked, mapStats_.systemsFound, mapStats_.systemsAsked,
        mapStats_.resourcesFound, mapStats_.resourcesAsked);
#endif

    Bind();
    initialized_ = true;
}

void EntitySystemHelpersBase::ValidatePropertyMapBindings()
{
    for (uint32_t componentType = 0; componentType < extComponentMap_.size(); componentType++) {
        if (!extComponentMap_[componentType].Properties) {
            ERR("[ECS] Component %s has no property map!",
                EnumInfo<ExtComponentType>::Find((ExtComponentType)componentType).GetString());
        }
    }
}

void EntitySystemHelpersBase::BindExtComponent(ComponentTypeIndex componentIndex, ExtComponentType type)
{
    auto idx = (uint32_t)SparseHashMapHash(componentIndex);
    if (idx >= componentMap_.size()) {
        componentMap_.resize(idx + 1);
    }

    componentMap_[idx] = &extComponentMap_[(unsigned)type];
}

#if defined(BG3SE_LINUX)
// PORT (linux): upstream's EngineClass constants are MSVC __FUNCSIG__ spellings, with
// `struct `/`class `/`enum ` on every template argument and `> >` -- e.g.
// "eoc::character_creation::_private::DefaultValuesManager<struct ...DefaultValues<enum EAbility> >".
// The names the Linux table carries are llvm-cxxfilt's, without the keywords and with `>>`,
// which is also how the game's own GetTypeName() [T = ...] strings spell them. Seven of the
// resource-manager names differ this way; no component or system name does. The lookup key
// is normalised here so upstream's constants stay as they are (tools/typeids.py coverage
// applies the same rule offline).
static STDString EngineClassKey(char const* name)
{
    STDString key;
    std::string_view s{ name };
    while (!s.empty()) {
        if (s.starts_with("struct ")) { s.remove_prefix(7); continue; }
        if (s.starts_with("class ")) { s.remove_prefix(6); continue; }
        if (s.starts_with("enum ")) { s.remove_prefix(5); continue; }
        if (s.starts_with("> >")) { key += ">>"; s.remove_prefix(3); continue; }
        key += s[0];
        s.remove_prefix(1);
    }
    return key;
}
#else
static char const* EngineClassKey(char const* name) { return name; }
#endif

void EntitySystemHelpersBase::MapComponentIndices(char const* componentName, ExtComponentType type, std::size_t size, bool isProxy, bool oneFrame)
{
#if defined(BG3SE_LINUX)
    mapStats_.componentsAsked++;
#endif
    auto it = componentNameToIndexMappings_.find(EngineClassKey(componentName));
    if (it == componentNameToIndexMappings_.end()) {
        WARN("Could not find index for component: %s", componentName);
        return;
    }
#if defined(BG3SE_LINUX)
    mapStats_.componentsFound++;
#endif

    auto& comp = extComponentMap_[(unsigned)type];
    comp.Type = type;

    if (it->second.ComponentIndex != UndefinedComponent) {
        auto& binding = ecsComponentData_.GetOrAdd(it->second.ComponentIndex);
        binding.Name = &it->first;
        binding.ExtType = type;
        binding.ReplicationType = it->second.ReplicationIndex;

        comp.ComponentIndex = it->second.ComponentIndex;
        BindExtComponent(it->second.ComponentIndex, type);
    }

    if (it->second.ReplicationIndex != UndefinedReplicationComponent) {
        auto& binding = ecsComponentData_.GetOrAdd(it->second.ReplicationIndex);
        binding.Name = &it->first;
        binding.ExtType = type;
        binding.ComponentType = it->second.ComponentIndex;

        comp.ReplicationIndex = it->second.ReplicationIndex;
    }

    se_assert(size < 0x10000);
    if (isProxy) {
        comp.InlineSize = (uint16_t)sizeof(void*);
        comp.ExternalSize = (uint16_t)size;
    } else {
        comp.InlineSize = (uint16_t)size;
        comp.ExternalSize = 0;
    }
    comp.IsProxy = isProxy;
    comp.OneFrame = oneFrame;
}

void EntitySystemHelpersBase::MapResourceManagerIndex(char const* componentName, ExtResourceManagerType type)
{
#if defined(BG3SE_LINUX)
    mapStats_.resourcesAsked++;
#endif
    auto it = staticDataMappings_.find(EngineClassKey(componentName));
    if (it != staticDataMappings_.end()) {
        staticDataIndices_[(unsigned)type] = it->second;
#if defined(BG3SE_LINUX)
        mapStats_.resourcesFound++;
#endif
    } else {
        OsiWarn("Could not find index for resource manager: " << componentName);
    }
}

void EntitySystemHelpersBase::MapSystemIndex(char const* systemName, ExtSystemType type)
{
#if defined(BG3SE_LINUX)
    mapStats_.systemsAsked++;
#endif
    auto it = systemIndexMappings_.find(EngineClassKey(systemName));
    if (it != systemIndexMappings_.end()) {
        systemIndices_[(unsigned)type] = it->second;
#if defined(BG3SE_LINUX)
        mapStats_.systemsFound++;
#endif
    } else {
        OsiWarn("Could not find index for system: " << systemName);
    }
}

void* EntitySystemHelpersBase::GetRawComponent(Guid const& guid, ExtComponentType type)
{
    auto handle = GetEntityHandle(guid);
    if (handle) {
        return GetRawComponent(handle, type);
    } else {
        return nullptr;
    }
}

void* EntitySystemHelpersBase::GetRawComponent(FixedString const& guid, ExtComponentType type)
{
    auto handle = GetEntityHandle(guid);
    if (handle) {
        return GetRawComponent(handle, type);
    } else {
        return nullptr;
    }
}

void* EntitySystemHelpersBase::GetRawComponent(EntityHandle entityHandle, PerComponentData const& meta)
{
    if (!meta.ComponentIndex) {
        return nullptr;
    }

    auto world = GetEntityWorld();
    if (!world) {
        return nullptr;
    }

    if (meta.IsProxy) {
        return world->GetAndDereferenceRawComponent(entityHandle, *meta.ComponentIndex, meta.InlineSize);
    } else {
        return world->GetRawComponent(entityHandle, *meta.ComponentIndex, meta.InlineSize);
    }
}

void* EntitySystemHelpersBase::GetRawComponent(EntityHandle entityHandle, ExtComponentType type)
{
    auto const& meta = GetComponentMeta(type);
    return GetRawComponent(entityHandle, meta);
}

bool EntitySystemHelpersBase::MarkComponentAsChanged(EntityHandle entityHandle, ExtComponentType type)
{
    auto world = GetEntityWorld();
    if (!world) {
        return false;
    }

    auto const& meta = GetComponentMeta(type);
    if (meta.ComponentIndex) {
        return world->MarkComponentAsChanged(entityHandle, *meta.ComponentIndex);
    } else {
        return false;
    }
}

bool EntitySystemHelpersBase::WasComponentChanged(EntityHandle entityHandle, ExtComponentType type)
{
    auto world = GetEntityWorld();
    if (!world) {
        return false;
    }

    auto const& meta = GetComponentMeta(type);
    if (meta.ComponentIndex) {
        return world->WasComponentChanged(entityHandle, *meta.ComponentIndex);
    } else {
        return false;
    }
}

void* EntitySystemHelpersBase::GetRawSingleton(ExtComponentType type)
{
    auto& meta = GetComponentMeta(type);
    if (meta.SingleComponentQuery == ecs::UndefinedQuery) {
        WARN("No query defined for singleton %s?", GetComponentName(*meta.ComponentIndex)->c_str());
        return nullptr;
    }

    auto world = GetEntityWorld();
    auto const& query = world->Queries.Queries[(unsigned)meta.SingleComponentQuery];
    if (query.EntityStorages.empty()) {
        return nullptr;
    }

    auto const& storage = query.EntityStorages.values()[0];
    if (storage.Storage->InstanceToPageMap.empty()) {
        return nullptr;
    }

    auto page = storage.Storage->InstanceToPageMap.values()[0];
    auto component = storage.Storage->GetComponent(page, storage.GetComponentIndex(0), meta.InlineSize);
    if (component && meta.IsProxy) {
        component = DereferenceProxyComponent(component);
    }
    return component;
}

EntityHandle EntitySystemHelpersBase::GetSingletonEntity(ExtComponentType type)
{
    auto& meta = GetComponentMeta(type);
    if (meta.SingleComponentQuery == ecs::UndefinedQuery) {
        WARN("No query defined for singleton %s?", GetComponentName(*meta.ComponentIndex)->c_str());
        return {};
    }

    auto world = GetEntityWorld();
    auto const& query = world->Queries.Queries[(unsigned)meta.SingleComponentQuery];
    if (query.EntityStorages.empty()) {
        return {};
    }

    auto const& storage = query.EntityStorages.values()[0];
    if (storage.Storage->InstanceToPageMap.empty()) {
        return {};
    }

    return storage.Storage->InstanceToPageMap.keys()[0];
}

SystemTypeEntry* EntitySystemHelpersBase::GetSystemEntry(ExtSystemType type)
{
    auto world = GetEntityWorld();
    if (!world) {
        return nullptr;
    }

    auto index = systemIndices_[(unsigned)type];
    if (index != UndefinedSystem) {
        return &world->Systems.Systems[(unsigned)index];
    } else {
        return nullptr;
    }
}

void* EntitySystemHelpersBase::GetRawSystem(ExtSystemType type)
{
    auto world = GetEntityWorld();
    if (!world) {
        return nullptr;
    }

    auto index = systemIndices_[(unsigned)type];
    if (index != UndefinedSystem) {
        return world->Systems.Systems[(unsigned)index].System;
    } else {
        return nullptr;
    }
}

void EntitySystemHelpersBase::Bind()
{
    auto world = GetEngineEntityWorld();
    // The underlying EntityWorld pointer should never change after binding
    se_assert(world == nullptr || World == nullptr || world == World);
    World = world;
}

void EntitySystemHelpersBase::PreUpdate()
{
}

void EntitySystemHelpersBase::PostUpdate()
{
    OPTICK_EVENT();
    if (!validated_ && GetEntityWorld() != nullptr) {
        ValidateMappedComponentSizes();
        UpdateQueryCache();
        validated_ = true;
    }

    if (CheckLevel == RuntimeCheckLevel::FullECS) {
        ValidateReplication();
    }

    tracer_.LogReplicatedChanges();
}

void EntitySystemHelpersBase::OnInit()
{
    if (World == nullptr) return;   // PORT (linux): no entity world bound yet (phase 6)
    InitSystemUpdateHooks();
}

void EntitySystemHelpersBase::OnDestroy()
{
    ClearSystemUpdateHooks();
}

void EntitySystemHelpersBase::OnFlushECBs()
{
    OPTICK_EVENT();
    if (CheckLevel == RuntimeCheckLevel::FullECS) {
        ValidateECBFlushChanges();
        ValidateImmediateWorldCacheChanges();
    }

    ThrowECBFlushEvents();
    tracer_.LogECBChanges();
    tracer_.LogImmediateWorldCacheChanges();
    tracer_.LogComponentModifications();
}

void ECSChangeTracer::StartTracing()
{
    tracing_ = true;
}

void ECSChangeTracer::StopTracing()
{
    tracing_ = false;
}

void ECSChangeTracer::LogECBChanges()
{
    if (!tracing_ || !options_.TrackECB) return;

    OPTICK_EVENT(Optick::Category::Debug);

    auto world = ecs_->GetEntityWorld();
    for (auto& ecb : world->CommandBuffers) {
        for (unsigned i = 0; i < ecb.Data.EntityChanges.size(); i++) {
            auto entityHandle = ecb.Data.EntityChanges.key_at(i);
            auto const& entityChanges = ecb.Data.EntityChanges.Values[i];

            log_.AddEntityChange(entityHandle, entityChanges.Flags);

            for (unsigned j = 0; j < entityChanges.Store.size(); j++) {
                auto const& upd = entityChanges.Store[j];

                log_.AddComponentChange(world, entityHandle, upd.ComponentTypeId,
                    upd.Index ? ComponentChangeFlags::Create : ComponentChangeFlags::Destroy);
            }
        }
    }
}

void ECSChangeTracer::LogImmediateWorldCacheChanges()
{
    if (!tracing_ || !options_.TrackImmediateWorldCache) return;

    OPTICK_EVENT(Optick::Category::Debug);

    auto world = ecs_->GetEntityWorld();
    auto const& changes = world->Cache->WriteChanges;
    for (unsigned i = 0; i < changes.AvailableComponentTypes.NumBits; i++) {
        if (changes.AvailableComponentTypes[i]) {
            auto const& changeSet = changes.ComponentsByType[i];
            for (unsigned j = 0; j < changeSet.Components.size(); j++) {
                auto entityHandle = changeSet.Components.key_at(j);
                auto const& change = changeSet.Components.Values[j];

                log_.AddComponentChange(world, entityHandle, ComponentTypeIndex{ (uint16_t)i }, change.Ptr ? ComponentChangeFlags::Create : ComponentChangeFlags::Destroy);
            }
        }
    }
}

void ECSChangeTracer::LogComponentModifications()
{
    if (!tracing_ || !options_.TrackModifications) return;

    OPTICK_EVENT(Optick::Category::Debug);

    auto world = ecs_->GetEntityWorld();
    auto storages = world->Storage;
    for (auto index = storages->UsedFrameDataStorages.FindFirst(); index; index = storages->UsedFrameDataStorages.FindNext(*index)) {
        auto storage = storages->Storages[*index];
        for (auto componentSlot = storage->ModifiedComponents.FindFirst(); componentSlot; componentSlot = storage->ModifiedComponents.FindNext(*componentSlot)) {
            auto componentType = storage->ComponentDtors[*componentSlot].ComponentTypeId;

            if (!options_.ExcludeModificationTypes[(unsigned)componentType]) {
                for (unsigned pageIdx = 0; pageIdx < storage->Components.size(); pageIdx++) {
                    auto& page = storage->Components[pageIdx]->Components[*componentSlot];
                    auto handles = storage->Handles[pageIdx];

                    auto modifications = page.ModifiedEntities.load();
                    for (auto componentIdx = BitSetScan(&modifications, &modifications + 1); componentIdx;
                        componentIdx = BitSetScan(&modifications, &modifications + 1, *componentIdx)) {
                        if (handles->Pool[*componentIdx]) {
                            log_.AddComponentChange(world, handles->Pool[*componentIdx], componentType, ComponentChangeFlags::Modify);
                        }
                    }
                }
            }
        }
    }
}

void ECSChangeTracer::LogReplicatedChanges()
{
    if (!tracing_ || !options_.TrackReplication) return;

    auto world = ecs_->GetEntityWorld();
    ecs::SyncBuffers* buffers = world->Replication;
    if (!buffers) {
        buffers = *GetStaticSymbols().GetEoCClient()->GameClient->ReplicationPeer.Buffers;
    }

    if (!buffers) return;

    OPTICK_EVENT(Optick::Category::Debug);
    for (unsigned i = 0; i < buffers->ComponentPools.size(); i++) {
        auto const& pool = buffers->ComponentPools[i];
        if (pool.size() > 0) {
            for (auto const& entity : pool) {
                auto type = ecs_->GetComponentIndex(ReplicationTypeIndex{ (uint16_t)i });

                if (type != UndefinedComponent) {
                    log_.AddComponentChange(world, entity.Key(), type, ComponentChangeFlags::Replicate);
                }
            }
        }
    }
}

void EntitySystemHelpersBase::ThrowECBFlushEvents()
{
    auto state = GetExtensionState();
    if (state == nullptr || !state->GetLua()) return;

    OPTICK_EVENT();
    auto world = GetEntityWorld();
    auto& hooks = state->GetLua()->GetComponentEventHooks();

    for (auto& ecb : world->CommandBuffers) {
        for (unsigned i = 0; i < ecb.Data.EntityChanges.size(); i++) {
            auto entity = ecb.Data.EntityChanges.key_at(i);
            auto const& entityChanges = ecb.Data.EntityChanges.Values[i];

            for (unsigned j = 0; j < entityChanges.Store.size(); j++) {
                auto const& upd = entityChanges.Store[j];
                if (upd.Index) {
                    auto typeInfo = GetComponentMeta(upd.ComponentTypeId);
                    if (typeInfo && typeInfo->OneFrame) {
                        hooks.OnEntityEvent(*world, entity, upd.ComponentTypeId, lua::EntityComponentEvent::Create, nullptr);
                    }
                }
            }
        }
    }
}

void EntitySystemHelpersBase::ValidateReplication()
{
    auto world = GetEntityWorld();
    if (!world->Replication || !world->Replication->Dirty) return;

    OPTICK_EVENT(Optick::Category::Debug);
    for (unsigned i = 0; i < world->Replication->ComponentPools.size(); i++) {
        auto const& pool = world->Replication->ComponentPools[i];
        auto componentType = GetComponentType(ReplicationTypeIndex(i));
        if (componentType && pool.size() > 0) {
            auto pm = GetComponentMeta(*componentType).Properties;
            if (pm != nullptr) {
                for (auto const& entity : pool) {
                    auto component = GetRawComponent(entity.Key(), *componentType);
                    if (component) {
                        pm->ValidateObject(component);
                    }
                }
            }
        }
    }
}

void EntitySystemHelpersBase::ValidateMappedComponentSizes()
{
    OPTICK_EVENT(Optick::Category::Debug);
    auto world = GetEntityWorld();

    Array<ComponentTypeIndex> deletions;
    for (unsigned componentIdx = 0; componentIdx < extComponentMap_.size(); componentIdx++) {
        auto componentType = extComponentMap_[componentIdx].ComponentIndex;
        if (componentType) {
            if (!ValidateMappedComponentSize(world, *componentType, ExtComponentType{ componentIdx })) {
                deletions.push_back(*componentType);
            }
        }
    }

    for (auto typeId : deletions) {
        auto& mapping = ecsComponentData_.GetOrAdd(typeId);
        if (mapping.ExtType) {
            ERR("[ECS INTEGRITY CHECK] Force unmap component %s", mapping.Name->c_str());
            extComponentMap_[(unsigned)*mapping.ExtType].ComponentIndex = UndefinedComponent;
            mapping.ExtType = {};
        }
    }
}

bool EntitySystemHelpersBase::ValidateMappedComponentSize(ecs::EntityWorld* world, ComponentTypeIndex typeId, ExtComponentType extType)
{
    auto mapped = world->ComponentRegistry_.Get(typeId);
    auto const& local = extComponentMap_[(unsigned)extType];
    auto name = ecsComponentData_.Get(typeId).Name;
    if (mapped != nullptr) {
        if (!local.Properties) {
            WARN("[ECS INTEGRITY CHECK] '%s' has no property map", name->c_str());
            return false;
        }

        if (mapped->TotalSize < mapped->InlineSize) {
            ERR("[ECS INTEGRITY CHECK] '%s' has incorrect component size? (inline %d, total %d)",
                name->c_str(), mapped->InlineSize, mapped->TotalSize);
            return false;
        }

        if (local.IsProxy) {
            assert(local.InlineSize == sizeof(void*));
            if (mapped->InlineSize != sizeof(void*)) {
                ERR("[ECS INTEGRITY CHECK] '%s' marked as proxy, but entity inline data is not a proxy (%d)", 
                    name->c_str(), mapped->InlineSize);
                return false;
            }

            if (mapped->TotalSize - mapped->InlineSize != local.ExternalSize) {
                ERR("[ECS INTEGRITY CHECK] '%s' OOB size mismatch: local %d, ECS %d", 
                    name->c_str(), local.ExternalSize, mapped->TotalSize - mapped->InlineSize);
                // Don't unmap since OOB mismatch doesn't invalidate component store layout
                return true;
            }
        } else {
            if (mapped->InlineSize != mapped->TotalSize) {
                ERR("[ECS INTEGRITY CHECK] '%s' marked as non-proxy, but entity has OOB data (inline %d, total %d)",
                    name->c_str(), mapped->InlineSize, mapped->TotalSize);
                return false;
            }

            if (mapped->InlineSize != local.InlineSize) {
                ERR("[ECS INTEGRITY CHECK] '%s' inline size mismatch: local %d, ECS %d",
                    name->c_str(), local.InlineSize, mapped->InlineSize);
                return false;
            }
        }

        if (mapped->OneFrame != local.OneFrame) {
            ERR("[ECS INTEGRITY CHECK] '%s' one-frame type mismatch", name->c_str());
            return false;
        }
    }

    return true;
}

void EntitySystemHelpersBase::ValidateECBFlushChanges()
{
    OPTICK_EVENT(Optick::Category::Debug);
    auto world = GetEntityWorld();

    for (auto& ecb : world->CommandBuffers) {
        for (unsigned i = 0; i < ecb.Data.EntityChanges.size(); i++) {
            auto entityHandle = ecb.Data.EntityChanges.key_at(i);
            auto const& entityChanges = ecb.Data.EntityChanges.Values[i];

            for (unsigned j = 0; j < entityChanges.Store.size(); j++) {
                auto const& change = entityChanges.Store[j];

                if (change.Index) {
                    auto meta = GetComponentMeta(change.ComponentTypeId);
                    if (meta) {
                        auto component = ecb.GetComponentChange(change.ComponentTypeId, change.Index);
                        if (component) {
                            if (meta->IsProxy) {
                                meta->Properties->ValidateObject(*(void**)component);
                            } else {
                                meta->Properties->ValidateObject(component);
                            }
                        }
                    }
                }
            }
        }
    }
}

void EntitySystemHelpersBase::ValidateImmediateWorldCacheChanges()
{
    OPTICK_EVENT(Optick::Category::Debug);
    auto world = GetEntityWorld();
    ValidateImmediateWorldCacheChanges(world->Cache->WriteChanges);
}

void EntitySystemHelpersBase::ValidateImmediateWorldCacheChanges(ImmediateWorldCache::Changes& changes)
{
    for (auto i = 0; i < extComponentMap_.size(); i++) {
        auto const& componentInfo = extComponentMap_[i];
        if (componentInfo.ComponentIndex) {
            if (changes.AvailableComponentTypes[(unsigned)*componentInfo.ComponentIndex]) {
                auto const& pool = changes.ComponentsByType[(unsigned)*componentInfo.ComponentIndex];
                if (pool.FrameStorage.ComponentSizeInBytes != componentInfo.InlineSize) {
                    auto name = ecsComponentData_.Get(*componentInfo.ComponentIndex).Name;
                    ERR("[ECS INTEGRITY CHECK] Inline component size mismatch (%s): local %d, ECS %d", 
                        name->c_str(), componentInfo.InlineSize, pool.FrameStorage.ComponentSizeInBytes);
                }
            }
        }
    }

    for (uint32_t componentId = 0; componentId < changes.AvailableComponentTypes.size(); componentId++) {
        if (changes.AvailableComponentTypes[componentId]) {
            auto const& components = changes.ComponentsByType[componentId].Components;
            auto meta = GetComponentMeta(ComponentTypeIndex(componentId));
            if (meta && components.Values.size() > 0) {
                for (uint32_t j = 0; j < components.Values.size(); j++) {
                    auto const& component = components.Values[j];
                    if (component.StorageIndex) {
                        meta->Properties->ValidateObject(component.Ptr);
                    }
                }
            }
        }
    }
}

void EntitySystemHelpersBase::MapSingleComponentQuery(QueryIndex queryIndex, ComponentTypeIndex component)
{
    auto extComponent = GetComponentType(component);
    if (!extComponent) return;

    auto& desc = extComponentMap_[(unsigned)*extComponent];
    if (desc.SingleComponentQuery != ecs::UndefinedQuery) {
        auto const& currentQuery = GetEntityWorld()->Queries.Queries[(unsigned)desc.SingleComponentQuery];
        auto const& newQuery = GetEntityWorld()->Queries.Queries[(unsigned)queryIndex];

        // Only allow replacing the query if it is different from the previous and has less optionals/adds
        if (!(
            newQuery.GetOptionalOneFrames().size() < currentQuery.GetOptionalOneFrames().size()
            || newQuery.GetOptionals().size() < currentQuery.GetOptionals().size()
            || newQuery.GetIncludeAny().size() < currentQuery.GetIncludeAny().size()
            || newQuery.GetImmediates().size() < currentQuery.GetImmediates().size()
            ))
        {
            return;
        }
    }

    desc.SingleComponentQuery = queryIndex;
}

void EntitySystemHelpersBase::UpdateQueryCache()
{
    if (queryCacheInitialized_) return;

    OPTICK_EVENT();
    auto world = GetEntityWorld();

    BitSet<> persistentQueries;
    BitSet<> aliveQueries;

    for (auto queryIndex : world->Queries.PersistentQueries) {
        persistentQueries.Set((uint32_t)queryIndex);
    }

    for (auto queryIndex : world->Queries.AliveQueries) {
        aliveQueries.Set((uint32_t)queryIndex);
    }

    unsigned queryIndex{ 0 };
    unsigned oneFrameMapped{ 0 }, includeMapped{ 0 }, includeAnyMapped{ 0 }, misbound{ 0 }, reported{ 0 };
    for (auto const& query : world->Queries.Queries) {

        if (query.Flags == QueryFlags::Added
            && aliveQueries[queryIndex]
            && query.GetIncludes().empty()
            && query.GetIncludeAny().empty()
            && query.GetExcludes().empty()
            // && query.GetAddOrs().empty()
            // && query.GetOptionalOneFrames().empty()
            // && query.GetImmediates().empty()
            && query.GetAddAnds().size() == 1
            && query.GetOneFrames().size() == 1) {

            MapSingleComponentQuery((QueryIndex)queryIndex, query.GetOneFrames()[0]);
            oneFrameMapped++;
        }
        
        // Only include "normal" single-component include queries
        if ((unsigned)query.Flags == 0
            && persistentQueries[queryIndex]
            && (
                (query.GetIncludes().size() == 1 && query.GetIncludeAny().empty())
                || (query.GetIncludes().empty() && query.GetIncludeAny().size() == 1)
                )
            && query.GetExcludes().empty()) {

            // PORT (linux): the two branches were the wrong way round upstream -- an empty
            // Includes took GetIncludes()[0] and a one-element Includes took GetIncludeAny()[0].
            // Every section accessor is a span into the one ComponentIndices array delimited by
            // the cumulative section offsets, so indexing the *empty* one does not fault: with
            // Includes empty (IncludeAny == 0) both spans start at ComponentIndices[0] and the
            // old code happened to read the right element, but with Includes of size 1 the old
            // code read ComponentIndices[1] -- the first element of whatever section follows --
            // and bound the query to a component that is not in it at all, leaving the component
            // that *is* in it with no query and a whole-world scan. That is why the client's
            // Ext.Entity.GetAllEntitiesWithComponent('Visual') returned 7 entities out of the
            // 2762 that hold one. Not a Linux difference:
            // the same logic is in upstream as of the vendored tree.
            if (query.GetIncludes().empty()) {
                MapSingleComponentQuery((QueryIndex)queryIndex, query.GetIncludeAny()[0]);
                includeAnyMapped++;
            } else {
                MapSingleComponentQuery((QueryIndex)queryIndex, query.GetIncludes()[0]);
                includeMapped++;

                // The witness for the paragraph above, read off the live query table: what the
                // swapped branch would have bound this query to, and what it actually contains.
                // Counted and named only where the swapped read would have *bound* something:
                // MapSingleComponentQuery drops an index with no ExtComponentType, so a stray
                // that is past the section list or unmapped did no damage beyond leaving this
                // query's own component unbound.
                auto const wanted = query.GetIncludes()[0];
                if (query.WriteEnd > 1 && query.ComponentIndices[1] != wanted
                    && GetComponentType(query.ComponentIndices[1])) {
                    misbound++;
                    if (reported < 40) {
                        auto wantedName = GetComponentName(wanted);
                        auto strayName = GetComponentName(query.ComponentIndices[1]);
                        INFO("UpdateQueryCache(): query %u includes %s; upstream's swapped branch bound it to ComponentIndices[1] = %s instead",
                            queryIndex,
                            wantedName ? wantedName->c_str() : "<unnamed>",
                            strayName ? strayName->c_str() : "<unnamed>");
                        reported++;
                    }
                }
            }
        }

        queryIndex++;
    }

    INFO("UpdateQueryCache(): %u queries, %u single-component mappings (%u Includes, %u IncludeAny, %u one-frame); %u of the Includes ones upstream's swapped branches would have bound to a different named component",
        (unsigned)world->Queries.Queries.size(), oneFrameMapped + includeMapped + includeAnyMapped,
        includeMapped, includeAnyMapped, oneFrameMapped, misbound);

    queryCacheInitialized_ = true;
}

EntityHandle EntitySystemHelpersBase::GetEntityHandle(FixedString const& guidString)
{
    auto guid = Guid::ParseGuidString(guidString.GetStringView());
    if (guid) {
        return GetEntityHandle(*guid);
    } else {
        return {};
    }
}

EntityHandle EntitySystemHelpersBase::GetEntityHandle(Guid const& uuid)
{
    auto entityMap = GetSingleton<UuidToHandleMappingComponent>();
    if (entityMap) {
        auto handle = entityMap->Mappings.try_get(uuid);
        if (handle) {
            return *handle;
        }
    }

    return {};
}

resource::GuidResourceBankBase* EntitySystemHelpersBase::GetRawResourceManager(ExtResourceManagerType type)
{
    auto index = staticDataIndices_[(unsigned)type];
    if (index == resource::UndefinedStaticDataType || (unsigned)type >= std::size(staticDataIndices_)) {
        OsiError("No resource manager index mapping registered for " << type);
        return {};
    }

    auto defns = GetStaticSymbols().eoc__gGuidResourceManager;
    if (!defns || !*defns) {
        OsiError("Resource definition manager not available yet!");
        return {};
    }

    auto res = (*defns)->Definitions.try_get(index);
    if (!res) {
        OsiError("Resource manager missing for " << type);
        return {};
    }

    return *res;
}

bool EntitySystemHelpersBase::SetSystemUpdateHook(SystemTypeIndex systemType, std::function<SystemHookProc> preUpdate, std::function<SystemHookProc> postUpdate)
{
    assert((unsigned)systemType < systemTypeIdToName_.size());

    if ((unsigned)systemType >= systemHooks_.size()) return false;

    auto& hook = systemHooks_[(unsigned)systemType];
    if (hook.OriginalUpdateProc == nullptr) {
        auto& system = GetEntityWorld()->Systems.Systems[(unsigned)systemType];
        if (system.System == nullptr) {
            return false;
        }

        systemToId_.insert(std::make_pair(system.System, systemType));
        hook.OriginalUpdateProc = system.UpdateProc;
        system.UpdateProc = &StaticSystemUpdateHook;
    }

    hook.PreUpdate = preUpdate;
    hook.PostUpdate = postUpdate;
    return true;
}

void EntitySystemHelpersBase::SystemUpdateHook(BaseSystem* system, EntityWorld& world, GameTime const& time)
{
    auto systemId = systemToId_.find(system);
    assert(systemId != systemToId_.end());
    auto& hooks = systemHooks_[(unsigned)systemId->second];
    if (hooks.PreUpdate) {
        hooks.PreUpdate(this, system, time, systemId->second);
    }

    hooks.OriginalUpdateProc(system, world, time);

    if (hooks.PostUpdate) {
        hooks.PostUpdate(this, system, time, systemId->second);
    }
}

void EntitySystemHelpersBase::StaticSystemUpdateHook(BaseSystem* system, EntityWorld& world, GameTime const& time)
{
    auto helpers = (&world == gExtender->GetClient().GetEntityHelpers().GetEntityWorld())
        ? (EntitySystemHelpersBase*)&gExtender->GetClient().GetEntityHelpers()
        : (EntitySystemHelpersBase*)&gExtender->GetServer().GetEntityHelpers();

    helpers->SystemUpdateHook(system, world, time);
}

void EntitySystemHelpersBase::InitSystemUpdateHooks()
{
    systemHooks_.resize(systemTypeIdToName_.size());
}

void EntitySystemHelpersBase::ClearSystemUpdateHooks()
{
    for (unsigned i = 0; i < systemHooks_.size(); i++) {
        if (systemHooks_[i].OriginalUpdateProc != nullptr) {
            auto& system = GetEntityWorld()->Systems.Systems[i];
            system.UpdateProc = systemHooks_[i].OriginalUpdateProc;
        }
    }

    systemToId_.clear();
    systemHooks_.clear();
}


void ServerEntitySystemHelpers::Setup()
{
    UpdateComponentMappings();
}

void ClientEntitySystemHelpers::Setup()
{
    UpdateComponentMappings();
}



#if defined(BG3SE_LINUX)
// PORT (linux): fail-closed binding. ecs::EntityWorld's Linux offsets are measured
// (EntitySystem.h), but a wrong one costs a crash deep in ECS rather than
// a bad value, so the world is checked against itself before it is handed out: four of its
// members are pointed at by the two objects it owns, and those identities hold in both live
// worlds. If any of them fails the world stays unbound and the extender runs without an
// entity world rather than dereferencing into the dark.
static bool ValidateEntityWorld(EntityWorld* world, char const* which)
{
    if (world == nullptr) return false;

    auto storage = world->Storage;
    auto cache = world->Cache;
    if (storage == nullptr || cache == nullptr) {
        ERR("[ECS BIND] %s EntityWorld %p has no Storage/Cache (%p/%p); not binding",
            which, world, storage, cache);
        return false;
    }

    struct Check { char const* what; void const* got; void const* want; };
    Check const checks[] = {
        { "Storage->ComponentRegistry", storage->ComponentRegistry, &world->ComponentRegistry_ },
        { "Storage->Queries", storage->Queries, &world->Queries },
        { "Cache->EntityWorld", cache->EntityWorld, world },
        { "Cache->Callbacks", cache->Callbacks, &world->ComponentCallbacks },
        { "Cache->Allocator", cache->Allocator, &world->ComponentData },
    };

    for (auto const& c : checks) {
        if (c.got != c.want) {
            ERR("[ECS BIND] %s EntityWorld %p: %s is %p, expected %p; not binding",
                which, world, c.what, c.got, c.want);
            return false;
        }
    }

    auto& reg = world->ComponentRegistry_;
    if (reg.Types.size() != reg.Bitmask.Size || reg.Types.size() == 0
        || world->Systems.Systems.size() == 0) {
        ERR("[ECS BIND] %s EntityWorld %p: %d component types, bitmask %d bits, %d systems; not binding",
            which, world, reg.Types.size(), reg.Bitmask.Size, world->Systems.Systems.size());
        return false;
    }

    return true;
}
#endif


EntityWorld* ServerEntitySystemHelpers::GetEngineEntityWorld() const
{
    auto server = GetStaticSymbols().GetEoCServer();
    if (server == nullptr) return nullptr;
    auto world = server->EntityWorld;
#if defined(BG3SE_LINUX)
    if (!ValidateEntityWorld(world, "server")) return nullptr;
#endif
    return world;
}

ExtensionStateBase* ServerEntitySystemHelpers::GetExtensionState() const
{
    return gExtender->GetServer().HasExtensionState() ? &gExtender->GetServer().GetExtensionState() : nullptr;
}

std::optional<EntityHandle> ServerEntitySystemHelpers::NetIdToEntity(NetId netId) const
{
    auto& repl = GetStaticSymbols().GetEoCServer()->GameServer->Replication;
    auto entity = repl.NetIdToEntity.try_get(netId);
    return entity ? *entity : std::optional<EntityHandle>{};
}

std::optional<NetId> ServerEntitySystemHelpers::EntityToNetId(EntityHandle entity) const
{
    auto& repl = GetStaticSymbols().GetEoCServer()->GameServer->Replication;
    auto netId = repl.EntityToNetId.try_get(entity);
    return netId ? *netId : std::optional<NetId>{};
}

EntityWorld* ClientEntitySystemHelpers::GetEngineEntityWorld() const
{
    auto client = GetStaticSymbols().GetEoCClient();
    if (client == nullptr) return nullptr;
    auto world = client->EntityWorld;
#if defined(BG3SE_LINUX)
    if (!ValidateEntityWorld(world, "client")) return nullptr;
#endif
    return world;
}

ExtensionStateBase* ClientEntitySystemHelpers::GetExtensionState() const
{
    return gExtender->GetClient().HasExtensionState() ? &gExtender->GetClient().GetExtensionState() : nullptr;
}

std::optional<EntityHandle> ClientEntitySystemHelpers::NetIdToEntity(NetId netId) const
{
    auto client = GetStaticSymbols().GetEoCClient()->GameClient;
    if (client) {
        auto entity = client->ReplicationPeer.NetIdToEntity.try_get(netId);
        return entity ? *entity : std::optional<EntityHandle>{};
    } else {
        return {};
    }
}

std::optional<NetId> ClientEntitySystemHelpers::EntityToNetId(EntityHandle entity) const
{
    auto client = GetStaticSymbols().GetEoCClient()->GameClient;
    if (client) {
        auto netId = client->ReplicationPeer.EntityToNetId.try_get(entity);
        return netId ? *netId : std::optional<NetId>{};
    } else {
        return {};
    }
}

END_NS()
