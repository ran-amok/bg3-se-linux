#pragma once

BEGIN_SE()

template <class Pred>
inline void SpinWait(Pred pred)
{
    while (!pred()) {
        unsigned spinCount = 0;
        while (!pred()) {
            _mm_pause();
            if (spinCount++ > 400) {
                Sleep(0);
                break;
            }
        }
    }
}

// PORT (linux): the game has a *third* SRW primitive, and it is the only one whose size
// Win32's SRWLOCK happens to get right -- so a mirror holding it needs no lock fix at all,
// and it has to be kept apart from the ones that do.  It is a bare
// 4-byte state word spun on entirely in userspace: a writer takes 0x100000 with a
// `lock xadd` and backs it out with `lock sub` on failure, readers live in the low 20 bits
// (`test eax, 0xfffff`), and the contended path is `pause` x400 then a yield.  No kernel
// object, no owner, no recursion count, so it is 4 bytes on every platform and the 4 bytes
// behind it are padding -- unlike SRWSpinLock above, which is that word plus an owner and a
// count, and unlike SRWLock below, which is a whole pthread_rwlock_t here.
// Witnesses, both on bin/bg3 build-id c8da62b1 and both on ls::Material's word at +0x3d0:
//   the write side, ls::Material::ClearConstantBufferData 0x27a5db0 -- `lock xadd
//     [rbx+0x3d0], 0x100000`, back out with `lock sub` when the old value was already
//     above 0xfffff, then spin until `test eax, 0xfffff` is zero (readers drained),
//     and release with `lock sub [rbx+0x3d0], 0x100000`;
//   the read side, ls::Material::GetOrCreateConstantBuffer 0x2503080 -- `lock xadd
//     [rdi+0x3d0], 1`, back out if the result reached 0x100000, read
//     MaterialCBs[i] at `[rdi + 16*i + 0x3d8]`, release with `lock dec [rdi+0x3d0]`.
// The next member sitting 8 bytes on is what Win32's SRWLOCK gives too, which is why
// this is the one SRW site in the tree that needed no offset fix.
class SpinRWLock
{
public:
    void ReadLock();
    void ReadUnlock();
    void WriteLock();
    void WriteUnlock();

private:
    std::atomic<uint32_t> FastLock;
    uint32_t Padding_;
};

static_assert(sizeof(SpinRWLock) == 8, "the game's spin rw-lock is a 4-byte word plus padding");

class SRWSpinLock
{
public:
    SRWSpinLock() = default;
    SRWSpinLock(SRWSpinLock const&) = delete;
    SRWSpinLock(SRWSpinLock &&) = delete;
    SRWSpinLock& operator = (SRWSpinLock const&) = delete;
    SRWSpinLock& operator = (SRWSpinLock &&) = delete;

    void ReadLock();
    void ReadUnlock();
    void WriteLock();
    void WriteUnlock();

#if defined(BG3SE_LINUX)
    // PORT (linux): the game's spin lock is {u32 state; u64 owner = pthread_self(); u32 WriteEnterCount},
    // 0x18 bytes: read off ls::TranslatedStringRepository::GetOrCreateTranslatedArgumentString
    // (lock xadd 0x100000 at +0, the owner compared with -1 at +8, the count at +0x10).
    // A mirror with a DWORD owner (upstream's 12-byte {u32, DWORD, u32}) puts ++WriteEnterCount
    // on the low half of the game's owner word, misaligns the write-unlock and aborts in
    // SRWSpinLock::WriteUnlock; any object holding one of these by value must use this layout.
    using ThreadId = uint64_t;
    static constexpr ThreadId NullThreadId = ~ThreadId(0);

private:
    std::atomic<uint32_t> FastLock;
    uint32_t Padding_;
    ThreadId OwningThreadId{ NullThreadId };
    std::atomic<uint32_t> WriteEnterCount{ 0 };
    uint32_t Padding2_;
#else
    using ThreadId = DWORD;
    static constexpr ThreadId NullThreadId = 0xffffffffu;

private:
    std::atomic<uint32_t> FastLock;
    ThreadId OwningThreadId{ NullThreadId };
    std::atomic<uint32_t> WriteEnterCount{ 0 };
#endif

    void WriteWait();
    void ReadWait();
};

#if defined(BG3SE_LINUX)
static_assert(sizeof(SRWSpinLock) == 0x18, "the game's spin lock is 0x18 bytes on Linux");
#endif

class SRWLock
{
public:
    SRWLock();
    ~SRWLock();
    SRWLock(SRWLock const&) = delete;
    SRWLock(SRWLock &&) = delete;
    SRWLock& operator = (SRWLock const&) = delete;
    SRWLock& operator = (SRWLock &&) = delete;

    void ReadLock();
    void ReadUnlock();
    void WriteLock();
    void WriteUnlock();

#if defined(BG3SE_LINUX)
    // PORT (linux): the game's ls::SRWLock (upstream's name for it) wraps a bare
    // pthread_rwlock_t, not Win32's 8-byte handle, and keys recursion on pthread_self()
    // rather than on a kernel thread id -- so the owner slot is 8 bytes, not 4, and the
    // class is 0x48 where MSVC's is 0x10. Both measured on bin/bg3; witnesses below.
    using ThreadId = uint64_t;
    static constexpr ThreadId NullThreadId = ~ThreadId(0);

private:
    LS_SRWLOCK Lock;
    ThreadId OwningThreadId{ NullThreadId };
    std::atomic<uint32_t> WriteEnterCount{ 0 };
#else
    using ThreadId = DWORD;
    static constexpr ThreadId NullThreadId = 0xffffffffu;

private:
    SRWLOCK Lock;
    ThreadId OwningThreadId{ NullThreadId };
    std::atomic<uint32_t> WriteEnterCount{ 0 };
#endif
};

#if defined(BG3SE_LINUX)
// PORT (linux): witnessed on bin/bg3 itself, not carried over from the macOS oracle (which
// implements the primitive as a 12-byte ls::SRWKernelLock, so nothing absolute past it
// transfers).  ls::CacheTemplateManagerBase is the only mirror that
// holds this class by value, and it holds it at +0xd0:
//   ls::CacheTemplateManagerBase::GetTemplate, bin/bg3 0x3e02760 (matched s3:0.86), is
//     upstream's ReadLock/ReadUnlock inlined: `lea rbx, [rdi + 0xd0]` is the only thing ever
//     passed to the shared acquire (0x218f6b0, pthread_rwlock_tryrdlock then a spin) and to
//     pthread_rwlock_unlock@plt, while `[rdi + 0x108]` is compared with -1 and then with the
//     return of pthread_self@plt.  0x108 - 0xd0 = 0x38 = sizeof(pthread_rwlock_t), so the
//     primitive is bare and the owner is a pthread_t sitting immediately behind it.
//   ls::CacheTemplateManagerBase::CacheTemplate, bin/bg3 0x35559d0 (matched r6:4), is
//     WriteUnlock inlined: `dec dword [rbx + 0x110]`, and on reaching zero
//     `mov qword [rbx + 0x108], 0xffffffffffffffff` followed by the unlock on rbx + 0xd0.
//     So WriteEnterCount is at +0x40 from the lock, and the next member of the manager --
//     AddedTemplates, whose buffer is `[rbx + 0x118]` in the same function -- puts the end of
//     the lock at 0x118, i.e. sizeof(SRWLock) == 0x48.
// Derived from sizeof(LS_SRWLOCK) rather than hand-counted, with the literal as the whole
// right-hand side so that a calibration that flips the constant can see it.
static_assert(sizeof(SRWLock) - sizeof(LS_SRWLOCK) == 16,
              "the game's ls::SRWLock is a bare pthread_rwlock_t, a pthread_t owner and a u32");
static_assert(sizeof(SRWLock) == 0x48, "ls::SRWLock is 0x48 bytes on Linux, not Win32's 0x10");
#endif

END_SE()
