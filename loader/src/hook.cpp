// hook.cpp — see hook.h. The decoder is calibrated against capstone on the prologue
// of every function in bin/bg3 (tools/hook-decode-check.py); the relocation and the
// patch are exercised by loader/test/hook_test.cpp.

#include "hook.h"

#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <mutex>

namespace bg3se::hook {

namespace {

// ---------------------------------------------------------------- decoder

// One-byte map. M: has ModRM. Immediates: B byte, W word, Z imm16/32 by operand
// size, V imm16/32/64 (B8+r), A moffs. X: invalid in 64-bit mode (or not handled).
enum : uint16_t { M = 1, B = 2, W = 4, Z = 8, V = 16, A = 32, X = 64, P = 128 /* prefix */ };

constexpr uint16_t one_byte(unsigned op)
{
    if (op < 0x40) {
        switch (op & 7) {
        case 0: case 1: case 2: case 3: return M;
        case 4: return B;
        case 5: return Z;
        default:  // 06/07/0E/16/17/1E/1F/27/2F/37/3F invalid; 26/2E/36/3E segment; 0F escape
            if (op == 0x26 || op == 0x2E || op == 0x36 || op == 0x3E) return P;
            return X;
        }
    }
    if (op < 0x50) return P;                     // REX, handled by the caller
    if (op < 0x60) return 0;                     // push/pop r
    switch (op) {
    case 0x63: return M;
    case 0x64: case 0x65: case 0x66: case 0x67: return P;
    case 0x68: return Z;
    case 0x69: return M | Z;
    case 0x6A: return B;
    case 0x6B: return M | B;
    case 0x6C: case 0x6D: case 0x6E: case 0x6F: return 0;
    case 0x80: case 0x83: return M | B;
    case 0x81: return M | Z;
    case 0x82: return X;
    case 0x9A: return X;
    case 0xA0: case 0xA1: case 0xA2: case 0xA3: return A;
    case 0xA8: return B;
    case 0xA9: return Z;
    case 0xC0: case 0xC1: case 0xC6: return M | B;
    case 0xC2: case 0xCA: return W;
    case 0xC7: return M | Z;
    case 0xC8: return W | B;
    case 0xCD: return B;
    case 0xCE: return X;
    case 0xD4: case 0xD5: case 0xD6: return X;
    case 0xE4: case 0xE5: case 0xE6: case 0xE7: return B;
    case 0xEA: return X;
    case 0xF0: case 0xF2: case 0xF3: return P;
    case 0xF6: case 0xF7: return M;              // test takes an immediate: see below
    }
    if (op >= 0x60 && op <= 0x62) return X;      // 62 is EVEX: not handled
    if (op >= 0x70 && op <= 0x7F) return B;      // jcc rel8
    if (op >= 0x84 && op <= 0x8F) return M;
    if (op >= 0xB0 && op <= 0xB7) return B;
    if (op >= 0xB8 && op <= 0xBF) return V;
    if (op >= 0xD0 && op <= 0xD3) return M;
    if (op >= 0xD8 && op <= 0xDF) return M;      // x87
    if (op >= 0xE0 && op <= 0xE3) return B;      // loop, jrcxz
    if (op == 0xE8 || op == 0xE9) return Z;      // rel32 (operand size is 32 in 64-bit mode)
    if (op == 0xEB) return B;
    if (op == 0xFE || op == 0xFF) return M;
    return 0;                                    // 90-9F, A4-AF, C3, C9, CB, CC, CF, EC-EF, F1, F4-FD
}

// Two-byte map (0F xx).
constexpr uint16_t two_byte(unsigned op)
{
    switch (op) {
    case 0x04: case 0x0A: case 0x0C: case 0x0F:  // 0F 0F is 3DNow!
    case 0x24: case 0x25: case 0x26: case 0x27:
    case 0x36: case 0x39: case 0x3B: case 0x3C: case 0x3D: case 0x3E: case 0x3F:
        return X;
    case 0x05: case 0x06: case 0x07: case 0x08: case 0x09: case 0x0B: case 0x0E:
    case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x37:
    case 0x77: case 0xA0: case 0xA1: case 0xA2: case 0xA8: case 0xA9: case 0xAA:
        return 0;
    case 0x70: case 0x71: case 0x72: case 0x73:
    case 0xA4: case 0xAC: case 0xBA: case 0xC2: case 0xC4: case 0xC5: case 0xC6:
        return M | B;
    }
    if (op >= 0x80 && op <= 0x8F) return Z;      // jcc rel32
    if (op >= 0xC8 && op <= 0xCF) return 0;      // bswap
    return M;
}

bool vex_has_imm8(unsigned map, unsigned op)
{
    if (map == 3) return true;
    if (map == 1) return (op >= 0x70 && op <= 0x73) || op == 0xC2 || (op >= 0xC4 && op <= 0xC6);
    return false;
}

// ModRM (+SIB, +displacement) at p; returns its length, or 0. Sets riprel if the
// operand is [rip + disp32], with the disp at offset `*disp_at` from p.
size_t modrm_len(const uint8_t* p, bool addr32, bool* riprel, size_t* disp_at)
{
    unsigned mod = p[0] >> 6, rm = p[0] & 7;
    size_t n = 1;
    *riprel = false;
    if (mod == 3) return n;
    if (rm == 4) {
        unsigned base = p[1] & 7;
        n++;
        if (mod == 0 && base == 5) {
            *disp_at = n;
            return n + 4;
        }
    } else if (mod == 0 && rm == 5) {
        *riprel = !addr32;   // with 0x67 it is eip-relative: reported as not understood
        *disp_at = n;
        if (addr32) return 0;
        return n + 4;
    }
    if (mod == 1) return n + 1;
    if (mod == 2) {
        *disp_at = n;
        return n + 4;
    }
    return n;
}

}  // namespace

bool decode(const uint8_t* p, uint64_t ip, Insn* out)
{
    *out = Insn{};
    size_t i = 0;
    bool opsize16 = false, addr32 = false, rexw = false;
    for (;; i++) {
        if (i >= 14) return false;
        unsigned b = p[i];
        if (b == 0x66) opsize16 = true;
        else if (b == 0x67) addr32 = true;
        else if (b == 0xF0 || b == 0xF2 || b == 0xF3 || b == 0x26 || b == 0x2E || b == 0x36 ||
                 b == 0x3E || b == 0x64 || b == 0x65) {
        } else break;
    }
    if ((p[i] & 0xF0) == 0x40) {                 // REX: must be the last prefix
        rexw = p[i] & 8;
        i++;
    }
    out->opcode_off = (uint8_t)i;
    unsigned op = p[i++];
    uint16_t f;
    size_t imm = 0;
    bool riprel = false;
    size_t disp_at = 0, modrm_at = 0;

    if (op == 0xC4 || op == 0xC5) {              // VEX
        unsigned map = 1;
        if (op == 0xC4) {
            map = p[i] & 0x1F;
            i += 2;
        } else {
            i += 1;
        }
        if (map < 1 || map > 3) return false;
        unsigned vop = p[i++];
        if (map == 1 && vop == 0x77) {           // vzeroupper / vzeroall: no ModRM
            out->len = (uint8_t)i;
            return true;
        }
        modrm_at = i;
        size_t m = modrm_len(p + i, addr32, &riprel, &disp_at);
        if (!m) return false;
        i += m;
        if (vex_has_imm8(map, vop)) imm = 1;
        f = M;
    } else if (op == 0x0F) {
        unsigned op2 = p[i++];
        if (op2 == 0x38 || op2 == 0x3A) {
            i++;                                 // the third opcode byte
            f = M | (op2 == 0x3A ? B : 0);
        } else {
            f = two_byte(op2);
            if (f & X) return false;
            if (op2 >= 0x80 && op2 <= 0x8F) {
                if (opsize16) return false;
                int32_t rel;
                memcpy(&rel, p + i, 4);
                out->len = (uint8_t)(i + 4);
                out->kind = Insn::JccRel32;
                out->target = ip + out->len + (int64_t)rel;
                return true;
            }
            if (op2 == 0x0B || op2 == 0xB9 || op2 == 0xFF) out->kind = Insn::Stop;  // ud2/ud1/ud0
        }
        if (f & M) {
            modrm_at = i;
            size_t m = modrm_len(p + i, addr32, &riprel, &disp_at);
            if (!m) return false;
            i += m;
        }
        if (f & B) imm = 1;
    } else {
        f = one_byte(op);
        if (f & (X | P)) return false;           // P here: a prefix after REX
        if (f & M) {
            modrm_at = i;
            size_t m = modrm_len(p + i, addr32, &riprel, &disp_at);
            if (!m) return false;
            i += m;
        }
        unsigned reg = (f & M) ? (p[modrm_at] >> 3) & 7 : 0;
        if (op == 0xF6 && reg < 2) imm = 1;
        else if (op == 0xF7 && reg < 2) imm = opsize16 ? 2 : 4;
        else if (f & B) imm = 1;
        else if (f & W) imm = 2 + ((f & B) ? 1 : 0);
        else if (f & Z) imm = opsize16 && op != 0xE8 && op != 0xE9 ? 2 : 4;
        else if (f & V) imm = rexw ? 8 : opsize16 ? 2 : 4;
        else if (f & A) imm = addr32 ? 4 : 8;
        if (op == 0xC8) imm = 3;

        if (op >= 0x70 && op <= 0x7F) out->kind = Insn::JccRel8;
        else if (op == 0xEB) out->kind = Insn::JmpRel8;
        else if (op == 0xE9) out->kind = Insn::JmpRel32;
        else if (op == 0xE8) out->kind = Insn::CallRel32;
        else if (op >= 0xE0 && op <= 0xE3) out->kind = Insn::LoopRel8;
        else if (op == 0xC3 || op == 0xC2) out->kind = Insn::Ret;
        else if (op == 0xCC || op == 0xF4 || op == 0xCB || op == 0xCA || op == 0xCF || op == 0xCD)
            out->kind = Insn::Stop;
        else if (op == 0xFF && (reg == 2 || reg == 3 || reg == 4 || reg == 5)) {
            // call/jmp through a register or memory: jmp ends the flow, call is a call
            out->kind = (reg == 4 || reg == 5) ? Insn::Stop : Insn::Other;
        }
    }
    i += imm;
    if (i > 15) return false;
    out->len = (uint8_t)i;
    if (riprel) {
        out->disp_off = (int8_t)(modrm_at + disp_at);
        int32_t d;
        memcpy(&d, p + out->disp_off, 4);
        out->target = ip + out->len + (int64_t)d;
    }
    if (out->kind == Insn::JccRel8 || out->kind == Insn::JmpRel8 || out->kind == Insn::LoopRel8) {
        out->target = ip + out->len + (int64_t)(int8_t)p[i - 1];
    } else if (out->kind == Insn::JmpRel32 || out->kind == Insn::CallRel32) {
        int32_t rel;
        memcpy(&rel, p + i - 4, 4);
        out->target = ip + out->len + (int64_t)rel;
    }
    return true;
}

namespace {

// ---------------------------------------------------------------- near memory

constexpr size_t kSlot = 128;          // relay (14 bytes, padded to 16) + trampoline
constexpr size_t kChunk = 1 << 16;
constexpr int64_t kReach = 0x7FF00000; // a little under ±2 GB, for the rel32s inside a slot

struct Chunk {
    uint8_t* base;
    size_t used;
};
Chunk g_chunks[64];
int g_nchunks = 0;

bool reachable(uint64_t a, uint64_t b)
{
    int64_t d = (int64_t)(a - b);
    return d > -kReach && d < kReach;
}

uint8_t* alloc_slot(uint64_t near)
{
    for (int k = 0; k < g_nchunks; k++) {
        Chunk& c = g_chunks[k];
        if (c.used + kSlot <= kChunk && reachable((uint64_t)c.base, near) &&
            reachable((uint64_t)c.base + kChunk, near)) {
            uint8_t* s = c.base + c.used;
            c.used += kSlot;
            return s;
        }
    }
    if (g_nchunks == (int)(sizeof g_chunks / sizeof *g_chunks)) return nullptr;
    // Ask for addresses on both sides of `near`, closest first; without MAP_FIXED the
    // kernel treats the address as a hint and may put the mapping anywhere, so check.
    uint64_t start = near & ~(uint64_t)(kChunk - 1);
    for (uint64_t step = kChunk; step < (uint64_t)kReach - kChunk; step += kChunk) {
        for (int side = 0; side < 2; side++) {
            uint64_t want = side ? start + step : start - step;
            if (!side && want > start) continue;          // wrapped below zero
            void* p = mmap((void*)want, kChunk, PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (p == MAP_FAILED) continue;
            if (reachable((uint64_t)p, near) && reachable((uint64_t)p + kChunk, near)) {
                g_chunks[g_nchunks++] = Chunk{(uint8_t*)p, kSlot};
                return (uint8_t*)p;
            }
            munmap(p, kChunk);
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------- patching

constexpr size_t kPatch = 5;

// Every target ever hooked keeps its slot: a thread may still be in the trampoline
// after remove(), and hooking the same target again reuses it (same address, so the
// same relocated bytes; only the detour pointer changes, by one aligned store).
struct Record {
    uint8_t* target;
    uint8_t* slot;
    bool active;
    uint8_t original[kPatch];
};
Record g_hooks[1024];
int g_nhooks = 0;
std::mutex g_mutex;

bool set_protection(uint8_t* at, size_t n, int prot)
{
    long page = sysconf(_SC_PAGESIZE);
    uintptr_t a = (uintptr_t)at & ~(uintptr_t)(page - 1);
    uintptr_t e = ((uintptr_t)at + n + page - 1) & ~(uintptr_t)(page - 1);
    return mprotect((void*)a, e - a, prot) == 0;
}

// Replace n <= 8 bytes at `at` with `bytes`, in one aligned atomic store.
Status write_atomic(uint8_t* at, const uint8_t* bytes, size_t n)
{
    uintptr_t a = (uintptr_t)at;
    bool in8 = (a & 7) + n <= 8, in16 = (a & 15) + n <= 16;
    if (!in8 && !in16) return Status::unaligned;
    uint8_t* word = (uint8_t*)(a & ~(uintptr_t)(in8 ? 7 : 15));
    if (!set_protection(word, 16, PROT_READ | PROT_WRITE | PROT_EXEC)) return Status::protect_failed;
    if (in8) {
        uint64_t v = __atomic_load_n((uint64_t*)word, __ATOMIC_ACQUIRE);
        memcpy((uint8_t*)&v + (at - word), bytes, n);
        __atomic_store_n((uint64_t*)word, v, __ATOMIC_RELEASE);
    } else {
        unsigned __int128 old, v;
        memcpy(&old, word, 16);
        do {
            v = old;
            memcpy((uint8_t*)&v + (at - word), bytes, n);
        } while (!__sync_bool_compare_and_swap((unsigned __int128*)word, old, v) &&
                 (memcpy(&old, word, 16), true));
    }
    set_protection(word, 16, PROT_READ | PROT_EXEC);
    return Status::ok;
}

void put_jmp_rel32(uint8_t* at, uint64_t from_ip, uint64_t to)
{
    at[0] = 0xE9;
    int32_t rel = (int32_t)(int64_t)(to - (from_ip + 5));
    memcpy(at + 1, &rel, 4);
}

// Copy the displaced instructions to `out` (which will execute at `out` itself),
// relocating rip-relative operands and relative branches. Returns bytes written.
Status relocate(const uint8_t* src, const Insn* ins, int count, uint8_t* out, size_t* written)
{
    size_t o = 0;
    for (int k = 0; k < count; k++) {
        const Insn& x = ins[k];
        uint64_t here = (uint64_t)out + o;
        switch (x.kind) {
        case Insn::JccRel8: {                    // 7x rel8 -> 0F 8x rel32
            if (x.opcode_off) return Status::not_relocatable;
            uint8_t cc = src[0] & 0x0F;
            if (!reachable(here, x.target)) return Status::not_relocatable;
            out[o] = 0x0F;
            out[o + 1] = 0x80 | cc;
            int32_t rel = (int32_t)(int64_t)(x.target - (here + 6));
            memcpy(out + o + 2, &rel, 4);
            o += 6;
            break;
        }
        case Insn::JmpRel8:
        case Insn::JmpRel32:
        case Insn::CallRel32:
            if (x.opcode_off) return Status::not_relocatable;
            if (!reachable(here, x.target)) return Status::not_relocatable;
            out[o] = x.kind == Insn::CallRel32 ? 0xE8 : 0xE9;
            {
                int32_t rel = (int32_t)(int64_t)(x.target - (here + 5));
                memcpy(out + o + 1, &rel, 4);
            }
            o += 5;
            break;
        case Insn::JccRel32: {
            if (x.opcode_off) return Status::not_relocatable;
            if (!reachable(here, x.target)) return Status::not_relocatable;
            out[o] = 0x0F;
            out[o + 1] = src[1];
            int32_t rel = (int32_t)(int64_t)(x.target - (here + 6));
            memcpy(out + o + 2, &rel, 4);
            o += 6;
            break;
        }
        case Insn::LoopRel8:
            return Status::not_relocatable;
        default:
            memcpy(out + o, src, x.len);
            if (x.disp_off >= 0) {
                if (!reachable(here, x.target)) return Status::not_relocatable;
                int32_t d = (int32_t)(int64_t)(x.target - (here + x.len));
                memcpy(out + o + x.disp_off, &d, 4);
            }
            o += x.len;
        }
        src += x.len;
    }
    *written = o;
    return Status::ok;
}

// Everything install() can decide from the bytes alone: the instructions to displace
// (at least kPatch bytes of them), and whether they can be.
Status plan(const uint8_t* code, uint64_t ip, Insn* ins, int* count, size_t* len)
{
    size_t n = 0;
    int c = 0;
    while (n < kPatch) {
        Insn& x = ins[c];
        if (!decode(code + n, ip + n, &x)) return Status::bad_insn;
        c++;
        n += x.len;
        if (n < kPatch && (x.kind == Insn::Ret || x.kind == Insn::Stop || x.kind == Insn::JmpRel8 ||
                           x.kind == Insn::JmpRel32))
            return Status::too_short;
    }
    for (int k = 0; k < c; k++) {
        const Insn& x = ins[k];
        bool rel = x.kind != Insn::Other && x.kind != Insn::Ret && x.kind != Insn::Stop;
        if (rel && x.target >= ip && x.target < ip + n) return Status::internal_branch;
        if (x.kind == Insn::LoopRel8 || (rel && x.opcode_off)) return Status::not_relocatable;
    }
    if ((ip & 7) + kPatch > 8 && (ip & 15) + kPatch > 16) return Status::unaligned;
    *count = c;
    *len = n;
    return Status::ok;
}

}  // namespace

Status probe(const uint8_t* code, size_t size, uint64_t ip)
{
    Insn ins[kPatch];
    int count;
    size_t n;
    Status st = plan(code, ip, ins, &count, &n);
    if (st != Status::ok || !size) return st;
    // the rest of the body, linearly: nothing may branch into the patched bytes
    for (size_t o = n; o < size;) {
        Insn x;
        if (!decode(code + o, ip + o, &x) || o + x.len > size) return Status::body_not_decoded;
        bool rel = x.kind != Insn::Other && x.kind != Insn::Ret && x.kind != Insn::Stop;
        if (rel && x.target > ip && x.target < ip + n) return Status::branch_into_patch;
        o += x.len;
    }
    return Status::ok;
}

const char* describe(Status s)
{
    switch (s) {
    case Status::ok: return "ok";
    case Status::bad_insn: return "instruction not understood";
    case Status::too_short: return "function may end within the patch";
    case Status::internal_branch: return "branch into the patched bytes";
    case Status::not_relocatable: return "instruction not relocatable";
    case Status::unaligned: return "patch fits no aligned word";
    case Status::no_memory: return "no memory within reach";
    case Status::protect_failed: return "mprotect failed";
    case Status::busy: return "already hooked / not hooked";
    case Status::branch_into_patch: return "a later branch lands inside the patch";
    case Status::body_not_decoded: return "function body not decodable to its end";
    }
    return "?";
}

Status install(void* target_, void* detour, void** original_out)
{
    auto* target = (uint8_t*)target_;
    std::lock_guard<std::mutex> lock(g_mutex);
    Record* r = nullptr;
    for (int k = 0; k < g_nhooks; k++)
        if (g_hooks[k].target == target) r = &g_hooks[k];
    if (r && r->active) return Status::busy;
    if (!r && g_nhooks == (int)(sizeof g_hooks / sizeof *g_hooks)) return Status::no_memory;

    Insn ins[kPatch];
    int count = 0;
    size_t n = 0;
    Status st = plan(target, (uint64_t)target, ins, &count, &n);
    if (st != Status::ok) return st;

    uint8_t* slot = r ? r->slot : alloc_slot((uint64_t)target);
    if (!slot) return Status::no_memory;
    uint8_t* tramp = slot + 16;
    if (!r) {
        // relay: jmp [rip+2]; 2 bytes of int3; dq detour (at +8, aligned)
        const uint8_t relay[8] = {0xFF, 0x25, 0x02, 0x00, 0x00, 0x00, 0xCC, 0xCC};
        memcpy(slot, relay, 8);
        size_t w = 0;
        st = relocate(target, ins, count, tramp, &w);
        if (st != Status::ok) return st;         // the slot is leaked; slots are cheap
        put_jmp_rel32(tramp + w, (uint64_t)tramp + w, (uint64_t)target + n);
    }
    __atomic_store_n((void**)(slot + 8), detour, __ATOMIC_RELEASE);

    uint8_t original[kPatch], patch[kPatch];
    memcpy(original, target, kPatch);
    put_jmp_rel32(patch, (uint64_t)target, (uint64_t)slot);
    st = write_atomic(target, patch, kPatch);
    if (st != Status::ok) return st;
    if (!r) {
        r = &g_hooks[g_nhooks++];
        r->target = target;
        r->slot = slot;
    }
    memcpy(r->original, original, kPatch);
    r->active = true;
    if (original_out) *original_out = tramp;
    return Status::ok;
}

Status remove(void* target_)
{
    auto* target = (uint8_t*)target_;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (int k = 0; k < g_nhooks; k++) {
        Record& r = g_hooks[k];
        if (r.target != target || !r.active) continue;
        Status st = write_atomic(target, r.original, kPatch);
        if (st != Status::ok) return st;
        r.active = false;
        return Status::ok;
    }
    return Status::busy;
}

const void* trampoline_owner(uint64_t ip)
{
    // No lock: the table only ever grows, and this runs from a signal handler where
    // taking g_mutex could deadlock against the thread that faulted while holding it.
    int n = g_nhooks;
    for (int k = 0; k < n; k++) {
        auto slot = (uint64_t)g_hooks[k].slot;
        if (slot && ip >= slot && ip < slot + kSlot) return g_hooks[k].target;
    }
    return nullptr;
}

}  // namespace bg3se::hook
