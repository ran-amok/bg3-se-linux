// DebugMessages.h — stand-in, see Osiris/Debugger/DebugInterface.h (this shim).
#pragma once

#include <Osiris/Debugger/DebugInterface.h>

BEGIN_NS(osidbg)
class Debugger;
class DebugMessageHandler
{
public:
    DebugMessageHandler(OsirisDebugInterface&) {}
    bool IsConnected() const { return false; }
    void SetDebugger(Debugger*) {}
    static constexpr uint32_t ProtocolVersion = 0;
};
END_NS()
