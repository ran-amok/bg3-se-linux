#include <GameDefinitions/Base/Lock.h>
#if defined(BG3SE_LINUX)
#include <pthread.h>
#endif

BEGIN_SE()

// PORT (linux): the game keys its spin lock on pthread_self(), not on a kernel thread id
// (its WriteLock compares the owner word with pthread_self()), so
// ours must store the same value for recursion to interoperate in both directions.
static inline SRWSpinLock::ThreadId CurrentLockThread()
{
#if defined(BG3SE_LINUX)
    return (SRWSpinLock::ThreadId)pthread_self();
#else
    return GetCurrentThreadId();
#endif
}

// PORT (linux): the game's bare spin rw-lock -- the same 0x100000 / 0xfffff protocol
// SRWSpinLock below implements, without the owner and recursion count, so it is not
// recursive and has no fast path for the owning thread.  On Linux this has to be the game's
// own algorithm on the game's word: the shim's SRWLOCK is a lazily created pthread_rwlock_t*
// (windows.h), so AcquireSRWLockShared() on this member would have written a pointer into a
// state word the game is concurrently spinning on -- the same corruption class as a wrong
// SRWSpinLock layout (Lock.h).  The Windows path is left exactly as upstream has it (the
// member is an SRWLOCK there and is the same 8 bytes), because whether the Windows game's
// field is this spin word too has not been read off that binary.
void SpinRWLock::ReadLock()
{
#if defined(BG3SE_LINUX)
    for (;;) {
        SpinWait([&] () { return (FastLock & 0xfff00000u) == 0; });

        if ((FastLock.fetch_add(1) & 0xfff00000u) == 0) {
            break;
        }

        --FastLock;
    }
#else
    AcquireSRWLockShared(reinterpret_cast<PSRWLOCK>(this));
#endif
}

void SpinRWLock::ReadUnlock()
{
#if defined(BG3SE_LINUX)
    se_assert((FastLock & 0x000fffffu) > 0);
    --FastLock;
#else
    ReleaseSRWLockShared(reinterpret_cast<PSRWLOCK>(this));
#endif
}

void SpinRWLock::WriteLock()
{
#if defined(BG3SE_LINUX)
    for (;;) {
        SpinWait([&] () { return (FastLock & 0xfff00000u) == 0; });

        if ((FastLock.fetch_add(0x100000u) & 0xfff00000u) == 0) {
            break;
        }

        FastLock -= 0x100000u;
    }

    if ((FastLock & 0x000fffffu) != 0) {
        SpinWait([&] () { return (FastLock & 0x000fffffu) == 0; });
    }
#else
    AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(this));
#endif
}

void SpinRWLock::WriteUnlock()
{
#if defined(BG3SE_LINUX)
    se_assert((FastLock & 0xfff00000u) > 0);
    FastLock -= 0x100000u;
#else
    ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(this));
#endif
}


void SRWSpinLock::ReadLock()
{
    if (OwningThreadId == NullThreadId || OwningThreadId != CurrentLockThread()) {
        ReadWait();
    }
}

void SRWSpinLock::ReadUnlock()
{
    if (OwningThreadId == NullThreadId || OwningThreadId != CurrentLockThread()) {
        se_assert((FastLock & 0x000fffffu) > 0);
        --FastLock;
    }
}

void SRWSpinLock::WriteLock()
{
    if (OwningThreadId == NullThreadId || OwningThreadId != CurrentLockThread()) {
        WriteWait();

        OwningThreadId = CurrentLockThread();
    }

    ++WriteEnterCount;
}

void SRWSpinLock::WriteUnlock()
{
    se_assert(WriteEnterCount > 0);
    if (--WriteEnterCount == 0) {
        se_assert(OwningThreadId == CurrentLockThread());
        se_assert((FastLock & 0xfff00000u) > 0);
        OwningThreadId = NullThreadId;
        FastLock -= 0x100000u;
    }
}

void SRWSpinLock::WriteWait()
{
    for (;;) {
        SpinWait([&] () { return (FastLock & 0xfff00000u) == 0; });

        if ((FastLock.fetch_add(0x100000u) & 0xfff00000u) == 0) {
            break;
        }

        FastLock -= 0x100000u;
    }

    if ((FastLock & 0x000fffffu) != 0) {
        SpinWait([&] () { return (FastLock & 0x000fffffu) == 0; });
    }
}

void SRWSpinLock::ReadWait()
{
    for (;;) {
        SpinWait([&] () { return (FastLock & 0xfff00000u) == 0; });

        if ((FastLock.fetch_add(1) & 0xfff00000u) == 0) {
            break;
        }

        --FastLock;
    }
}


// PORT (linux): Lock is the game's own pthread_rwlock_t, so every operation on it is the
// game's algorithm rather than the shim's storage -- the same split RootTemplates.inl makes
// for LocalTemplateManager's bare lock.  The recursion key is pthread_self() for the same
// reason as SRWSpinLock's: ls::CacheTemplateManagerBase::GetTemplate (bin/bg3 0x3e02760)
// compares the owner word against pthread_self@plt, so ours has to store that value or the
// game and the extender would each think the other never holds the lock.
#if defined(BG3SE_LINUX)
#define LS_SRW_INIT()             InitializeGameSRWLock(&Lock)
#define LS_SRW_DESTROY()          DestroyGameSRWLock(&Lock)
#define LS_SRW_LOCK_SHARED()      AcquireGameSRWLockShared(&Lock)
#define LS_SRW_UNLOCK_SHARED()    ReleaseGameSRWLockShared(&Lock)
#define LS_SRW_LOCK_EXCLUSIVE()   AcquireGameSRWLockExclusive(&Lock)
#define LS_SRW_UNLOCK_EXCLUSIVE() ReleaseGameSRWLockExclusive(&Lock)
#else
#define LS_SRW_INIT()             InitializeSRWLock(&Lock)
#define LS_SRW_DESTROY()          ((void)0)
#define LS_SRW_LOCK_SHARED()      AcquireSRWLockShared(&Lock)
#define LS_SRW_UNLOCK_SHARED()    ReleaseSRWLockShared(&Lock)
#define LS_SRW_LOCK_EXCLUSIVE()   AcquireSRWLockExclusive(&Lock)
#define LS_SRW_UNLOCK_EXCLUSIVE() ReleaseSRWLockExclusive(&Lock)
#endif

static inline SRWLock::ThreadId CurrentRWLockThread()
{
#if defined(BG3SE_LINUX)
    return (SRWLock::ThreadId)pthread_self();
#else
    return GetCurrentThreadId();
#endif
}

SRWLock::SRWLock()
{
    LS_SRW_INIT();
}

SRWLock::~SRWLock()
{
    assert(OwningThreadId == NullThreadId);
    LS_SRW_DESTROY();
}

void SRWLock::ReadLock()
{
    if (OwningThreadId == NullThreadId || OwningThreadId != CurrentRWLockThread()) {
        LS_SRW_LOCK_SHARED();
    }
}

void SRWLock::ReadUnlock()
{
    if (OwningThreadId == NullThreadId || OwningThreadId != CurrentRWLockThread()) {
        LS_SRW_UNLOCK_SHARED();
    }
}

void SRWLock::WriteLock()
{
    if (OwningThreadId == NullThreadId || OwningThreadId != CurrentRWLockThread()) {
        LS_SRW_LOCK_EXCLUSIVE();
        OwningThreadId = CurrentRWLockThread();
    }

    ++WriteEnterCount;
}

void SRWLock::WriteUnlock()
{
    se_assert(WriteEnterCount > 0);
    if (--WriteEnterCount == 0) {
        se_assert(OwningThreadId == CurrentRWLockThread());
        OwningThreadId = NullThreadId;
        LS_SRW_UNLOCK_EXCLUSIVE();
    }
}

END_SE()
