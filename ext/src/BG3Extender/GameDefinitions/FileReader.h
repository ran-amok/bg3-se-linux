#pragma once

#include <GameDefinitions/Base/Base.h>

BEGIN_SE()

enum class FileType
{
    Unknown,
    MemBuffer,
    MemoryMapped
};

struct FileReader : public Noncopyable<FileReader>
{
    bool IsLoaded{ false };
    void* DataPtr{ nullptr };
    void* ReadPtr{ nullptr };
    uint64_t FileSize{ 0 };
    uint64_t FileHandle{ 0 };
    uint64_t FileHandle2{ 0 };
    ScratchBuffer Buffer;
    FileType Type{ FileType::Unknown };
    void* FileObject{ nullptr };
};

#if defined(BG3SE_LINUX)
// PORT (linux): the game's open routine (bin/bg3 0x27009f0, reached from the FileReader ctor
// 0x2704030 -- the one engine hook on EngineHooks' Linux allowlist) stores DataPtr at +8,
// ReadPtr at +0x10, FileSize at +0x18 and the IsLoaded byte at +0.
// FileReaderPin reads exactly these four off an object the game filled in. The tail is not
// asserted: the game writes up to +0x88 and the mirror stops short, which is why the port
// allocates 0x100 for one.
static_assert(offsetof(FileReader, IsLoaded) == 0x00);
static_assert(offsetof(FileReader, DataPtr) == 0x08);
static_assert(offsetof(FileReader, ReadPtr) == 0x10);
static_assert(offsetof(FileReader, FileSize) == 0x18);
#endif

class FileReaderPin
{
public:
    inline FileReaderPin(FileReader* reader)
        : reader_(reader)
    {}

    ~FileReaderPin();

    FileReaderPin(FileReaderPin const&) = delete;
    FileReaderPin& operator =(FileReaderPin const&) = delete;
    FileReaderPin& operator =(FileReaderPin&&) = delete;

    inline FileReaderPin(FileReaderPin&& other) noexcept
    {
        reader_ = other.reader_;
        if (&other != this) {
            other.reader_ = nullptr;
        }
    }

    bool IsLoaded() const
    {
        return reader_ != nullptr && reader_->IsLoaded;
    }

    void* Buf() const
    {
        if (IsLoaded()) {
            return reader_->DataPtr;
        } else {
            return nullptr;
        }
    }

    std::size_t Size() const
    {
        if (IsLoaded()) {
            return reader_->FileSize;
        } else {
            return 0;
        }
    }

    STDString ToString() const;

private:
    FileReader* reader_;
};

END_SE()