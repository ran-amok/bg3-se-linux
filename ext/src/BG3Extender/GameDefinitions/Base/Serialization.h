#pragma once

BEGIN_SE()

struct ObjectVisitor : public ProtectedGameObject<ObjectVisitor>
{
#if defined(BG3SE_LINUX)
    // PORT (linux): this mirror's virtuals are declared in the *game's* vtable order here,
    // which differs from upstream's MSVC order in three ways. All three were read off the
    // macOS _ZTVN2ls16LSFObjectVisitorE 0x1094e36f8, slot by slot, after a save taken with
    // esv::OsirisVariableHelper::SavegameVisit hooked crashed the game inside
    // SavegameSerializer::SavegameVisit's VisitUInt32 (core 209242,
    // `mov (%rax,%rcx,1),%r13` with rax = 0x430100001, which is upstream's uint32 landing on
    // the game's Visit(FixedString const&, Path&, Path const&) at slot 0x198).
    //
    //  1. Itanium spends TWO slots on a virtual destructor (D1 then D0) where MSVC has the
    //     single scalar-deleting destructor upstream models as Destroy(). The game's
    //     SavegameVisit calls EnterRegion at [vptr+0x70], EnterNode at [+0x88], ExitNode at
    //     [+0x98] and ExitRegion at [+0x80], which is upstream's order shifted by exactly one
    //     slot, and the oracle's first two entries are LSFObjectVisitorD1Ev / D0Ev.
    //  2. MSVC emits overloads of one name in REVERSE declaration order; Itanium emits them
    //     in declaration order. Upstream's mirror is written to match the MSVC reversal, so
    //     everything from VisitRaw down to VisitInt8 is back to front here. The oracle's
    //     block runs int8, uint8, bool, int16, uint16, uint32, int32, double, float, int64,
    //     uint64, ... , Path, FixedString, STDString, TranslatedString, Guid,
    //     TranslatedFSString, VisitRaw -- upstream's list read bottom-up.
    //  3. The oracle has one virtual the *Linux* build does not, and it is a macOS-only slot:
    //     `Visit(FixedString const&, unsigned long&, unsigned long)`, macOS vtable +0x108,
    //     between the uint64 one and Vector2i (__ZN2ls13ObjectVisitor5VisitERKNS_11FixedStringERmm,
    //     ICF-folded to 0x101fd5ff0 there). On Apple's LP64 `uint64_t` is `unsigned long long`,
    //     so `unsigned long` is a distinct type and a distinct virtual; on Linux `uint64_t` IS
    //     `unsigned long`, the two declarations are one function, and the game's vtable has a
    //     single slot at 0x100. So this mirror must NOT carry it: the offsets below are the
    //     oracle's minus 8 from VisitVector2i down.
    //
    // Witnesses for that -8, all on build c8da62b1 (the savegame visitor's own vtable is the
    // run at 0x7af0fa8, 60 function-pointer slots, tools/vtables.py --dump 0x7af1160 -- exactly
    // as many as this list declares):
    //   - ls::ObjectVisitor::Visit(FixedString const&, Transform&, Transform const&) is
    //     Linux 0x178 (match.py s5:1.00 to macOS 0x1074969c0) and macOS +0x180.
    //   - every scalar Visit forwards to the raw one at [vptr+0x1c0] here (e.g. the int8
    //     overload 0x7724400: ecx=1, r8d=0x1b) and at [vptr+0x1c8] on macOS.
    //   - the (size, EDataType) pair each scalar overload passes aligns macOS to Linux 1:1
    //     with the break exactly at Vector2i (8, 0x8): Linux 0x108 against macOS 0x110,
    //     everything up to and including uint64 (8, 0x18) at the same offset in both.
    //   - the game's Guid visit is Linux 0x1b0 = 0x2393970, which interns 16 bytes and writes
    //     the LSF descriptor 0x41f = type 31 (Guid) | length 16 << 6; macOS's is +0x1b8 with
    //     the same (16, 0x1f) pair.
    //
    // Names and signatures are upstream's, so every call site is unchanged; only the order is
    // the game's. Anything added to the non-Linux list below has to be placed here too.
    virtual void Destroy() = 0;                                              // 0x00 (D1)
    virtual void DestroyDeleting() = 0;                                      // 0x08 (D0)
    virtual void SetFileName(Path*) = 0;                                     // 0x10
    virtual void SetFileVersion(uint32_t*) = 0;                              // 0x18
    virtual void SetCodeVersion(uint32_t*) = 0;                              // 0x20
    virtual Path* GetFileName() = 0;                                         // 0x28
    virtual uint32_t GetCodeVersion() = 0;                                   // 0x30
    virtual uint32_t GetFileVersion() = 0;                                   // 0x38
    virtual void SetIO() = 0;                                                // 0x40
    virtual bool IsReading() = 0;                                            // 0x48
    virtual void Save(Path const&) = 0;                                      // 0x50
    virtual void Load(Path const&, FileReader&) = 0;                         // 0x58
    virtual void LoadFromPath(Path const&) = 0;                              // 0x60
    virtual bool TryEnterRegion(FixedString const&) = 0;                     // 0x68
    virtual bool EnterRegion(FixedString const&) = 0;                        // 0x70
    virtual void HasRegion(FixedString const&) = 0;                          // 0x78
    virtual void ExitRegion(FixedString const&) = 0;                         // 0x80
    virtual bool EnterNode(FixedString const&, FixedString const&) = 0;      // 0x88
    virtual void HasNode(FixedString const&) = 0;                            // 0x90
    virtual void ExitNode(FixedString const&) = 0;                           // 0x98
    virtual void VisitCount(FixedString const&, uint32_t*) = 0;              // 0xa0
    virtual void FindDataType(FixedString const&) = 0;                       // 0xa8
    virtual void VisitInt8(FixedString const&, int8_t&, int8_t) = 0;         // 0xb0
    virtual void VisitUInt8(FixedString const&, uint8_t&, uint8_t) = 0;      // 0xb8
    virtual void VisitBool(FixedString const&, bool&, bool) = 0;             // 0xc0
    virtual void VisitInt16(FixedString const&, int16_t&, int16_t) = 0;      // 0xc8
    virtual void VisitUInt16(FixedString const&, uint16_t&, uint16_t) = 0;   // 0xd0
    virtual void VisitUInt32(FixedString const&, uint32_t&, uint32_t) = 0;   // 0xd8
    virtual void VisitInt32(FixedString const&, int32_t&, int32_t) = 0;      // 0xe0
    virtual void VisitDouble(FixedString const&, double&, double) = 0;       // 0xe8
    virtual void VisitFloat(FixedString const&, float&, float) = 0;          // 0xf0
    virtual void VisitInt64(FixedString const&, int64_t&, int64_t) = 0;      // 0xf8
    virtual void VisitUInt64(FixedString const&, uint64_t&, uint64_t) = 0;   // 0x100
    virtual void VisitVector2i(FixedString const&, glm::ivec2&, glm::ivec2 const&) = 0;   // 0x108
    virtual void VisitVector3i(FixedString const&, glm::ivec3&, glm::ivec3 const&) = 0;   // 0x110
    virtual void VisitVector2(FixedString const&, glm::vec2&, glm::vec2 const&) = 0;      // 0x118
    virtual void VisitVector3(FixedString const&, glm::vec3&, glm::vec3 const&) = 0;      // 0x120
    virtual void VisitVector4(FixedString const&, glm::vec4&, glm::vec4 const&) = 0;      // 0x128
    virtual void VisitVector4Unaligned(FixedString const&, glm::vec4&, glm::vec4 const&) = 0;  // 0x130
    virtual void VisitMatrix2x2(FixedString const&, glm::mat2&, glm::mat2 const&) = 0;    // 0x138
    virtual void VisitMatrix3x3(FixedString const&, glm::mat3&, glm::mat3 const&) = 0;    // 0x140
    virtual void VisitMatrix4x4(FixedString const&, glm::mat4&, glm::mat4 const&) = 0;    // 0x148
    virtual void VisitMatrix3x4(FixedString const&, glm::mat3x4&, glm::mat3x4 const&) = 0;  // 0x150
    virtual void VisitMatrix4x3(FixedString const&, glm::mat4x3&, glm::mat4x3 const&) = 0;  // 0x158
    virtual void VisitQuaternion(FixedString const&, glm::quat&, glm::quat const&) = 0;   // 0x160
    virtual void VisitComponentHandle(FixedString const&, void*, void const*) = 0;        // 0x168
    virtual void VisitWeight() = 0;                                          // 0x170
    virtual void VisitTransform(FixedString const&, Transform&, Transform const&) = 0;    // 0x178
    virtual void VisitCharArray2(FixedString const&, char*) = 0;             // 0x180
    virtual void VisitCharArray(FixedString const&, char*, char const*) = 0; // 0x188
    virtual void VisitPath(FixedString const&, Path&, Path const&) = 0;      // 0x190
    virtual void VisitFixedString(FixedString const&, FixedString&, FixedString const&) = 0;  // 0x198
    virtual void VisitSTDString(FixedString const&, STDString&, STDString const&) = 0;        // 0x1a0
    virtual void VisitTranslatedString(FixedString const&, TranslatedString&, TranslatedString const&) = 0;  // 0x1a8
    virtual void VisitGuid(FixedString const&, Guid&, Guid const&) = 0;      // 0x1b0
    virtual void VisitTransatedFSString() = 0;                               // 0x1b8
    virtual void VisitRaw(FixedString const&, void*, uint64_t, uint32_t) = 0;  // 0x1c0
    virtual void VisitBuffer(FixedString const&, ScratchBuffer&) = 0;        // 0x1c8
    virtual void VisitBufferSize(FixedString const&, uint32_t&) = 0;         // 0x1d0
    virtual void ResetRegions() = 0;                                         // 0x1d8
#else
    virtual void Destroy() = 0;
    virtual void SetFileName(Path*) = 0;
    virtual void SetFileVersion(uint32_t*) = 0;
    virtual void SetCodeVersion(uint32_t*) = 0;
    virtual Path* GetFileName() = 0;
    virtual uint32_t GetCodeVersion() = 0;
    virtual uint32_t GetFileVersion() = 0;
    virtual void SetIO() = 0;
    virtual bool IsReading() = 0;
    virtual void Save(Path const&) = 0;
    virtual void Load(Path const&, FileReader&) = 0;
    virtual void LoadFromPath(Path const&) = 0;
    virtual bool TryEnterRegion(FixedString const&) = 0;
    virtual bool EnterRegion(FixedString const&) = 0;
    virtual void HasRegion(FixedString const&) = 0;
    virtual void ExitRegion(FixedString const&) = 0;
    virtual bool EnterNode(FixedString const&, FixedString const&) = 0;
    virtual void HasNode(FixedString const&) = 0;
    virtual void ExitNode(FixedString const&) = 0;
    virtual void VisitCount(FixedString const&, uint32_t*) = 0;
    virtual void FindDataType(FixedString const&) = 0;
    virtual void VisitRaw(FixedString const&, void*, uint64_t, uint32_t) = 0;
    virtual void VisitTransatedFSString() = 0;
    virtual void VisitGuid(FixedString const&, Guid&, Guid const&) = 0;
    virtual void VisitTranslatedString(FixedString const&, TranslatedString&, TranslatedString const&) = 0;
    virtual void VisitSTDString(FixedString const&, STDString&, STDString const&) = 0;
    virtual void VisitFixedString(FixedString const&, FixedString&, FixedString const&) = 0;
    virtual void VisitPath(FixedString const&, Path&, Path const&) = 0;
    virtual void VisitCharArray(FixedString const&, char*, char const*) = 0;
    virtual void VisitCharArray2(FixedString const&, char*) = 0;
    virtual void VisitTransform(FixedString const&, Transform&, Transform const&) = 0;
    virtual void VisitWeight() = 0;
    virtual void VisitComponentHandle(FixedString const&, void*, void const*) = 0;
    virtual void VisitQuaternion(FixedString const&, glm::quat&, glm::quat const&) = 0;
    virtual void VisitMatrix4x3(FixedString const&, glm::mat4x3&, glm::mat4x3 const&) = 0;
    virtual void VisitMatrix3x4(FixedString const&, glm::mat3x4&, glm::mat3x4 const&) = 0;
    virtual void VisitMatrix4x4(FixedString const&, glm::mat4&, glm::mat4 const&) = 0;
    virtual void VisitMatrix3x3(FixedString const&, glm::mat3&, glm::mat3 const&) = 0;
    virtual void VisitMatrix2x2(FixedString const&, glm::mat2&, glm::mat2 const&) = 0;
    virtual void VisitVector4Unaligned(FixedString const&, glm::vec4&, glm::vec4 const&) = 0;
    virtual void VisitVector4(FixedString const&, glm::vec4&, glm::vec4 const&) = 0;
    virtual void VisitVector3(FixedString const&, glm::vec3&, glm::vec3 const&) = 0;
    virtual void VisitVector2(FixedString const&, glm::vec2&, glm::vec2 const&) = 0;
    virtual void VisitVector3i(FixedString const&, glm::ivec3&, glm::ivec3 const&) = 0;
    virtual void VisitVector2i(FixedString const&, glm::ivec2&, glm::ivec2 const&) = 0;
    virtual void VisitUInt64(FixedString const&, uint64_t&, uint64_t) = 0;
    virtual void VisitInt64(FixedString const&, int64_t&, int64_t) = 0;
    virtual void VisitFloat(FixedString const&, float&, float) = 0;
    virtual void VisitDouble(FixedString const&, double&, double) = 0;
    virtual void VisitInt32(FixedString const&, int32_t&, int32_t) = 0;
    virtual void VisitUInt32(FixedString const&, uint32_t&, uint32_t) = 0;
    virtual void VisitUInt16(FixedString const&, uint16_t&, uint16_t) = 0;
    virtual void VisitInt16(FixedString const&, int16_t&, int16_t) = 0;
    virtual void VisitBool(FixedString const&, bool&, bool) = 0;
    virtual void VisitUInt8(FixedString const&, uint8_t&, uint8_t) = 0;
    virtual void VisitInt8(FixedString const&, int8_t&, int8_t) = 0;
    virtual void VisitBuffer(FixedString const&, ScratchBuffer&) = 0;
    virtual void VisitBufferSize(FixedString const&, uint32_t&) = 0;
    virtual void ResetRegions() = 0;
#endif

    int field_8;
    STDString field_10;
    Version FileVersion;
    Version CodeVersion;
    Array<ScratchBuffer> Array_ScratchBuffer;
};

END_SE()
