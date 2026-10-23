// preload.cpp — the LD_PRELOAD entry of bg3-se-linux (Phase 2: prove we are inside).
//
// LD_PRELOAD is inherited by every process of the launch chain (launch wrapper,
// reaper, the sniper entry point, bwrap, pv-adverb, and only then bin/bg3), so the
// constructor identifies the game first and is a no-op everywhere else.
//
// bg3 forks once without exec early on, so the constructor
// does nothing fork-unsafe: no threads, no locks held on return, no allocation it
// keeps. Real initialisation is deferred to the first hooked call — SDL_Init, which
// bg3 imports through the PLT — and that is also where the C++ runtime is exercised.

#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <gnu/libc-version.h>
#include <link.h>
#include <pthread.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include "crash.h"
#include "host_api.h"
#include "loader.h"
#include "manifest.h"

namespace bg3se {

bool g_is_game = false;
bool g_active = false;

}  // namespace bg3se

using namespace bg3se;

namespace {

// A build is known when it has an offset manifest (manifest/<build-id>.tsv, embedded).
// Anything else: log it and stay out of the way.
bool g_known_build = false;
uintptr_t g_base = 0;
char g_build_id[2 * 64 + 1] = "";
char g_log_path[4096] = "";

}  // namespace

const char* bg3se::log_path() { return g_log_path; }

namespace {

// One mapping per thread, split into the slots of loader.h; never freed. mmap(2) and a
// TLS pointer only, so this is safe on the signal path (the crash handler logs).
__thread char* t_scratch = nullptr;

}  // namespace

char* bg3se::scratch(int slot)
{
    if (slot < 0 || slot >= kScratchSlots) return nullptr;
    if (!t_scratch) {
        void* p = mmap(nullptr, (size_t)kScratchSlots * kScratchSize, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) return nullptr;
        t_scratch = (char*)p;
    }
    return t_scratch + (size_t)slot * kScratchSize;
}

// One write(2) per line on an O_APPEND fd opened and closed each time: nothing is
// held across the fork, and lines from both processes interleave whole.
//
// The line buffer is per-thread scratch and not a local: 8 kB of frame killed a game
// thread here (loader.h). This is also where the crash reader
// first sees most threads, so it is where their alternate signal stack goes in.
void bg3se::log_line(const char* fmt, ...)
{
    if (!g_log_path[0]) return;
    crash::ensure_alt_stack();
    char* buf = scratch(kScratchLog);
    if (!buf) return;
    const size_t bufsz = kScratchSize;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    size_t n = strftime(buf, bufsz, "%Y-%m-%d %H:%M:%S", &tm);
    n += snprintf(buf + n, bufsz - n, ".%03ld [%d/%ld] ", ts.tv_nsec / 1000000,
                  (int)getpid(), (long)syscall(SYS_gettid));
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(buf + n, bufsz - n - 1, fmt, ap);
    va_end(ap);
    if (m > 0) n += (size_t)m < bufsz - n - 1 ? (size_t)m : bufsz - n - 2;
    buf[n++] = '\n';
    int fd = open(g_log_path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
    if (fd < 0) return;
    (void)!write(fd, buf, n);
    close(fd);
}

namespace {

void resolve_log_path()
{
    const char* p = getenv("BG3SE_LOG");
    if (p && *p) {
        snprintf(g_log_path, sizeof g_log_path, "%s", p);
        return;
    }
    char dir[3072];
    const char* state = getenv("XDG_STATE_HOME");
    const char* home = getenv("HOME");
    if (state && *state) snprintf(dir, sizeof dir, "%s/bg3-se-linux", state);
    else if (home && *home) snprintf(dir, sizeof dir, "%s/.local/state/bg3-se-linux", home);
    else return;
    mkdir(dir, 0700);
    snprintf(g_log_path, sizeof g_log_path, "%s/bg3se.log", dir);
}

// The main program's own program headers, from the aux vector: no loader lock, no
// file I/O. Fills g_base and g_build_id.
void read_own_build_id()
{
    auto* phdr = (const ElfW(Phdr)*)getauxval(AT_PHDR);
    size_t phnum = getauxval(AT_PHNUM);
    if (!phdr || !phnum) return;
    for (size_t i = 0; i < phnum; i++)
        if (phdr[i].p_type == PT_PHDR) g_base = (uintptr_t)phdr - phdr[i].p_vaddr;
    for (size_t i = 0; i < phnum; i++) {
        if (phdr[i].p_type != PT_NOTE) continue;
        auto* p = (const uint8_t*)(g_base + phdr[i].p_vaddr);
        auto* end = p + phdr[i].p_memsz;
        while (p + sizeof(ElfW(Nhdr)) <= end) {
            auto* nh = (const ElfW(Nhdr)*)p;
            const uint8_t* name = p + sizeof *nh;
            const uint8_t* desc = name + ((nh->n_namesz + 3) & ~3u);
            const uint8_t* next = desc + ((nh->n_descsz + 3) & ~3u);
            if (next > end) break;
            if (nh->n_type == NT_GNU_BUILD_ID && nh->n_namesz == 4 && !memcmp(name, "GNU", 4) &&
                nh->n_descsz <= 64) {
                for (uint32_t k = 0; k < nh->n_descsz; k++)
                    snprintf(g_build_id + 2 * k, 3, "%02x", desc[k]);
                return;
            }
            p = next;
        }
    }
}

int thread_count()
{
    FILE* f = fopen("/proc/self/status", "re");
    if (!f) return -1;
    char line[256];
    int n = -1;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "Threads: %d", &n) == 1) break;
    fclose(f);
    return n;
}

void on_fork_child()
{
    log_line("fork: child of %d", (int)getppid());
}

std::atomic<bool> g_deferred_done{false};

// ---- the extender proper: libbg3se-ext.so (ext/), dlopened on a known build ----------
// It lives beside this object unless BG3SE_EXT names it; BG3SE_NO_EXT=1 leaves it out
// (the loader then only logs, as in Phases 2-4). Its one export takes the host table.

void host_log(const char* line) { log_line("%s", line); }
void* host_manifest_get(const char* symbol) { return manifest::get(symbol); }
int host_manifest_hookable(const char* symbol) { return manifest::hookable(symbol); }
int host_manifest_require(const char* feature, const char* const* symbols, size_t n, int hooks)
{
    // manifest::require takes an initializer_list; go one by one, the logging is per symbol anyway
    bool ok = true;
    for (size_t i = 0; i < n; i++) {
        ok = (hooks ? manifest::require_hooks(feature, {symbols[i]}) : manifest::require(feature, {symbols[i]})) && ok;
    }
    return ok;
}

char g_state_dir[3072] = "";

void load_extender()
{
    if (getenv("BG3SE_NO_EXT")) {
        log_line("ext: BG3SE_NO_EXT set; the extender is not loaded");
        return;
    }
    char path[4096];
    const char* override = getenv("BG3SE_EXT");
    if (override && *override) {
        snprintf(path, sizeof path, "%s", override);
    } else {
        Dl_info info;
        if (!dladdr((void*)&load_extender, &info) || !info.dli_fname) {
            log_line("ext: cannot locate libbg3se.so itself (dladdr); the extender is not loaded");
            return;
        }
        snprintf(path, sizeof path, "%s", info.dli_fname);
        char* slash = strrchr(path, '/');
        if (!slash) return;
        snprintf(slash + 1, sizeof path - (slash + 1 - path), "libbg3se-ext.so");
    }
    void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        log_line("ext: dlopen(%s) failed: %s", path, dlerror());
        return;
    }
    auto init = (bg3se_ext_init_fn)dlsym(h, "bg3se_ext_init");
    if (!init) {
        log_line("ext: %s has no bg3se_ext_init: %s", path, dlerror());
        return;
    }
    // The crash reader armed before this dlopen, so its image table does not know the
    // extender yet and a frame inside it would be dropped by the stack scan. `init` is
    // an address in the image.
    crash::note_image((const void*)init, "libbg3se-ext.so");
    snprintf(g_state_dir, sizeof g_state_dir, "%s", log_path());
    if (char* slash = strrchr(g_state_dir, '/')) *slash = 0;
    bg3se_host_api host{};
    host.version = BG3SE_HOST_API_VERSION;
    host.size = sizeof host;
    host.build_id = g_build_id;
    host.base = g_base;
    host.state_dir = g_state_dir;
    host.log = host_log;
    host.manifest_get = host_manifest_get;
    host.manifest_hookable = host_manifest_hookable;
    host.manifest_require = host_manifest_require;
    log_line("ext: %s loaded, calling bg3se_ext_init", path);
    int rc = init(&host);
    log_line("ext: bg3se_ext_init = %d%s", rc, rc ? " (the extender is not running)" : "");
}

}  // namespace

// First hooked call: past the constructor, on a game thread. Verifies the manifest,
// probes the C++ runtime, then loads the extender proper (Phase 5).
void bg3se::deferred_init(const char* via)
{
    if (g_deferred_done.exchange(true)) return;
    log_line("deferred init via %s: threads=%d", via, thread_count());
    // Before the hooks, and before bin/bg3 installs LinuxCrashReporter: a fault after
    // this point leaves a CRASH block in this log (crash.h).
    crash::arm();
    manifest::load(g_build_id, g_base);
    manifest::load_library("libOsiris.so");   // the Osiris globals (manifest/b5dc….tsv)

    // The C++ runtime is linked statically into this object; bg3 carries its own
    // static libc++ (std::__2) and the host's libstdc++ is mapped as well. Check
    // that ours works in that company: allocation, and a throw through our unwinder.
    if (!getenv("BG3SE_NO_EH_PROBE")) {
        try {
            std::string s = "thrown and caught inside ";
            s += "the game";
            throw std::runtime_error(s);
        } catch (const std::exception& e) {
            log_line("c++ runtime: %s", e.what());
        }
    }

    load_extender();
}

namespace {

__attribute__((constructor)) void bg3se_attach()
{
    char exe[4096];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n <= 0) return;
    exe[n] = 0;
    const char* base = strrchr(exe, '/');
    base = base ? base + 1 : exe;
    const char* want = getenv("BG3SE_EXE_NAME");  // test knob
    if (strcmp(base, want && *want ? want : "bg3") != 0) return;

    g_is_game = true;
    resolve_log_path();
    read_own_build_id();
    g_known_build = g_build_id[0] && manifest::known(g_build_id);
    g_active = g_known_build || getenv("BG3SE_ALLOW_UNKNOWN");

    const char* preload = getenv("LD_PRELOAD");
    log_line("attach: exe=%s build-id=%s (%s) base=0x%lx glibc=%s ppid=%d LD_PRELOAD=%s", exe,
             g_build_id[0] ? g_build_id : "?", g_known_build ? "known" : "UNKNOWN - staying passive",
             (unsigned long)g_base, gnu_get_libc_version(), (int)getppid(), preload ? preload : "");
    pthread_atfork(nullptr, nullptr, on_fork_child);
}

}  // namespace

extern "C" {

// bg3 imports SDL_Init through the PLT; as a preloaded object we come first in the
// lookup order, and the real one is whatever comes next.
__attribute__((visibility("default"))) int SDL_Init(uint32_t flags)
{
    static int (*real)(uint32_t) = nullptr;
    if (!real) real = (int (*)(uint32_t))dlsym(RTLD_NEXT, "SDL_Init");
    if (g_active) deferred_init("SDL_Init");
    if (!real) return -1;
    int rc = real(flags);
    if (g_is_game) log_line("SDL_Init(0x%x) = %d", flags, rc);
    return rc;
}

}  // extern "C"
