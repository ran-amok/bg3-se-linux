#include <stdafx.h>
#include <Extender/Shared/Hooks.h>
#include <Extender/ScriptExtender.h>
#include <GameDefinitions/Net.h>

BEGIN_SE()

// PORT (linux): gHook is an inline static member now (CoreLib/Wrappers.h)

void Hooks::Startup()
{
    if (loaded_) {
        return;
    }

    auto& lib = gExtender->GetEngineHooks();
    lib.RPGStats__ParseDataBuffers.SetPreHook(&Hooks::OnParseDataBuffers, this);
    eocnet__ClientConnectMessage__Serialize.SetWrapper(&Hooks::OnClientConnectMessage, this);
    
    loaded_ = true;
}

void Hooks::HookNetworkMessages(net::MessageFactory* factory)
{
    if (networkingInitialized_) {
        return;
    }

    if (factory->MessagePools.size() <= (unsigned)NetMessage::NETMSG_CLIENT_CONNECT) {
        ERR("MessageFactory not initialized yet");
        return;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    auto clientConnect = factory->MessagePools[(unsigned)NetMessage::NETMSG_CLIENT_CONNECT]->Template;
    eocnet__ClientConnectMessage__Serialize.Wrap((*(net::Message::VMT**)clientConnect)->Serialize);

    DetourTransactionCommit();

    networkingInitialized_ = true;
}

void Hooks::OnParseDataBuffers(stats::RPGStats* self)
{
#if defined(BG3SE_LINUX)
    // PORT (linux): upstream logs nothing here, and the two things this pre-hook does are both
    // invisible unless something is listening -- StatsStructureLoaded goes to whichever Lua state
    // the calling thread's context names (the client's Load-context state at menu load, the
    // server's during a session load), and both are reset often enough that a subscriber in a
    // bootstrap is not a witness that the hook ran. So the hook says so itself: this is the
    // in-game reading for RPGStats__ParseDataBuffers 0x27ddb00.
    DEBUG("Hooks::OnParseDataBuffers(): %u pre-parsed buffers, %u map entries, %u stat objects",
        self ? self->PreParsedDataBuffers.size() : 0u,
        self ? self->PreParsedDataBufferMap.size() : 0u,
        self ? self->Objects.Values.size() : 0u);
#endif

    LuaVirtualPin lua(gExtender->GetCurrentExtensionState());
    if (lua) {
        lua->OnStatsStructureLoaded();
    }

    gExtender->GetStatLoadOrderHelper().OnLoadFinished();
}

void Hooks::OnClientConnectMessage(net::Message::SerializeProc* wrapped, net::Message* msg, net::BitstreamSerializer* serializer)
{
    auto m = (net::ClientConnectMessage*)msg;
    if (serializer->IsWriting) {
        gExtender->GetClient().GetNetworkManager().OnClientConnectMessage(m);
    }

    wrapped(msg, serializer);
}

END_SE()
