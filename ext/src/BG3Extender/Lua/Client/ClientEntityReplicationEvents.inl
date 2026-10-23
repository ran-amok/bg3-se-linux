#include <Lua/Shared/LuaMethodCallHelpers.h>
#include <Lua/Client/ClientEntityReplicationEvents.h>

BEGIN_NS(ecl::lua)

using namespace bg3se::lua;

DummyFieldTracker::~DummyFieldTracker() {}

// PORT (linux): upstream's Add/FireEvents/Remove keep their slots but not their meanings; the
// game's interface is Track / Dispatch(entity) / Dispatch(), and the per-frame flush that has to
// fire the Lua handlers is the last of the three (witnesses in
// ecs::FieldTracker). Written with upstream's names it recorded changes into the shared set and
// then, once a frame, ran upstream's Remove() -- a HashMap::remove on an uninitialised register --
// instead of firing anything, which is why a client Ext.Entity.OnChange subscription was accepted
// and never called back.
void DummyFieldTracker::Track(EntityHandle const& entity, void* component)
{
    se_assert(Changes.empty());

    if (Base) {
        se_assert(ChangeEvent.Connections.size() == Base->ChangeEvent.Connections.size());
        Base->Track(entity, component);
    }

    auto change = SharedChanges->ChangedEntities.try_get(entity);
    if (!change) {
        SharedChanges->ChangedEntities.set(entity, SharedChange{FieldMask, component});
    } else if (!(change->FieldMask & FieldMask)) {
        change->FieldMask |= FieldMask;
    }
}

void DummyFieldTracker::Dispatch(EntityHandle const& entity)
{
    se_assert(Changes.empty());

    if (Base) {
        se_assert(ChangeEvent.Connections.size() == Base->ChangeEvent.Connections.size());
        Base->Dispatch(entity);
    }

    auto change = SharedChanges->ChangedEntities.try_get(entity);
    if (change) {
        PerComponentChangeEvent.Invoke(entity, change->Component, change->FieldMask);
        SharedChanges->ChangedEntities.remove(entity);
    }
}

void DummyFieldTracker::Dispatch()
{
    se_assert(Changes.empty());

    if (Base) {
        se_assert(ChangeEvent.Connections.size() == Base->ChangeEvent.Connections.size());
        Base->Dispatch();
    }

    for (auto& it : SharedChanges->ChangedEntities) {
        PerComponentChangeEvent.Invoke(it.Key(), it.Value().Component, it.Value().FieldMask);
    }

    SharedChanges->ChangedEntities.clear();
}

ClientEntityReplicationEventHooks::ClientEntityReplicationEventHooks(State& state)
    : state_(state)
{}

ClientEntityReplicationEventHooks::~ClientEntityReplicationEventHooks()
{
    for (uint32_t i = 0; i < hookedReplicationComponentMask_.Size; i++) {
        RemoveComponentType((ecs::ReplicationTypeIndex)i);
    }
}

EntityReplicationEventHooks::ReplicationHooks* ClientEntityReplicationEventHooks::AddComponentType(ecs::ReplicationTypeIndex type)
{
    auto index = (unsigned)type;
    if (!hookedReplicationComponentMask_[index]) {
        auto& peer = GetStaticSymbols().GetEoCClient()->GameClient->ReplicationPeer;
#if defined(BG3SE_LINUX)
        // PORT (linux): fail closed. try_get only asserts its bound, and a release build reads on
        // past it; a wrong peer mirror would crash the game here.
        if ((uint16_t)type >= peer.Deserializers.size()) {
            ERR("Replication type %u is past the deserializer table (%u), cannot hook",
                (unsigned)type, (unsigned)peer.Deserializers.size());
            return nullptr;
        }
#endif
        auto deserializer = peer.Deserializers.try_get((uint16_t)type);
        if (!deserializer) {
            auto name = state_.GetEntitySystemHelpers()->GetComponentName(type);
            ERR("Replication component '%s' has no deserializer, cannot hook", name->c_str());
#if defined(BG3SE_LINUX)
            // PORT (linux): a client mod's OnChange('ShapeshiftReplicatedChanges') can be refused
            // here at client bootstrap although bin/bg3 registers a peer deserializer for that
            // type (registrant 0x4382b00, callback 0x4afb0a0, index
            // TypeId<T, ReplicatedTypeContext> 0x7cfdde8). Say what the table holds so the log
            // decides between "not yet inserted" and "inserted at another index".
            auto const& mask = peer.Deserializers.mask();
            auto ix = (uint16_t)type;
            auto word = ix < mask.Size ? mask.GetBuf()[ix / 64] : 0ull;
            ERR("  replication index %u: mask size %u cap %u word 0x%016llx; values size %u cap %u; peer %p",
                (unsigned)ix, mask.Size, mask.Capacity, (unsigned long long)word,
                (unsigned)peer.Deserializers.size(), (unsigned)peer.Deserializers.capacity(), (void*)&peer);
#endif
            return nullptr;
        }

        auto& trackers = (*deserializer)->FieldTrackers;
        auto sharedState = GameAlloc<DummyFieldTracker::SharedChangeSet>();

        if (trackers.empty()) {
            for (uint32_t i = 0; i < 32; i++) {
                auto tracker = GameAlloc<DummyFieldTracker>();
                tracker->SharedChanges = sharedState;
                tracker->FieldMask = (1ull << i);
                tracker->PerComponentChangeEvent.Add(MakeFunction(&ClientEntityReplicationEventHooks::OnEntityReplication, this, type));
                trackers.push_back(tracker);
            }
        } else {
            // Replace original trackers with a dummy; we need these to be able to 
            // 'read' entity handles from FieldChangeData without handling different field sizes
            for (uint32_t i = 0; i < trackers.size(); i++) {
                auto base = trackers.try_get(i);
                if (!sharedState) {
                    sharedState = GameAlloc<DummyFieldTracker::SharedChangeSet>();
                }

                auto tracker = GameAlloc<DummyFieldTracker>();
                tracker->SharedChanges = sharedState;
                tracker->Base = base ? *base : nullptr;
                if (base) {
                    for (auto const& conn : (*base)->ChangeEvent.Connections) {
                        tracker->ChangeEvent.Add(conn.Handler);
                    }
                }
                tracker->FieldMask = (1ull << i);
                tracker->PerComponentChangeEvent.Add(MakeFunction(&ClientEntityReplicationEventHooks::OnEntityReplication, this, type));
                trackers.set(i, tracker);
            }
        }

        hookedReplicationComponentMask_.Set(index);
        while (hookedReplicationComponents_.size() <= index) {
            hookedReplicationComponents_.push_back(ReplicationHooks{});
        }
    }

    return &hookedReplicationComponents_[index];
}

void ClientEntityReplicationEventHooks::RemoveComponentType(ecs::ReplicationTypeIndex type)
{
    auto index = (unsigned)type;
    if (!hookedReplicationComponentMask_[index]) return;

    auto& peer = GetStaticSymbols().GetEoCClient()->GameClient->ReplicationPeer;
    auto& trackers = peer.Deserializers[(uint16_t)type]->FieldTrackers;

    // Restore original trackers
    for (uint32_t i = 0; i < trackers.size(); i++) {
        auto tracker = trackers.try_get(i);
        if (tracker && *tracker) {
            auto base = static_cast<DummyFieldTracker*>(*tracker)->Base;
            GameDelete(*tracker);
            if (base) {
                trackers.set(i, base);
            } else {
                trackers.clear(i);
            }
        }
    }

    hookedReplicationComponents_[index] = ReplicationHooks{};
    hookedReplicationComponentMask_.Clear(index);
}

std::optional<EntityReplicationEventHooks::SubscriptionIndex> ClientEntityReplicationEventHooks::Subscribe(ecs::ReplicationTypeIndex type, EntityHandle entity, uint64_t fields, RegistryEntry&& hook)
{
    auto pool = AddComponentType(type);
    if (!pool) return {};

    SubscriptionIndex index;
    auto sub = subscriptions_.Add(index);
    sub->Fields = fields;
    sub->Hook = std::move(hook);
    sub->Type = type;
    sub->Entity = entity;

    pool->Fields |= fields;
    if (!entity) {
        pool->GlobalHooks.push_back(index);
    } else {
        auto entityHooks = pool->EntityHooks.try_get(entity);
        if (entityHooks) {
            entityHooks->push_back(index);
        } else {
            entityHooks = pool->EntityHooks.add_key(entity);
            entityHooks->push_back(index);
        }
    }

    return index;
}

bool ClientEntityReplicationEventHooks::Unsubscribe(SubscriptionIndex index)
{
    auto sub = subscriptions_.Find(index);
    if (sub == nullptr) {
        return false;
    }

    auto& pool = hookedReplicationComponents_[(unsigned)sub->Type];
    if (!sub->Entity) {
        for (unsigned i = 0; i < pool.GlobalHooks.size(); i++) {
            if (pool.GlobalHooks[i] == index) {
                pool.GlobalHooks.ordered_remove_at(i);
                break;
            }
        }
    } else {
        auto hookMap = pool.EntityHooks.find(sub->Entity);
        if (hookMap) {
            auto& hooks = hookMap.Value();
            for (unsigned i = 0; i < hooks.size(); i++) {
                if (hooks[i] == index) {
                    hooks.ordered_remove_at(i);
                    break;
                }
            }

            if (hooks.empty()) {
                pool.EntityHooks.remove(hookMap);
            }
        }
    }

    if (pool.GlobalHooks.empty() && pool.EntityHooks.empty()) {
        RemoveComponentType(sub->Type);
    }

    subscriptions_.Free(index);
    return true;
}

void ClientEntityReplicationEventHooks::PostUpdate()
{
    if (!pendingEvents_.empty()) {
        ecl::LuaClientPin lua(gExtender->GetClient().GetExtensionState());
        if (lua) {
            for (auto const& e : pendingEvents_) {
                auto sub = subscriptions_.Find(e.Index);
                if (sub) {
                    CallHandler(e.Entity, e.Fields, e.Type, *sub);
                }
            }
        }
    }

    pendingReplications_.clear();
    pendingEvents_.clear();
}

void ClientEntityReplicationEventHooks::OnEntityReplication(ecs::ReplicationTypeIndex const& type, EntityHandle entity, void* component, uint64_t fields)
{
    auto& hooks = hookedReplicationComponents_[(unsigned)type];
    if ((hooks.Fields & fields) == 0) return;

    ComponentRef ref{ entity, type };
    if (pendingReplications_.contains(ref)) return;

    pendingReplications_.insert(ref);

    for (auto index : hooks.GlobalHooks) {
        auto hook = subscriptions_.Find(index);
        if (hook != nullptr && (hook->Fields & fields) != 0) {
            pendingEvents_.push_back({
                .Entity = entity,
                .Fields = fields,
                .Type = type,
                .Index = index
            });
        }
    }

    auto entityHooks = hooks.EntityHooks.try_get(entity);
    if (entityHooks) {
        for (auto index : *entityHooks) {
            auto hook = subscriptions_.Find(index);
            if (hook != nullptr && (hook->Fields & fields) != 0) {
                pendingEvents_.push_back({
                    .Entity = entity,
                    .Fields = fields,
                    .Type = type,
                    .Index = index
                });
            }
        }
    }
}

void ClientEntityReplicationEventHooks::CallHandler(EntityHandle entity, uint64_t fields, ecs::ReplicationTypeIndex type, ReplicationHook const& hook)
{
    auto L = state_.GetState();
    auto componentType = state_.GetEntitySystemHelpers()->GetComponentType(type);
    hook.Hook.Push(L);
    Ref func(L, lua_absindex(L, -1));

    ProtectedFunctionCaller<std::tuple<EntityHandle, ExtComponentType, uint64_t>, void> caller{ func, std::tuple(entity, *componentType, fields) };
    caller.Call(L, "Entity replication event dispatch");
    lua_pop(L, 1);
}

END_NS()
