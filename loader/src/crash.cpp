// crash.cpp — see crash.h.

#include "crash.h"

#include <dlfcn.h>
#include <errno.h>
#include <link.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ucontext.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>

#include <sys/syscall.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "hook.h"
#include "loader.h"

using namespace bg3se;

namespace {

// The signals bin/bg3 hands to LinuxCrashReporter, plus the two a bad hook produces.
constexpr int kFatal[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP};
constexpr int kNFatal = (int)(sizeof kFatal / sizeof *kFatal);

struct Chained {
    struct sigaction act;
    bool set = false;
};
Chained g_chained[kNFatal];

// One mapped image, as the handler needs it: everything resolved before the fault.
// `base` is the load base and `lo/hi` the executable range: membership is decided by the
// executable range (a data pointer on the stack is not a frame), but the offset printed
// is against the *base*, which is what tools/disasm.py takes. bin/bg3's first executable
// segment sits at vaddr 0x218be00, so the two differ by 35 MB, and an offset printed
// against the segment would resolve to nothing.
struct Image {
    const char* name;
    uintptr_t base, lo, hi;
};
Image g_images[3];   // [0] bin/bg3, [1] libbg3se.so, [2] libbg3se-ext.so (note_image)
int g_nimages = 0;

std::atomic<bool> g_armed{false};
std::atomic<int> g_in_handler{0};

using SigactionFn = int (*)(int, const struct sigaction*, struct sigaction*);

// This object exports `sigaction`, so a call to it from inside binds to the interposer
// below and not to libc -- every one here goes through the next definition in the
// lookup order instead. A plain ::sigaction() here recurses forever.
SigactionFn real_sigaction()
{
    static SigactionFn fn = nullptr;
    if (!fn) fn = (SigactionFn)dlsym(RTLD_NEXT, "sigaction");
    return fn;
}

int fatal_index(int sig)
{
    for (int i = 0; i < kNFatal; i++)
        if (kFatal[i] == sig) return i;
    return -1;
}

// --------------------------------------------------------------- image ranges

struct ScanCtx {
    uintptr_t self;
    // PORT (linux): set by note_image() for an image that is not the loader -- the main
    // image is then not wanted either (it is already in the table), so `name` doubles as
    // "this is a later, named image" and suppresses the is_game branch.
    const char* name = nullptr;
};

int collect(struct dl_phdr_info* info, size_t, void* arg)
{
    auto* ctx = (ScanCtx*)arg;
    bool is_game = ctx->name == nullptr
                   && (info->dlpi_name == nullptr || info->dlpi_name[0] == 0);  // the main image
    bool is_self = false;
    uintptr_t lo = UINTPTR_MAX, hi = 0;
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr)& p = info->dlpi_phdr[i];
        if (p.p_type != PT_LOAD || !(p.p_flags & PF_X)) continue;
        uintptr_t a = info->dlpi_addr + p.p_vaddr;
        if (a < lo) lo = a;
        if (a + p.p_memsz > hi) hi = a + p.p_memsz;
    }
    if (lo >= hi) return 0;
    if (ctx->self >= lo && ctx->self < hi) is_self = true;
    if (!is_game && !is_self) return 0;
    if (g_nimages == (int)(sizeof g_images / sizeof *g_images)) return 0;
    g_images[g_nimages].name = is_game ? "bg3" : (ctx->name ? ctx->name : "libbg3se.so");
    g_images[g_nimages].base = info->dlpi_addr;
    g_images[g_nimages].lo = lo;
    g_images[g_nimages].hi = hi;
    g_nimages++;
    return 0;
}

const Image* image_of(uintptr_t a)
{
    for (int i = 0; i < g_nimages; i++)
        if (a >= g_images[i].lo && a < g_images[i].hi) return &g_images[i];
    return nullptr;
}

// --------------------------------------------------------------- the handler

const char* signame(int s)
{
    switch (s) {
        case SIGSEGV: return "SIGSEGV";
        case SIGBUS:  return "SIGBUS";
        case SIGILL:  return "SIGILL";
        case SIGFPE:  return "SIGFPE";
        case SIGABRT: return "SIGABRT";
        case SIGTRAP: return "SIGTRAP";
        default:      return "signal";
    }
}

// `bg3+0x3c4ae20` when the address is in an image we know (the offset tools/disasm.py
// takes), the raw address otherwise. Two rotating buffers so one line can hold two.
const char* where(uintptr_t a, int slot)
{
    static char buf[2][64];
    char* b = buf[slot & 1];
    const Image* im = image_of(a);
    if (!im) {
        const void* owner = hook::trampoline_owner(a);
        if (owner) {
            // In one of our slots: name the target it belongs to, which is a game offset.
            const Image* g = image_of((uintptr_t)owner);
            snprintf(b, sizeof buf[0], "0x%lx (our trampoline for %s+0x%lx)", (unsigned long)a,
                     g ? g->name : "?", (unsigned long)((uintptr_t)owner - (g ? g->base : 0)));
            return b;
        }
        snprintf(b, sizeof buf[0], "0x%lx", (unsigned long)a);
        return b;
    }
    snprintf(b, sizeof buf[0], "%s+0x%lx", im->name, (unsigned long)(a - im->base));
    return b;
}

// ------------------------------------------------------- the alternate signal stack

// Generous on purpose: our own handler and then the game's chained one both run on it,
// and LinuxCrashReporter is not a small function. SIGSTKSZ is 8 kB under the glibc 2.31
// floor, which is thinner than what the reporter may want. Virtual only -- the pages
// are touched on a crash and never before -- with a PROT_NONE page below it, so an
// overflow *of the alternate stack* faults instead of walking into whatever is there.
constexpr size_t kAltStack = 512 * 1024;

__thread void* t_alt = nullptr;
std::atomic<int> g_alt_count{0};
// BG3SE_NO_ALT_STACK=1 leaves the alternate stacks out, which is how the fix is
// calibrated: the same overflow then kills the process with no CRASH block at all
// (loader/test/run.sh runs it both ways). Read once, at arm() -- not on the signal path.
bool g_no_alt = false;

// ---------------------------------------------------- reading the faulting stack

// The stack is copied with process_vm_readv(2) rather than dereferenced. On a
// stack-overflow SIGSEGV the faulting rsp is *below* the guard page, so the old
// `((uintptr_t*)rsp)[i]` faulted inside the handler and took the block with it; a
// syscall returns EFAULT instead, and how much of the stack it can read is itself
// evidence. A partial read (a mapped prefix, then a hole) comes back as the prefix's
// length, which is what we want. Returns -1 with errno set.
ssize_t read_stack(uintptr_t addr, void* out, size_t n)
{
    struct iovec l{out, n};
    struct iovec r{(void*)addr, n};
    return process_vm_readv(getpid(), &l, 1, &r, 1, 0);
}

void handler(int sig, siginfo_t* si, void* uc)
{
    // A fault inside the handler must not loop; let the second one through to the game.
    if (g_in_handler.fetch_add(1) == 0) {
        auto* c = (ucontext_t*)uc;
        auto& g = c->uc_mcontext.gregs;
        uintptr_t rip = (uintptr_t)g[REG_RIP], rsp = (uintptr_t)g[REG_RSP];

        log_line("CRASH %s at rip=%s, si_addr=%s, si_code=%d, tid=%d", signame(sig),
                 where(rip, 0), where((uintptr_t)si->si_addr, 1), si->si_code,
                 (int)syscall(SYS_gettid));
        log_line("CRASH  rax=0x%lx rbx=0x%lx rcx=0x%lx rdx=0x%lx rsi=0x%lx rdi=0x%lx",
                 (unsigned long)g[REG_RAX], (unsigned long)g[REG_RBX], (unsigned long)g[REG_RCX],
                 (unsigned long)g[REG_RDX], (unsigned long)g[REG_RSI], (unsigned long)g[REG_RDI]);
        log_line("CRASH  rbp=0x%lx rsp=0x%lx r8=0x%lx r9=0x%lx r12=0x%lx r13=0x%lx",
                 (unsigned long)g[REG_RBP], (unsigned long)rsp, (unsigned long)g[REG_R8],
                 (unsigned long)g[REG_R9], (unsigned long)g[REG_R12], (unsigned long)g[REG_R13]);

        // No frame pointers under LTO, and a trampoline has no unwind info, so this is a
        // scan and not a backtrace: the words above rsp that land in an image we know.
        // Some are live return addresses and some are stale; every one is a candidate to
        // read with tools/disasm.py, and the order is the order they sit on the stack.
        //
        // Copied out first (read_stack), because rsp itself may be unmapped -- which is
        // the diagnosis, not an obstacle: it means the thread overflowed its stack.
        uintptr_t from = rsp;
        size_t avail = 0;
        auto* words = (uintptr_t*)scratch(kScratchCrash);
        bool direct = false;
        if (words) {
            ssize_t got = read_stack(rsp, words, kScratchSize);
            if (got < 0 && errno != EFAULT) {
                // The syscall is unusable here (seccomp, ptrace policy); fall back to a
                // plain dereference, and say that is what happened.
                log_line("CRASH  stack: process_vm_readv unusable (errno %d), reading it directly",
                         errno);
                direct = true;
            } else if (got <= 0) {
                log_line("CRASH  stack: rsp=0x%lx is not mapped — this thread overflowed its "
                         "stack; the frames below are what is left above the guard page",
                         (unsigned long)rsp);
                uintptr_t page = (rsp + 0xfff) & ~(uintptr_t)0xfff;
                for (int i = 0; i < 64 && got <= 0; i++, page += 0x1000) {
                    got = read_stack(page, words, kScratchSize);
                    if (got > 0) from = page;
                }
                if (got > 0)
                    log_line("CRASH  stack: first mapped word at 0x%lx (0x%lx past rsp)",
                             (unsigned long)from, (unsigned long)(from - rsp));
                else
                    log_line("CRASH  stack: nothing mapped in the 256 kB above rsp either");
            }
            if (got > 0) avail = (size_t)got;
        } else {
            direct = true;
        }

        int found = 0;
        size_t nwords = direct ? 1024 : avail / sizeof(uintptr_t);
        for (size_t i = 0; i < nwords && found < 16; i++) {
            uintptr_t w = direct ? ((uintptr_t*)rsp)[i] : words[i];
            if (!image_of(w) && !hook::trampoline_owner(w)) continue;
            log_line("CRASH  stack[+0x%lx] %s", (unsigned long)(from - rsp + i * 8), where(w, 0));
            found++;
        }
        if (!found) log_line("CRASH  stack: nothing in a known image in the %zu bytes read",
                             nwords * sizeof(uintptr_t));
    }
    g_in_handler.fetch_sub(1);

    // Chain: the game's reporter still sees exactly the signal it would have seen.
    int idx = fatal_index(sig);
    if (idx >= 0 && g_chained[idx].set) {
        const struct sigaction& a = g_chained[idx].act;
        if ((a.sa_flags & SA_SIGINFO) && a.sa_sigaction) {
            a.sa_sigaction(sig, si, uc);
            return;
        }
        if (a.sa_handler != SIG_IGN && a.sa_handler != SIG_DFL && a.sa_handler) {
            a.sa_handler(sig);
            return;
        }
        if (a.sa_handler == SIG_IGN) return;
    }
    // Nothing to chain to: restore the default and let it fault again, so the exit
    // status is the real one.
    struct sigaction dfl{};
    dfl.sa_handler = SIG_DFL;
    sigemptyset(&dfl.sa_mask);
    if (auto fn = real_sigaction()) fn(sig, &dfl, nullptr);
}

void install_ours(int sig)
{
    struct sigaction ours{};
    ours.sa_sigaction = handler;
    ours.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
    sigemptyset(&ours.sa_mask);
    if (auto fn = real_sigaction()) fn(sig, &ours, nullptr);
}

}  // namespace

// The interposer. bin/bg3 installs LinuxCrashReporter through this; we keep what it
// asked for, answer its oldact from what *it* last set, and leave ours in the kernel.
extern "C" __attribute__((visibility("default")))
int sigaction(int sig, const struct sigaction* act, struct sigaction* oldact)
{
    SigactionFn real = real_sigaction();
    if (!real) return -1;

    int idx = g_armed.load() ? fatal_index(sig) : -1;
    if (idx < 0) return real(sig, act, oldact);

    if (oldact) {
        if (g_chained[idx].set) *oldact = g_chained[idx].act;
        else real(sig, nullptr, oldact);
    }
    if (act) {
        g_chained[idx].act = *act;
        g_chained[idx].set = true;
        log_line("crash reader: kept the game's handler for %s, staying in front", signame(sig));
        install_ours(sig);
    }
    return 0;
}

// bin/bg3 imports signal(2) as well as sigaction(2). glibc implements it on top of
// rt_sigaction internally, so a call to it would never reach the interposer above --
// and would take the fatal signals back from us without a word.
extern "C" __attribute__((visibility("default")))
void (*signal(int sig, void (*handler)(int)))(int)
{
    using SignalFn = void (*(*)(int, void (*)(int)))(int);
    static SignalFn real = nullptr;
    if (!real) real = (SignalFn)dlsym(RTLD_NEXT, "signal");

    int idx = g_armed.load() ? fatal_index(sig) : -1;
    if (idx < 0) return real ? real(sig, handler) : SIG_ERR;

    auto prev = g_chained[idx].set ? g_chained[idx].act.sa_handler : SIG_DFL;
    struct sigaction act{};
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    g_chained[idx].act = act;
    g_chained[idx].set = true;
    log_line("crash reader: kept the game's signal() handler for %s, staying in front",
             signame(sig));
    install_ours(sig);
    return prev;
}

// ------------------------------------------------------- the exit interposers
//
// The LoadMenu crash leaves no CRASH block, no core and no Crashpad minidump -- three
// independent signal paths that all miss it -- while Steam's
// log times one of those game processes as gone 94 s into its launch. An exit that no
// signal handler sees is either a call (exit(3), _exit(2), _Exit, quick_exit) or an
// external SIGKILL, which nothing inside the process can observe. These interposers
// separate the two, and if it is a call they name the caller: its return address as
// `bg3+0xOFFSET`, which tools/disasm.py takes directly.
//
// They change nothing else -- one line, then the real function, and the fallback is the
// raw exit_group syscall rather than our own _exit (which would recurse back here).
// A forked child that calls _exit is logged too; log_line prints the pid, so a child's
// line is told apart from the game's. Silence from all five, on a process that is gone,
// is itself the measurement: it was killed from outside.
namespace {

void log_exit(const char* who, int status, void* ret)
{
    log_line("EXIT %s(%d) from %s, tid=%d", who, status, where((uintptr_t)ret, 0),
             (int)syscall(SYS_gettid));
}

}  // namespace

#define BG3SE_EXIT_INTERPOSER(name)                                            \
    extern "C" __attribute__((visibility("default"), noreturn))                \
    void name(int status)                                                      \
    {                                                                          \
        using Fn = void (*)(int);                                              \
        static Fn real = nullptr;                                              \
        if (!real) real = (Fn)dlsym(RTLD_NEXT, #name);                         \
        log_exit(#name, status, __builtin_return_address(0));                  \
        if (real) real(status);                                                \
        syscall(SYS_exit_group, status);                                       \
        __builtin_unreachable();                                               \
    }

BG3SE_EXIT_INTERPOSER(exit)
BG3SE_EXIT_INTERPOSER(_exit)
BG3SE_EXIT_INTERPOSER(_Exit)
BG3SE_EXIT_INTERPOSER(quick_exit)

#undef BG3SE_EXIT_INTERPOSER

// The other half of "gone without a word". A launch can end with no CRASH block, no
// core, no Crashpad minidump and no EXIT line, and Larian's reporter still coming up --
// which is what a SIGKILL looks like from inside: uncatchable, so neither our handler
// nor Crashpad's sees it, and the parent then finds WIFSIGNALED and starts the reporter.
// If the game kills itself, it goes through one of
// these; if it does not, the kill came from outside the process and these stay silent.
// Only the signals that would end the process are logged, so the game's ordinary signal
// traffic does not fill the log.
namespace {

bool terminating(int sig)
{
    switch (sig) {
        case SIGKILL: case SIGTERM: case SIGQUIT: case SIGINT: case SIGHUP:
        case SIGABRT: case SIGSEGV: case SIGBUS: case SIGILL: case SIGFPE:
        case SIGTRAP: case SIGSYS: case SIGXCPU: case SIGXFSZ:
            return true;
        default:
            return false;
    }
}

void log_kill(const char* who, long target, int sig, void* ret)
{
    if (!terminating(sig)) return;
    log_line("KILL %s(%ld, %d) from %s, tid=%d", who, target, sig, where((uintptr_t)ret, 0),
             (int)syscall(SYS_gettid));
}

}  // namespace

extern "C" __attribute__((visibility("default")))
int kill(pid_t pid, int sig)
{
    using Fn = int (*)(pid_t, int);
    static Fn real = nullptr;
    if (!real) real = (Fn)dlsym(RTLD_NEXT, "kill");
    log_kill("kill", (long)pid, sig, __builtin_return_address(0));
    return real ? real(pid, sig) : -1;
}

extern "C" __attribute__((visibility("default")))
int raise(int sig)
{
    using Fn = int (*)(int);
    static Fn real = nullptr;
    if (!real) real = (Fn)dlsym(RTLD_NEXT, "raise");
    log_kill("raise", (long)syscall(SYS_gettid), sig, __builtin_return_address(0));
    return real ? real(sig) : -1;
}

extern "C" __attribute__((visibility("default")))
int pthread_kill(pthread_t th, int sig)
{
    using Fn = int (*)(pthread_t, int);
    static Fn real = nullptr;
    if (!real) real = (Fn)dlsym(RTLD_NEXT, "pthread_kill");
    log_kill("pthread_kill", (long)th, sig, __builtin_return_address(0));
    return real ? real(th, sig) : -1;
}

// And the view from the parent. bin/bg3 forks: the outer process sits in wait4(2) for the
// whole run (its backtrace is wait4 <- ?? <- main), and it is the only
// thing in the system that learns *how* the game process died -- the wait status carries
// either the exit code or the terminating signal. We are preloaded into that process too,
// so one line here answers the question the four silent observation points leave open.
namespace {

void log_wait(const char* who, long pid, int status)
{
    if (pid <= 0) return;
    if (WIFSIGNALED(status))
        log_line("WAIT %s: pid %ld killed by signal %d%s", who, pid, WTERMSIG(status),
                 WCOREDUMP(status) ? " (core)" : "");
    else if (WIFEXITED(status))
        log_line("WAIT %s: pid %ld exited with %d", who, pid, WEXITSTATUS(status));
}

}  // namespace

extern "C" __attribute__((visibility("default")))
pid_t wait4(pid_t pid, int* status, int options, struct rusage* usage)
{
    using Fn = pid_t (*)(pid_t, int*, int, struct rusage*);
    static Fn real = nullptr;
    if (!real) real = (Fn)dlsym(RTLD_NEXT, "wait4");
    if (!real) return -1;
    pid_t r = real(pid, status, options, usage);
    if (r > 0 && status) log_wait("wait4", (long)r, *status);
    return r;
}

extern "C" __attribute__((visibility("default")))
pid_t waitpid(pid_t pid, int* status, int options)
{
    using Fn = pid_t (*)(pid_t, int*, int);
    static Fn real = nullptr;
    if (!real) real = (Fn)dlsym(RTLD_NEXT, "waitpid");
    if (!real) return -1;
    pid_t r = real(pid, status, options);
    if (r > 0 && status) log_wait("waitpid", (long)r, *status);
    return r;
}

// abort(3) raises SIGABRT, which the handler above already catches -- but only on a
// thread that has an alternate stack, and only while we are still in front of the
// game's handler. This line costs nothing and does not depend on either.
extern "C" __attribute__((visibility("default"), noreturn))
void abort(void)
{
    using Fn = void (*)(void);
    static Fn real = nullptr;
    if (!real) real = (Fn)dlsym(RTLD_NEXT, "abort");
    log_exit("abort", 0, __builtin_return_address(0));
    if (real) real();
    syscall(SYS_exit_group, 134);
    __builtin_unreachable();
}

void bg3se::crash::ensure_alt_stack()
{
    if (t_alt || g_no_alt || !g_armed.load()) return;

    // Leave one the thread already has alone -- SDL, the game or a sanitizer may have
    // installed it, and ours would be no better.
    stack_t cur{};
    if (sigaltstack(nullptr, &cur) == 0 && cur.ss_sp && !(cur.ss_flags & SS_DISABLE) &&
        cur.ss_size >= (size_t)MINSIGSTKSZ) {
        t_alt = cur.ss_sp;
        // Say so: which of the game's threads bring their own is the thing that decides
        // whether the lazy install above is enough, and silence here reads as "no thread
        // needed one" exactly like it reads as "the branch never ran".
        int n = g_alt_count.fetch_add(1) + 1;
        if (n <= 4 || n % 16 == 0)
            log_line("crash reader: tid %d already had an alt stack (%zu kB, %d thread%s so far)",
                     (int)syscall(SYS_gettid), cur.ss_size / 1024, n, n == 1 ? "" : "s");
        return;
    }
    void* p = mmap(nullptr, kAltStack + 0x1000, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (p == MAP_FAILED) return;
    if (mprotect((char*)p + 0x1000, kAltStack, PROT_READ | PROT_WRITE) != 0) {
        munmap(p, kAltStack + 0x1000);
        return;
    }
    stack_t ss{};
    ss.ss_sp = (char*)p + 0x1000;
    ss.ss_size = kAltStack;
    ss.ss_flags = 0;
    if (sigaltstack(&ss, nullptr) != 0) {
        munmap(p, kAltStack + 0x1000);
        return;
    }
    t_alt = ss.ss_sp;   // before the log line below: log_line calls back in here
    int n = g_alt_count.fetch_add(1) + 1;
    if (n <= 4 || n % 16 == 0)
        log_line("crash reader: alt signal stack for tid %d (%zu kB, %d thread%s so far)",
                 (int)syscall(SYS_gettid), kAltStack / 1024, n, n == 1 ? "" : "s");
}

void bg3se::crash::arm()
{
    if (getenv("BG3SE_NO_CRASH_READER")) return;
    if (g_armed.exchange(true)) return;
    g_no_alt = getenv("BG3SE_NO_ALT_STACK") != nullptr;
    ensure_alt_stack();

    ScanCtx ctx{(uintptr_t)(void*)&bg3se::crash::arm};
    dl_iterate_phdr(collect, &ctx);

    // bin/bg3 imports both sigaction and signal through the PLT, but it installs
    // LinuxCrashReporter in main(), before its first SDL_Init -- which is where
    // deferred_init runs us. So for the real game this read-back, and not the
    // interposer below, is what finds the handler to chain to. Say which it found:
    // without that line a launch that leaves no CRASH block is ambiguous between
    // "never faulted" and "something took the handler away from us".
    char taken[128] = "";
    size_t n = 0;
    for (int i = 0; i < kNFatal; i++) {
        struct sigaction cur{};
        auto fn = real_sigaction();
        if (fn && fn(kFatal[i], nullptr, &cur) == 0 && cur.sa_handler != SIG_DFL) {
            g_chained[i].act = cur;
            g_chained[i].set = true;
            n += snprintf(taken + n, sizeof taken - n, " %s", signame(kFatal[i]));
            if (n >= sizeof taken) n = sizeof taken - 1;
        }
        install_ours(kFatal[i]);
    }
    log_line("crash reader: armed over %d signals; images:%s%s; the game already held:%s%s",
             kNFatal, g_nimages > 0 ? " bg3" : " (none)",
             g_nimages > 1 ? " libbg3se.so" : "", taken[0] ? taken : " none",
             g_no_alt ? "; BG3SE_NO_ALT_STACK: no alternate stacks" : "");
}

// PORT (linux): the extender proper is dlopened *after* arm(), and collect() keeps only
// the game and the loader, so every return address inside libbg3se-ext.so was invisible to
// the stack scan -- a crash in extender code would print the game's main loop and nothing
// of its own, and a faulting call through a null function pointer (rip=0) would leave a
// block naming no frame at all.  One entry, added once, by an address the caller knows is
// inside the image.
void bg3se::crash::note_image(const void* addr, const char* name)
{
    if (!g_armed.load() || addr == nullptr || name == nullptr) return;
    if (image_of((uintptr_t)addr)) return;                 // already in the table
    int before = g_nimages;
    ScanCtx ctx{(uintptr_t)addr, name};
    dl_iterate_phdr(collect, &ctx);
    if (g_nimages == before) {
        log_line("crash reader: %s is not in any mapped image; frames in it stay unresolved", name);
        return;
    }
    const Image& im = g_images[g_nimages - 1];
    log_line("crash reader: %s base 0x%lx, text 0x%lx-0x%lx; its frames now resolve as %s+0xOFFSET",
             name, (unsigned long)im.base, (unsigned long)im.lo, (unsigned long)im.hi, name);
}

void bg3se::crash::selftest_fault()
{
    volatile int* p = (int*)0x1234;
    *p = 1;
}
