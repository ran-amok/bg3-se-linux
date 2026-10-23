// LuaDebugger.h — stand-in: the Lua debugger (protobuf over a socket) is out of scope.
// gExtender->GetLuaDebugger() is always null; these declarations
// exist so the call sites compile.
#pragma once

#include <GameDefinitions/Base/Base.h>

BEGIN_NS(lua)
class State;
END_NS()
BEGIN_NS(esv::lua)
class ServerState;
END_NS()
BEGIN_NS(ecl::lua)
class ClientState;
END_NS()

BEGIN_NS(lua::dbg)

class DebugMessageHandler;

class Debugger
{
public:
    Debugger(DebugMessageHandler&) {}
    void ServerStateCreated(esv::lua::ServerState*) {}
    void ClientStateCreated(ecl::lua::ClientState*) {}
    void ServerStateDeleted() {}
    void ClientStateDeleted() {}
    void ServerTick() {}
    void ClientTick() {}
    void OnLuaHook(lua_State*, lua_Debug*) {}
    void OnLuaError(lua_State*, char const*) {}
    void OnGenericError(char const*) {}
    void OnLogMessage(DebugMessageType, char const*) {}
    void DebugBreak(lua_State*) {}
    bool IsDebuggerReady() const { return false; }
};

END_NS()

BEGIN_SE()
class LuaDebugInterface
{
public:
    LuaDebugInterface(uint32_t) {}
    void Run() {}
    void Shutdown() {}
};
END_SE()
