#pragma once

#include <cstdint>
#include <cassert>
#include <type_traits>
#include <iostream>
#include <chrono>
#include <variant>

#define BEGIN_SE() namespace bg3se {
#define END_SE() }

#define BEGIN_NS(ns) namespace bg3se::ns {
#define END_NS() }

#define BEGIN_BARE_NS(ns) namespace ns {
#define END_BARE_NS() }

// PORT (linux): the Itanium C++ ABI may start a derived class's first member inside the
// tail padding of a non-POD base; MSVC never does. Upstream's mirrors are calibrated to
// MSVC, so a derived mirror whose base has tail padding comes out short here even though
// every member is right. Written on its own line before the derived class's *first*
// member, this pushes that member back to sizeof(Base), which is where MSVC starts it:
// Itanium's sizeof for a non-empty class is its dsize rounded up to the class alignment,
// so sizeof(Base) is the first alignof(Base)-aligned offset at or past the base
// subobject's data. It is a no-op under MSVC, which already places it there.
//
// Two rules of use. It is a layout claim like any other, so it goes in only where a
// witness says the mirror is short -- never on suspicion. And it must stay on its own
// line: make_property_map.py parses these headers one line at a time and silently drops
// a member whose declaration line carries a macro call it cannot match.
#define MSVC_TAIL_PADDING(base) alignas(alignof(base))

#if defined(NDEBUG)
#if defined(SE_RELEASE_ASSERTS)
#define se_assert(x) if (!(x)) [[unlikely]] { bg3se::AssertionFailed(#x); }
#else
#define se_assert(x)
#endif
#else
#define se_assert(x) assert(x)
#endif

BEGIN_SE()

__declspec(noinline) void AssertionFailed(char const* expr);

template <class>
// false value attached to a dependent name (for static_assert)
constexpr bool AlwaysFalse = false;


// Helper struct to allow function overloading without (real) template-dependent parameters
template <class>
struct Overload {};

// PORT (linux): is the libc++ sfinae-tag overlay in front of the standard headers?  Stock
// libc++ lays a variant nested in a variant out with an 8-byte hole at the front (the outer
// variant's two empty sfinae bases collide with the inner's, Itanium forbids two subobjects
// of one type at one address, so clang moves the union to +8): variant<variant<int,double>,
// int> is 32 bytes there and 24 under the overlay
// (ext/toolchain/libcxx-sfinae-tag.patch).  The game's ls::Variant has no hole, so every
// nested-variant
// mirror and both spell_cast mirrors are right only with the overlay in place; a TU that
// compiled without it would also define those types differently from every other TU.
// Calibrated by compiling this TU once with the overlay's -I removed.
static_assert(sizeof(std::variant<std::variant<int, double>, int>) == 24,
    "the libc++ sfinae-tag overlay is not on the include path (ext/toolchain/libcxx-overlay.sh)");


// Base class for game objects that cannot be copied.
template <class T>
class Noncopyable
{
public:
    inline Noncopyable() = default;

    Noncopyable(const Noncopyable&) = delete;
    T& operator = (const T&) = delete;
    Noncopyable(Noncopyable&&) = delete;
    T& operator = (T&&) = delete;
};

class ProtectedGameObjectBase {};

// Base class for game objects that are managed entirely
// by the game and we cannot create/copy them.
#if defined(BG3SE_LINUX)
// PORT (linux): ProtectedGameObject<T> must NOT share an empty base across instantiations
// here. The Itanium ABI refuses to place an empty base at an address already occupied by
// another object of the same type, so in
//
//     struct Gen : ProtectedGameObject<Gen> { std::array<ThreadState, 0x40> States; };
//     struct alignas(64) ThreadState : ProtectedGameObject<ThreadState> { ... };
//
// States[0]'s inherited ProtectedGameObjectBase would land on Gen's own, and clang pushes
// the *first member* past it instead -- rounded up to the member's alignment. ecs::
// EntityHandleGenerator came out 0x40 bytes too big with ThreadStates at +0x40 instead of
// +0, which is why every EntityHandle failed validation. MSVC has
// no such rule, hence upstream's layout is right on Windows and wrong here.
//
// ProtectedGameObjectBase has no other use in the tree -- nothing tests it as a trait -- so
// dropping it under Linux is the whole fix, and it fixes every member of the family at once
// rather than one struct at a time.
template <class T>
class ProtectedGameObject
{
#else
template <class T>
class ProtectedGameObject : public ProtectedGameObjectBase
{
#endif
public:
    ProtectedGameObject(const ProtectedGameObject&) = delete;
    T& operator = (const T&) = delete;
    ProtectedGameObject(ProtectedGameObject&&) = delete;
    T& operator = (T&&) = delete;

protected:
    ProtectedGameObject() = delete;
    //~ProtectedGameObject() = delete;
};


inline constexpr uint64_t Hash(uint8_t v)
{
    return v;
}

inline constexpr uint64_t Hash(uint16_t v)
{
    return v;
}

inline constexpr uint64_t Hash(uint32_t v)
{
    return v;
}

inline constexpr uint64_t Hash(int32_t v)
{
    return v;
}

inline constexpr uint64_t Hash(uint64_t v)
{
    return v;
}

inline constexpr uint64_t Hash(float v)
{
    if (v >= 9.223372e18f) {
        v -= 9.223372e18f;
        if (v < 9.223372e18f) {
            return 0x8000000000000000ull | (uint32_t)(int32_t)v;
        }
    }
    
    return (uint32_t)(int32_t)v;
}

template <class T> requires std::is_enum_v<T>
inline uint64_t Hash(T v)
{
    return Hash(std::underlying_type_t<T>(v));
}

template <class T> requires std::is_pointer_v<T>
inline uint64_t Hash(T v)
{
    return Hash(std::uintptr_t(v));
}

// See https://github.com/google/cityhash/blob/master/src/city.h#L101
inline constexpr uint64_t HashMix(uint64_t x, uint64_t y)
{
    constexpr uint64_t K = 0x9ddfea08eb382d69ull;

    uint64_t r1 = K * (x ^ y);
    uint64_t r2 = r1 ^ (r1 >> 47);
    uint64_t r3 = (y ^ r2) * K;
    return K * (r3 ^ (r3 >> 47));
}

template <class T1, class T2>
inline uint64_t HashMulti(T1 const& a, T2 const& b)
{
    return HashMix(Hash(a), Hash(b));
}

template <class T1, class T2, class T3>
inline uint64_t HashMulti(T1 const& a, T2 const& b, T3 const& c)
{
    auto h1 = HashMix(Hash(a), Hash(b));
    return HashMix(h1, Hash(c));
}

template <class T1, class T2, class T3, class T4>
inline uint64_t HashMulti(T1 const& a, T2 const& b, T3 const& c, T4 const& d)
{
    auto h1 = HashMix(Hash(a), Hash(b));
    auto h2 = HashMix(h1, Hash(c));
    return HashMix(h2, Hash(d));
}

template <class T>
struct OverrideableProperty
{
    using Type = T;

    T Value;
    bool IsOverridden;
};

enum PropertyOperationResult
{
    Success,
    NoSuchProperty,
    ReadOnly,
    UnsupportedType,
    Unknown
};

enum class DebugMessageType
{
    Debug,
    Info,
    Osiris,
    Warning,
    Error
};

void LSAcquireSRWLockExclusive(PSRWLOCK SRWLock);

struct SRWLockPin
{
    inline SRWLockPin(PSRWLOCK SRWLock)
        : Lock(SRWLock)
    {
        LSAcquireSRWLockExclusive(Lock);
    }
    
    inline ~SRWLockPin()
    {
        ReleaseSRWLockExclusive(Lock);
    }

    PSRWLOCK Lock;
};

class PerfTimer
{
public:
    inline PerfTimer()
    {
        start_ = std::chrono::high_resolution_clock::now();
    }
    
    inline long long time() const
    {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now - start_).count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

END_SE()
