/* Stand-in for bin/bg3: the things the preload cares about, in the game's order — a
 * fork without exec, then SDL_Init through the PLT from the child, then Osiris
 * (fake_osiris_client.cpp). */
#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int SDL_Init(uint32_t flags);
int run_osiris(void);

/* Rows for the stand-in's offset manifest, which run.sh writes with tools/manifest.py:
 * a function hook::probe accepts, one too short to hook, a global, and a read-only
 * string (what the game's version literal is). */
int fake_manifest_global = 7;
const char fake_manifest_str[] = "9.8.7.6543210";   /* referenced below, like the game's */
__attribute__((noinline, used)) int fake_manifest_fn(const char* s)
{
    return printf("fake bg3: %s\n", s) + fake_manifest_global;
}
/* in asm: a compiler keeping frame pointers would give it a hookable prologue */
__asm__(".text\n"
        ".globl fake_manifest_leaf\n"
        ".type fake_manifest_leaf, @function\n"
        "fake_manifest_leaf:\n"
        ".cfi_startproc\n"
        "  lea 1(%rdi), %eax\n"
        "  ret\n"
        ".cfi_endproc\n"
        ".size fake_manifest_leaf, .-fake_manifest_leaf\n");
int fake_manifest_leaf(int x);

/* BG3SE_FAKE_CRASH=1: stand in for LinuxCrashReporter — install a handler through
 * sigaction(2) the way bin/bg3 does, then fault. The preload's crash reader has to
 * write its block first and still hand the signal to this one (src/crash.cpp). */
static void fake_game_handler(int sig)
{
    printf("fake bg3: game handler saw signal %d\n", sig);
    fflush(stdout);
    _exit(70);
}

/* BG3SE_FAKE_CRASH_OVERFLOW=1 (with BG3SE_FAKE_CRASH=1): the decisive test of the
 * alternate signal stack. A thread with a small stack logs once -- through the SDL_Init
 * interposer, which is how the loader first sees a thread -- and then recurses until
 * the stack is gone. The kernel delivers SIGSEGV with rsp below the guard page, so
 * without sigaltstack(2) the handler has nowhere to run and the process dies with no
 * CRASH block at all.
 * With one, the block has to be there, has to say rsp is unmapped, and the chain to the
 * game's handler still has to happen (exit 70). */
static volatile unsigned long sink;

static unsigned long __attribute__((noinline)) eat_stack(unsigned long depth)
{
    volatile char pad[1024];
    pad[0] = (char)depth;
    pad[1023] = (char)depth;
    sink += (unsigned char)pad[0] + (unsigned char)pad[1023];
    if (depth > (1ul << 30)) return depth;
    return eat_stack(depth + 1) + (unsigned char)pad[0];
}

/* BG3SE_FAKE_ALT_KB=N (with the overflow): the thread arrives with an alt stack of its
 * own, N kB over a guard page, before the loader first sees it -- the way the game's
 * threads do (8 kB, "already had an alt stack" in bg3se.log).
 * The handler, the kernel's signal frame and the game's chained handler all run on it. */
static void maybe_own_alt_stack(void)
{
    const char* kb = getenv("BG3SE_FAKE_ALT_KB");
    size_t sz;
    char* p;
    stack_t ss;
    if (!kb) return;
    sz = (size_t)atoi(kb) * 1024;
    p = mmap(NULL, sz + 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (p == MAP_FAILED || mprotect(p + 4096, sz, PROT_READ | PROT_WRITE) != 0) _exit(76);
    memset(&ss, 0, sizeof ss);
    ss.ss_sp = p + 4096;
    ss.ss_size = sz;
    if (sigaltstack(&ss, NULL) != 0) _exit(76);
}

static void* overflow_thread(void* arg)
{
    (void)arg;
    maybe_own_alt_stack();
    SDL_Init(0x20);   /* the loader logs on this thread, and installs its alt stack */
    sink += eat_stack(1);
    return NULL;
}

static void maybe_overflow(void)
{
    pthread_attr_t at;
    pthread_t th;
    if (!getenv("BG3SE_FAKE_CRASH_OVERFLOW")) return;
    pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, 128 * 1024);
    if (pthread_create(&th, &at, overflow_thread, NULL) != 0) _exit(72);
    pthread_join(th, NULL);
    _exit(73);   /* the thread was supposed to die of a stack overflow */
}

/* BG3SE_FAKE_THIN_LOG=1: the other half of the stack-overflow case. A thread walks
 * its own stack down to ~4 kB left and then logs (SDL_Init through the interposer).
 * A local buffer in log_line would be an 8488-byte frame, which does not fit there (the
 * game's Osiris thread is that thin); with the buffer in per-thread scratch (loader.h)
 * the frame is 296 bytes and the line gets written. It must log from a tid
 * that is not the pid, and the process must live. */
static char* stack_floor(void)
{
    pthread_attr_t at;
    void* lo = NULL;
    size_t sz = 0;
    if (pthread_getattr_np(pthread_self(), &at) != 0) return NULL;
    pthread_attr_getstack(&at, &lo, &sz);
    pthread_attr_destroy(&at);
    return (char*)lo;
}

static int __attribute__((noinline)) descend_and_log(char* floor, size_t keep)
{
    volatile char pad[512];
    pad[0] = 1;
    pad[511] = 2;
    sink += (unsigned char)pad[0];
    if ((size_t)((char*)&pad[0] - floor) > keep)
        return descend_and_log(floor, keep) + (unsigned char)pad[0];
    return SDL_Init(0x20);
}

static void* thin_log_thread(void* arg)
{
    char* floor = stack_floor();
    (void)arg;
    if (!floor) _exit(74);
    if (descend_and_log(floor, 4096) < 42) _exit(75);
    return NULL;
}

static void maybe_thin_log(void)
{
    pthread_attr_t at;
    pthread_t th;
    if (!getenv("BG3SE_FAKE_THIN_LOG")) return;
    pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, 128 * 1024);
    if (pthread_create(&th, &at, thin_log_thread, NULL) != 0) _exit(72);
    pthread_join(th, NULL);
}

/* BG3SE_FAKE_EXIT=N: the exit interposers. The LoadMenu crash kills the game with no
   signal any of three handlers sees, so the loader exports
   exit/_exit/_Exit/quick_exit/abort and logs the caller. This calls exit(N) from a
   function run.sh can name, which is the check that the return address is the caller's
   and resolves the way tools/disasm.py takes it. */
__attribute__((noinline, noreturn)) static void fake_exit_caller(int n)
{
    exit(n);
}

static void maybe_exit(void)
{
    const char* v = getenv("BG3SE_FAKE_EXIT");
    if (!v) return;
    fake_exit_caller(atoi(v));
}

/* BG3SE_FAKE_KILL=1: the kill interposers. A process that SIGKILLs itself leaves
   nothing behind -- no CRASH block, no exit call -- so the loader logs kill/raise/
   pthread_kill for the signals that would end the process. SIGTERM stands in for
   SIGKILL here so the stand-in can still be waited on; SIGWINCH is the calibration,
   a signal that must not be logged. */
__attribute__((noinline)) static void fake_kill_caller(void)
{
    volatile int sink = 0;
    kill(getpid(), SIGWINCH);   /* must not be logged */
    kill(getpid(), SIGTERM);
    /* Not decoration: without something after it, gcc turns the last kill() into a tail
       jump and __builtin_return_address(0) then reports *this* function's caller instead
       (an address inside main). The interposer reads the caller's
       caller whenever the game tail-calls too, which is worth knowing before disasm'ing
       a KILL line's address. */
    sink = 1;
    (void)sink;
}

static void maybe_kill(void)
{
    const char* v = getenv("BG3SE_FAKE_KILL");
    if (!v) return;
    /* =1 ignores SIGTERM, so the KILL line is tested on its own; =2 lets it land, which
       is how the parent's WAIT line gets a killed-by-signal case to report. */
    if (*v == '1') signal(SIGTERM, SIG_IGN);
    fake_kill_caller();
}

static void maybe_crash(void)
{
    volatile int* p;
    struct sigaction sa;
    if (!getenv("BG3SE_FAKE_CRASH")) return;
    if (getenv("BG3SE_FAKE_CRASH_SIGNAL")) {
        /* the other door: bin/bg3 imports signal(2) as well */
        signal(SIGSEGV, fake_game_handler);
    } else {
        memset(&sa, 0, sizeof sa);
        sa.sa_handler = fake_game_handler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGSEGV, &sa, NULL);
    }
    maybe_overflow();
    p = (int*)0x1234;
    *p = 1;
    _exit(71);   /* the handler did not run */
}

int main(void)
{
    pid_t pid = fork();
    if (pid == 0) {
        int rc = SDL_Init(0x20);
        printf("fake bg3: SDL_Init returned %d\n", rc);
        if (rc != 42) return 1;
        if (fake_manifest_fn("manifest rows present") <= 0 || fake_manifest_leaf(1) != 2) return 3;
        if (fake_manifest_fn(fake_manifest_str) <= 0) return 3;
        maybe_thin_log();
        maybe_exit();
        maybe_kill();
        maybe_crash();
        return run_osiris() ? 2 : 0;
    }
    int st = 0;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}
