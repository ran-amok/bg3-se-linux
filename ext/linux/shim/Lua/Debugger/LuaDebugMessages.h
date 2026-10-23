// LuaDebugMessages.h — stand-in, see Lua/Debugger/LuaDebugger.h (this shim).
#pragma once

#include <Lua/Debugger/LuaDebugger.h>

BEGIN_NS(lua::dbg)

class DebugMessageHandler
{
public:
    static constexpr uint32_t ProtocolVersion = 0;
    DebugMessageHandler(LuaDebugInterface&) {}
    bool IsDebuggerReady() const { return false; }
    void SendModInfo() {}
};

END_NS()
