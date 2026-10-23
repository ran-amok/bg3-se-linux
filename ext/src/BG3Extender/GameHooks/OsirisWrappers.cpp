// OsirisWrappers.cpp — PORT (linux). libOsiris.so exports COsiris's entry points as
// ordinary dynamic symbols (Itanium-mangled), so each wrapper
// is hooked on the exported function itself through CoreLib/Wrappers.h (the inline
// hooking engine). The loader's own interposers (loader/src/osiris.cpp) sit in front
// of these, in the game's PLT, and only log; the two layers do not know of each other.
//
// The Osiris globals come from libOsiris.so's own manifest (manifest/b5dc7357395776f8.tsv)
// by upstream's names — osi__Functions, osi__Nodes, … — which the loader
// verified against the mapped library; FindOsirisGlobals/FindDebugFlags below read that
// table instead of scanning the constructor and SetOption. Still null on Linux:
// RuleAction::Call (debugger-only upstream; not exported here), the node VMTs
// (ResolveNodeVMTs is a later step: the tables are named, the hooks' calling conventions
// are not read yet), and the log-file hooks (CreateFileW/CloseHandle).
#include "stdafx.h"
#include <Extender/ScriptExtender.h>
#include <platform.h>
#include <dlfcn.h>
#include <string>

namespace bg3se
{

#define STATIC_HOOK(name)   /* gHook is an inline static member (CoreLib/Wrappers.h) */

STATIC_HOOK(RegisterDivFunctions)
STATIC_HOOK(InitGame)
STATIC_HOOK(DeleteAllData)
STATIC_HOOK(GetFunctionMappings)
STATIC_HOOK(OpenLogFile)
STATIC_HOOK(CloseLogFile)
STATIC_HOOK(Compile)
STATIC_HOOK(Load)
STATIC_HOOK(Merge)
STATIC_HOOK(Event)
STATIC_HOOK(RuleActionCall)
STATIC_HOOK(Call)
STATIC_HOOK(Query)
STATIC_HOOK(Error)
STATIC_HOOK(Assert)
STATIC_HOOK(CreateFileW)
STATIC_HOOK(CloseHandle)


OsirisWrappers::OsirisWrappers()
{}

void OsirisWrappers::RegisterDIVFunctionsPreHook(void * Osiris, DivFunctions * Functions)
{
    CallOriginal = Functions->Call;
    QueryOriginal = Functions->Query;
    ErrorOriginal = Functions->ErrorMessage;
    AssertOriginal = Functions->Assert;

    Functions->Call = &CallWrapper;
    Functions->Query = &QueryWrapper;
    Functions->ErrorMessage = &ErrorWrapper;
    Functions->Assert = &AssertWrapper;
}

void OsirisWrappers::Initialize()
{
    OsirisModule = dlopen("libOsiris.so", RTLD_NOW | RTLD_NOLOAD);
    if (OsirisModule == NULL) {
        Fail("Could not find libOsiris.so in the process");
    }

    Globals.GetString = (COsiStringTableGetStringProc*)GetProcAddress(OsirisModule, "_ZN15COsiStringTable6GetStrE16COsiStringHandle");
    Globals.AddString = (COsiStringTableAddStringProc*)GetProcAddress(OsirisModule, "_ZN15COsiStringTable6AddStrEPKcb");
    Globals.AddStringRef = (COsiStringTableAddStringRefProc*)GetProcAddress(OsirisModule, "_ZN15COsiStringTable9AddStrRefE16COsiStringHandle");
    Globals.RemoveString = (COsiStringTableRemoveStringProc*)GetProcAddress(OsirisModule, "_ZN15COsiStringTable9RemoveStrE16COsiStringHandle");

    FindOsirisGlobals(nullptr);
    FindDebugFlags(nullptr);

    RegisterDivFunctions.Wrap(OsirisModule, "_ZN7COsiris20RegisterDIVFunctionsEP19TOsirisInitFunction");
    InitGame.Wrap(OsirisModule, "_ZN7COsiris8InitGameEv");
    DeleteAllData.Wrap(OsirisModule, "_ZN7COsiris13DeleteAllDataEb");
    GetFunctionMappings.Wrap(OsirisModule, "_ZN7COsiris19GetFunctionMappingsEPP11MappingInfoPj");
    OpenLogFile.Wrap(OsirisModule, "_ZN7COsiris11OpenLogFileEPKwj");
    CloseLogFile.Wrap(OsirisModule, "_ZN7COsiris12CloseLogFileEv");
    Load.Wrap(OsirisModule, "_ZN7COsiris4LoadER12COsiSmartBuf");
    Compile.Wrap(OsirisModule, "_ZN7COsiris7CompileEPKwS1_");
    Merge.Wrap(OsirisModule, "_ZN7COsiris5MergeEPKw");
    Event.Wrap(OsirisModule, "_ZN7COsiris5EventEjP16COsiArgumentDesc");

    Call.Wrap((void*)&CallWrapper);
    Query.Wrap((void*)&QueryWrapper);
    Error.Wrap((void*)&ErrorWrapper);
    // FIXME - PERF Assert.Wrap(&AssertWrapper);

    unsigned hooked = 0;
    for (bool w : { RegisterDivFunctions.IsWrapped(), InitGame.IsWrapped(), DeleteAllData.IsWrapped(),
                    GetFunctionMappings.IsWrapped(), OpenLogFile.IsWrapped(), CloseLogFile.IsWrapped(),
                    Load.IsWrapped(), Compile.IsWrapped(), Merge.IsWrapped(), Event.IsWrapped() }) {
        hooked += w;
    }
    DEBUG("OsirisWrappers::Initialize: %u of 10 libOsiris.so exports hooked", hooked);
}

void OsirisWrappers::Shutdown()
{
    RegisterDivFunctions.Unwrap();
    InitGame.Unwrap();
    DeleteAllData.Unwrap();
    GetFunctionMappings.Unwrap();
    OpenLogFile.Unwrap();
    CloseLogFile.Unwrap();
    Load.Unwrap();
    Compile.Unwrap();
    Merge.Unwrap();
    Event.Unwrap();
    RuleActionCall.Unwrap();

    Call.Unwrap();
    Query.Unwrap();
    Error.Unwrap();
    Assert.Unwrap();
}

bool OsirisWrappers::CallWrapper(uint32_t FunctionId, OsiArgumentDesc * Params)
{
    auto& osi = gExtender->GetServer().Osiris();

    auto wrappers = osi.GetVMTWrappers();
    if (wrappers && wrappers->OsirisCallbacksAttachment) {
        wrappers->OsirisCallbacksAttachment->CallPreHook(FunctionId, Params);
    }

    bool succeeded = gExtender->GetServer().Osiris().GetWrappers().CallOriginal(FunctionId, Params);

    if (wrappers && wrappers->OsirisCallbacksAttachment) {
        wrappers->OsirisCallbacksAttachment->CallPostHook(FunctionId, Params, succeeded);
    }

    return succeeded;
}

bool OsirisWrappers::QueryWrapper(uint32_t FunctionId, OsiArgumentDesc * Params)
{
    return gExtender->GetServer().Osiris().GetWrappers().QueryOriginal(FunctionId, Params);
}

void OsirisWrappers::ErrorWrapper(char const * Message)
{
    gExtender->GetServer().Osiris().GetWrappers().ErrorOriginal(Message);
}

void OsirisWrappers::AssertWrapper(bool Successful, char const * Message, bool Unknown2)
{
    gExtender->GetServer().Osiris().GetWrappers().AssertOriginal(Successful, Message, Unknown2);
}

void * OsirisWrappers::FindRuleActionCallProc()
{
    return nullptr;
}

void OsirisWrappers::FindOsirisGlobals(FARPROC)
{
    auto const& host = platform::host();
    if (!host.manifest_get) {
        ERR("OsirisWrappers::FindOsirisGlobals: no manifest lookup from the loader; Osiris globals stay null");
        return;
    }

    // All ten or none: upstream's code treats them as one set, and a partial set would put
    // the first null dereference wherever the missing one is used.
    struct { char const* name; void** slot; } rows[] = {
        { "osi__StringTable", (void**)&Globals.StringTable },
        { "osi__Variables", (void**)&Globals.Variables },
        { "osi__Types", (void**)&Globals.Types },
        { "osi__Enums", (void**)&Globals.Enums },
        { "osi__Functions", (void**)&Globals.Functions },
        { "osi__Objects", (void**)&Globals.Objects },
        { "osi__Goals", (void**)&Globals.Goals },
        { "osi__Adapters", (void**)&Globals.Adapters },
        { "osi__Databases", (void**)&Globals.Databases },
        { "osi__Nodes", (void**)&Globals.Nodes },
    };

    void* found[std::size(rows)];
    unsigned missing = 0;
    for (unsigned i = 0; i < std::size(rows); i++) {
        found[i] = host.manifest_get(rows[i].name);
        if (!found[i]) {
            WARN("OsirisWrappers::FindOsirisGlobals: %s is not in libOsiris.so's manifest (or failed verification)", rows[i].name);
            missing++;
        }
    }

    if (missing) {
        ERR("OsirisWrappers::FindOsirisGlobals: %u of %zu globals missing; Osiris globals stay null (Osi.* off)", missing, std::size(rows));
        return;
    }

    for (unsigned i = 0; i < std::size(rows); i++) {
        *rows[i].slot = found[i];
    }

    DEBUG("OsirisWrappers::FindOsirisGlobals: %zu globals from libOsiris.so's manifest (Functions = %p, Nodes = %p)",
        std::size(rows), (void*)Globals.Functions, (void*)Globals.Nodes);

    ExpectedInsertTuple = (NodeVMT::InsertTupleProc)host.manifest_get("osi__CReteStartNode__InsertTuple");
    ExpectedDeleteTuple = (NodeVMT::InsertTupleProc)host.manifest_get("osi__CReteStartNode__DeleteTuple");
    if (ExpectedInsertTuple && ExpectedDeleteTuple) {
        DEBUG("OsirisWrappers::FindOsirisGlobals: start-node tuple slots InsertTuple = %p, DeleteTuple = %p",
            (void*)ExpectedInsertTuple, (void*)ExpectedDeleteTuple);
    } else {
        WARN("OsirisWrappers::FindOsirisGlobals: the manifest does not name the start-node tuple slots; Osi inserts (PROCs, DBs, events) stay off");
    }
}

char const* OsirisWrappers::CheckTupleSlots(Node const* node) const
{
    if (node == nullptr) {
        return "the function has no node";
    }

    if (ExpectedInsertTuple == nullptr || ExpectedDeleteTuple == nullptr) {
        return "libOsiris.so's manifest does not name the start-node tuple slots (osi__CReteStartNode__InsertTuple/DeleteTuple)";
    }

    auto vmt = *reinterpret_cast<NodeVMT* const*>(node);
    if (vmt == nullptr) {
        return "the node has no vtable";
    }

    if (vmt->InsertTuple != ExpectedInsertTuple || vmt->DeleteTuple != ExpectedDeleteTuple) {
        return "the node's InsertTuple/DeleteTuple slots are not the start-node ones the manifest names";
    }

    return nullptr;
}

void OsirisWrappers::FindDebugFlags(FARPROC)
{
    auto const& host = platform::host();
    if (host.manifest_get) {
        Globals.DebugFlags = (DebugFlag*)host.manifest_get("osi__DebugFlags");
    }

    if (Globals.DebugFlags) {
        DEBUG("OsirisWrappers::FindDebugFlags: DebugFlags = %p (0x%x)", (void*)Globals.DebugFlags, (unsigned)*Globals.DebugFlags);
    } else {
        WARN("OsirisWrappers::FindDebugFlags: osi__DebugFlags not in the manifest; Osiris log flags stay untouched");
    }
}

void OsirisWrappers::SaveNodeVMT(NodeType type, NodeVMT * vmt)
{
    se_assert(type >= NodeType::Database && type <= NodeType::Max);
    VMTs[(unsigned)type] = vmt;
}

bool OsirisWrappers::ResolveNodeVMTs()
{
    if (Globals.Nodes == nullptr) {
        WARN_ONCE("Osiris node VMTs are not available on Linux yet (no Osiris globals)");
        return false;
    }

    if (!resolvedVMTs_ || !*resolvedVMTs_) {
        resolvedVMTs_ = ResolveNodeVMTsInternal();
    }

    return *resolvedVMTs_;
}

bool OsirisWrappers::ResolveNodeVMTsInternal()
{
    return false;
}

}
