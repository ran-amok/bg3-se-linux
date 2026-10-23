// Debugger.h — stand-in, see Osiris/Debugger/DebugInterface.h (this shim).
#pragma once

#include <Osiris/Debugger/DebugMessages.h>
#include <GameDefinitions/Osiris.h>

BEGIN_NS(osidbg)
class Debugger
{
public:
    Debugger(OsirisStaticGlobals&, DebugMessageHandler&) {}
    void StoryLoaded() {}
    void MergeStarted() {}
    void MergeFinished() {}
    void GameInitHook() {}
    void DeleteAllDataHook() {}
    void RuleActionPreHook(RuleActionNode*) {}
    void RuleActionPostHook(RuleActionNode*) {}
    void IsValidPreHook(Node*, SmallTuple*, uint32_t) {}
    void IsValidPostHook(Node*, SmallTuple*, uint32_t, bool) {}
    void PushDownPreHook(Node*, SmallTuple*, uint32_t, EntryPoint, bool) {}
    void PushDownPostHook(Node*, SmallTuple*, uint32_t, EntryPoint, bool) {}
    void InsertPreHook(Node*, TuplePtrLL*, bool) {}
    void InsertPostHook(Node*, TuplePtrLL*, bool) {}
    void CallQueryPreHook(Node*, OsiArgumentDesc*) {}
    void CallQueryPostHook(Node*, OsiArgumentDesc*, bool) {}
    bool IsInitialized() const { return false; }
    bool IsPaused() const { return false; }
};
END_NS()
