#include <stdafx.h>
#include <Extender/Server/ServerNetworking.h>
#include <Extender/ScriptExtender.h>

BEGIN_NS(esv)

net::ProtocolResult ExtenderProtocol::ProcessMsg(void* unused, net::MessageContext* context, net::Message* msg)
{
    auto base = ExtenderProtocolBase::ProcessMsg(unused, context, msg);
    if (base == net::ProtocolResult::Unhandled && msg->MsgId == NetMessage::NETMSG_CLIENT_CONNECT) {
        gExtender->GetServer().GetNetworkManager().OnClientConnectMessage(context, (net::ClientConnectMessage*)msg);
    }

    return base;
}

void ExtenderProtocol::ProcessExtenderMessage(net::MessageContext& context, net::MessageWrapper & msg)
{
    switch (msg.msg_case()) {
    case net::MessageWrapper::kPostLua:
    {
        if (gExtender->GetServer().HasExtensionState()) {
            auto& postMsg = msg.post_lua();
            esv::LuaServerPin pin(esv::ExtensionState::Get());
            if (pin) {
                pin->OnNetMessageReceived(postMsg.channel_name(), postMsg.payload(), postMsg.module(), 
                    postMsg.request_id(), postMsg.reply_id(), context.UserID,
                    postMsg.serializer() == net::SerializerType::SERIALIZER_BINARY);
            }
        }
        break;
    }

    case net::MessageWrapper::kC2SExtenderHello:
    {
        auto const& hello = msg.c2s_extender_hello();
        DEBUG("Got extender support notification from user %d (version %d)", context.UserID.Id, hello.version());
        gExtender->GetServer().GetNetworkManager().AllowExtenderMessages(context.UserID.GetPeerId(), (net::ProtoVersion)hello.version());
        break;
    }

    case net::MessageWrapper::kUserVars:
    {
        SyncUserVars(msg.user_vars());
        break;
    }

    default:
        OsiErrorS("Unknown extension message type received!");
    }
}

void NetworkManager::Reset()
{
    peerVersions_.clear();
}

bool NetworkManager::LocalPeerOnly() const
{
    auto server = GetServer();
    return server != nullptr
        && server->ActivePeerIds.size() == 1
        && server->ActivePeerIds[0] == (PeerId)server->LocalPeerId;
}

bool NetworkManager::CanSendExtenderMessages(PeerId peerId) const
{
    return peerVersions_.find(peerId) != peerVersions_.end();
}

std::optional<net::ProtoVersion> NetworkManager::GetPeerVersion(PeerId peerId) const
{
    auto it = peerVersions_.find(peerId);
    if (it != peerVersions_.end()) {
        return it->second;
    } else {
        return {};
    }
}

void NetworkManager::AllowExtenderMessages(PeerId peerId, net::ProtoVersion version)
{
    peerVersions_.insert_or_assign(peerId, version);
}


void NetworkManager::OnClientConnectMessage(net::MessageContext* context, net::ClientConnectMessage* msg)
{
    if (!msg->Build.contains(" Extender")) {
        DEBUG("No extender trailer found in ClientConnect message");
        // PORT (linux): a joiner with no extender cannot run the session's SE mods, and until
        // b851872 the first per-user extender message killed it. Turn it
        // away here, where the game's own refusals happen -- this protocol is first in
        // ProtocolList, so the ClientConnect has not reached esv::LoadProtocol yet.
        if (gExtender->GetConfig().RefusePeersWithoutExtender) {
            RefusePeer(context->UserID.GetPeerId(), gExtender->GetConfig().PeerRefuseReason);
        }
    } else if (!msg->Build.contains(" Extender_0")) {
        WARN("Extender signature incorrect in ClientConnect message");
        if (gExtender->GetConfig().RefusePeersWithoutExtender) {   // PORT (linux): as above
            RefusePeer(context->UserID.GetPeerId(), gExtender->GetConfig().PeerRefuseReason);
        }
    } else {
        DEBUG("Sending ExtenderHello to client");
        ExtendNetworking();
        auto helloMsg = GetFreeMessage();
        if (helloMsg != nullptr) {
            auto hello = helloMsg->GetMessage().mutable_c2s_extender_hello();
            hello->set_version((uint32_t)net::ProtoVersion::Current);
            // PORT (linux): the hello precedes the peer's entry in peerVersions_ (see Send()).
            Send(helloMsg, context->UserID, true);
        } else {
            OsiErrorS("Could not get free message!");
        }
    }
}

void NetworkManager::ExtendNetworking()
{
    auto server = GetServer();
    if (server == nullptr) return;

    if (server->NetMessageFactory->MessagePools.size() > (unsigned)net::ExtenderMessage::MessageId) {
        return;
    }

    protocol_ = new ExtenderProtocol();
    server->ProtocolList.insert_at(0, protocol_);
    server->ProtocolMap.set(ExtenderProtocol::ProtocolId, protocol_);

    auto extenderMsg = new net::ExtenderMessage();
    server->NetMessageFactory->Register((uint32_t)net::ExtenderMessage::MessageId, extenderMsg);
    gExtender->GetHooks().HookNetworkMessages(server->NetMessageFactory);
    DEBUG("Registered custom server network protocol");
}

net::GameServer * NetworkManager::GetServer() const
{
    auto server = GetStaticSymbols().esv__EoCServer;
    if (server != nullptr && *server != nullptr) {
        return (*server)->GameServer;
    } else {
        return nullptr;
    }
}

net::ExtenderMessage * NetworkManager::GetFreeMessage(UserId userId)
{
    if (userId && !CanSendExtenderMessages(userId.GetPeerId())) {
        ERR("Attempted to send extender message to user %d that does not understand extender protocol!", userId.Id);
        return nullptr;
    }

    return GetFreeMessage();
}

net::ExtenderMessage * NetworkManager::GetFreeMessage()
{
    auto server = GetServer();
    if (server != nullptr) {
        return (net::ExtenderMessage*)server->NetMessageFactory->GetFreeMessage((uint32_t)net::ExtenderMessage::MessageId);
    } else {
        return nullptr;
    }
}

net::ProtoVersion NetworkManager::SharedVersion()
{
    auto ver{ net::ProtoVersion::Current };
    for (auto const& peer : peerVersions_) {
        ver = std::min(ver, peer.second);
    }

    return ver;
}

void NetworkManager::HandleLocalMessage(net::LocalMessage const& msg)
{
    esv::LuaServerPin pin(esv::ExtensionState::Get());
    if (pin) {
        pin->OnNetMessageReceived(msg.Channel, msg.Payload, msg.Module, msg.RequestId, msg.ReplyId, msg.User, msg.Binary);
    }
}

void NetworkManager::Send(net::ExtenderMessage * msg, UserId userId, bool allowUnverifiedPeer)
{
    auto server = GetServer();
    if (server == nullptr) return;

    // PORT (linux): refuse a per-user send to a peer that never announced the extender.
    // The game's own net::MessageFactory::GetFreeMessage(int) (bg3+0x2380220) indexes
    // MessagePools with no bounds check, so a peer with no extender that receives message id
    // 400 reads MessagePools[400] past the end of the array and dies on the garbage pointer
    // it finds (a SIGSEGV in ProcessPackets under ecl::GameClient::PreUpdate). Broadcast()
    // below already filters exactly this way, and the client's own GetFreeMessage() says why
    // in as many words ("otherwise
    // the peer will crash while trying to parse the packet"); the server's per-user path is
    // the one that misses the check, because Lua's BuildMessage() leases the message with
    // ReservedUserId and GetFreeMessage(UserId) does not test that. A refused message stays
    // leased from its pool -- one pooled object per refused send, against a dead peer.
    // The ExtenderHello is the one message that must reach a peer before it is in
    // peerVersions_, so OnClientConnectMessage passes allowUnverifiedPeer.
    if (!allowUnverifiedPeer && !CanSendExtenderMessages(userId.GetPeerId())) {
        WARN("Not sending extender message to user %d as its peer does not understand extender protocol!", userId.Id);
        return;
    }

    server->SendMessageSinglePeer((TPeerId)userId.GetPeerId(), msg);
}

// PORT (linux): what esv::GameServer::KickPeer (bg3+0x707b6b0) and esv::LoadProtocol::ProcessMsg
// (bg3+0x4016788) do to a peer they turn away, step for step: lease the game's HostRefuseMessage
// (id 3) from the server's own factory, set the reason, send it to that one peer, and ask the
// peer layer to drop the connection 1500 ms later (the delay the game uses, so the message gets
// out first). The leased message carries the pool's Reset() state -- Reason 0 and an empty
// Mods -- which is exactly the empty ModuleSettings both sites assign, so only Reason is written.
// The client's reading of each reason is in GameDefinitions/Net.h above the mirror.
void NetworkManager::RefusePeer(PeerId peerId, uint32_t reason)
{
    auto server = GetServer();
    if (server == nullptr || server->NetMessageFactory == nullptr) {
        ERR("RefusePeer(%d): no server", peerId);
        return;
    }

    auto msg = (net::HostRefuseMessage*)server->NetMessageFactory->GetFreeMessage((uint32_t)NetMessage::NETMSG_HOST_REFUSE);
    if (msg == nullptr) {
        ERR("RefusePeer(%d): could not get a free HostRefuseMessage", peerId);
        return;
    }

    msg->Reason = (int32_t)reason;
    WARN("Refusing peer %d: its ClientConnect carries no extender (HostRefuseType %u)", peerId, reason);
    server->SendMessageSinglePeer((TPeerId)peerId, msg);
    server->RequestDisconnect((TPeerId)peerId, 1500, 1500);
}

void NetworkManager::Broadcast(net::ExtenderMessage * msg, UserId excludeUserId, bool excludeLocalPeer)
{
    auto server = GetServer();
    if (server == nullptr) return;

    Array<PeerId> peerIds;
    for (auto peerId : server->ActivePeerIds) {
        if (CanSendExtenderMessages(peerId)) {
            if (peerId != LocalPeerId || !excludeLocalPeer) {
                peerIds.push_back(peerId);
            }
        } else {
            WARN("Not sending extender message to peer %d as it does not understand extender protocol!", peerId);
        }
    }

    server->SendMessageMultiPeerMoveIds(peerIds, msg, (TPeerId)excludeUserId.GetPeerId());
}

void NetworkManager::BroadcastToConnectedPeers(net::ExtenderMessage* msg, UserId excludeUserId, bool excludeLocalPeer)
{
    auto server = GetServer();
    if (server == nullptr) return;

    Array<PeerId> peerIds;
    for (auto peerId : server->ConnectedPeerIds) {
        if (CanSendExtenderMessages(peerId)) {
            if (peerId != LocalPeerId || !excludeLocalPeer) {
                peerIds.push_back(peerId);
            }
        } else {
            WARN("Not sending extender message to peer %d as it does not understand extender protocol!", peerId);
        }
    }

    server->SendMessageMultiPeerMoveIds(peerIds, msg, (TPeerId)excludeUserId.GetPeerId());
}

END_NS()
