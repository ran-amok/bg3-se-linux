// win32.cpp — the Win32 functions of shim/windows.h, on Linux. Faithful where upstream
// depends on the semantics (UTF-8 conversion, SRW locks, IsBadReadPtr, timers, files);
// a logged stub where the concept has no Linux counterpart (dialogs, PE resources,
// module handles by Windows name).
#include <windows.h>

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace bg3se::platform {
void log_line(char const* fmt, ...) __attribute__((format(printf, 1, 2)));
}
using bg3se::platform::log_line;

namespace {

std::string narrow(LPCWSTR w)
{
    std::string s;
    if (!w) return s;
    for (; *w; w++) {
        uint32_t c = (uint32_t)*w;
        if (c < 0x80) s += (char)c;
        else if (c < 0x800) { s += (char)(0xc0 | c >> 6); s += (char)(0x80 | (c & 0x3f)); }
        else if (c < 0x10000) { s += (char)(0xe0 | c >> 12); s += (char)(0x80 | (c >> 6 & 0x3f)); s += (char)(0x80 | (c & 0x3f)); }
        else { s += (char)(0xf0 | (c >> 18 & 7)); s += (char)(0x80 | (c >> 12 & 0x3f)); s += (char)(0x80 | (c >> 6 & 0x3f)); s += (char)(0x80 | (c & 0x3f)); }
    }
    return s;
}

// Decode one UTF-8 sequence; invalid bytes become U+FFFD and advance by one.
uint32_t decode(unsigned char const* s, int n, int* used)
{
    unsigned char c = s[0];
    int len = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 0;
    if (!len || len > n) { *used = 1; return 0xfffd; }
    uint32_t cp = len == 1 ? c : c & (0x7f >> len);
    for (int i = 1; i < len; i++) {
        if ((s[i] & 0xc0) != 0x80) { *used = 1; return 0xfffd; }
        cp = cp << 6 | (s[i] & 0x3f);
    }
    *used = len;
    return cp;
}

pthread_rwlock_t* rw(PSRWLOCK l)
{
    auto* p = std::atomic_ref<void*>(l->Ptr).load(std::memory_order_acquire);
    if (p) return (pthread_rwlock_t*)p;
    auto* fresh = new pthread_rwlock_t;
    pthread_rwlock_init(fresh, nullptr);
    void* expected = nullptr;
    if (std::atomic_ref<void*>(l->Ptr).compare_exchange_strong(expected, fresh)) return fresh;
    pthread_rwlock_destroy(fresh);
    delete fresh;
    return (pthread_rwlock_t*)expected;
}

}  // namespace

extern "C" {

int WideCharToMultiByte(UINT cp, DWORD, LPCWSTR s, int n, LPSTR out, int outn, LPCSTR, LPBOOL)
{
    if (cp != CP_UTF8 || !s) return 0;
    std::string r;
    if (n < 0) r = narrow(s);
    else { std::wstring w(s, (size_t)n); r = narrow(w.c_str()); if ((size_t)n && !s[n - 1]) r.push_back(0); }
    if (outn == 0) return (int)r.size();
    if ((int)r.size() > outn) return 0;
    memcpy(out, r.data(), r.size());
    return (int)r.size();
}

int MultiByteToWideChar(UINT cp, DWORD, LPCSTR s, int n, LPWSTR out, int outn)
{
    if (cp != CP_UTF8 || !s) return 0;
    if (n < 0) n = (int)strlen(s) + 1;
    std::wstring w;
    auto* p = (unsigned char const*)s;
    for (int i = 0; i < n;) {
        int used;
        w.push_back((wchar_t)decode(p + i, n - i, &used));
        i += used;
    }
    if (outn == 0) return (int)w.size();
    if ((int)w.size() > outn) return 0;
    memcpy(out, w.data(), w.size() * sizeof(wchar_t));
    return (int)w.size();
}

HANDLE GetCurrentProcess(void) { return (HANDLE)(LONG_PTR)-1; }
HANDLE GetCurrentThread(void) { return (HANDLE)(LONG_PTR)-2; }
DWORD GetCurrentProcessId(void) { return (DWORD)getpid(); }
DWORD GetCurrentThreadId(void) { return (DWORD)syscall(SYS_gettid); }
BOOL TerminateProcess(HANDLE, UINT code) { _exit((int)code); }
void Sleep(DWORD ms) { if (ms == 0) sched_yield(); else usleep(ms * 1000); }
DWORD GetLastError(void) { return (DWORD)errno; }

BOOL IsDebuggerPresent(void)
{
    FILE* f = fopen("/proc/self/status", "re");
    if (!f) return FALSE;
    char line[256];
    int pid = 0;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "TracerPid: %d", &pid) == 1) break;
    fclose(f);
    return pid != 0;
}

void DebugBreak(void)
{
    if (getenv("BG3SE_DEBUGBREAK")) raise(SIGTRAP);
    else log_line("DebugBreak() (set BG3SE_DEBUGBREAK=1 to SIGTRAP)");
}

void OutputDebugStringA(LPCSTR) {}
void OutputDebugStringW(LPCWSTR) {}

int MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT)
{
    log_line("MessageBox [%s]: %s", caption ? caption : "", text ? text : "");
    return 1;
}
int MessageBoxW(HWND, LPCWSTR text, LPCWSTR caption, UINT)
{
    log_line("MessageBox [%s]: %s", narrow(caption).c_str(), narrow(text).c_str());
    return 1;
}

LPWSTR GetCommandLineW(void)
{
    static std::wstring cmd;
    if (cmd.empty()) {
        std::string raw;
        if (FILE* f = fopen("/proc/self/cmdline", "re")) {
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof buf, f)) > 0) raw.append(buf, n);
            fclose(f);
        }
        for (char& c : raw) if (c == 0) c = ' ';
        while (!raw.empty() && raw.back() == ' ') raw.pop_back();
        int n = MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), (int)raw.size(), nullptr, 0);
        cmd.resize((size_t)n);
        MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), (int)raw.size(), cmd.data(), n);
    }
    return cmd.data();
}

BOOL QueryPerformanceCounter(LARGE_INTEGER* v)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    v->QuadPart = (LONGLONG)ts.tv_sec * 1000000000ll + ts.tv_nsec;
    return TRUE;
}
BOOL QueryPerformanceFrequency(LARGE_INTEGER* v) { v->QuadPart = 1000000000ll; return TRUE; }
ULONGLONG GetTickCount64(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ULONGLONG)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
static void fill_systemtime(SYSTEMTIME* st, struct tm const& tm, long ms)
{
    st->wYear = tm.tm_year + 1900; st->wMonth = tm.tm_mon + 1; st->wDayOfWeek = tm.tm_wday; st->wDay = tm.tm_mday;
    st->wHour = tm.tm_hour; st->wMinute = tm.tm_min; st->wSecond = tm.tm_sec; st->wMilliseconds = (WORD)ms;
}
void GetSystemTime(SYSTEMTIME* st)
{
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm; gmtime_r(&ts.tv_sec, &tm);
    fill_systemtime(st, tm, ts.tv_nsec / 1000000);
}
void GetLocalTime(SYSTEMTIME* st)
{
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm; localtime_r(&ts.tv_sec, &tm);
    fill_systemtime(st, tm, ts.tv_nsec / 1000000);
}

BOOL CreateDirectoryW(LPCWSTR path, LPSECURITY_ATTRIBUTES)
{
    if (mkdir(narrow(path).c_str(), 0755) == 0) return TRUE;
    if (errno == EEXIST) errno = ERROR_ALREADY_EXISTS;   // GetLastError() reads errno
    return FALSE;
}
BOOL PathFileExistsW(LPCWSTR path) { struct stat st; return stat(narrow(path).c_str(), &st) == 0; }
BOOL PathIsDirectoryW(LPCWSTR path) { struct stat st; return stat(narrow(path).c_str(), &st) == 0 && S_ISDIR(st.st_mode); }
BOOL RemoveDirectoryW(LPCWSTR path) { return rmdir(narrow(path).c_str()) == 0; }
BOOL DeleteFileW(LPCWSTR path) { return unlink(narrow(path).c_str()) == 0; }
BOOL CopyFileW(LPCWSTR from, LPCWSTR to, BOOL failIfExists)
{
    std::string f = narrow(from), t = narrow(to);
    int in = open(f.c_str(), O_RDONLY | O_CLOEXEC);
    if (in < 0) return FALSE;
    int out = open(t.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC | (failIfExists ? O_EXCL : O_TRUNC), 0644);
    if (out < 0) { close(in); return FALSE; }
    std::vector<char> buf(65536);   // not a 64 kB frame: see entry.cpp's log_line
    ssize_t n;
    bool ok = true;
    while ((n = read(in, buf.data(), buf.size())) > 0)
        if (write(out, buf.data(), (size_t)n) != n) { ok = false; break; }
    close(in); close(out);
    return ok && n == 0;
}

HMODULE GetModuleHandleW(LPCWSTR) { return nullptr; }
HMODULE GetModuleHandleA(LPCSTR) { return nullptr; }
HMODULE LoadLibraryW(LPCWSTR name) { return dlopen(narrow(name).c_str(), RTLD_NOW | RTLD_NOLOAD); }
DWORD GetModuleFileNameW(HMODULE m, LPWSTR out, DWORD n)
{
    Dl_info info;
    if (!dladdr((void*)&GetModuleFileNameW, &info) || !info.dli_fname) return 0;
    (void)m;
    int k = MultiByteToWideChar(CP_UTF8, 0, info.dli_fname, -1, out, (int)n);
    return k ? (DWORD)k - 1 : 0;
}
FARPROC GetProcAddress(HMODULE m, LPCSTR name) { return (FARPROC)dlsym(m ? m : RTLD_DEFAULT, name); }
BOOL GetModuleInformation(HANDLE, HMODULE, MODULEINFO*, DWORD) { return FALSE; }

BOOL VirtualProtect(LPVOID p, SIZE_T n, DWORD prot, LPDWORD old)
{
    long page = sysconf(_SC_PAGESIZE);
    auto a = (uintptr_t)p & ~(uintptr_t)(page - 1);
    size_t len = ((uintptr_t)p + n - a + page - 1) & ~(uintptr_t)(page - 1);
    int flags = PROT_READ;
    if (prot == PAGE_EXECUTE_READWRITE) flags = PROT_READ | PROT_WRITE | PROT_EXEC;
    else if (prot == PAGE_EXECUTE_READ) flags = PROT_READ | PROT_EXEC;
    else if (prot == PAGE_READWRITE) flags = PROT_READ | PROT_WRITE;
    if (old) *old = PAGE_EXECUTE_READ;    // not tracked; callers restore what they think was there
    return mprotect((void*)a, len, flags) == 0;
}
SIZE_T VirtualQuery(LPCVOID, MEMORY_BASIC_INFORMATION*, SIZE_T) { return 0; }

// PORT (linux). Win32's contract, and the order matters: a zero-length range is readable
// whatever the pointer ("If ucb is zero, the return value is zero"), and an empty game buffer is
// {nullptr, 0}. Testing the pointer first would fail validation on every empty ls::MemBuffer and
// cost the class holding it its proxy -- the character-creation portrait, `Icon` of
// `character_creation::Appearance`, is empty until one is rendered, and a mod reaching the CC
// dummy through `ecl::character_creation::DummyDefinitionComponent` needs that proxy. A
// refusal explains itself while the budget lasts: the range, what the
// probe returned, and the /proc/self/maps line under the pointer.
static std::atomic<int> g_badptr_explain_budget{8};

static void explain_bad_ptr(const void* p, UINT_PTR n, ssize_t got, int err)
{
    if (g_badptr_explain_budget.fetch_sub(1) <= 0) return;
    std::string line = "(no /proc/self/maps line)";
    if (FILE* f = fopen("/proc/self/maps", "r")) {
        std::vector<char> buf(1024);   // heap, not a game thread's stack
        while (fgets(buf.data(), (int)buf.size(), f)) {
            unsigned long lo = 0, hi = 0;
            if (sscanf(buf.data(), "%lx-%lx", &lo, &hi) == 2 && (uintptr_t)p >= lo && (uintptr_t)p < hi) {
                line = buf.data();
                while (!line.empty() && line.back() == '\n') line.pop_back();
                break;
            }
        }
        fclose(f);
    }
    log_line("IsBadReadPtr(%p, %zu) = TRUE: process_vm_readv %zd (%s); maps: %s", p, (size_t)n, got, strerror(err), line.c_str());
}

BOOL IsBadReadPtr(const void* p, UINT_PTR n)
{
    if (n == 0) return FALSE;      // before the pointer test: Win32 semantics, see above
    if (!p) return TRUE;
    static long page = sysconf(_SC_PAGESIZE);
    // one byte per page of the range, at most 16 pages (a longer range is judged by its head)
    auto first = (uintptr_t)p & ~(uintptr_t)(page - 1);
    auto last = ((uintptr_t)p + n - 1) & ~(uintptr_t)(page - 1);
    size_t pages = (last - first) / (size_t)page + 1;
    if (pages > 16) pages = 16;
    char sink[16];
    struct iovec local[16], remote[16];
    for (size_t i = 0; i < pages; i++) {
        local[i] = {&sink[i], 1};
        remote[i] = {(void*)(i == 0 ? (uintptr_t)p : first + i * (size_t)page), 1};
    }
    ssize_t got = process_vm_readv(getpid(), local, (unsigned long)pages, remote, (unsigned long)pages, 0);
    if (got != (ssize_t)pages) {
        explain_bad_ptr(p, n, got, errno);
        return TRUE;
    }
    return FALSE;
}

void InitializeSRWLock(PSRWLOCK l) { l->Ptr = nullptr; }
void AcquireSRWLockExclusive(PSRWLOCK l) { pthread_rwlock_wrlock(rw(l)); }
void ReleaseSRWLockExclusive(PSRWLOCK l) { pthread_rwlock_unlock(rw(l)); }
BOOL TryAcquireSRWLockExclusive(PSRWLOCK l) { return pthread_rwlock_trywrlock(rw(l)) == 0; }
void AcquireSRWLockShared(PSRWLOCK l) { pthread_rwlock_rdlock(rw(l)); }
void ReleaseSRWLockShared(PSRWLOCK l) { pthread_rwlock_unlock(rw(l)); }
BOOL TryAcquireSRWLockShared(PSRWLOCK l) { return pthread_rwlock_tryrdlock(rw(l)) == 0; }

static void cs_warn(char const* what)
{
    static std::atomic<bool> warned{false};
    if (!warned.exchange(true)) log_line("%s on a CRITICAL_SECTION: the Linux game has none; this is a no-op (Phase 6)", what);
}
// The game's own SRW lock (LS_SRWLOCK = pthread_rwlock_t): its algorithm, not a stub.
// ls::LocalTemplateManager::DeleteLocalTemplate (bin/bg3 0x23986e0) takes the write side of
// exactly this object with pthread_rwlock_trywrlock/wrlock, so these interoperate.
// A mirror that holds one of these is always an overlay on the game's memory, so these two
// only ever run on an SRWLock the extender constructs itself; they are here so that the class
// compiles with the same shape either way.
void InitializeGameSRWLock(LS_SRWLOCK* l)       { pthread_rwlock_init(l, nullptr); }
void DestroyGameSRWLock(LS_SRWLOCK* l)          { pthread_rwlock_destroy(l); }
void AcquireGameSRWLockExclusive(LS_SRWLOCK* l) { pthread_rwlock_wrlock(l); }
void ReleaseGameSRWLockExclusive(LS_SRWLOCK* l) { pthread_rwlock_unlock(l); }
void AcquireGameSRWLockShared(LS_SRWLOCK* l)    { pthread_rwlock_rdlock(l); }
void ReleaseGameSRWLockShared(LS_SRWLOCK* l)    { pthread_rwlock_unlock(l); }

// Noesis's gMutex and the SymbolManager's lock: also live game memory, and also the game's
// own algorithm rather than a stub -- Reflection::RegisterType (bin/bg3 0x2fdfea0) and
// SymbolManager::AddString (0x3155c00) call pthread_spin_lock/_unlock on these very objects,
// so these interoperate with them. Never pthread_spin_init: the game initialised them.
// There is no shared pair; the recursion protocol gMutex needs on top of this lives in
// GameDefinitions/UI.h, which is where the two globals it uses are declared.
void AcquireNoesisSpinLock(NS_SPINLOCK* l)      { pthread_spin_lock(l); }
void ReleaseNoesisSpinLock(NS_SPINLOCK* l)      { pthread_spin_unlock(l); }

// The exception to "never pthread_spin_init" above: Noesis::TypeClass's Ancestors block is
// allocated by the extender when the extender registers a type (Builtins.inl), and the game
// then takes that very lock in TypeClass::CollapseAncestors. It has to start out *unlocked*,
// which on glibc is the word 1 -- see the NS_SPINLOCK comment in shim/windows.h.
void InitializeNoesisSpinLock(NS_SPINLOCK* l)   { pthread_spin_init(l, PTHREAD_PROCESS_PRIVATE); }

void InitializeCriticalSection(CRITICAL_SECTION*) {}
void DeleteCriticalSection(CRITICAL_SECTION*) {}
void EnterCriticalSection(CRITICAL_SECTION*) { cs_warn("EnterCriticalSection"); }
void LeaveCriticalSection(CRITICAL_SECTION*) { cs_warn("LeaveCriticalSection"); }

// "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx", to Windows' in-memory order: Data1..3 little
// endian numbers, Data4 as bytes
RPC_STATUS UuidFromStringA(RPC_CSTR s, UUID* out)
{
    if (!s || !out) return RPC_S_INVALID_STRING_UUID;
    auto* p = (char const*)s;
    if (strlen(p) < 36 || p[8] != '-' || p[13] != '-' || p[18] != '-' || p[23] != '-') return RPC_S_INVALID_STRING_UUID;
    uint8_t b[16];
    int k = 0;
    for (int i = 0; i < 36; i++) {
        if (p[i] == '-') continue;
        auto hex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int hi = hex(p[i]), lo = hex(p[i + 1]);
        if (hi < 0 || lo < 0) return RPC_S_INVALID_STRING_UUID;
        b[k++] = (uint8_t)(hi << 4 | lo);
        i++;
    }
    out->Data1 = (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
    out->Data2 = (unsigned short)(b[4] << 8 | b[5]);
    out->Data3 = (unsigned short)(b[6] << 8 | b[7]);
    memcpy(out->Data4, b + 8, 8);
    return RPC_S_OK;
}
RPC_STATUS UuidCreate(UUID* out)
{
    uint8_t b[16];
    if (getrandom(b, sizeof b, 0) != (ssize_t)sizeof b) for (auto& x : b) x = (uint8_t)rand();
    b[6] = (b[6] & 0x0f) | 0x40;
    b[8] = (b[8] & 0x3f) | 0x80;
    out->Data1 = (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
    out->Data2 = (unsigned short)(b[4] << 8 | b[5]);
    out->Data3 = (unsigned short)(b[6] << 8 | b[7]);
    memcpy(out->Data4, b + 8, 8);
    return RPC_S_OK;
}

HRSRC FindResourceW(HMODULE, LPCWSTR, LPCWSTR) { return nullptr; }
HGLOBAL LoadResource(HMODULE, HRSRC) { return nullptr; }
LPVOID LockResource(HGLOBAL) { return nullptr; }
DWORD SizeofResource(HMODULE, HRSRC) { return 0; }
BOOL FreeResource(HGLOBAL) { return FALSE; }
HLOCAL LocalAlloc(UINT, SIZE_T n) { return malloc(n); }
HLOCAL LocalFree(HLOCAL p) { free(p); return nullptr; }
HANDLE GetStdHandle(DWORD) { return nullptr; }
BOOL SetConsoleTextAttribute(HANDLE, WORD) { return FALSE; }

}  // extern "C"
