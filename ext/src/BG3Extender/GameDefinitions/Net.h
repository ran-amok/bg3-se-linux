#pragma once

#include <windows.h>
#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/Enumerations.h>
// Temporarily, for ComponentCallbakcList
#include <GameDefinitions/EntitySystem.h>
#include <GameDefinitions/Module.h>   // PORT (linux): ls::ModuleSettings, for HostRefuseMessage

BEGIN_NS(net)

struct Bitstream : ProtectedGameObject<Bitstream>
{
    uint32_t NumBits;
    uint32_t AllocatedBits;
    uint32_t CurrentOffsetBits;
    uint8_t* Buf;
    bool Unknown;
};

// PORT (linux): the game's class is net::MessageVisitor, and its concrete visitor's vtable
// (__ZTVN3net13MemoryVisitorE 0x1094e1b58, 17 slots)
// is: 0 D1, 1 D0, 2 WriteInternal(void const*, size_t) = WriteBytes,
// 3 ReadInternal(void*, size_t) = ReadBytes, 4 WriteInternal0, 5 WriteInternal1,
// 6 ReadInternalBit, 7.. the typed Visit(NetId&/Vector2i/Vector2f/Vector3f/...) overloads.
// So upstream's three declarations are an MSVC scalar deleting destructor standing in one slot
// (`Unknown`) where Itanium has two: WriteBytes and ReadBytes would be **one slot early** here.
//
// The Linux witness, independent of the oracle's:
// four call sites in eocnet::ClientConnectMessage's serialization dispatch on the visitor's
// own vtable, `cmp dword [visitor+8], 0; sete al; call [vtable + rax*8 + 0x10]` --
// bg3+0x3baa2bd and 0x3baa2f4 (eocnet::ClientConnectMessage::Visit 0x3baa280, 4 and 1 bytes),
// 0x3baa3ef (the HashSet<Guid> helper 0x3baa3c0) and the pair at 0x3baa7e8/0x3baa807
// (the enum helper 0x3baa7c0). rax is 1 exactly when IsWriting == 0, so **+0x10 is the write
// slot and +0x18 the read slot**: slot 2 and slot 3, two destructor slots in front of them.
// Giving the mirror a virtual destructor is how Message below already spells the same pair.
struct BitstreamSerializer : ProtectedGameObject<BitstreamSerializer>
{
#if defined(BG3SE_LINUX)
    // Itanium emits D1 at slot 0 and D0 at slot 1 for this; MSVC emits one deleting-dtor slot.
    inline virtual ~BitstreamSerializer() {}
#else
    virtual void Unknown() = 0;
#endif
    virtual void WriteBytes(void const* Buf, uint64_t Size) = 0;
    virtual void ReadBytes(void* Buf, uint64_t Size) = 0;

    uint32_t IsWriting;
    Bitstream* Bitstream;
};

// PORT (linux): both members measured.
// IsWriting: net::MessageVisitor::Visit(ls::FixedString&) 0x2b04a90 opens with
// `cmp dword ptr [rdi + 8], 1` and takes the writing path when equal.
// The pointer at +0x10 is there as upstream says, but in net::MemoryVisitor it is an
// **ls::ScratchBuffer**, not the net::Bitstream above: MemoryVisitor::WriteInternal 0x26ca920
// is `mov rdi, [rdi+0x10]; jmp ls::ScratchBuffer::Write`, and ReadInternal 0x75b31b0 reads
// {base +0x38, end +0x40, cursor +0x48} off it -- none of which is {NumBits, AllocatedBits,
// CurrentOffsetBits, Buf, Unknown}. Nothing on Linux dereferences it yet, so the Bitstream
// mirror is left alone and gets no assert; any code that does must either go
// through the WriteBytes/ReadBytes virtuals or mirror ls::ScratchBuffer.
static_assert(offsetof(BitstreamSerializer, IsWriting) == 0x8);    // Visit(FixedString&) 0x2b04a90
static_assert(offsetof(BitstreamSerializer, Bitstream) == 0x10);   // MemoryVisitor::WriteInternal 0x26ca920

template <class T>
struct QueueCS : public Queue<T>
{
    QueueCS()
    {
        InitializeCriticalSection(&CriticalSection);
    }
    
    ~QueueCS()
    {
        DeleteCriticalSection(&CriticalSection);
    }

    CRITICAL_SECTION CriticalSection;
};

struct Message : Noncopyable<Message>
{
    using SerializeProc = void (Message* msg, BitstreamSerializer* serializer);

    // PORT (linux): this hand-written table is MSVC's -- one scalar deleting destructor slot
    // where Itanium has the D1/D0 pair, so upstream's `VMT->Serialize` reads the deleting
    // destructor on this build. The second pointer below is that fix.
    //
    // The 6-slot shape was read off the oracle's eocnet::ClientConnectMessage
    // (__ZTVN6eocnet20ClientConnectMessageE 0x1093589d8) and is now
    // witnessed **in bin/bg3 itself**. The game's
    // own ClientConnectMessage template is `operator new(0xa0)` + ctor 0x3cdea30 at
    // eocnet::RegisterMessages+0x2bc (bg3+0x3cdf46c..0x3cdf493, id 6), the ctor stores the
    // vptr 0x79280b8, and tools/vtables.py --dump 0x79280b8 is a run of exactly **6** slots:
    //   0 0x3e80320  ~D1  -- frees the two STDStrings and destroys the HashSet, no delete
    //   1 0x3e80300  ~D0  -- calls 0x3e80320 then operator delete, i.e. the Itanium pair
    //   2 0x3baa280  Serialize = eocnet::ClientConnectMessage::Visit(net::MessageVisitor*)
    //   3 0x5ed6780  Unknown   = net::Message::GetDebugInfo, a bare `ret`
    //   4 0x3cdea10  CreateNew = CreateInstance: `operator new(0xa0)` + the same ctor 0x3cdea30
    //   5 0x5edc6a0  Reset
    // The *class* below needs no change -- it declares a virtual destructor, so Itanium lays
    // the five out in exactly that order. Only this hand-written view of the table was short.
    // Hooks.cpp's HookNetworkMessages() is its only reader (eocnet__ClientConnectMessage__
    // Serialize), and that hook's signature is right on Linux: slot 2 takes rdi = the message
    // and rsi = the visitor and returns nothing (0x3baa280's prologue), which is upstream's
    // `void (Message*, BitstreamSerializer*)`. Neither ServerNetworking.cpp nor
    // ClientNetworking.cpp is in ext/CMakeLists.txt yet, so nothing calls it until step 4.
    struct VMT
    {
        void* Dtor;
#if defined(BG3SE_LINUX)
        void* Dtor2;
#endif
        void* Serialize;
        void* Unknown;
        void* CreateNew;
        void* Reset;
    };

    inline virtual ~Message() {}
    virtual void Serialize(BitstreamSerializer & serializer) = 0;
    virtual void Unknown() = 0;
    virtual Message * CreateNew() = 0;
    virtual void Reset() = 0;

    NetMessage MsgId;
    uint32_t Reliability{ 4 };
    uint32_t Priority{ 1 };
    uint8_t OrderingSequence{ 0 };
    bool Timestamped{ 0 };
    uint64_t Timestamp{ 0 };
    uint32_t OriginalSize{ 0 };
    float Latency{ 0 };
};

// PORT (linux): MsgId at +8, measured twice. eocnet::RegisterMessages
// 0x3cdf1b0 fills each template in place -- for NETMSG_HANDSHAKE, `movabs rcx, 0x400000001` into
// [msg+8] is {MsgId = 1, Reliability = 4} in one store, then [msg+0x10] = 1 is Priority and
// [msg+0x14] = 0 the OrderingSequence/Timestamped pair; and esv::LoadProtocol::ProcessMsg
// 0x4015f50 switches on `dword ptr [rcx + 8]` over the ids 0xa2..0xae.
static_assert(offsetof(Message, MsgId) == 0x8);                        // RegisterMessages 0x3cdf1b0
static_assert(offsetof(Message, Reliability) == 0xc);                  // RegisterMessages 0x3cdf1b0
static_assert(offsetof(Message, Priority) == 0x10);                    // RegisterMessages 0x3cdf1b0

// PORT (linux): the hand-written VMT view above, against the game's own 6-slot run
// (tools/vtables.py --dump 0x79280b8). sizeof pins the slot count;
// the offset pins where Serialize landed once the Itanium destructor pair is in front of it.
#if defined(BG3SE_LINUX)
static_assert(sizeof(Message::VMT) == 0x30);                           // run 0x79280b8: 6 slots
static_assert(offsetof(Message::VMT, Serialize) == 0x10);              // slot 2 = Visit 0x3baa280
static_assert(offsetof(Message::VMT, CreateNew) == 0x20);              // slot 4 = CreateInstance 0x3cdea10
#endif

struct MessagePool : Noncopyable<MessagePool>
{
    Message* Template{ nullptr };
    uint64_t GrowSize{ 1 };
    Queue<Message*> Messages;
    Array<Message*> LeasedMessages;

    Message* GetFreeMessage();
};

struct MessageFactory : ProtectedGameObject<MessagePool>
{
    LegacyArray<MessagePool*> MessagePools;
    CRITICAL_SECTION CriticalSection;

    Message* GetFreeMessage(uint32_t messageId);
    void Grow(uint32_t lastMessageId);
    void Register(uint32_t messageId, Message* tmpl);
};

// PORT (linux): the factory, the pool and the message head are upstream's exactly -- measured,
// not assumed. Two functions carry all of it:
//   net::MessageFactory::RegisterMessage(int, Message*, int, char const*) 0x3ce78f0
//     `operator new(0x38)` is the pool, then [pool] = tmpl, [pool+8] = growSize (8 bytes),
//     and the factory's LegacyArray is indexed at [this+8] with capacity [this+0x10],
//     size [this+0x14], Used [this+0x18] and GrowSize [this+0x1c] -- so MessagePools sits at
//     +0 with the vptr of its virtual destructor, exactly as the mirror lays it out.
//   net::MessageFactory::GetFreeMessage(int) 0x2380220
//     `lea r14, [rdi + 0x20]` is the lock: pthread_mutex_trylock@plt on it, and the counter
//     the body ++'s and --'s around the critical region is [rdi+0x48] = the lock's own
//     RecursionCount at +0x28 (shim/windows.h). So CriticalSection is at +0x20, i.e. the
//     0x20-byte LegacyArray is the whole of what precedes it.
// The pool's Queue and Array are read in the same body: Messages {buf +0x10, capacity +0x18,
// size +0x1c, read +0x20, write +0x24} and LeasedMessages {buf +0x28, capacity +0x30,
// size +0x34}, with the wrap stored as `mov qword [pool+0x20], -1` (read = -1, write = 0).
static_assert(offsetof(MessageFactory, MessagePools) == 0x0);          // RegisterMessage 0x3ce78f0
static_assert(offsetof(MessageFactory, CriticalSection) == 0x20);      // GetFreeMessage 0x2380220: trylock(this+0x20)
static_assert(sizeof(MessagePool) == 0x38);                            // RegisterMessage 0x3ce78f0: operator new(0x38)
static_assert(offsetof(MessagePool, Template) == 0x0);                 // RegisterMessage 0x3ce78f0
static_assert(offsetof(MessagePool, GrowSize) == 0x8);                 // RegisterMessage 0x3ce78f0
static_assert(offsetof(MessagePool, Messages) == 0x10);                // GetFreeMessage 0x2380220
static_assert(offsetof(MessagePool, LeasedMessages) == 0x28);          // GetFreeMessage 0x2380220

struct MessageContext
{
    UserId UserID;
    Array<PeerId> PeerIDClassNames;
    Array<UserId> UserIDs;
    Message * Msg;
    uint32_t Unknown1;
    uint8_t Unknown2;
};

// PORT (linux): UserID is the first word, as upstream has it.
// esv::LoadProtocol::ProcessMsg 0x4015f50 -- (rdx = MessageContext const*) -- opens with
// `mov ebx, dword ptr [rdx]; shr ebx, 0x10`, which is UserId::GetPeerId() on the member at +0.
// Nothing else in this struct is measured, so nothing else gets an assert.
static_assert(offsetof(MessageContext, UserID) == 0x0);                // LoadProtocol::ProcessMsg 0x4015f50

enum class ProtocolResult : int32_t
{
    Unhandled = 0,
    Handled = 1,
    Abort = 2,
    AbortAndDisconnect = 3
};

struct Protocol
{
    inline virtual ~Protocol() {}
    virtual ProtocolResult ProcessMsg(void * Unused, MessageContext * Unknown, Message * Msg) = 0;
    virtual ProtocolResult PreUpdate(GameTime const& time) = 0;
    virtual ProtocolResult PostUpdate(GameTime const& time) = 0;
    virtual void OnAddedToHost() = 0;
    virtual void OnRemovedFromHost() = 0;
    virtual void Reset() = 0;

    AbstractPeer* Peer{ nullptr };
};

struct PacketHandlers : ProtectedGameObject<PacketHandlers>
{
    LegacyRefMap<int32_t, void*> field_0;
    AbstractPeer* Peer;
    double LastUpdate;
    __int64 field_20;
    __int64 field_28;
    __int64 field_30;
    __int64 field_38;
};

// PORT (linux): stale and uncompiled -- nothing in the tree names this struct. Its Unknown[27]
// / Unknown2[3] slot indices are an MSVC reading of the vtable AbstractPeerBase below now
// describes slot for slot, and they are wrong here (SendToPeer is slot 25, not 27). Left as
// upstream wrote it rather than deleted, because deleting upstream declarations is not this
// port's business; use AbstractPeerBase, not this.
struct AbstractPeerVMT
{
    typedef void (* SendToPeerProc)(AbstractPeer *, int32_t *, Message *);
    typedef void (* SendToMultiplePeersProc)(AbstractPeer *, ObjectSet<int32_t> *, Message *, int32_t);
    typedef void (* ClientSendProc)(AbstractPeer *, int32_t, Message *);

    uint64_t Unknown[27];
    SendToPeerProc SendToPeer;
    uint64_t Unknown2[3];
    SendToMultiplePeersProc SendToMultiplePeers;
    ClientSendProc ClientSend;
};

// PORT (linux): the real vtable, read off the macOS oracle's __ZTVN3net12AbstractPeerE
// (0x1094dfb28, dumps/oracles/bg3-macos-x86_64-af3f1c71) and
// applied here, because the port compiles upstream's three net TUs and esv::GameServer /
// ecl::GameClient derive from this class. The primary vtable is 38 slots; upstream's declaration
// describes 28 of them in the wrong order. The mapping, the game's slot -> what is declared below:
//   0..14   the 15 net::NetEventListener virtuals, upstream's exactly, in order (fun_30..fun_70
//           are OnOfflinePing, OnOfflinePong, OnPasswordRequested, OnFCMNewHost,
//           OnFCMVerifiedJoinStart/Capable/Failed/Accepted/Rejected)
//   15, 16  the Itanium destructor pair (D1, D0) -- upstream has NO destructor slot, which is
//           MSVC's one-slot scalar deleting dtor, so every
//           upstream name from here on is >= 2 slots late
//   17      AbstractPeer::Destroy()
//   18      Init(uint16, uint16, net::ENetworkMode, SocketOverride*, INetworkCommunityInterface*)
//   19      Shutdown(uint32) -- upstream declares Shutdown *before* Init; the game is Init first
//   20, 21  CreateConnection(NetAddress const&, STDString const&) / CreateConnection(NetGUID const&)
//   22      RequestDisconnect(PeerId, uint32, uint32)
//   23, 24  PreUpdate / PostUpdate(ls::GameTime const&)
//   25      SendMsg(PeerId, Message*)                 = upstream's SendMessageSinglePeer
//   26      SendMsg(DynamicArray<PeerId>&&, Message*, PeerId) = SendMessageMultiPeerMoveIds
//   27      ProcessMsg(ls::GameTime const&, MessageContext const*, Message*) -- the first
//           argument is a GameTime, not upstream's void*
//   28      ConnectToNatPunchServer(NetAddress const&, STDString const&)
//   29      CanFireNetEvents() const = upstream's fun_110
//   30..37  OnWTCompletion, OnWTCanceled, ExecuteWTKernel, AddMessageToSend,
//           UpdateMessagesToSend, QueueConnectionIncomingEvent, QueueConnectionLostEvent,
//           QueueMessageReceivedEvent -- declared as fun_* so the slot count is the game's 38
// Two more differences from upstream beyond the dtor shift: its Disconnect(uint32) is no virtual
// of this class and is gone, and **its two send methods are the wrong way round** -- they are one
// overload set (SendMsg) in the game, which MSVC groups at the first declaration
// in reverse order, so the single-peer one comes first on Itanium. The two keep
// upstream's parameter types: SendMessageMultiPeerMoveIds's recipient list is a
// DynamicArray<PeerId>&& in the game, which is the same ABI as upstream's Array<PeerId>& (one
// pointer) and is what lets ServerNetworking.cpp's two call sites pass an lvalue; the game moves
// the buffer out, which is what upstream's "MoveIds" records.
// The compiled callers are ClientNetworking.cpp:176 (SendMessageSinglePeer) and
// ServerNetworking.cpp:178 / 198 / 217.
// The game's own classes are esv::GameServer (__ZTVN3esv10GameServerE) and ecl::GameClient
// (__ZTVN3ecl10GameClientE), not net::; net::Protocol is a template over them.
struct AbstractPeerBase : ProtectedGameObject<AbstractPeerBase>
{
    virtual void OnNewIncomingConnection() = 0;
    virtual void OnConnectionRequestAccepted(void*) = 0;
    virtual void OnConnectionLost(void*) = 0;
    virtual void OnConnectionAttemptFailed(void*) = 0;
    virtual void OnPunchThroughFailed(void*) = 0;
    virtual void OnPunchThroughSuccess(void*) = 0;
    virtual void fun_30(void*) = 0;
    virtual void fun_38(void*) = 0;
    virtual void fun_40() = 0;
    virtual void fun_48() = 0;
    virtual void fun_50() = 0;
    virtual void fun_58() = 0;
    virtual void fun_60() = 0;
    virtual void fun_68() = 0;
    virtual void fun_70() = 0;
    virtual ~AbstractPeerBase() = default;                  // slots 15, 16 -- the Itanium pair
    virtual void Destroy() = 0;
    virtual void Init(uint16_t port, uint16_t port2, uint32_t networkMode, void* socketOverride, void* communityInterface) = 0;
    virtual void Shutdown(uint32_t reason) = 0;
    virtual void CreateConnection(void* address, STDString const& password) = 0;
    virtual void CreateConnection2(void* netGuid) = 0;
    virtual void RequestDisconnect(TPeerId peerId, uint32_t delay, uint32_t delayInfo) = 0;
    virtual void PreUpdate(GameTime const&) = 0;
    virtual void PostUpdate(GameTime const&) = 0;
    virtual void SendMessageSinglePeer(TPeerId peerId, Message* message) = 0;
    virtual void SendMessageMultiPeerMoveIds(Array<PeerId>& recipients, Message* message, TPeerId excludePeerId) = 0;
    virtual ProtocolResult ProcessMsg(GameTime const& time, MessageContext const* context, Message* message) = 0;
    virtual bool ConnectToNatPunchServer(void* address, STDString const& password) = 0;
    virtual bool CanFireNetEvents() const = 0;
    virtual void fun_WTCompletion() = 0;
    virtual void fun_WTCanceled() = 0;
    virtual void fun_ExecuteWTKernel() = 0;
    virtual void fun_AddMessageToSend() = 0;
    virtual void fun_UpdateMessagesToSend() = 0;
    virtual void fun_QueueConnectionIncomingEvent() = 0;
    virtual void fun_QueueConnectionLostEvent() = 0;
    virtual void fun_QueueMessageReceivedEvent() = 0;
};

struct AbstractPeerCompressor
{
    HashMap<PeerId, void*> field_0; // compression::PrANS*
    HashMap<PeerId, void*> field_50; // compression::PrANS*
    bool field_A0;
};

struct AbstractPeer : public AbstractPeerBase
{
    void* VMT2;
    void* VMT3;
    void* VMT4;
    void* RakNetPeer;
    int SocketBindingType_M;
    int PeerAddressID;
    STDString PeerHost1;
    uint16_t PeerPort1;
    STDString PeerHost2;
    void* LoopbackPeer;
    void* field_60;
    AbstractPeerCompressor Compressor;
    bool field_F0;
    PacketHandlers Handlers;
    Pool MessageContextPool;
    QueueCS<Message*> ImmediateIncomingMessages;
    QueueCS<Message*> Inbox;
    QueueCS<Message*> Outbox;
    Array<TPeerId> PeerIdClassName;
    Array<void*> DisconnectDelays;
    void* SocketLayerOverride;
    MessageFactory *NetMessageFactory;
    void *AddressManager;
    void *NetEventManager;
    void *RakNetNatPunchthroughClient;
    void *RakNetNatTypeDetectionClient;
    void *FullyConnectedMesh;
    bool UseMesh;
    void *NatPunchDebugInterface;
#if 0 // Editor only
    void *NatPunchDebugInterface2;
#endif
    int NatPunchServerPeerID;
    GameTime LastTickTime;
    int Timeout;
    uint16_t MaxPeers;
    uint16_t NetworkVersion;
    bool FreezeProcessing;
    bool WasInitialized;
    bool IsAutoProcessing;
    bool NextIsAutoProcessing;
    uint32_t MaxMessageProcessTime;
    uint32_t MaxConnectRetries;
    uint32_t UpdateInterval;
    STDString RemoteHost1;
    uint16_t RemotePort;
    STDString RemoteHost2;
    bool NeedsConnecting;
    int RetryIndex;
    double LastUpdateTime;
    void *EventVisitor;
    CRITICAL_SECTION FireEventsCriticalSection;
    bool FirePeerEvents;
    bool field_319;
    uint16_t Port1;
    uint16_t Port2;
    Array<Protocol*> ProtocolList;
    HashMap<uint32_t, Protocol*> ProtocolMap;
    QueueCS<Message*> PacketsToSend;
    QueueCS<Message*> MessagesToDecompress;
#if 0 // Editor only
    QueueCS<Message*> MessagesToDecompress2;
#endif
    void* DecompressJobBatch;
    bool DecompressJobLaunched;

#if 0 // Editor only
    STDString Name;
    double LocalLatency;
    double LocalLatency2;
    QueueCS<Message*> Ring4;
#endif
};

// PORT (linux): the four members Ext.Net needs inside the peer, each read off a
// call site in bin/bg3 rather than counted forward from upstream's Win32 lock sizes.
// The peer is reached as [esv::EoCServer + 0xa8] on the server and
// [ecl::EoCClient + 0x90] on the client -- both established independently: the first reads
// 0x688/0x6e0/0x700/0x720/0x740 (this file's GameServer asserts), the second 0x240/0x490/0x550
// (the GameClient asserts), over a two-level census of the loads off each global.
//   NetMessageFactory +0x240 -- `mov rdi, [peer + 0x240]; call net::MessageFactory::GetFreeMessage`
//     at 0x296e375 (esv::ItemProtocol::BroadcastOffstage, peer via Protocol::Peer at +8),
//     0x29bbfe4 (esv::CombatLogProtocol::AddText) and 0x3bf7a08 (Eoc::StopServer, client side);
//     56 of the server census's reads and 49 of the client's land on it.
//   ProtocolList +0x330 -- net::AbstractPeer::ProcessMsg 0x75b3a00 is the whole witness:
//     `mov r13d, [rdi+0x33c]` is the count and `mov rbp, [rdi+0x330]` the buffer, and it calls
//     slot 2 of each element (Protocol's ProcessMsg behind the Itanium dtor pair).
//   ProtocolMap +0x340 -- net::AbstractPeer::GetProtocol<T>() is outlined with the *map* as its
//     argument (0x634e8d0, 0x66246b0), so its callers spell the offset out: `lea rdi, [r14+0x340]`
//     at 0x315e8fe (esv::ASUseItem::EnterRequest, server peer) and `mov edi, 0x340; add rdi,
//     [EoCClient+0x90]` at 0x34938dc (ecl::TryExecuteTargetAction, client peer). The body also
//     measures ls::HashMap: HashKeys {buf +0, size +8}, NextIds +0x10, Keys +0x20, Values +0x30,
//     so sizeof(HashMap) is 0x40 here -- upstream's AbstractPeerCompressor field names (field_50,
//     field_A0) are from a 0x50 reading and are 0x10 out, but they are names, not offsets.
//   HostPeerId +0x428 (net::Client) -- Eoc::StopServer 0x3bf79f0: `cmp dword [client+0x428], -1`
//     decides between the client and server paths, and the same dword is esi for
//     net::AbstractPeer::SendMsg(PeerId, Message*, short) 0x3bf8620 a few instructions later.
//     That also fixes sizeof(AbstractPeer) at 0x420, which is what the asserts below check.
// Everything between PacketsToSend and the end of the peer falls out of the same reading:
// net::AbstractPeer::PreUpdate 0x2b054f0 works a QueueCS at +0x3c8 (lock +0x3e0, its
// RecursionCount +0x408) and then waits on the ls::WorkerThreadBatch at +0x410 behind the bool
// at +0x418 -- MessagesToDecompress, DecompressJobBatch, DecompressJobLaunched -- and
// net::AbstractPeer::Init 0x41eb920 stores its two port arguments at +0x32a and +0x32c.
static_assert(offsetof(AbstractPeer, NetMessageFactory) == 0x240);     // GetFreeMessage call sites 0x296e375, 0x29bbfe4, 0x3bf7a08
static_assert(offsetof(AbstractPeer, ProtocolList) == 0x330);          // AbstractPeer::ProcessMsg 0x75b3a00
static_assert(offsetof(AbstractPeer, ProtocolMap) == 0x340);           // GetProtocol call sites 0x315e8fe (esv), 0x34938dc (ecl)
static_assert(offsetof(AbstractPeer, Port1) == 0x32a);                 // AbstractPeer::Init 0x41eb920
static_assert(offsetof(AbstractPeer, MessagesToDecompress) == 0x3c8);  // AbstractPeer::PreUpdate 0x2b054f0
static_assert(offsetof(AbstractPeer, DecompressJobBatch) == 0x410);    // AbstractPeer::PreUpdate 0x2b054f0: WorkerThreadBatch::WaitForCompletion
static_assert(sizeof(AbstractPeer) == 0x420);                          // Eoc::StopServer 0x3bf79f0: Client::HostPeerId is at +0x428
static_assert(sizeof(HashMap<uint32_t, Protocol*>) == 0x40);           // GetProtocol body 0x634e8d0

// PORT (linux): upstream's GameServerVMT2 is real and this class is 0x428 bytes -- measured on
// bin/bg3 itself. The vtable group of net::Host (primary address point
// 0x7ae89c8, found from net::Host::Shutdown 0x75b61d0 at slot 19) carries three secondary
// vtables, by their offset-to-top words: -8 and -0x10 (net::AbstractPeer's own two secondary
// bases, the destructor/Queue* interface and ls::WorkerThreadJob) and **-0x420**. So net::Host
// adds a polymorphic base subobject immediately after net::AbstractPeer, whose one virtual is
// SetLANDiscoveryReponse: the secondary's single slot 0x75b6240 is `mov rdi, [rdi - 0x400]; mov
// rax, [rdi]; jmp [rax + 0x170]`, the non-virtual thunk of net::Host::SetLANDiscoveryReponse
// 0x75b6200 (`mov rdi, [rdi + 0x20]`, the same forward from the peer's +0x20) adjusted by -0x420.
// The macOS oracle agrees with the locks taken out: __ZTVN3net4HostE's secondaries are -8, -0x10
// and -0x390, and its third slot is `non-virtual thunk to net::Host::SetLANDiscoveryReponse`
// and 0x420 - 0x390 = 0x90 is the lock difference inside AbstractPeer.
// So the three pointers upstream's esv::GameServer mirror declares at +0x420/+0x428/+0x430 are
// *this* one plus two of GameServer's own, not three of GameServer's.
struct Host : public AbstractPeer
{
    void* GameServerVMT2;
};
static_assert(offsetof(Host, GameServerVMT2) == 0x420);                // net::Host vtable group 0x7ae89c8: secondary offset-to-top -0x420
static_assert(sizeof(Host) == 0x428);                                  // same

struct ExternalCommunityUserID
{
    uint32_t field_0;
    uint8_t CommunityType;
    uint64_t ID;
};

struct GameServerUserMappings
{
    Array<UserId> UserIds;
    Array<void*> field_10_UserId_Mapping;
    Array<void*> field_20_UserSlotId_Mapping;
    LegacyMap<FixedString, uint32_t> field_30_FS_UserSlotId;
};

struct GamePlayerInfo
{
    uint32_t Flags;
    EntityHandle Character;
    FixedString PlayerProfileId;
    STDString Username;
    ExternalCommunityUserID CommunityUserId;
    __int64 field_40;
    bool TutorialCompleted;
};

struct GamePeerInfo
{
    uint32_t Flags;
    LegacyRefMap<uint16_t, GamePlayerInfo> Players;
    HashSet<Guid> DLCs;
    STDString BuildName;
};

#if defined(BG3SE_LINUX)
// PORT (linux): read off esv::GameServer's constructor (bin/bg3 0x4011c10) and destructor
// (0x50e3090). The Linux net::Host is 0x420 bytes where upstream's
// (Win32 lock sizes) is 0x428 under this toolchain, so everything from the three vtables on sat
// 8 bytes too far; the base is left opaque -- nothing here reads it. The three vtables are
// stored at +0x420/+0x428/+0x430 by both ctor and dtor, and the authority is constructed in
// place at +0x438 with (this, world) and torn down there by 0x3940820, whose member offsets
// (0x10, 0x48, 0x70, 0x90, 0xd0, 0x110, 0x150, 0x180, 0x1b0, 0x1d0, 0x1e0, 0x1f0, 0x200) are
// upstream's exactly, so the authority mirror itself is unchanged. The tail past field_6E8
// (0x790.., DynamicArrays and a static-allocator handle) is not upstream's and not mirrored.
struct GameServer : public Host
{
    void* VMT_NetEventListener;
    void* VMT_GameEventListener;
    ecs::EntityReplicationAuthority Replication;
    void* LobbyManager;
    ecs::EntityWorld* EntityWorld;
    Array<PeerId> ConnectedPeerIds;
    GameServerUserMappings UserMappings;
    Array<PeerId> ActivePeerIds;
    Array<PeerId> SessionPeerIds;
    Array<PeerId> LevelPeerIds;
    Array<PeerId> KickedPeerIds;
    LegacyRefMap<PeerId, GamePeerInfo> PeerInfo;
    LegacyRefMap<EntityHandle, UserId> CharacterOwners;
    int ServerState;
    PeerId LocalPeerId;
    void* VoicePeerInterface;
    UnknownFunction field_6E8;

    std::optional<UserId> GetLocalUserId() const;
};
// The authority: a replication registrant's callback (0x5cf5140, Register<eoc::MaximumRollResult
// BoostComponent, EntityReplicationAuthority>) writes the serializer table at +0x70 (bitset word
// +0x70, capacity +0x7c, values +0x80, cap +0x88, count +0x8c) and pushes into the Array at
// +0x1f0; the dtor 0x3940820 destroys the two HashMaps at +0x90 (keys an EntityHandle array,
// dtor 0x3940930) and +0xd0 (NetId keys, 0x39407f0), and tears +0x200 down first.
static_assert(offsetof(ecs::EntityReplicationAuthority, field_70_IComponentSerializer) == 0x70);   // callback 0x5cf5140
static_assert(offsetof(ecs::EntityReplicationAuthority, EntityToNetId) == 0x90);                   // dtor 0x3940820
static_assert(offsetof(ecs::EntityReplicationAuthority, NetIdToEntity) == 0xd0);                   // dtor 0x3940820
static_assert(offsetof(ecs::EntityReplicationAuthority, field_1F0_pIComponentSubscription) == 0x1f0);   // callback 0x5cf5140
static_assert(offsetof(ecs::EntityReplicationAuthority, field_200_PeerID_pECSReplicationMessage) == 0x200);   // dtor 0x3940820
static_assert(sizeof(ecs::EntityReplicationAuthority) == 0x240);         // 0x438 + 0x240 = LobbyManager 0x678
// The server: ctor 0x4011c10 / dtor 0x50e3090 unless named.
static_assert(offsetof(GameServer, VMT_NetEventListener) == 0x428);      // esv::GameServer vtable group 0x7a88198: secondary offset-to-top -0x428, 13 slots (esv::NetEventListener)
static_assert(offsetof(GameServer, VMT_GameEventListener) == 0x430);    // same group: secondary offset-to-top -0x430 (esv::GameEventListener)
static_assert(offsetof(GameServer, Replication) == 0x438);               // ctor: authority ctor 0x38ec980(this+0x438, this, world); PostUpdate 0x255afa0
static_assert(offsetof(GameServer, LobbyManager) == 0x678);              // dtor: virtual delete; PreUpdate 0x2b05470
static_assert(offsetof(GameServer, EntityWorld) == 0x680);               // ctor stores the world it gave the authority
static_assert(offsetof(GameServer, ConnectedPeerIds) == 0x688);          // ctor zeroes {0x688, 0x690, 0x694}; KickPeer 0x707b630
static_assert(offsetof(GameServer, ActivePeerIds) == 0x6e0);             // ctor zeroes {0x6e0, 0x6e8, 0x6ec}
static_assert(offsetof(GameServer, LevelPeerIds) == 0x700);              // OnNetLevelLoaded 0x40234d0 reads 0x700 / 0x70c
static_assert(offsetof(GameServer, PeerInfo) == 0x720);                  // DeactivatePeer 0x2f34020; OnPlayerDisconnect 0x7077d10 divides by +0x724, indexes +0x728
static_assert(offsetof(GameServer, CharacterOwners) == 0x730);           // RemovePlayerCharacter 0x2f329a0
static_assert(offsetof(GameServer, ServerState) == 0x740);               // Init 0x41eb7e0 writes 2, Shutdown 0x50dcf70 writes 0
static_assert(offsetof(GameServer, LocalPeerId) == 0x744);               // bracketed only: ServerState is the int at 0x740 and VoicePeerInterface the pointer at 0x748; none of the 33 matched methods reads 0x744
static_assert(offsetof(GameServer, VoicePeerInterface) == 0x748);        // Destroy 0x3943f70
static_assert(offsetof(GameServer, field_6E8) == 0x750);                 // dtor: functor at 0x750, storage 0x758
// PeerInfo's value: GetMasterUserID's outlined helper 0x26fd510 walks a node {next, PeerId key,
// value} and reads the value's Players map at +8 (HashSize +0xc, table +0x10), then each
// player's Flags at +0 and its uint16 key -- UserId(peer << 16 | slot).
static_assert(offsetof(GamePeerInfo, Players) == 0x8);                   // 0x26fd510
static_assert(sizeof(LegacyRefMap<PeerId, GamePeerInfo>) == 0x10);       // 0x720 -> HashSize 0x724, HashTable 0x728
#else
struct GameServer : public Host
{
    void* VMT3;
    void* VMT_EventListenerBase;
    void* VMT4;
    ecs::EntityReplicationAuthority Replication;
    void* LobbyManager;
    ecs::EntityWorld* EntityWorld;
    Array<PeerId> ConnectedPeerIds;
    GameServerUserMappings UserMappings;
    Array<PeerId> ActivePeerIds;
    Array<PeerId> SessionPeerIds;
    Array<PeerId> LevelPeerIds;
    Array<PeerId> KickedPeerIds;
    LegacyRefMap<PeerId, GamePeerInfo> PeerInfo;
    LegacyRefMap<EntityHandle, UserId> CharacterOwners;
    int ServerState;
    PeerId LocalPeerId;
    void* VoicePeerInterface;
    UnknownFunction field_6E8;

    std::optional<UserId> GetLocalUserId() const;
};
#endif

// PORT (linux): upstream's VMT5 is the same thing Host calls GameServerVMT2 -- net::Client's
// vtable group (primary address point 0x79eab58, found from ecl::GameClient::Shutdown 0x65265f0
// at slot 19, which inherits it) carries secondaries at -8, -0x10 and **-0x420**, so the word at
// +0x420 is the vptr of the interface base both peers put after net::AbstractPeer. HostPeerId
// then lands at +0x428, which is the live reading, and the class is 0x430 bytes (4 bytes of tail
// padding after HostPeerId).
struct Client : public AbstractPeer
{
    void* VMT5;
    PeerId HostPeerId;
};
static_assert(offsetof(Client, VMT5) == 0x420);                        // net::Client vtable group 0x79eab58: secondary offset-to-top -0x420
static_assert(offsetof(Client, HostPeerId) == 0x428);                  // Eoc::StopServer 0x3bf79f0
static_assert(sizeof(Client) == 0x430);                                // HostPeerId + 4 bytes of tail padding

#if defined(BG3SE_LINUX)
// PORT (linux): read off ecl::GameClient::~GameClient (bin/bg3 0x6525e80).
// The destructor tears the replication peer's members down at +0x4a8 / +0x4e8 (the two HashMaps,
// peer +0x18 / +0x58) and +0x528 (Deserializers, peer +0x98 -- the same offset the component
// registrants write, e.g. 0x5cf5e80), so the peer is at +0x490, against upstream's 0x538; then
// deletes the object at +0x548 through its vtable after checking the u32 at +0x550 (LobbyManager,
// Status). Everything in front of the peer is net::Client (which ends before four members the dtor
// destroys at +0x438/+0x450/+0x468/+0x480, sizes unread) and is left opaque: upstream's
// AbstractPeer carries Win32 lock sizes and nothing here reads it. Past Status the Linux tail does
// not match upstream's and nothing reads it either, so it is not mirrored.
struct GameClient : public Client
{
    // PORT (linux): the base chain is net::Client's, measured (see its asserts); what the Linux
    // tail holds between the end of net::Client at +0x430 and the replication peer at +0x490 is
    // not upstream's four UnknownSignals -- ~GameClient 0x6525e80 tears members down at +0x438,
    // +0x450, +0x468 and +0x480 and reads nothing at +0x430 -- and nothing here needs it, so it
    // stays opaque. ecl::GameClient adds no vptr of its own: its vtable group (0x79eab58) ends
    // after the -0x420 secondary, so the eight bytes at +0x430 are a plain member. uint64_t,
    // not uint8_t, because net::Client's tail padding would otherwise put the array at +0x42c.
    uint64_t ClientTail_[(0x490 - 0x430) / 8];
    ecs::EntityReplicationPeer ReplicationPeer;
    void* LobbyManager;
    uint32_t Status;
};
static_assert(offsetof(GameClient, ReplicationPeer) == 0x490);   // ~GameClient 0x6525e80
static_assert(offsetof(ecs::EntityReplicationPeer, NetIdToEntity) == 0x18);   // ~GameClient: +0x4a8
static_assert(offsetof(ecs::EntityReplicationPeer, EntityToNetId) == 0x58);   // ~GameClient: +0x4e8
static_assert(offsetof(ecs::EntityReplicationPeer, Deserializers) == 0x98);   // registrant 0x5cf5e80
static_assert(offsetof(GameClient, LobbyManager) == 0x548);      // ~GameClient: virtual delete
static_assert(offsetof(GameClient, Status) == 0x550);            // ~GameClient: state check
#else
struct GameClient : public Client
{
    UnknownSignal OnPlayerConnected;
    UnknownSignal OnPlayerDisconnected;
    UnknownSignal field_508;
    UnknownSignal field_520;
    ecs::EntityReplicationPeer ReplicationPeer;
    void* LobbyManager;
    uint32_t Status;
    bool field_5FC;
    void* VoiceChatPeer;
    Array<PeerId> ClientPeers;
    Array<UserId> Users;
    Array<uint64_t> UserIdToUserSlot;
    Array<uint64_t> UserSlotToUserId;
    LegacyRefMap<UserId, STDString> UserToName;
    LegacyRefMap<UserId, uint64_t> UserToCommunityUserId;
    __int64 field_668;
    uint32_t LastAttemptNo;
};
#endif


struct ClientConnectMessage : public Message
{
    STDString GameVersion;
    STDString Build;
    int field_58;
    char field_5C;
    Guid field_60;
    uint8_t field_70;
    HashSet<Guid> field_78;
    int field_A8;
    uint8_t field_AC;
};

// PORT (linux): the whole message is measured -- every member is
// upstream's, and the `field_NN` names keep upstream's **Win32** offsets, which are 0x10 too
// high from field_58 on because ls::STDString is 16 bytes under libc++ ABI v2 and 32 under
// MSVC. Names, not offsets; the asserts below are what this build does.
//
// The witness is the one function that fills the message, ecl's connect sender bg3+0x34f8880:
// it is the only `net::MessageFactory::GetFreeMessage(6)` call site in the binary (383 callers
// of 0x2380220, scanned for `mov esi, 6`), and it stores every member in turn --
//   +0x28 STDString::operator= from the version getter   -> GameVersion
//   +0x38 STDString::operator=(STDString("Gold"))        -> Build  (bg3+0x34f8989: "Gold")
//   +0x48 dword, +0x4c byte, +0x50 qword-or-xmm16        -> field_58, field_5C, field_60
//   +0x60 byte, +0x68 ls::HashTable<Guid>::operator=     -> field_70, field_78
//   +0x98 dword (a counter + 1), +0x9c byte              -> field_A8, field_AC
// and then calls net::AbstractPeer::SendMsg(peer, dword [peer+0x428], msg) 0x2b06370, which
// re-witnesses HostPeerId at +0x428. `Build` is the one the extender writes (ClientNetworking's
// NetworkManager::OnClientConnectMessage appends " Extender_0" to it), so it is the member
// trap 4 of the baton was about. Confirmed twice over by the destructor 0x3e80320, which frees
// [+0x38] when byte [+0x47] is negative and [+0x28] when byte [+0x37] is (the libc++ SSO flag
// of a 16-byte string), destroys the HashSet at +0x68 / +0x78 / +0x88, and by the ctor
// 0x3cdea30, which is reached through `operator new(0xa0)`.
//
// Not mirrored, deliberately: there is a **second byte at +0x61**, written 0 by the ctor
// (`mov word [rdi+0x60], 4`) and by the sender (bg3+0x34f89b2), and serialized by Visit's tail
// call 0x3baa370. It lands in the padding upstream already has between field_70 and field_78,
// so no offset moves; it has no name here because nothing measured says what it is.
#if defined(BG3SE_LINUX)
static_assert(sizeof(Message) == 0x28);                                // GameVersion starts at 0x28
static_assert(offsetof(ClientConnectMessage, GameVersion) == 0x28);    // sender 0x34f88ec
static_assert(offsetof(ClientConnectMessage, Build) == 0x38);          // sender 0x34f899a ("Gold")
static_assert(offsetof(ClientConnectMessage, field_58) == 0x48);       // sender 0x34f88fc
static_assert(offsetof(ClientConnectMessage, field_5C) == 0x4c);       // sender 0x34f8904
static_assert(offsetof(ClientConnectMessage, field_60) == 0x50);       // sender 0x34f8915/0x34f89f7
static_assert(offsetof(ClientConnectMessage, field_70) == 0x60);       // sender 0x34f892a
static_assert(offsetof(ClientConnectMessage, field_78) == 0x68);       // sender 0x34f8948
static_assert(offsetof(ClientConnectMessage, field_A8) == 0x98);       // sender 0x34f8982
static_assert(offsetof(ClientConnectMessage, field_AC) == 0x9c);       // sender 0x34f8937
static_assert(sizeof(ClientConnectMessage) == 0xa0);                   // RegisterMessages 0x3cdf461: new(0xa0)
#endif

// PORT (linux): eocnet::HostRefuseMessage, message id 3 (NETMSG_HOST_REFUSE) -- what the game's
// server sends a joiner it turns away, and what the port sends a joiner that has no extender
// (esv::NetworkManager::RefusePeer). The game's class is
// net::IdentifiedSerializableDataMessage<eocnet::HostRefuseInfo, 3>; HostRefuseInfo is
// {HostRefuseType Reason; ls::ModuleSettings Settings}, and Visit() writes Reason as ONE byte and
// Settings only for reasons 5 and 6 (oracle 0x1025b9250). The reason codes, read off every
// Linux site that leases id 3 and the client's handler (ecl::JoinProtocol::ProcessMsg, case
// table at oracle 0x103c5a89c, English loca):
//   0 server full            "Cannot join - the party is full and no companions can be dismissed."
//   1 private / 2 lobby join failed   "This lobby isn't available for you to join. ..."
//   3 network version        "Players have different game versions. Please ensure you're up-to-date!"
//   4 kicked (KickPeer)      "You have been kicked from the game"
//   5 mod mismatch (+list)   "Connection failed: Mod '[1]' and/or its dependencies don't match ..."
//   6 mod list mismatch (+list; the client validates its own list against it)
//                            "Failed to join: installed mods do not match the session's mods ..."
//   7 server stopping, 8 campaign loading, 9 "Can't join this game right now!", 10 CC region change
// The client shows the dialog for 5, 6 and 10 unconditionally and for the rest only in one join
// state, so 6 with an EMPTY list -- the payload the game's own esv::LoadProtocol::ProcessMsg
// sends at bg3+0x40167ae -- is the one that always reads as "your mods do not match".
// Witnesses on a187af94: esv::GameServer::KickPeer bg3+0x707b6b0 (GetFreeMessage(3) at +0x8d,
// `mov [msg+0x28], 4`, ModuleSettings assigned at msg+0x30 with an empty Mods, SendMsg slot 25,
// RequestDisconnect(peer, 1500, 1500) slot 22), esv::LoadProtocol::ProcessMsg 0x4016788..0x40167e0
// (the same with reason 6), eocnet::RegisterMessages 0x3cdf2a1 `operator new(0x48)` for id 3.
struct HostRefuseMessage : public Message
{
    int32_t Reason;                 // eocnet::HostRefuseType, serialized as a byte
    ModuleSettings Settings;        // read by the client for Reason 5 and 6 only
};

#if defined(BG3SE_LINUX)
static_assert(offsetof(HostRefuseMessage, Reason) == 0x28);            // KickPeer 0x707b76c, LoadProtocol::ProcessMsg 0x40167ae
static_assert(offsetof(HostRefuseMessage, Settings) == 0x30);          // KickPeer 0x707b759 (lea rdi, [msg+0x30]; ModuleSettings::operator=)
static_assert(offsetof(HostRefuseMessage, Settings.Mods) == 0x38);     // Reset() 0x1025b93c0 frees [msg+0x38]; Visit() of the set at +0x38
static_assert(sizeof(HostRefuseMessage) == 0x48);                      // RegisterMessages 0x3cdf2a1: new(0x48); oracle CreateInstance malloc(0x48)
#endif

END_NS()
