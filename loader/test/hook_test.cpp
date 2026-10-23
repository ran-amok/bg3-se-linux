// hook-test: the hooking engine on hand-written prologues (hook_test_fns.S). For each
// relocatable case: the value before, through a detour that calls the trampoline, and
// after remove(). Then the refusals, and threads calling a function while it is
// hooked and unhooked underneath them: one patched by an 8-byte store, one by the
// 16-byte cmpxchg. Both start with an instruction of 5 bytes or more, because what
// the engine promises is that a thread *entering* the function never sees a torn
// patch; a thread paused between two displaced instructions is not handled (hook.h).
#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>

#include "hook.h"

using namespace bg3se::hook;
using Fn = long (*)(long);

extern "C" long fn_plain(long), fn_riprel(long), fn_jcc8(long), fn_jmp32(long), fn_call(long),
    fn_cx16(long), fn_short(long), fn_internal(long), fn_jrcxz(long), fn_unaligned(long);

namespace {

int g_fail = 0;

void check(bool ok, const char* what)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) g_fail = 1;
}

// One detour per case, each calling its own trampoline: +100 over the original.
template <int N>
struct Detour {
    static inline Fn orig = nullptr;
    static long fn(long x) { return orig(x) + 100; }
};

template <int N>
void relocatable(const char* name, Fn f, long x, long want)
{
    // call through a volatile pointer: the compiler must not assume what f does
    Fn volatile call = f;
    char what[160];
    snprintf(what, sizeof what, "%-12s before: %ld", name, want);
    check(call(x) == want, what);
    void* orig = nullptr;
    Status st = install((void*)f, (void*)&Detour<N>::fn, &orig);
    snprintf(what, sizeof what, "%-12s install: %s", name, describe(st));
    check(st == Status::ok, what);
    if (st != Status::ok) return;
    Detour<N>::orig = (Fn)orig;
    snprintf(what, sizeof what, "%-12s hooked: %ld (got %ld)", name, want + 100, call(x));
    check(call(x) == want + 100, what);
    snprintf(what, sizeof what, "%-12s trampoline alone: %ld", name, want);
    check(((Fn)orig)(x) == want, what);
    check(install((void*)f, (void*)&Detour<N>::fn, nullptr) == Status::busy,
          "             second install refused");
    st = remove((void*)f);
    snprintf(what, sizeof what, "%-12s remove: %s, back to %ld", name, describe(st), want);
    check(st == Status::ok && call(x) == want, what);
}

void refused(const char* name, Fn f, Status want)
{
    unsigned char before[8], after[8];
    memcpy(before, (void*)f, 8);
    Status st = install((void*)f, (void*)&Detour<99>::fn, nullptr);
    memcpy(after, (void*)f, 8);
    char what[160];
    snprintf(what, sizeof what, "%-12s refused: %s (want: %s), bytes untouched", name, describe(st),
             describe(want));
    check(st == want && !memcmp(before, after, 8), what);
}

template <int N>
void hammer(const char* name, Fn f, long add)
{
    std::atomic<bool> stop{false};
    std::atomic<long> bad{0}, calls{0};
    std::thread t([&] {
        Fn volatile call = f;
        while (!stop.load(std::memory_order_relaxed)) {
            long r = call(1);
            if (r != 1 + add && r != 101 + add) bad++;
            calls++;
        }
    });
    int rounds = 0;
    Status st = Status::ok;
    for (; rounds < 20000; rounds++) {
        void* orig;
        if ((st = install((void*)f, (void*)&Detour<N>::fn, &orig)) != Status::ok) break;
        Detour<N>::orig = (Fn)orig;
        if ((st = remove((void*)f)) != Status::ok) break;
    }
    stop = true;
    t.join();
    char what[160];
    snprintf(what, sizeof what, "%-12s concurrent: %d install/remove rounds under %ld calls, %ld wrong (%s)",
             name, rounds, calls.load(), bad.load(), describe(st));
    check(rounds == 20000 && bad == 0, what);
}

}  // namespace

int main()
{
    relocatable<1>("plain", fn_plain, 41, 42);
    relocatable<2>("riprel", fn_riprel, 1, 1001);
    relocatable<3>("jcc8 taken", fn_jcc8, 0, -1);
    relocatable<4>("jcc8 not", fn_jcc8, 5, 7);
    relocatable<5>("jmp32", fn_jmp32, 1, 4);
    relocatable<6>("call", fn_call, 1, 6);
    relocatable<7>("cx16", fn_cx16, 1, 7);

    refused("short", fn_short, Status::too_short);
    refused("internal", fn_internal, Status::internal_branch);
    refused("jrcxz", fn_jrcxz, Status::not_relocatable);
    refused("unaligned", fn_unaligned, Status::unaligned);

    hammer<2>("riprel", fn_riprel, 1000);
    hammer<7>("cx16", fn_cx16, 6);
    return g_fail;
}
