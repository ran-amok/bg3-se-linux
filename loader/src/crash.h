// crash.h — read a fatal fault before Larian's own handler eats it.
//
// bin/bg3 installs LinuxCrashReporter on SIGSEGV and friends: the handler re-execs a
// reporter, and by the time it is up the faulting process is gone. There is no core
// (`coredumpctl` has nothing, and the reporter keeps whatever it bundles to itself),
// so a crash inside a hook or a mirror leaves nothing to read. A crash
// owes a frame and a faulting access before anything else.
//
// arm() interposes sigaction(2) so that whatever the game installs for a fatal signal
// is kept and ours goes in front. On a fault we write one block to bg3se.log — signal,
// si_addr, the faulting rip as `bg3+0xOFFSET` when it is inside the game image, the
// hooked target when it is inside one of our trampolines, and a bounded scan of the
// stack for return addresses in the same image — then chain to the game's handler, so
// the reporter still behaves exactly as it did.
//
// Everything the handler reads is cached before the first fault: no dl_iterate_phdr,
// no dladdr, no allocation on the signal path.
#pragma once

#include <cstdint>

namespace bg3se::crash {

// Once, from deferred_init, before the game installs its own handlers. A no-op when
// BG3SE_NO_CRASH_READER is set.
void arm();

// Install this thread's alternate signal stack, once per thread; a no-op before arm()
// and when the thread already has one. Cheap (one TLS read) and idempotent, so it goes
// at the top of anything of ours that runs on a game thread -- log_line and osi_line
// between them see every thread we speak on.
//
// Why it exists: install_ours() passes SA_ONSTACK, and sigaltstack(2) is per-thread
// state, so the flag does nothing on a thread that has not set one. On a *stack overflow*
// SIGSEGV the handler then has no stack to run on and the process dies with no CRASH
// block at all.
// A thread that never logs still gets no alternate stack; hooking thread creation is
// what would close that, and it has not been needed yet.
void ensure_alt_stack();

// Add a later image to the table the handler resolves frames against -- the extender
// proper (libbg3se-ext.so) is dlopened after arm(), and until it is noted every return
// address inside it is dropped by the stack scan. `addr` is any
// address inside the image; `name` is what the CRASH lines will print. Idempotent, and a
// no-op before arm(). Not signal-safe: call it from ordinary code, once.
void note_image(const void* addr, const char* name);

// Test knob: raise the fault ourselves and return (the handler chains to SIG_DFL when
// the game has installed nothing). Used by loader/test/run.sh.
void selftest_fault();

}  // namespace bg3se::crash
