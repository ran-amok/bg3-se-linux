#pragma once

#include <cstdint>
#include <string>
#include <span>

void MurmurHash3_x64_128(const void* key, int len, uint32_t seed, void* out);

namespace bg3se
{
    template <class T>
    class LSBaseStringView
    {
    public:
        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using size_type = int32_t;
        using difference_type = int32_t;

        static constexpr auto npos{static_cast<size_type>(-1)};

        inline constexpr LSBaseStringView() noexcept : data_(), size_(0) {}
        inline constexpr LSBaseStringView(const LSBaseStringView&) noexcept = default;
        inline constexpr LSBaseStringView& operator=(const LSBaseStringView&) noexcept = default;

        inline constexpr LSBaseStringView(const_pointer s) noexcept
            : data_(s), size_((int32_t)strlen(s)) {}

        LSBaseStringView(std::nullptr_t) = delete;

        inline constexpr LSBaseStringView(std::basic_string_view<T> const& sv) noexcept
            : data_(sv.data()), size_((int32_t)sv.size())
        {}

        inline constexpr LSBaseStringView(const_pointer s, size_type size) noexcept
            : data_(s), size_(size)
        {}

        inline constexpr size_type size() const noexcept
        {
            return size_;
        }

        inline constexpr bool empty() const noexcept
        {
            return size_ == 0;
        }

        inline constexpr const_pointer data() const noexcept
        {
            return data_;
        }

        inline constexpr operator std::basic_string_view<T>() const noexcept
        {
            return std::basic_string_view<T>(data_, size_);
        }

    private:
        const_pointer data_;
        size_type size_;
    };


    using STDString = std::basic_string<char, std::char_traits<char>, GameAllocator<char>>;
    using STDWString = std::basic_string<wchar_t, std::char_traits<wchar_t>, GameAllocator<wchar_t>>;
    using StringView = std::string_view;
    using LSStringView = LSBaseStringView<char>;
    using WStringView = std::wstring_view;

#if defined(BG3SE_LINUX)
    // PORT (linux): ls::STDString is libc++'s basic_string with a 32-bit size_type in the
    // unstable ABI's alternate layout -- 16 bytes, not MSVC's 24. Witness: ls::Path::Path
    // (EPathRoot, StringView const&) at bin/bg3 0x22f3300 reads the short form's data at +0
    // and its size byte at +15, and the long form's ptr at +0, size (u32) at +8, capacity
    // (u32) at +12 with bit 31 set. ls::Path is that string alone,
    // and Path objects are handed to the game's own ctor, so the size is load-bearing: this
    // is what the whole _LIBCPP_ABI_VERSION=2 build exists to get right.
    static_assert(sizeof(STDString) == 16,
        "ls::STDString must be libc++ ABI v2's 16-byte basic_string (bin/bg3 0x22f3300)");
#endif

    std::string ToStdUTF8(std::wstring_view s);
    std::wstring FromStdUTF8(std::string_view s);
    STDString ToUTF8(WStringView s);
    STDWString FromUTF8(StringView s);

    struct FixedStringId
    {
        static constexpr uint32_t NullIndex = 0xffffffffu;

        inline FixedStringId() {}
        
        inline FixedStringId(uint32_t index)
            : Index(index)
        {}

        inline bool operator !() const
        {
            return Index == NullIndex;
        }

        inline explicit operator bool() const
        {
            return Index != NullIndex;
        }

        uint32_t Index{ NullIndex };
    };

    struct FixedStringBase : public FixedStringId
    {
        using CreateFromStringProc = uint32_t (LSStringView const&);
        using GetStringProc = LSStringView * (FixedStringBase const*, LSStringView&);
        using IncRefProc = void(uint32_t index);
        using DecRefProc = void(uint32_t index);

        inline FixedStringBase() {}

        struct Header
        {
            uint32_t Hash;
            uint32_t RefCount;
            uint32_t Length;
            uint32_t Id;
            uint64_t NextFreeIndex;
        };

#if defined(BG3SE_LINUX)
        // PORT (linux): ls::FixedString::GetString (manifest row ls__FixedString__GetString)
        // walks the global string table and returns `entry + 0x18` -- the header is what it
        // skips, so its size is the game's own statement.
        // GetMetadata() subtracts exactly this from the pooled string pointer.
        static_assert(sizeof(Header) == 0x18,
            "FixedString's pool header is 0x18 bytes: GetString returns entry + 0x18");
#endif

        char const* GetString() const;
        StringView GetStringView() const;
        uint32_t GetLength() const;
        uint32_t GetHash() const;
        bool IsValid() const;
        Header const* GetMetadata() const;

        inline operator char const* () const
        {
            return GetString();
        }

        inline operator StringView () const
        {
            return GetStringView();
        }

        static void StaticIncRef(uint32_t index);
        static void StaticDecRef(uint32_t index);
        static uint32_t CreateFixedString(StringView const& sv);

    protected:
        explicit inline FixedStringBase(uint32_t index)
            : FixedStringId(index)
        {}

        inline FixedStringBase(FixedStringBase const& fs)
            : FixedStringId(fs.Index)
        {}

        void IncRef();
        void DecRef();

        char const* GetPooledStringPtr() const;
        static Header const* FindEntry(uint32_t id);
    };

    struct FixedString : public FixedStringBase
    {
        inline FixedString()
            : FixedStringBase()
        {}

        explicit FixedString(StringView str);
        explicit FixedString(char const* str);
        explicit FixedString(std::nullptr_t);

        inline FixedString(FixedString const& fs)
            : FixedStringBase(fs.Index)
        {
            IncRef();
        }

        inline FixedString(FixedStringBase const& fs)
            : FixedStringBase(fs.Index)
        {
            IncRef();
        }

        inline FixedString(FixedString&& fs) noexcept
            : FixedStringBase(fs.Index)
        {
            fs.Index = NullIndex;
        }

        inline ~FixedString()
        {
            DecRef();
        }

        inline FixedString& operator = (FixedString const& fs)
        {
            if (fs.Index != Index) {
                DecRef();
                Index = fs.Index;
                IncRef();
            }

            return *this;
        }

        inline FixedString& operator = (FixedString&& fs) noexcept
        {
            if (this != &fs) {
                DecRef();
                Index = fs.Index;
                fs.Index = NullIndex;
            }

            return *this;
        }

        inline bool operator == (FixedString const& fs) const
        {
            return Index == fs.Index;
        }

        inline bool operator != (FixedString const& fs) const
        {
            return Index != fs.Index;
        }
    };

    struct FixedStringUnhashed : public FixedStringBase
    {
        inline FixedStringUnhashed()
            : FixedStringBase()
        {}

        explicit FixedStringUnhashed(StringView str);
        explicit FixedStringUnhashed(char const* str);

        inline FixedStringUnhashed(FixedStringUnhashed const& fs)
            : FixedStringBase(fs.Index)
        {
            IncRef();
        }

        inline FixedStringUnhashed(FixedStringId const& fs)
            : FixedStringBase(fs.Index)
        {
            IncRef();
        }

        inline FixedStringUnhashed(FixedStringUnhashed&& fs) noexcept
            : FixedStringBase(fs.Index)
        {
            fs.Index = NullIndex;
        }

        inline ~FixedStringUnhashed()
        {
            DecRef();
        }

        inline FixedStringUnhashed& operator = (FixedStringUnhashed const& fs)
        {
            if (fs.Index != Index) {
                DecRef();
                Index = fs.Index;
                IncRef();
            }

            return *this;
        }

        inline FixedStringUnhashed& operator = (FixedStringUnhashed&& fs) noexcept
        {
            if (this != &fs) {
                DecRef();
                Index = fs.Index;
                fs.Index = NullIndex;
            }

            return *this;
        }

        inline bool operator == (FixedStringUnhashed const& fs) const
        {
            return Index == fs.Index;
        }

        inline bool operator != (FixedStringUnhashed const& fs) const
        {
            return Index != fs.Index;
        }
    };

    struct FixedStringNoRef : public FixedStringId
    {
        explicit FixedStringNoRef(StringView str);
        explicit FixedStringNoRef(char const* str);

        inline FixedStringNoRef(FixedStringId const& fs)
            : FixedStringId(fs.Index)
        {}

        inline FixedStringNoRef& operator = (FixedStringId const& fs)
        {
            Index = fs.Index;
            return *this;
        }

        inline bool operator == (FixedStringId const& fs) const
        {
            return Index == fs.Index;
        }

        inline bool operator != (FixedStringId const& fs) const
        {
            return Index != fs.Index;
        }
    };


    inline uint64_t Hash(FixedStringId const& s)
    {
        return (uint64_t)s.Index;
    }


    struct GlobalStringTable : public ProtectedGameObject<GlobalStringTable>
    {
        struct StringEntry : public FixedString::Header
        {
            char Str[1];
        };

        struct SubTable
        {
            struct Element
            {
                uint64_t field_0[8];
            }; 

            Element field_0[64];
            uint64_t LockCounter;
            uint64_t _Pad1[7];
            uint64_t LockEvent;
            uint64_t _Pad2[7];
            int32_t TableIndex;
            uint64_t EntrySize;
            uint32_t EntriesPerBucket;
            uint64_t _Pad3[5];
            uint32_t NumBuckets;
            uint64_t _Pad4[7];
            uint32_t field_1100;
            uint64_t _Pad5[7];
            uint8_t** Buckets;
            uint64_t _Pad6[7];
            uint64_t field_1180;
            uint64_t _Pad7[7];
            CRITICAL_SECTION CriticalSection;
            // PORT (linux): every field in this struct sits in its own 64-byte slot, padded
            // out by the _PadN arrays; the lock and this tail share the last one. Sizing the
            // tail from the lock keeps the slot 64 bytes whatever CRITICAL_SECTION is, which
            // is what the game's stride needs -- ls::CriticalSection is 48 bytes on Linux
            // where Win32's is 40 (ext/linux/shim/windows.h), and the hand-counted [3] made
            // SubTable 0x1208 and moved Main to 0xC658. On Win32 the expression is still 3.
            uint64_t field_11E8[(0x40 - sizeof(CRITICAL_SECTION)) / sizeof(uint64_t)];
        };

        struct MainTable
        {
            using DecRefProc = void (MainTable* self, uint32_t* fs);
            using CreateFromStringProc = FixedString* (MainTable* self, uint32_t* fs, LSStringView* src);

            SubTable::Element SomeTable[64];
            uint64_t field_1000;
            uint64_t _Pad1[7];
            HANDLE EventHandle;
            uint64_t _Pad2[7];
            GlobalStringTable* StringTable;
            uint64_t _Pad3[7];
            uint32_t field_10C0[65535];
        };

        SubTable SubTables[11];
        MainTable Main;
    };

    // The witness for both, read off ~FixedString (bin/bg3 0x229fa40):
    // `mov edi, 0xc600` / `add rdi, [0x7d9b6a0]` / `call 0x229fa70` -- the game itself says
    // MainTable is at +0xC600, i.e. SubTable's stride is 0x1200. StaticDecRef() hands the
    // game that pointer, so a mirror that gets it wrong is a SIGSEGV inside DecRef and not a
    // misread field; assert it here instead of finding out in game again.
    static_assert(sizeof(GlobalStringTable::SubTable) == 0x1200,
        "GlobalStringTable::SubTable must be 0x1200 bytes (bin/bg3 0x229fa48)");
    static_assert(11 * sizeof(GlobalStringTable::SubTable) == 0xC600,
        "GlobalStringTable::Main must land at +0xC600 (bin/bg3 0x229fa48)");


    enum class MemBufferFlags : uint8_t
    {
        Mutable = 1,
        Tracked = 2
    };

    enum class MemoryOrigin : uint8_t
    {
        None = 0,
        Owned = 1,
        External = 2
    };


    struct MemBufferMeta
    {
        MemoryOrigin Origin{ MemoryOrigin::Owned };
        MemBufferFlags Flags{ MemBufferFlags::Mutable };
        uint16_t AllocatorTag{ 0 };
        uint32_t Alignment{ 1 };
    };


    struct MemBuffer
    {
        inline MemBuffer() {}
        MemBuffer(std::span<char const> s);
        MemBuffer(MemBuffer const&);
        MemBuffer(MemBuffer &&) noexcept;
        ~MemBuffer();

        MemBuffer& operator = (MemBuffer const&);
        MemBuffer& operator = (MemBuffer &&) noexcept;

        MemBufferMeta Meta;
        void* Buffer{ nullptr };
        uint64_t Size{ 0 };
    };


    struct MemReadStream
    {
        void* Buffer{ nullptr };
        uint64_t Size{ 0 };
        uint64_t Offset{ 0 };
    };


    struct MemWriteStream : public MemBuffer
    {
        uint64_t Offset{ 0 };
    };


    struct ScratchBuffer
    {
        inline ScratchBuffer() {}
        ScratchBuffer(std::span<char const> s);
        ScratchBuffer(ScratchBuffer const&);
        ScratchBuffer(ScratchBuffer&&) noexcept;
        ~ScratchBuffer();

        ScratchBuffer& operator = (ScratchBuffer const&);
        ScratchBuffer& operator = (ScratchBuffer&&) noexcept;

        void RemapWriteStream();
        void RemapReadStream();

        MemBuffer Buffer;
        MemWriteStream Write;
        MemReadStream Read;
        uint64_t Size{ 0 };
    };

    struct ScratchString
    {
        inline ScratchString() {}
        ScratchString(std::span<char const> s);
        ScratchString(ScratchString const&);
        ScratchString(ScratchString &&) noexcept;
        ~ScratchString();

        ScratchString& operator = (ScratchString const&);
        ScratchString& operator = (ScratchString&&) noexcept;

        char* Buffer{ nullptr };
        uint32_t Position{ 0 };
        uint32_t Capacity{ 0 };
        uint32_t Size{ 0 };
        int32_t FloatPrecision{ -1 };
        bool Managed{ false };
        bool CanGrow{ false };
    };

    struct Guid
    {
        static const Guid Null;

        uint64_t Val[2]{ 0 };

        inline constexpr operator bool() const
        {
            return Val[0] != 0 || Val[1] != 0;
        }

        inline constexpr bool operator !() const
        {
            return Val[0] == 0 && Val[1] == 0;
        }

        inline constexpr bool operator ==(Guid const& o) const
        {
            return Val[0] == o.Val[0] && Val[1] == o.Val[1];
        }

        inline constexpr bool operator !=(Guid const& o) const
        {
            return Val[0] != o.Val[0] || Val[1] != o.Val[1];
        }

        STDString ToString() const;
        static std::optional<Guid> Parse(StringView s);
        static std::optional<Guid> ParseGuidString(StringView nameGuid);
        static Guid Generate();
    };

    inline constexpr uint64_t Hash(Guid const& h)
    {
        return h.Val[0] ^ h.Val[1];
    }

    inline uint64_t Hash(StringView const& s)
    {
        uint64_t hash[2];
        MurmurHash3_x64_128(s.data(), (int)s.size(), 0, hash);
        return hash[0];
    }

    inline uint64_t Hash(STDString const& s)
    {
        uint64_t hash[2];
        MurmurHash3_x64_128(s.data(), (int)s.size(), 0, hash);
        return hash[0];
    }
}

namespace std
{
    template<> struct hash<bg3se::FixedString>
    {
        typedef bg3se::FixedString argument_type;
        typedef std::size_t result_type;

        result_type operator()(argument_type const& fn) const noexcept
        {
            return std::hash<std::uintptr_t>{}((std::uintptr_t)fn.Index);
        }
    };

    inline ostream& operator << (ostream& out, bg3se::Guid const& guid)
    {
        out << guid.ToString();
        return out;
    }

    inline ostream& operator << (ostream& out, bg3se::FixedString const& str)
    {
        if (str) {
            out << str.GetString();
        }
        else {
            out << "(null)";
        }
        return out;
    }
}
