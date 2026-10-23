#pragma once

BEGIN_SE()

struct RuntimeStringHandle
{
    FixedString Handle;
    uint16_t Version{ 0 };

    RuntimeStringHandle();

    inline RuntimeStringHandle(FixedString const& handle)
        : Handle(handle),
        Version(1)
    {}

    inline RuntimeStringHandle(FixedString const& handle, uint16_t version)
        : Handle(handle),
        Version(version)
    {}

    inline bool operator == (RuntimeStringHandle const& fs) const
    {
        return Handle == fs.Handle;
    }

    inline bool operator != (RuntimeStringHandle const& fs) const
    {
        return Handle != fs.Handle;
    }
};

template <>
inline uint64_t HashMapHash<RuntimeStringHandle>(RuntimeStringHandle const& v)
{
    return HashMapHash(v.Handle);
}

struct TranslatedString
{
    RuntimeStringHandle Handle;
    RuntimeStringHandle ArgumentString;

    static TranslatedString FromString(StringView const& sv);
    std::optional<StringView> Get() const;
};

struct TranslatedFSString
{
    RuntimeStringHandle Handle;
    struct TranslatedFSArgumentString* ArgumentString{ nullptr };

    std::optional<StringView> Get() const;
};

struct TranslatedFSArgumentString : private ProtectedGameObject<TranslatedFSArgumentString>
{
    struct Argument
    {
        uint32_t Type;
        TranslatedFSString TranslatedValue;
        STDString StringValue;
    };
    
    struct ArgumentPosition
    {
        uint32_t Start;
        uint32_t End;
    };

    LegacyRefMap<FixedString, Argument>* Arguments;
    RuntimeStringHandle String;
    char* Buffer;
    STDString ParsedString;
    uint32_t BufferLength;
    bool NeedsParsing;
    LegacyRefMap<FixedString, ArgumentPosition> ArgPositions;
};

struct TranslatedArgumentStringBuffer
{
    char* ArgumentsBuffer2;
    bool NeedsFormatting;
    char* ArgumentBuffer;
    STDString Formatted;
    uint32_t ArgumentsLength;
    uint32_t _Pad;
    RuntimeStringHandle Handle;
};


struct TranslatedStringRepository : public ProtectedGameObject<TranslatedStringRepository>
{
    struct TextEntry
    {
        RuntimeStringHandle* Handle;
        LSStringView* Text;
        __int64 field_10;
        __int64 field_18;
    };

    struct TextPool
    {
        Array<STDString*> Strings;
        HashMap<RuntimeStringHandle, LSStringView> Texts;
    };

    int field_0;
    TextPool* TranslatedStrings[9];
    TextPool* FallbackPool;
    TextPool* VersionedFallbackPool;
    Array<void*> field_60;
    HashMap<FixedString, TranslatedArgumentStringBuffer> ArgumentStrings;
    HashMap<FixedString, RuntimeStringHandle> TextToStringKey;
    SRWSpinLock Lock;
    bool IsLoaded;

    std::optional<StringView> GetTranslatedString(RuntimeStringHandle const& handle);
    void UpdateTranslatedString(RuntimeStringHandle const& handle, StringView translated);
};

#if defined(BG3SE_LINUX)
// PORT (linux): read off two of the repository's own methods with tools/disasm.py --
// GetOrCreateTranslatedArgumentString (bin/bg3 0x2380d10) and GetTranslatedArgumentString
// (0x2380b70), `this` in rdi/rbx. ArgumentStrings' walk is
// `div dword [this+0x78]`, the key compare `[this+0x90 + i*4]`, the value read
// `[this+0xA0 + i*8]` -- upstream's HashMap order and sizes, 0x40 wide -- and the lock's
// `lock xadd [this+0xF0]` / owner at +0xF8 / count at +0x100 puts Lock where upstream does.
// Ext.Loca writes through this object, so a wrong Lock offset crashes there.
static_assert(offsetof(TranslatedStringRepository, ArgumentStrings) == 0x70);
static_assert(sizeof(HashMap<FixedString, TranslatedArgumentStringBuffer>) == 0x40,
    "ls::HashMap is {HashKeys, NextIds, Keys, Values}, 0x10 each");
static_assert(offsetof(TranslatedStringRepository, TextToStringKey) == 0xB0);
static_assert(offsetof(TranslatedStringRepository, Lock) == 0xF0);
#endif

struct TranslatedStringKeyManager : public ProtectedGameObject<TranslatedStringKeyManager>
{
    void* VMT;
    Array<Path> Folders_Path;
    LegacyMap<FixedString, TranslatedString> Keys;
    LegacyMap<FixedString, FixedString> Speakers;
    bool DoDecoration;
};

END_SE()
