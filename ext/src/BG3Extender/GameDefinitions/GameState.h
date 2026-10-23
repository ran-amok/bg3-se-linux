#pragma once

#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/Module.h>
#include <GameDefinitions/EntitySystem.h>

BEGIN_SE()

struct IGameObject : public ProtectedGameObject<IGameObject>
{
    virtual ~IGameObject() = 0;
    virtual void HandleTextKeyEvent() = 0;
    virtual uint64_t Ret5() = 0;
    virtual void SetHandle(EntityHandle Handle) = 0;
    virtual void GetHandle(EntityHandle& Handle) const = 0;
    virtual void SetGuid(FixedString const& fs) = 0;
    virtual FixedString* GetGuid() const = 0;
    virtual void SetNetId(NetId netId) = 0;
    virtual void GetNetId(NetId& netId) const = 0;
    virtual void SetCurrentTemplate(void* esvTemplate) = 0;
    virtual void* GetCurrentTemplate() const = 0;
    virtual void SetGlobal(bool isGlobal) = 0;
    virtual bool IsGlobal() const = 0;
    virtual uint32_t GetComponentType() = 0;
    virtual void* GetEntityByHandle(EntityHandle handle) = 0;
    virtual void* GetUnknownHandle() = 0;
    virtual STDString* GetName() = 0;
    virtual void SetFlags(uint64_t flag) = 0;
    virtual void ClearFlags(uint64_t flag) = 0;
    virtual bool HasFlag(uint64_t flag) const = 0;
    virtual void SetAiColliding_M(bool colliding) = 0;
    virtual void Unknown1() = 0;
    virtual void Unknown2() = 0;
    virtual void Unknown3() = 0;
    virtual void Unknown4() = 0;
    virtual void Unknown5() = 0;
    /*virtual void GetTags(ObjectSet<FixedString> & tags) = 0;
    virtual bool IsTagged(FixedString & tag) = 0;*/
    virtual Vector3 const* GetTranslate() const = 0;
    virtual glm::mat3 const* GetRotation() const = 0;
    virtual float GetScale() const = 0;
    virtual void SetTranslate(Vector3 const& translate) = 0;
    virtual void SetRotation(glm::mat3 const& rotate) = 0;
    virtual void SetScale(float scale) = 0;
    virtual float GetHeight_M() = 0;
    virtual FixedString& GetParentUUID_M() = 0;
    virtual FixedString& GetCurrentLevel() = 0;
    virtual void SetCurrentLevel(FixedString const& fs) = 0;

    void* VMT2;
};

struct IEoCServerObject : public IGameObject
{
    virtual void MarkVisibleToPeerId(uint32_t* peerId) = 0;
    virtual void UnmarkVisibleToPeerId(uint32_t* peerId) = 0;
    virtual void ClearAllVisiblePeerIds() = 0;
    virtual void IsVisibleToPeer(uint32_t* peerId) = 0;
    virtual void* GetBoundComponent() = 0;
    virtual void LoadAi_M() = 0;
    virtual bool UnloadAi_M() = 0;
    virtual TranslatedString* GetDisplayName(TranslatedString& name) = 0;
    virtual void SetOriginalTemplate(void* esvTemplate) = 0;
    virtual bool SavegameVisit(void* visitor) = 0;
    virtual NetId& GetEntityNetworkId(NetId& netId) const = 0;
    virtual void SetTemplate(void* esvTemplate) = 0;
    virtual void SetTemplateId(uint8_t templateType, FixedString const& templateId) = 0;
    virtual void* GetSomeTransformValue() = 0;
    virtual void SetSomeTransformValue(void* unknown) = 0;

    BaseComponent Base;
};

struct GameStateEventManager
{
    void* VMT;
    Array<void*> Callbacks;
};

#if defined(BG3SE_LINUX)
// PORT (linux): read off esv::GameStateMachine::Update (bin/bg3 0x2c66a10): after the
// "SERVER STATE SWAP" log it loads esv::GameStateEventManager::m_ptr and
// walks {buf +8, cap +0x10, size +0x14}, calling vtable slot 2 of each element.
static_assert(offsetof(GameStateEventManager, Callbacks) == 8, "the listener array follows the VMT");
static_assert(sizeof(Array<void*>) == 0x10, "ls::Array is {T* buf; u32 cap; u32 size}");
#endif

struct App
{
};

END_SE()

BEGIN_NS(esv)

struct EntityManager;

struct GameStateMachine : public ProtectedGameObject<GameStateMachine>
{
    uint8_t Unknown;
    void * CurrentState;
    GameState State;
    void ** TargetStates;
    uint32_t TargetStateBufSize;
    uint32_t NumTargetStates;
    uint32_t ReadStateIdx;
    uint32_t WriteStateIdx;
};

#if defined(BG3SE_LINUX)
// PORT (linux): every field here is read by esv::GameStateMachine::Update (bin/bg3 0x2c66a10)
// at upstream's offsets -- the one Linux function the extender hooks this mirror for
// (HookStateMachineUpdates).
static_assert(offsetof(GameStateMachine, CurrentState) == 0x08);
static_assert(offsetof(GameStateMachine, State) == 0x10);
static_assert(offsetof(GameStateMachine, TargetStates) == 0x18);
static_assert(offsetof(GameStateMachine, TargetStateBufSize) == 0x20);
static_assert(offsetof(GameStateMachine, NumTargetStates) == 0x24);
static_assert(offsetof(GameStateMachine, ReadStateIdx) == 0x28);
static_assert(offsetof(GameStateMachine, WriteStateIdx) == 0x2c);
#endif

struct EoCServer : public ProtectedGameObject<EoCServer>
{
    char field_0;
    __int64 field_8;
    __int64 field_10;
    __int64 field_18;
    __int64 field_20;
    __int64 field_28;
    __int64 field_30;
    __int64 field_38;
    __int64 field_40;
    __int64 field_48;
    __int64 field_50;
    __int64 field_58;
    __int64 field_60;
    __int64 field_68;
    ScratchString field_70;
    FixedString field_90;
    FixedString field_94;
    FixedString field_98;
    FixedString field_9C;
    GameStateMachine* GameStateMachine;
    net::GameServer* GameServer;
    __int64 field_B0;
    int Rand1;
    int Rand2;
    __int64 GlobalRandom;
    __int64 ConditionManager;
    ModManager ModManager;
    char field_280;
    ecs::EntityWorld* EntityWorld;
    __int64 field_290;
    __int64 LobbyManager;
    char field_2A0;
    __int64 field_2A8;
    __int64 field_2B0;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the Linux build reads [EocServer + 0x288] at 2701 of the 3205 sites that load
// esv::EocServer::m_ptr (a census of [reg+disp] over every load site) -- upstream's offset.
// ecs::EntityWorld's own layout is measured in EntitySystem.h and the world is
// bound; ValidateEntityWorld() re-checks the layout on the live object before it is handed out.
static_assert(offsetof(EoCServer, EntityWorld) == 0x288, "EoCServer::EntityWorld is at +0x288 on Linux");
// The server ModManager is embedded, and the game says where: bin/bg3 0x6ff2b70 is
// `esi = 0xD0; rsi += [esv::EocServer::m_ptr]; jmp eoc::DialogPathResolver::Init(
// ls::ModManager*) 0x2d0efb0` -- an argument of that type built from the offset.
// Ext.Mod reads it.
static_assert(offsetof(EoCServer, ModManager) == 0xD0, "esv::EoCServer::ModManager is at +0xD0 (bin/bg3 0x6ff2b70)");
#endif

END_NS()


BEGIN_NS(ecl)

struct GameStateMachine : public ProtectedGameObject<GameStateMachine>
{
    void* CurrentState;
    GameState State;
};
        
struct EoCClient : public ProtectedGameObject<EoCClient>
{
    void* VMT_InputEventListener;
    void* VMT_NetEventListener;
    void* field_10;
    void* VMT_DLCEventListener;
    void* VMT_MessageBoxOwner;
    UnknownSignal ComponentCallbacks;
    UnknownSignal SetControllerModeCallbacks;
    UnknownSignal IncrementAchievementCallbacks;
#if defined(BG3SE_LINUX)
    // PORT (linux): read live off ecl::EocClient::m_ptr (peek.py): the
    // first three signals are {ptr, 1|1, 1} at 0x18 each, the fourth has no third qword, and
    // +0x80 is already a pointer. So Eoc is +0x80 and the state machine +0x88 -- its shape is
    // the server machine's without the leading byte, {ptr CurrentState; u32 State; ptr
    // TargetStates; u32 8; u32 -1}, and State read 7 (Menu) at the main menu and 18 in a
    // loaded save. The object at +0x90, with three vtables and the string 127.0.0.1, *is* the
    // GameClient: net::AbstractPeer
    // carries three vtables and a peer host string, the object at +0x90 holds the replication
    // peer at +0x490 whose Client member points back at +0x90 itself, and its deserializer
    // table has 582 entries, the registrant census of bin/bg3. +0x98, which upstream's mirror
    // has as GameClient, is an object
    // with one vtable and heap pointers behind it, and stays unnamed. The census agrees:
    // 244 of 2365 reads off ecl::EocClient::m_ptr take +0x90, against 249 for
    // esv::EocServer::m_ptr's GameServer at +0xA8 and 77 for +0x98.
    uint64_t UnlockedAchievementCallbacks[2];
    void* Eoc;
    GameStateMachine* GameStateMachine;
    net::GameClient* GameClient;
    void* field_98;
#else
    UnknownSignal UnlockedAchievementCallbacks;
    //UnknownSignal field_88;
    void* Eoc;
    GameStateMachine* GameStateMachine;
    net::GameClient* GameClient;
#endif
    void* CommunityManager;
    void* LobbyLogicManager;
    void* CrossSaveManager;
    void* UIManager;
    uint64_t CrossSaveConnectSignalId;
    FixedString FS1;
    FixedString LoadedLevel;
    FixedString GameId;
    FixedString CurrentGameSession;
#if defined(BG3SE_LINUX)
    // PORT (linux): one qword fewer here. Everything from the bools on is 8 lower in the
    // Linux build than upstream's mirror, and this blob is the only unknown ahead of them,
    // so it is where the 8 comes off. Four measurements fix the shift:
    // the FixedStrings above are at +0xC8..0xD4 (the game reads
    // +0xCC); GameStateInitDone reads as a byte at +0xE8 (`cmp byte [client+0xe8], 0`);
    // CastRequirementsManager is a pointer at +0x108; TempScratchString's Managed/CanGrow
    // pair sits at +0x130, putting ModManager at +0x138 -- which is where the manifest row
    // ecl__EoCClient has it -- and EntityWorld at +0x1A0, the
    // field 1838 of the 2365 reads off ecl::EocClient::m_ptr take.
    uint64_t Unknown4[2];
#else
    uint64_t Unknown4[3];
#endif
    bool IsReturningToMainMenu;
    bool MainMenuEntered;
    bool GameStateInitDone;
    bool IsQuitting;
    Array<int16_t> GameInputDisabled;
    void* ItemCombinationManager;
    void* CastRequirementsManager;
    bool UsingTempScratchString;
    ScratchString TempScratchString;
    ModManager* ModManager;
    void* ChatManager;
    TranslatedString LastError;
    uint8_t LastMessageBoxLocalId;
    Array<void*> field_170;
    Array<int> field_180;
    Array<void*> field_190; // EMessageBoxLocalId
    int BusyMessage;
    int InitializationState;
    int AutoPlayRequestState;
    int field_1AC;
    //void* Console;
    //__int64 field_158;
    ecs::EntityWorld* EntityWorld;
    void* EntityManager;
    void* LobbyDataCache;
    // etc.
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(EoCClient, GameStateMachine) == 0x88);   // peek.py on the live client, State 7 at the menu / 18 in a save
static_assert(offsetof(EoCClient, GameClient) == 0x90);     // live: three vtables, peer.Client == self, 582 deserializers; census 244 reads
static_assert(offsetof(EoCClient, CommunityManager) == 0xA0);   // unmoved by the swap above
// The `cmp byte [client+0xe8], 0` witness pins the *start* of the bool run, which is all a
// byte compare can say -- which of the four bools it reads is not decided by it; the offsets
// below are what carry the identifications.
static_assert(offsetof(EoCClient, IsReturningToMainMenu) == 0xE8);
static_assert(offsetof(EoCClient, CastRequirementsManager) == 0x108);   // a pointer, read live
// The Managed/CanGrow pair of the scratch string is the signature the shift was measured on
// (the world's own ScratchString shows the same at +0x2F8), and it is what puts ModManager at
// +0x138. Derive it rather than pinning TempScratchString itself: the pair is what was seen.
static_assert(offsetof(EoCClient, TempScratchString) + offsetof(ScratchString, Managed) == 0x130,
    "ecl::EoCClient::TempScratchString's Managed/CanGrow pair sits at +0x130");
static_assert(offsetof(EoCClient, ModManager) == 0x138);    // manifest row ecl__EoCClient
static_assert(offsetof(EoCClient, EntityWorld) == 0x1A0);
static_assert(offsetof(EoCClient, EntityManager) == 0x1A8);
#endif

END_NS()
