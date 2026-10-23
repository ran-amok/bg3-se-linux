#include "stdafx.h"
#include <CoreLib/Base/Base.h>
#include <combaseapi.h>
#if defined(BG3SE_LINUX)
// PORT (linux): symbolised frames for AssertionFailed() below
#include <execinfo.h>
#include <dlfcn.h>
#include <cxxabi.h>
#endif

#include <CoreLib/Base/BaseMemory.inl>
#include <CoreLib/Base/BaseString.inl>
#include <CoreLib/Base/BaseMap.inl>

BEGIN_SE()

CoreLibPlatformInterface gCoreLibPlatformInterface;

__declspec(noinline) void AssertionFailed(char const* expr)
{
    if (gCoreLibPlatformInterface.GlobalConsole) {
        gCoreLibPlatformInterface.GlobalConsole->Print(DebugMessageType::Error, "Assertion failed:");
        gCoreLibPlatformInterface.GlobalConsole->Print(DebugMessageType::Error, expr);
#if defined(BG3SE_LINUX)
        // PORT (linux): an assertion that fires in the port means our model of the game is
        // wrong, and the abort that follows is the right answer -- but the log named only
        // the expression, so the first one cost a core dump to place. The object
        // exports one symbol, so dladdr can rarely name a frame;
        // module+offset is what addr2line wants anyway.
        void* frames[24];
        int n = backtrace(frames, 24);
        for (int i = 1; i < n; i++) {   // frame 0 is AssertionFailed itself
            Dl_info info{};
            char line[512];
            bool located = dladdr(frames[i], &info) != 0;   // on failure info is untouched
            if (located && info.dli_sname != nullptr) {
                int status{ 0 };
                char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
                snprintf(line, sizeof(line), "  #%-2d %s+0x%tx", i,
                    (status == 0 && demangled) ? demangled : info.dli_sname,
                    (char const*)frames[i] - (char const*)info.dli_saddr);
                free(demangled);
            } else if (located && info.dli_fname != nullptr) {
                snprintf(line, sizeof(line), "  #%-2d %s+0x%tx", i, info.dli_fname,
                    (char const*)frames[i] - (char const*)info.dli_fbase);
            } else {
                snprintf(line, sizeof(line), "  #%-2d %p", i, frames[i]);
            }

            gCoreLibPlatformInterface.GlobalConsole->Print(DebugMessageType::Error, line);
        }
#endif
    }

    DebugBreak();
    std::terminate();
}

const Guid Guid::Null{};

std::optional<Guid> Guid::Parse(StringView s)
{
    Guid uuid;
    if (UuidFromStringA((RPC_CSTR)s.data(), (UUID*)&uuid) == RPC_S_OK) {
        // BG3 parses UUIDs into a weird byte swapped format
        auto v1 = uuid.Val[1];
        uuid.Val[1] = (((v1 >> 56) & 0xff) << 48) |
            (((v1 >> 48) & 0xff) << 56) |
            (((v1 >> 40) & 0xff) << 32) |
            (((v1 >> 32) & 0xff) << 40) |
            (((v1 >> 24) & 0xff) << 16) |
            (((v1 >> 16) & 0xff) << 24) |
            (((v1 >> 8) & 0xff) << 0) |
            (((v1 >> 0) & 0xff) << 8);

        return uuid;
    } else {
        return {};
    }
}

std::optional<Guid> Guid::ParseGuidString(StringView nameGuid)
{
    if (nameGuid.size() < 36) {
        return {};
    }

    auto guid = nameGuid.data() + nameGuid.size() - 36;
    return Parse(std::string_view(guid, 36));
}

STDString Guid::ToString() const
{
    uint8_t const* p = reinterpret_cast<uint8_t const*>(this);
    char s[100];
    sprintf_s(s, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        p[3], p[2], p[1], p[0],
        p[5], p[4],
        p[7], p[6],
        p[9], p[8],
        p[11], p[10], p[13], p[12], p[15], p[14]
    );
    return s;
}

Guid Guid::Generate()
{
    UUID uuid;
    UuidCreate(&uuid);
    return *reinterpret_cast<Guid*>(&uuid);
}

void LSAcquireSRWLockExclusive(PSRWLOCK SRWLock)
{
    if (!TryAcquireSRWLockExclusive(SRWLock))
    {
        auto loops = 0;
        while (loops++ < 4000)
        {
            if (TryAcquireSRWLockExclusive(SRWLock))
                return;
        }

        AcquireSRWLockExclusive(SRWLock);
    }
}

END_SE()
