#include <stdafx.h>
#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/Symbols.h>
#include <combaseapi.h>

#include <GameDefinitions/Base/TypeInformation.inl>

BEGIN_SE()

TranslatedString TranslatedString::FromString(StringView const& sv)
{
    TranslatedString ts;
    auto sep = sv.find_first_of(';');
    if (sep != STDString::npos) {
        ts.Handle.Handle = FixedString(sv.substr(0, sep));
        ts.Handle.Version = (uint16_t)atoi(sv.substr(sep + 1).data());
    } else {
        ts.Handle.Handle = FixedString(sv);
    }
    return ts;
}

std::optional<StringView> TranslatedString::Get() const
{
    return GetStaticSymbols().GetTranslatedStringRepository()->GetTranslatedString(Handle);
}

std::optional<StringView> TranslatedFSString::Get() const
{
    return GetStaticSymbols().GetTranslatedStringRepository()->GetTranslatedString(Handle);
}

uint32_t ThreadRegistry::RequestThreadIndex()
{
    // PORT (linux): not in the manifest yet (phase 6). Its callers are the ECS write paths
    // (EntityCommandBuffer::Deferred and the frame allocators); calling through the null
    // pointer would take the game down, so answer for thread 0 and say so once. The entity
    // world is bound, so a Lua write through Ext.Entity can reach this, and thread 0 is a
    // guess that is only right on the main thread. Locating the real symbol is the fix.
    auto request = GetStaticSymbols().ls__ThreadRegistry__RequestThreadIndex;
    if (request == nullptr) {
        static bool warned{ false };
        if (!warned) {
            warned = true;
            WARN("ls::ThreadRegistry::RequestThreadIndex is not in the manifest; assuming thread index 0");
        }

        return 0;
    }

    return request();
}

END_SE()
