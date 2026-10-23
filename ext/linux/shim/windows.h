// windows.h — the Win32 surface upstream's ported translation units use, on Linux.
//
// Types and macros came from the first clang builds of upstream's tree (declarations
// only); the functions declared here are real, implemented in
// ext/linux/win32.cpp, and each is either a faithful POSIX equivalent or a documented
// stub. Anything upstream calls that is not here is a compile error on purpose: it is
// a Win32 dependency the port has not decided about yet.
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <strings.h>
#include <pthread.h>                // pthread_rwlock_t: the game's own SRW lock (below)
#include <x86intrin.h>              // _mm_pause (Lock.h)
#include <algorithm>                // std::sort, which <windows.h> happens to drag in on MSVC

// --- basic integer typedefs -------------------------------------------------
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef uint32_t            DWORD;
typedef uint64_t            DWORD64;
typedef uint64_t            ULONGLONG;
typedef int64_t             LONGLONG;
typedef long                LONG_PTR;
typedef unsigned long       ULONG_PTR;
typedef ULONG_PTR           UINT_PTR;
typedef ULONG_PTR           SIZE_T;
typedef int                 INT;
typedef unsigned int        UINT;
typedef int32_t             LONG;     // 32-bit on Win64, unlike LP64's long
typedef uint32_t            ULONG;
typedef char                CHAR;
typedef wchar_t             WCHAR;
typedef WCHAR               TCHAR;
typedef void*               PVOID;
typedef void*               LPVOID;
typedef const void*         LPCVOID;
typedef char*               LPSTR;
typedef const char*         LPCSTR;
typedef wchar_t*            LPWSTR;
typedef const wchar_t*      LPCWSTR;
typedef LPCWSTR             LPCTSTR;
typedef DWORD*              LPDWORD;
typedef BOOL*               LPBOOL;
typedef int32_t             HRESULT;
typedef int64_t             LONG64;
typedef int32_t             LONG32;
typedef uint64_t            ULONG64;
typedef unsigned char*      RPC_CSTR;
typedef long                RPC_STATUS;

#define TRUE  1
#define FALSE 0
#ifndef NULL
#define NULL 0
#endif
#define MAX_PATH 4096
#define S_OK ((HRESULT)0)
#define RPC_S_OK 0
#define RPC_S_INVALID_STRING_UUID 1705
#define CP_UTF8 65001
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define MB_OK 0
#define MB_ICONERROR 0x10
#define MB_TASKMODAL 0x2000
#define MB_SETFOREGROUND 0x10000
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_READ 0x20
#define PAGE_READWRITE 0x04
#define PAGE_READONLY 0x02
#define FOREGROUND_BLUE 1
#define FOREGROUND_GREEN 2
#define FOREGROUND_RED 4
#define FOREGROUND_INTENSITY 8
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#define ENABLE_PROCESSED_OUTPUT 0x0001
#define ENABLE_WRAP_AT_EOL_OUTPUT 0x0002
#define NO_ERROR 0
#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define ERROR_ALREADY_EXISTS 183
#define IMAGE_SCN_MEM_EXECUTE 0x20000000
#define IMAGE_SCN_CNT_CODE 0x00000020
#define WINBASEAPI

// --- handles: HMODULE is a dlopen handle here ---------------------------------
typedef void* HMODULE;
typedef HMODULE HINSTANCE;
typedef void* HANDLE;
typedef void* HWND;
typedef void* HRSRC;
typedef void* HGLOBAL;
typedef void* HLOCAL;
typedef void (*FARPROC)();
typedef void* PDETOUR_TRAMPOLINE;

// --- sync primitives --------------------------------------------------------
// PORT (linux): CRITICAL_SECTION appears only inside mirrors of game structures, so its
// size has to be the game's, not Win32's. The Linux game has ls::CriticalSection, read off
// ls::CriticalSection::Lock (bin/bg3 0x3b7dbc0, matched to the macOS symbol):
// pthread_mutex_trylock on the object itself, ++[this+0x28] once taken, a
// `pause` spin bounded by [this+0x2c], then pthread_mutex_lock as the fallback. That is a
// glibc pthread_mutex_t (40 bytes, __kind at +0x10 reads 0x21 = recursive|prio-inherit in
// both of the live EntityWorld's instances) followed by {int RecursionCount; int SpinCount}
// -- 48 bytes where Win32's is 40, so every mirror that holds one is 8 wider past it.
// ecs::EntityWorld::Cache moves from upstream's +0x340 to +0x348 because of this.
// Enter/Leave are still the no-ops below: the algorithm is read, not yet ported.
typedef struct _RTL_CRITICAL_SECTION {
    uint64_t Mutex[5];              // pthread_mutex_t
    LONG RecursionCount;            // +0x28, ++'d by the game once the mutex is taken
    LONG SpinCount;                 // +0x2c, the trylock budget (4000 in the live world)
} CRITICAL_SECTION, *LPCRITICAL_SECTION;
static_assert(sizeof(CRITICAL_SECTION) == 0x30, "ls::CriticalSection is 48 bytes on Linux");

// SRWLOCK holds a lazily created pthread_rwlock_t* (win32.cpp); zero-initialised = unlocked.
// This is *ours*: the shim owns the storage, and 8 bytes is all it needs.
typedef struct _RTL_SRWLOCK { PVOID Ptr; } SRWLOCK, *PSRWLOCK;
#define SRWLOCK_INIT {0}

// PORT (linux): the game's own SRW lock is a bare pthread_rwlock_t -- 56 bytes, not the 8
// above. Read off ls::LocalTemplateManager::DeleteLocalTemplate (bin/bg3 0x23986e0): it
// calls pthread_rwlock_trywrlock@plt on `this + 0`, spins 4001 times
// retrying it, falls back to pthread_rwlock_wrlock, and unlocks with pthread_rwlock_unlock
// -- all on the object itself, with no indirection -- and its first LegacyRefMap lands at
// +0x38 exactly, which is sizeof(pthread_rwlock_t) here.
//
// So a mirror that holds the *game's* lock must declare LS_SRWLOCK; SRWLOCK there is wrong
// by 48 bytes. Derived from pthread_rwlock_t rather than hand-counted, the BaseString.h way.
// Mirrors that still have it wrong are
// converted one at a time, each against a witness, because a lock-size change is never local.
typedef pthread_rwlock_t LS_SRWLOCK;
static_assert(sizeof(LS_SRWLOCK) == 0x38, "the game's ls::SRWLock is a 56-byte pthread_rwlock_t");

// PORT (linux): and Noesis's two file-static locks are neither of the above -- each is a
// 4-byte pthread_spinlock_t, taken with pthread_spin_lock@plt: `gMutex` (bin/bg3 0x7b65ff8,
// `lea rdi` at 0x2fdfece in Noesis::Reflection::RegisterType 0x2fdfea0) and the
// SymbolManager's (0x7b66060, `lea rdi` at 0x3155c0f in Noesis::SymbolManager::AddString
// 0x3155c00). tools/targets-hand.tsv carries both witnesses.
//
// A spinlock has no shared mode, so upstream's AcquireSRWLockShared on either is wrong twice
// over: it asks for a mode that does not exist, and SRWLOCK here is the shim's own lazily
// created pthread_rwlock_t*, so acquiring through it would write an 8-byte pointer over
// gMutex and the 4 bytes after it (gRecursiveDepth).
typedef pthread_spinlock_t NS_SPINLOCK;
static_assert(sizeof(NS_SPINLOCK) == 4, "Noesis's gMutex is a 4-byte pthread_spinlock_t");

// PORT (linux): the extender also *creates* one of these -- Noesis::TypeClass's Ancestors
// block (Lua/Libs/ClientUI/Builtins.inl) -- and an unlocked pthread_spinlock_t here is **1**,
// not 0: glibc 2.44's x86-64 pthread_spin_lock is `lock decl` and waits for the word to go
// positive again, so pthread_spin_init writes 1, lock writes 0 and unlock writes 1 (measured
// on glibc 2.44). Zeroed storage therefore reads as *held*, and the game spins on
// it forever. So a spinlock the extender allocates must be initialised through this, never
// left zero and never through InitializeSRWLock (whose SRWLOCK is the shim's own lazily
// created pthread_rwlock_t* and is zero when unlocked).

typedef struct _GUID {
    uint32_t       Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[8];
} GUID, UUID, *LPGUID;

typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; };
    LONGLONG QuadPart;
} LARGE_INTEGER;

typedef struct _SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME;

typedef struct _FILETIME { DWORD dwLowDateTime, dwHighDateTime; } FILETIME;

typedef struct _MEMORY_BASIC_INFORMATION {
    PVOID BaseAddress; PVOID AllocationBase; DWORD AllocationProtect;
    SIZE_T RegionSize; DWORD State; DWORD Protect; DWORD Type;
} MEMORY_BASIC_INFORMATION;

typedef struct _MODULEINFO {
    LPVOID lpBaseOfDll; DWORD SizeOfImage; LPVOID EntryPoint;
} MODULEINFO;

struct EXCEPTION_RECORD { DWORD ExceptionCode; DWORD ExceptionFlags; EXCEPTION_RECORD* ExceptionRecord; PVOID ExceptionAddress; DWORD NumberParameters; ULONG_PTR ExceptionInformation[15]; };
struct CONTEXT;
struct EXCEPTION_POINTERS { EXCEPTION_RECORD* ExceptionRecord; CONTEXT* ContextRecord; };
struct SECURITY_ATTRIBUTES { DWORD nLength; LPVOID lpSecurityDescriptor; BOOL bInheritHandle; };
typedef SECURITY_ATTRIBUTES* LPSECURITY_ATTRIBUTES;
struct WIN32_FIND_DATAW { DWORD dwFileAttributes; FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime; DWORD nFileSizeHigh, nFileSizeLow, dwReserved0, dwReserved1; WCHAR cFileName[260]; WCHAR cAlternateFileName[14]; };
typedef WIN32_FIND_DATAW* LPWIN32_FIND_DATAW;
typedef UINT_PTR SOCKET;
#define INVALID_SOCKET (SOCKET)(~0)

// --- calling conventions / SAL ----------------------------------------------
#define WINAPI
#define CALLBACK
#define APIENTRY
#define _In_
#define _In_z_
#define _In_opt_
#define _Out_
#define _Out_opt_
#define _Inout_
#define _Printf_format_string_
#define _Post_ptr_invalid_
#define _Pre_valid_
#define _Post_writable_size_(x)
#define _Post_writable_byte_size_(x)
#define _Pre_writable_size_(x)
#define _TRUNCATE ((size_t)-1)
#define MAKEINTRESOURCE(i) ((LPCWSTR)(ULONG_PTR)((WORD)(i)))
#define MAKEINTRESOURCEW(i) MAKEINTRESOURCE(i)
#define MessageBox MessageBoxW
#define GetModuleHandle GetModuleHandleW
#define LoadLibrary LoadLibraryW
#define GetModuleFileName GetModuleFileNameW

// --- the API surface, implemented in ext/linux/win32.cpp ---------------------
extern "C" {
// UTF-8 <-> UTF-32 (wchar_t is 4 bytes here). CP_UTF8 only.
int    WideCharToMultiByte(UINT cp, DWORD flags, LPCWSTR s, int n, LPSTR out, int outn, LPCSTR, LPBOOL);
int    MultiByteToWideChar(UINT cp, DWORD flags, LPCSTR s, int n, LPWSTR out, int outn);
// process / thread
HANDLE GetCurrentProcess(void);
HANDLE GetCurrentThread(void);
DWORD  GetCurrentProcessId(void);
DWORD  GetCurrentThreadId(void);          // gettid()
BOOL   TerminateProcess(HANDLE, UINT);    // _exit
void   Sleep(DWORD ms);
DWORD  GetLastError(void);                // errno
BOOL   IsDebuggerPresent(void);           // /proc/self/status TracerPid
void   DebugBreak(void);                  // SIGTRAP only under BG3SE_DEBUGBREAK=1, else a log line
void   OutputDebugStringA(LPCSTR);        // no-op
void   OutputDebugStringW(LPCWSTR);       // no-op
int    MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT);   // logs; there is no dialog
int    MessageBoxW(HWND, LPCWSTR, LPCWSTR, UINT);
LPWSTR GetCommandLineW(void);             // /proc/self/cmdline, joined
// time
BOOL   QueryPerformanceCounter(LARGE_INTEGER*);   // CLOCK_MONOTONIC ns
BOOL   QueryPerformanceFrequency(LARGE_INTEGER*); // 1e9
ULONGLONG GetTickCount64(void);                   // CLOCK_MONOTONIC ms
void   GetSystemTime(SYSTEMTIME*);
void   GetLocalTime(SYSTEMTIME*);
// files (wide paths are converted to UTF-8)
BOOL   CreateDirectoryW(LPCWSTR, LPSECURITY_ATTRIBUTES);
BOOL   PathFileExistsW(LPCWSTR);
BOOL   PathIsDirectoryW(LPCWSTR);
BOOL   RemoveDirectoryW(LPCWSTR);
BOOL   DeleteFileW(LPCWSTR);
BOOL   CopyFileW(LPCWSTR from, LPCWSTR to, BOOL failIfExists);
// modules: HMODULE is a dlopen handle. GetModuleHandleW(name) is NULL for every Windows
// name (there is no BG3ScriptExtender.dll, no bg3.exe); GetProcAddress is dlsym.
HMODULE GetModuleHandleW(LPCWSTR);
HMODULE GetModuleHandleA(LPCSTR);
HMODULE LoadLibraryW(LPCWSTR);
DWORD  GetModuleFileNameW(HMODULE, LPWSTR, DWORD);   // our own .so for NULL
FARPROC GetProcAddress(HMODULE, LPCSTR);
BOOL   GetModuleInformation(HANDLE, HMODULE, MODULEINFO*, DWORD);   // fails
// memory
BOOL   VirtualProtect(LPVOID, SIZE_T, DWORD newProtect, LPDWORD oldProtect);  // mprotect
SIZE_T VirtualQuery(LPCVOID, MEMORY_BASIC_INFORMATION*, SIZE_T);              // /proc/self/maps
// Is [p, p+n) readable? process_vm_readv on ourselves: EFAULT, never a fault. See
// Lua/Shared/LuaTypeValidators.h for what upstream uses it for.
BOOL   IsBadReadPtr(const void* p, UINT_PTR n);
// SRW locks: a pthread_rwlock_t created on first use
void   InitializeSRWLock(PSRWLOCK);
void   AcquireSRWLockExclusive(PSRWLOCK);
void   ReleaseSRWLockExclusive(PSRWLOCK);
BOOL   TryAcquireSRWLockExclusive(PSRWLOCK);
void   AcquireSRWLockShared(PSRWLOCK);
void   ReleaseSRWLockShared(PSRWLOCK);
BOOL   TryAcquireSRWLockShared(PSRWLOCK);
// The same four on the *game's* lock (LS_SRWLOCK above). Named apart rather than overloaded
// because this block has C linkage -- and the different name is the better documentation
// anyway: these are not stubs and not the shim's own storage. The object is a live
// pthread_rwlock_t that the game locks the same way, so they interoperate with it.
void   InitializeGameSRWLock(LS_SRWLOCK*);
void   DestroyGameSRWLock(LS_SRWLOCK*);
void   AcquireGameSRWLockExclusive(LS_SRWLOCK*);
void   ReleaseGameSRWLockExclusive(LS_SRWLOCK*);
void   AcquireGameSRWLockShared(LS_SRWLOCK*);
void   ReleaseGameSRWLockShared(LS_SRWLOCK*);
// Noesis's own spinlock (NS_SPINLOCK above): exclusive only, and live game memory like the
// two above. No shared pair, because the object has no shared mode to ask for.
void   AcquireNoesisSpinLock(NS_SPINLOCK*);
void   ReleaseNoesisSpinLock(NS_SPINLOCK*);
// ... and the one the extender allocates itself (see the NS_SPINLOCK typedef): this is the
// only way to get an *unlocked* one, because unlocked is 1 here and not 0.
void   InitializeNoesisSpinLock(NS_SPINLOCK*);
// critical sections inside game-structure mirrors: no-ops (see the typedef)
void   InitializeCriticalSection(CRITICAL_SECTION*);
void   DeleteCriticalSection(CRITICAL_SECTION*);
void   EnterCriticalSection(CRITICAL_SECTION*);
void   LeaveCriticalSection(CRITICAL_SECTION*);
// UUIDs (Base.cpp): the Windows byte order, which Guid::Parse then swaps as upstream does
RPC_STATUS UuidFromStringA(RPC_CSTR, UUID*);
RPC_STATUS UuidCreate(UUID*);              // getrandom
// PE resources: none here (the Lua bundle is linked in, see LuaBundle.cpp); these fail
HRSRC  FindResourceW(HMODULE, LPCWSTR, LPCWSTR);
HGLOBAL LoadResource(HMODULE, HRSRC);
LPVOID LockResource(HGLOBAL);
DWORD  SizeofResource(HMODULE, HRSRC);
BOOL   FreeResource(HGLOBAL);
HLOCAL LocalAlloc(UINT, SIZE_T);
HLOCAL LocalFree(HLOCAL);
// console: nothing — CoreLib/Console.cpp is ported to ANSI/socket output
HANDLE GetStdHandle(DWORD);
BOOL   SetConsoleTextAttribute(HANDLE, WORD);
}
#define FindResource FindResourceW

// --- winnt.h's Interlocked* macros over the compiler intrinsics --------------
#define InterlockedOr64           _InterlockedOr64
#define InterlockedAnd64          _InterlockedAnd64
#define InterlockedExchange64     _InterlockedExchange64
#define InterlockedExchangeAdd64  _InterlockedExchangeAdd64
#define InterlockedIncrement      _InterlockedIncrement
#define InterlockedDecrement      _InterlockedDecrement

// --- <eh.h>/<corecrt_terminate.h> put terminate() in the global namespace ----
#include <exception>
using std::terminate;

// --- MSVC CRT spellings -------------------------------------------------------
#define sprintf_s snprintf_s_shim
#define _snprintf_s snprintf_s_shim
#define _vsnprintf_s vsnprintf_s_shim
#define strcpy_s strcpy_s_shim
#define wcscpy_s wcscpy_s_shim
#define _stricmp strcasecmp
#define _strdup strdup
#define _byteswap_ulong __builtin_bswap32
#define _byteswap_ushort __builtin_bswap16
#define _byteswap_uint64 __builtin_bswap64
inline int _wcsicmp(wchar_t const* a, wchar_t const* b) { return wcscasecmp(a, b); }

template <size_t N, class... Args>
inline int snprintf_s_shim(char (&buf)[N], char const* fmt, Args... args)
{
    return snprintf(buf, N, fmt, args...);
}
template <class... Args>
inline int snprintf_s_shim(char* buf, size_t n, char const* fmt, Args... args)
{
    return snprintf(buf, n, fmt, args...);
}
inline int vsnprintf_s_shim(char* buf, size_t n, size_t, char const* fmt, va_list args)
{
    return vsnprintf(buf, n, fmt, args);
}
inline int strcpy_s_shim(char* dst, size_t n, char const* src)
{
    size_t i = 0; for (; src[i] && i + 1 < n; i++) dst[i] = src[i]; dst[i] = 0; return 0;
}
template <size_t N>
inline int strcpy_s_shim(char (&dst)[N], char const* src) { return strcpy_s_shim(dst, N, src); }
inline int wcscpy_s_shim(wchar_t* dst, size_t n, wchar_t const* src)
{
    size_t i = 0; for (; src[i] && i + 1 < n; i++) dst[i] = src[i]; dst[i] = 0; return 0;
}
template <size_t N>
inline int wcscpy_s_shim(wchar_t (&dst)[N], wchar_t const* src) { return wcscpy_s_shim(dst, N, src); }
inline int strncpy_s(char* dst, size_t n, char const* src, size_t count)
{
    strncpy(dst, src, count < n ? count : n);
    if (n) dst[n - 1] = 0;
    return 0;
}
inline int freopen_s(FILE** f, char const* path, char const* mode, FILE* stream)
{
    *f = freopen(path, mode, stream); return *f ? 0 : -1;
}

// --- LLP64 -> LP64 ------------------------------------------------------------
#define __int64 long
