#include "stdafx.h"
#include <GameHooks/EngineHooks.h>
#include <GameDefinitions/Symbols.h>

namespace bg3se
{

// PORT (linux): gHook is an inline static member now (CoreLib/Wrappers.h); no out-of-line definitions

void EngineHooks::HookAll()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    auto& sym = GetStaticSymbols();

    /*if (sym.AppInstance != nullptr && *sym.AppInstance != nullptr) {
        sym.App__OnInputEvent = (*sym.AppInstance)->__vftable->OnInputEvent;
    }*/

#if defined(BG3SE_LINUX)
    // PORT (linux): a macOS prototype is not a Linux calling convention (LTO drops and
    // reorders arguments); a hook goes in only once its register use has been
    // read off the disassembly. Read so far:
    //   ls::FileReader::FileReader(this, Path const*, uint, uint)
    //   ecs::EntityWorld::Update(this in rdi, ls::GameTime const* in rsi, no return value)
    //     -- 0x2d1e130 on build c8da62b1, witness in that manifest row's note column.
    //     Same two integer arguments as upstream's Windows
    //     signature, so OnECSUpdate's wrapper carries over unchanged.
    //   RPGStats::Load(this in rdi = RPGStats*, rsi = the path array, no return value)
    //     -- 0x3b3c120 on build c8da62b1, Larian's RPGStats::Start; the count sits at
    //     +0xc of the array and the elements are 16-byte STDStrings, which is this build's
    //     Array<STDString>. OnStatsLoad's wrapper carries over unchanged.
    //   esv::OsirisVariableHelper::SavegameVisit(this in rdi = OsirisVariableHelper*,
    //     rsi = eoc::SavegameVisitor*, bool in al) -- 0x4198970 on build c8da62b1, read
    //     instruction for instruction against the macOS oracle 0x105636c80; upstream's
    //     declared signature unchanged by LTO. Its one use of the visitor is
    //     [rsi + 0xb0], the LSFVisitor, which OnSavegameVisit is the pre-hook for.
    //     That visit reaches the game's visitor virtuals, so it
    //     also depends on every correction to the ObjectVisitor mirror's slot order
    //     (Serialization.h; the last of the four, the macOS-only `unsigned long` overload,
    //     was read out of a core dump).
    //   ecs::EntityWorld::FlushECBs(this in rdi = ecs::EntityWorld*, no other argument)
    //     -- 0x2486dd0 on build c8da62b1, read prologue for prologue against the macOS
    //     oracle 0x1072e8f30; the manifest row's note has the
    //     four readings. Upstream's declared `bool` return is dead here: nothing sets al
    //     before the single `ret`, which costs OnECSFlushECBs nothing because it is a
    //     pre-hook that ignores the return. The one argument is witnessed twice over: the
    //     spilled rdi is read at +0x300, +0x348 and +0x350 (Executor, Cache, Settings --
    //     the offsets EntitySystem.h asserts for this build), and the caller 0x365d030
    //     passes [EoCServer + 0x288], the server world.
    //   stats::DealDamageFunctor::ApplyDamage -- 0x3c4ae20 on build c8da62b1, the game's
    //     esv::functor::StatsFunctorDealDamage::Execute (entity-target overload; macOS
    //     oracle 0x1064cebc0). All eighteen slots of upstream's ApplyDamageProc were read
    //     off the disassembly, because the matcher's evidence for the address is weak
    //     (r3:2, 1 of 3): rdi = the sret HitResult*, rsi = the functor, rdx/rcx the two
    //     ecs::EntityRef*, r8 = the Vector3f*, r9d = the bool, then twelve stack slots at
    //     entry rsp + 8 .. + 0x60 in upstream's declared order -- SpellInfo,
    //     storyActionId (a dword), ActionOriginator, ClassDescriptions, HitDesc,
    //     AttackDesc, EntityHandle, hitWith (a byte), conditionRollIndex (a dword), the
    //     bool (a byte), a17, SpellId. So the SysV lowering of upstream's typedef is the
    //     game's own convention and OnDealDamage's wrapper carries over unchanged. One
    //     caveat that costs nothing: the single `ret` never sets rax, so the Itanium sret
    //     return value is garbage here -- the wrapper returns whatever the original
    //     returned, and the game's fourteen callers read the sret buffer, not rax.
    //   esv::StatsSystem::ThrowDamageEvent -- 0x330fe20 on build c8da62b1, the game's
    //     esv::StatsSystem::ApplyDamage(EntityRefView<...> const&, HitDesc&, AttackDesc&,
    //     EAbility, bool). rdi = esv::StatsSystem*, rsi = the EntityRefView upstream
    //     types as `void* temp5`, rdx = HitDesc*, rcx = AttackDesc*, r8 = a byte, r9 = a
    //     byte: upstream's declared order, so OnEntityDamageEvent carries over unchanged.
    //     Upstream types the fifth argument `bool` where the oracle says EAbility, which
    //     would matter if clang normalised a re-passed register `bool` -- measured, it
    //     does not (it copies and spills all 32 bits), so the enum survives the wrapper.
    //     The identity rests on four readings, in the manifest row's note.
    //   RPGStats::ParseDataBuffers(this in rdi = RPGStats*, no other argument) -- 0x27ddb00
    //     on build c8da62b1. Upstream has no macOS name for it: the oracle keeps this loop
    //     inside RPGStats::ParseDataFolder 0x102501920 (at 0x10250378d) and its only
    //     RPGStats::ParseDataBuffer symbol is the *singular* per-buffer parse, which on Linux
    //     is 0x2fcdf90, one level below. What found the address is upstream's own
    //     BinaryMappings.xml signature for the name read on this build's offsets: mov
    //     rcx,[rdi+0x3e0]; test; je; movsxd rax,[rdi+0x3ec]; test; je; lea rax,[rcx+rax*8],
    //     against Windows' [rcx+3D8h]/[rcx+3E4h] -- PreParsedDataBuffers' buffer and size
    //     (Stats.h). It then aligns event for event with the macOS loop and has exactly one
    //     caller, ParseDataFolder 0x2fd4140, which calls it once after the file loop and
    //     before FinishAndCleanCurrentParsedType 0x3b3cb00, so OnParseDataBuffers sees the
    //     buffers still live. Upstream's declared `bool` return is dead
    //     here -- nothing sets al before the single `ret` -- which costs the pre-hook
    //     nothing. One integer argument, so OnParseDataBuffers' wrapper carries over
    //     unchanged.
    //   eoc::AiGrid::FindPath(this in rdi = eoc::AiGrid*, esi = the int32 AiPathId) -- 0x2663ae0
    //     on build c8da62b1, read against the macOS oracle 0x1013f5990 instruction for
    //     instruction: the same PathMap lookup on the same AiGrid offsets and the same stores
    //     into the AiPath. Two incoming registers and no more -- r8 and
    //     r9 are first written at 0x2663eb7/0x2663eba and every xmm is written before it is read
    //     -- so upstream's AiGrid__FindPathProc void(AiGrid*, AiPathId) is this build's
    //     convention and OnFindPath's wrapper carries over unchanged. This is a *pre*-hook whose
    //     handler dereferences the mirror (Lua's State::OnFindPath walks AiGrid::PathMap and
    //     reads AiPath::SearchStarted), so it also depends on the eleven layout asserts the same
    //     reading put in Ai.h -- PathMap at +0xf0 and SearchStarted at +0x270 are the two it
    //     cannot run without. Larian's LinePath is inlined into this body here where the oracle
    //     keeps it out of line, which changes nothing at the entry point.
    // The rest wait for phase 6.
    static char const* const linuxReadHooks[] = {
        "ls__FileReader__ctor",
        "ecs__EntityWorld__Update",
        "RPGStats__Load",
        "esv__OsirisVariableHelper__SavegameVisit",
        "ecs__EntityWorld__FlushECBs",
        "stats__DealDamageFunctor__ApplyDamage",
        "esv__StatsSystem__ThrowDamageEvent",
        "RPGStats__ParseDataBuffers",
        "eoc__AiGrid__FindPath",
    };
    auto readOnLinux = [&](char const* symbol) {
        for (auto s : linuxReadHooks) if (strcmp(s, symbol) == 0) return true;
        return false;
    };
#define HOOK_DEFN(name, sym, defn) if (GetStaticSymbols().sym != nullptr) { \
        if (readOnLinux(#sym)) { name.Wrap(GetStaticSymbols().sym); } \
        else { DEBUG("EngineHooks: %s not hooked on Linux (calling convention not read yet)", #sym); } }
#include <GameHooks/EngineHooks.inl>
#undef HOOK_DEFN
#else
#define HOOK_DEFN(name, sym, defn) if (GetStaticSymbols().sym != nullptr) { name.Wrap(GetStaticSymbols().sym); }
#include <GameHooks/EngineHooks.inl>
#undef HOOK_DEFN
#endif

    //sym.CharStatsGetters.WrapAll();

    DetourTransactionCommit();
}

void EngineHooks::UnhookAll()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

#define HOOK_DEFN(name, sym, defn) name.Unwrap();
#include <GameHooks/EngineHooks.inl>
#undef HOOK_DEFN

    DetourTransactionCommit();
}

}
