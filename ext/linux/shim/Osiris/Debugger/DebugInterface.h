// DebugInterface.h — stand-in: the Osiris debugger is out of scope.
#pragma once

#include <GameDefinitions/Base/Base.h>

BEGIN_SE()
class OsirisDebugInterface
{
public:
    OsirisDebugInterface(uint16_t) {}
    void Run() {}
    void Shutdown() {}
};
END_SE()
