// hook.h — inline hooks on functions inside bin/bg3 (the targets are not imported, so
// neither symbol interposition nor the GOT reaches them).
//
// install() overwrites the first 5 bytes of the target with `jmp rel32` to a relay
// (`jmp [rip+2]; dq detour`) allocated within ±2 GB, and hands back a trampoline:
// the displaced instructions, relocated, then `jmp rel32` back into the target.
//
// Fail-closed: anything the decoder does not know, a prologue that ends (ret, jmp,
// ud2) or branches back into itself before 5 bytes, a target that is not 8- or
// 16-byte placed well enough for one atomic store — all refuse and change nothing.
// What install() cannot see is a branch from later in the function into bytes 1..4
// of the patch; probe() with the function's size looks for that, offline.
//
// The 5 bytes are written with one aligned atomic store, so a thread never fetches
// a torn instruction; a thread already *inside* the first 5 bytes when the store
// lands is not handled. Upstream is in the same place: it calls DetourUpdateThread on
// its own thread only, so Detours never suspends the others, and it hooks at startup.
//
// A trampoline has no unwind info. That matters only where a displaced instruction is
// a call (its return address is then in the trampoline) and an exception crosses it.
#pragma once

#include <cstddef>
#include <cstdint>

namespace bg3se::hook {

// One decoded x86-64 instruction, as much as relocation needs.
struct Insn {
    enum Kind : uint8_t {
        Other,
        JmpRel8, JmpRel32,        // unconditional, target = ip + len + rel
        JccRel8, JccRel32,        // conditional
        CallRel32,
        LoopRel8,                 // loop/jrcxz: rel8 only, not relocatable
        Ret,                      // ret, ret imm16
        Stop,                     // int3, ud2, hlt, jmp/call through a register or memory...
    };
    uint8_t len = 0;
    Kind kind = Other;
    int8_t disp_off = -1;         // offset of a rip-relative disp32 in the instruction
    uint8_t opcode_off = 0;       // offset of the (first) opcode byte, after prefixes
    uint64_t target = 0;          // for the rel kinds, and for rip-relative operands
};

// Decode the instruction at p (which executes at ip). False: not understood.
bool decode(const uint8_t* p, uint64_t ip, Insn* out);

enum class Status {
    ok,
    bad_insn,         // the decoder does not know an instruction in the first 5 bytes
    too_short,        // ret/jmp/ud2... before 5 bytes: the function may end there
    internal_branch,  // a branch in the prologue lands inside the patched bytes
    not_relocatable,  // loop/jrcxz, or a rip-relative operand out of reach of the trampoline
    unaligned,        // the 5 bytes fit no aligned 8- or 16-byte word
    no_memory,        // no page within ±2 GB
    protect_failed,   // mprotect refused
    busy,             // target already hooked (or not hooked, for remove)
    // probe() only, with the function's size:
    branch_into_patch,  // a direct branch later in the body lands in bytes 1..4
    body_not_decoded,   // the sweep met an instruction the decoder does not know
};
const char* describe(Status s);

// What install() would say from the bytes alone (not memory, not mprotect), for the
// audit: `code` holds the function that runs at `ip`. With its size (from the FDE),
// the whole body is also swept for direct branches into the patched bytes — the case
// install() cannot see. Indirect ones (jump tables) are not seen by either.
Status probe(const uint8_t* code, size_t size, uint64_t ip);

// On ok, *original (if given) is a callable trampoline to the unhooked function.
Status install(void* target, void* detour, void** original);
// Put the original bytes back. The trampoline stays mapped: a thread may be inside.
Status remove(void* target);

// The hooked target whose relay/trampoline slot contains `ip`, or nullptr. For the
// crash reader (crash.cpp): a fault inside a slot has no image and no symbol, and this
// is the only thing that can name it. Reads the record table without the lock -- it is
// append-only, so the worst case is missing a hook installed in the last instant.
const void* trampoline_owner(uint64_t ip);

}  // namespace bg3se::hook
