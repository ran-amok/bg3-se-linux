#pragma once

BEGIN_NS(inventory)

struct DataComponent : public BaseComponent
{
    DEFINE_COMPONENT(InventoryData, "eoc::inventory::DataComponent")

    [[bg3::legacy(field_0)]] InventoryType Type;
    [[bg3::legacy(Flags)]] uint16_t SlotLimit;
};

struct OwnerComponent : public BaseComponent
{
    DEFINE_COMPONENT(InventoryOwner, "eoc::inventory::OwnerComponent")

    Array<EntityHandle> Inventories;
    EntityHandle PrimaryInventory;
};

struct IsOwnedComponent : public BaseComponent
{
    DEFINE_COMPONENT(InventoryIsOwned, "eoc::inventory::IsOwnedComponent")

    EntityHandle Owner;
};

struct ContainerSlotData
{
    EntityHandle Item;
    uint32_t field_8{ 0 };
};

struct ContainerComponent : public BaseComponent
{
    DEFINE_COMPONENT(InventoryContainer, "eoc::inventory::ContainerComponent")

    HashMap<uint16_t, ContainerSlotData> Items;
};

struct MemberComponent : public BaseComponent
{
    DEFINE_COMPONENT(InventoryMember, "eoc::inventory::MemberComponent")

    EntityHandle Inventory;
    int16_t EquipmentSlot;
};

struct MemberTransformComponent : public BaseComponent
{
    DEFINE_COMPONENT(InventoryMemberTransform, "eoc::inventory::MemberTransformComponent")

    Transform Transform;
};

struct StackEntry
{
    uint16_t EntityIndex{ 0 };
    uint32_t Quantity{ 0 };
};

struct StackComponent : public BaseComponent
{
    DEFINE_COMPONENT(InventoryStack, "eoc::inventory::StackComponent")

    [[bg3::legacy(Arr_u64)]] Array<EntityHandle> Elements;
    [[bg3::legacy(Arr_u8)]] Array<StackEntry> Entries;
};

struct StackMemberComponent : public BaseComponent
{
    DEFINE_COMPONENT(InventoryStackMember, "eoc::inventory::StackMemberComponent")

    EntityHandle Stack;
};

struct TopOwnerComponent : public BaseComponent
{
    DEFINE_COMPONENT(InventoryTopOwner, "eoc::inventory::TopOwnerComponent")

    EntityHandle TopOwner;
};

struct TradeBuybackDataComponent : public BaseComponent
{
    DEFINE_COMPONENT(TradeBuybackData, "eoc::inventory::TradeBuybackDataComponent")

    uint16_t Amount;
    EntityHandle Trader;
    EntityHandle Buyer;
};

struct WeightComponent : public BaseComponent
{
    DEFINE_COMPONENT(InventoryWeight, "eoc::inventory::WeightComponent")

    int Weight;
};

struct WieldedComponent : public BaseComponent
{
    DEFINE_COMPONENT(Wielded, "eoc::inventory::WieldedComponent")

    Guid field_0;
};

struct WieldingHistoryComponent : public BaseComponent
{
    DEFINE_COMPONENT(WieldingHistory, "eoc::inventory::WieldingHistoryComponent")

    Guid field_0;
};


DEFINE_TAG_COMPONENT(eoc::inventory, CanBeInComponent, CanBeInInventory)
DEFINE_TAG_COMPONENT(eoc::inventory, CannotBePickpocketedComponent, CannotBePickpocketed)
DEFINE_TAG_COMPONENT(eoc::inventory, CannotBeTakenOutComponent, CannotBeTakenOut)
DEFINE_TAG_COMPONENT(eoc::inventory, DropOnDeathBlockedComponent, DropOnDeathBlocked)
DEFINE_TAG_COMPONENT(eoc::inventory, IsLockedComponent, InventoryLocked)
DEFINE_TAG_COMPONENT(eoc::inventory, NewItemsInsideComponent, NewItemsInside)
DEFINE_TAG_COMPONENT(eoc::inventory, NonTradableComponent, NonTradable)

END_NS()

BEGIN_NS(esv::inventory)

struct ContainerDataComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerInventoryContainerData, "esv::inventory::ContainerDataComponent")

    uint16_t Flags;
    int field_4;
};

struct GroupCheckComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerInventoryGroupCheck, "esv::inventory::GroupCheckComponent")

    uint32_t Conditions;
};

struct ShapeshiftEquipmentHistoryComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerShapeshiftEquipmentHistory, "esv::inventory::ShapeshiftEquipmentHistoryComponent")

    Array<Guid> History;
};

DEFINE_TAG_COMPONENT(esv::inventory, CharacterHasGeneratedTradeTreasureComponent, CharacterHasGeneratedTradeTreasure)
DEFINE_TAG_COMPONENT(esv::inventory, IsReplicatedWithComponent, ServerInventoryIsReplicatedWith)
DEFINE_TAG_COMPONENT(esv::inventory, ReadyToBeAddedToInventoryComponent, ReadyToBeAddedToInventory)
DEFINE_TAG_COMPONENT(esv::inventory, EntityHasGeneratedTreasureComponent, HasGeneratedTreasure)

struct CanPlaceSystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerInventoryCanPlace, "esv::inventory::CanPlaceSystem")

    [[bg3::hidden]] void* FlagEventManagerVMT;
    [[bg3::hidden]] void* CanPlaceSystemHelper;
    [[bg3::hidden]] void* GlobalTemplateManager;
    [[bg3::hidden]] void* StatusPrototypeManager;
    [[bg3::hidden]] void* CacheTemplateManager;
    [[bg3::hidden]] void* LevelManager;
    HashSet<EntityHandle> ReevaluateTemplateChange;
    HashSet<EntityHandle> FlagsChanged;
};

struct EntityReceivalNotificationSystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerInventoryReceivalNotification, "esv::inventory::EntityReceivalNotificationSystem")

    HashMap<EntityHandle, EntityHandle> Notify;
};

struct EquipSettings
{
    bool IgnoreCombat{ false };
    bool IgnoreLocked{ false };
    bool OnlyMainHand{ false };
    bool AllowShapeshifted{ false };
    bool field_4{ false };
    bool BlockUnequip{ false };
    bool CanSwapHands{ false };
};


struct EquipRequest
{
    EntityHandle Item;
    bool CheckActionResourceCosts{ false };
    ItemSlot Slot;
    bool field_A{ false };
    bool EvaluateConditions{ false };
    bool DiscardOnEquipFail{ false };
    EquipSettings Settings;
};

struct DropMovement : public path::Settings
{
    glm::vec3 Position;
    glm::quat Rotation;
    bool field_6C;
    bool IsInInventory;
    EntityHandle Target;
    bool field_78;
    bool field_79;
    bool field_7A;
    STDString Event;
};

struct UnequipRequest
{
    EntityHandle Item;
    std::optional<EntityHandle> Inventory;
    std::optional<int16_t> Slot;
    bool TransferToOwner{ false };
    bool IsPickupAction{ false };
    UnequipType Type{ UnequipType::ToParentInventory };
    std::optional<EntityHandle> InventoryOwner;
    std::optional<glm::vec3> Position;
    std::optional<FixedString> Level;
    std::optional<DropMovement> DropMovement;
    Guid ShapeshiftState;
    EquipSettings Settings;
    bool field_FF{ false };
};

struct EquipmentRequest
{
    EntityHandle InventoryContainer;
    std::variant<EquipRequest, UnequipRequest> Action;
};

struct EquipmentSetRequest
{
    Array<FixedString> Equipment;
    bool RemoveOldEquipmentSets{ false };
    Guid ShapeshiftState;
};


struct EquipmentSystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerInventoryEquipment, "esv::inventory::EquipmentSystem")

    Array<EquipmentRequest> Equipment;
    HashMap<EntityHandle, EquipmentSetRequest> EquipmentSet;
    [[bg3::hidden]] void* ActionResourceTypes;
    [[bg3::hidden]] void* ActionResourceSystem;
    [[bg3::hidden]] void* RPGStats;
    [[bg3::hidden]] void* ThothMachine;
    [[bg3::hidden]] void* EocServer;
    [[bg3::hidden]] void* GlobalTemplateManager;
    [[bg3::hidden]] void* CacheTemplateManager;
    [[bg3::hidden]] void* LevelManager;
    [[bg3::hidden]] void* EquipmentUnitTestHelper;
};

struct ContainerSortData
{
    uint8_t SortType{ 0 };
};

struct ContainerScatterData
{
    bool ToParent{ false };
    bool MainInventory{ true };
    bool OnlyDropOnDeathItems{ false };
};

struct ContainerInteractionRequestData
{
    EntityHandle Inventory;
    std::variant<ContainerSortData, ContainerScatterData> Action;
};

struct MemberAddData
{
    EntityHandle Item;
    std::optional<int16_t> Slot;
    bool AddToOwner{ false };
    uint8_t AddType;
    bool Grouping{ false };
    bool UseTopOwner{ false };
    bool field_10{ false };
    bool field_11{ false };
};

struct MemberMoveToSlotData
{
    int16_t SourceSlot{ -1 };
    int16_t TargetSlot{ -1 };
};

struct MemberTransferData
{
    EntityHandle TargetInventory;
    int16_t SourceSlot{ -1 };
    std::optional<int16_t> TargetSlot;
    bool TransferToOwner{ false };
    bool Trade{ false };
    bool Grouping{ false };
    bool IsPickupAction{ false };
    bool field_12{ false };
    bool field_13{ false };
    EntityHandle field_18;
};

struct MemberDropData
{
    int16_t Slot{ -1 };
    std::optional<glm::vec3> Position;
    std::optional<FixedString> Level;
    std::optional<DropMovement> DropMovement;
    EntityHandle InventoryOwner;
};

struct MemberEquipmentRequestFailedData
{
    EntityHandle InventoryOwner;
    uint8_t field_8{ 0 };
};

struct MemberRemoveData
{
    int16_t Slot{ -1 };
};

struct MemberInteractionRequestData
{
    EntityHandle Inventory;
    int Amount{ 1 };
    std::variant<MemberAddData, MemberMoveToSlotData, MemberTransferData, MemberDropData, MemberEquipmentRequestFailedData, MemberRemoveData> Action;
};

struct NotifyInteractionRequestData
{
    EntityHandle Container;
    EntityHandle Actor;
};

struct InteractionRequestSystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerInventoryInteractionRequest, "esv::inventory::InteractionRequestSystem")

    Array<ContainerInteractionRequestData> ContainerInteractions;
    Array<MemberInteractionRequestData> MemberInteraction;
    Array<NotifyInteractionRequestData> NotifyInteraction;
    // Editor only?
    // HashMap<EntityHandle, uint8_t> DeferredRequests;
#if defined(BG3SE_LINUX)
// PORT (linux): the map upstream comments out is in the native build. Its factory, bin/bg3
// 0x4cfb320, is operator new(0x90): vptr at 0, [+8] = 0, zeroes 0x10..0x80, then stores its two
// arguments at 0x80 and 0x88. The D1 (0x71f8970) deletes [+0x88] through its vtable, runs a
// map's dtor on this+0x40 and the three Arrays' on 0x30/0x20/0x10. The macOS oracle (build
// af3f1c71: operator new(0x90) at 0x104449d61, D2 0x10606c8e0, ~HashTable<EntityHandle> on
// this+0x40, Values freed at this+0x70) agrees. The value type has not been read; a HashMap is
// 0x40 bytes whatever it is, so the member stays hidden.
    [[bg3::hidden]] HashMap<EntityHandle, uint8_t> DeferredRequests;
#endif
    [[bg3::hidden]] void* qword80;
    [[bg3::hidden]] void* qword88;
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(InteractionRequestSystem, NotifyInteraction) == 0x30,
    "bin/bg3 D1 0x71f89a4 destroys the Array at this+0x30");
static_assert(offsetof(InteractionRequestSystem, DeferredRequests) == 0x40,
    "bin/bg3 D1 0x71f8990 destroys the map at this+0x40");
static_assert(offsetof(InteractionRequestSystem, qword80) == 0x80,
    "bin/bg3 factory 0x4cfb370 stores its second argument at this+0x80");
static_assert(offsetof(InteractionRequestSystem, qword88) == 0x88,
    "bin/bg3 D1 0x71f897e deletes the pointer at this+0x88");
static_assert(sizeof(InteractionRequestSystem) == 0x90,
    "operator new(0x90) in the bin/bg3 factory 0x4cfb328");
#endif

struct MemberData
{
    EntityHandle Inventory;
    int16_t InventorySlot{ -1 };
};

struct InteractionSystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerInventoryInteraction, "esv::inventory::InteractionSystem")

    [[bg3::hidden]] UnknownFunction qword10;
    HashMap<EntityHandle, Array<EntityHandle>> DestroyedEntities;
    HashMap<EntityHandle, EntityHandle> field_90;
    HashMap<EntityHandle, MemberData> DestroyedMembers;
    Array<EntityHandle> field_110;
    HashMap<EntityHandle, FixedString> LevelOverride;
    HashMap<EntityHandle, glm::vec3> PositionOverride;
    HashSet<EntityHandle> BrokenEntities;
    [[bg3::hidden]] void* InteractionSystemHelper;
    [[bg3::hidden]] void* EocServer;
    [[bg3::hidden]] void* RPGStats;
    [[bg3::hidden]] void* ClassDescriptions;
    [[bg3::hidden]] void* SpellPrototypeManager;
#if defined(BG3SE_LINUX)
// PORT (linux): one pointer more than upstream at the end. The bin/bg3 factory (operator
// new(0x200) at 0x514a887) stores six pointers at 0x1d0..0x1f8, the fifth (0x1f0) being
// eoc::SpellPrototypeManager::m_ptr (0x7bb6a50, matched s1:0.83) and the sixth a member of
// one of its arguments ([arg+0x68]). Its D1 (0x71f9b20) deletes [+0x1d0] and destroys every
// container where the mirror puts it, BrokenEntities' three arrays at 0x1a0/0x1b0/0x1c0.
// The macOS oracle (af3f1c71: new(0x200) at 0x10444a8ef, the same six stores) agrees.
    [[bg3::hidden]] void* field_1F8;
#endif
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(InteractionSystem, DestroyedEntities) == 0x50,
    "bin/bg3 D1 0x71f9bab destroys the map at this+0x50");
static_assert(offsetof(InteractionSystem, field_110) == 0x110,
    "bin/bg3 D1 0x71f9b87 destroys the Array at this+0x110");
static_assert(offsetof(InteractionSystem, BrokenEntities) == 0x1A0,
    "bin/bg3 D1 0x71f9b63 frees the set's HashKeys at this+0x1a0");
static_assert(offsetof(InteractionSystem, InteractionSystemHelper) == 0x1D0,
    "bin/bg3 D1 0x71f9b2e deletes the pointer at this+0x1d0");
static_assert(offsetof(InteractionSystem, SpellPrototypeManager) == 0x1F0,
    "bin/bg3 factory 0x514a947 stores eoc::SpellPrototypeManager::m_ptr at this+0x1f0");
static_assert(sizeof(InteractionSystem) == 0x200,
    "operator new(0x200) in the bin/bg3 factory 0x514a887");
#endif

struct LockingSystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerInventoryLocking, "esv::inventory::LockingSystem")

    HashMap<EntityHandle, InventoryLockChange> Change;
};

struct MagicPocketsTrackingSystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerMagicPocketsTracking, "esv::inventory::MagicPocketsTrackingSystem")

    Array<EntityHandle> RequestTrack;
};

struct CreateRequestData
{
    EntityHandle Owner;
    InventoryType Type{ InventoryType::Main };
    int16_t SlotLimit{ -1 };
    // Editor only?
    // bool WarnIfExists{ false };
};

struct ManagementSystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerInventoryManagement, "esv::inventory::ManagementSystem")

    Array<CreateRequestData> CreateInventory;
    Array<EntityHandle> DestroyInventory;
    [[bg3::hidden]] void* GlobalTemplateManager;
    [[bg3::hidden]] void* CacheTemplateManager;
    [[bg3::hidden]] void* LevelManager;
};

struct NewInventoryMemberSystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerNewInventoryMember, "esv::inventory::NewInventoryMemberSystem")

    HashSet<EntityHandle> NewMembers;
};

struct DirectStackRequest
{
    EntityHandle Item;
    Guid field_8;
    int Amount{ 1 };
    bool Split{ true };
    bool UpdateEntity{ false };
};

struct StackSystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerInventoryStack, "esv::inventory::StackSystem")

    Array<DirectStackRequest> Requests;
    [[bg3::hidden]] void* StackSystemHelper;
    [[bg3::hidden]] void* EntityManager;
    [[bg3::hidden]] void* StatusPrototypeManager;
};

struct SetBuybackPriceRequest
{
    EntityHandle Item;
    int16_t Price{ 1 };
};

struct TradeBuybackSystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerTradeBuyback, "esv::inventory::TradeBuybackSystem")

    Array<SetBuybackPriceRequest> SetBuybackPrice;
};

struct GenerateTreasureRequestData
{
    EntityHandle InventoryHolder;
    EntityHandle Finder;
    Array<FixedString> Treasure;
    int32_t Level{ 1 };
    bool IsTradeTreasure{ false };
    bool AllowMultipleGenerations{ false };
};

struct TreasureGenerationSystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerTreasureGeneration, "esv::inventory::TreasureGenerationSystem")

    [[bg3::hidden]] void* TreasureGenerationUnitTestHelper;
    [[bg3::hidden]] void* EocServer;
    [[bg3::hidden]] void* RPGStats;
    [[bg3::hidden]] void* TagManager;
    Array<GenerateTreasureRequestData> AnubisGenerate;
    Array<GenerateTreasureRequestData> OsirisGenerate;
    Array<GenerateTreasureRequestData> CommandGenerate;
};

END_NS()


BEGIN_NS(esv)

struct InventoryPropertyCanBePickpocketedComponent : public BaseComponent
{
    DEFINE_COMPONENT(InventoryPropertyCanBePickpocketed, "esv::InventoryPropertyCanBePickpocketedComponent")

    GenericPropertyTag Tag;
};

struct InventoryPropertyIsDroppedOnDeathComponent : public BaseComponent
{
    DEFINE_COMPONENT(InventoryPropertyIsDroppedOnDeath, "esv::InventoryPropertyIsDroppedOnDeathComponent")

    GenericPropertyTag Tag;
};

struct InventoryPropertyIsTradableComponent : public BaseComponent
{
    DEFINE_COMPONENT(InventoryPropertyIsTradable, "esv::InventoryPropertyIsTradableComponent")

    GenericPropertyTag Tag;
};

DEFINE_TAG_COMPONENT(esv, ScriptPropertyCanBePickpocketedComponent, ScriptPropertyCanBePickpocketed)
DEFINE_TAG_COMPONENT(esv, ScriptPropertyIsDroppedOnDeathComponent, ScriptPropertyIsDroppedOnDeath)
DEFINE_TAG_COMPONENT(esv, ScriptPropertyIsTradableComponent, ScriptPropertyIsTradable)

END_NS()
