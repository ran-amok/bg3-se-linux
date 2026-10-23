#pragma once
#include <list> // PATCH (clang): MSVC reaches <list> transitively
#include <cstddef> // PORT (linux): offsetof in the layout asserts

#include <cstdint>
#include <array>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <cassert>
#include <glm/vec3.hpp>
#include <GameDefinitions/Base/Base.h>

namespace bg3se
{

// Osiris strings still use 64-bit size fields, unlike game strings that are 32-bit
using OsiString = std::basic_string<char, std::char_traits<char>, GameStdAllocator<char>>;
template <class T> using OsiVector = std::vector<T, GameStdAllocator<T>>;
template <class T> using OsiList = std::list<T, GameStdAllocator<T>>;

enum class ValueType : uint16_t
{
    None = 0,
    Integer = 1,
    Integer64 = 2,
    Real = 3,
    String = 4,
    GuidString = 5, // NOTE - this is now an alias of String!
    Undefined = 0x7f
};

struct OsiArgumentValue
{
    union {
        char const * String;
        int32_t Int32;
        int64_t Int64;
        float Float;
    };
    ValueType TypeId;
    bool Unknown;

    inline OsiArgumentValue()
        : TypeId(ValueType::None), Int64(0), Unknown(false)
    {}

    inline OsiArgumentValue(ValueType type, char const * str)
        : TypeId(type), String(const_cast<char *>(str)), Unknown(false)
    {}

    inline OsiArgumentValue(float flt)
        : TypeId(ValueType::Real), Float(flt), Unknown(false)
    {}

    inline OsiArgumentValue(int32_t i32)
        : TypeId(ValueType::Integer), Int32(i32), Unknown(false)
    {}

    inline OsiArgumentValue(int64_t i64)
        : TypeId(ValueType::Integer64), Int64(i64), Unknown(false)
    {}

    inline OsiArgumentValue(OsiArgumentValue const & v)
    {
        *this = v;
    }

    inline OsiArgumentValue & operator = (OsiArgumentValue const & v)
    {
        TypeId = v.TypeId;
        switch (v.TypeId) {
        case ValueType::None:
            break;

        case ValueType::Integer:
            Int32 = v.Int32;
            break;

        case ValueType::Integer64:
            Int64 = v.Int64;
            break;

        case ValueType::Real:
            Float = v.Float;
            break;

        case ValueType::String:
        case ValueType::GuidString:
        default:
            String = v.String;
            break;
        }

        return *this;
    }

    void Set(int32_t value);
    void Set(int64_t value);
    void Set(float value);
    void Set(char const * value);

    std::string ToString() const
    {
        switch (TypeId)
        {
        case ValueType::None:
            return "(Untyped)";

        case ValueType::Integer:
            return std::to_string(Int32);

        case ValueType::Integer64:
            return std::to_string(Int64);

        case ValueType::Real:
            return std::to_string(Float);

        case ValueType::String:
        case ValueType::GuidString:
            return String ? String : "";

        default:
            return "(Unknown)";
        }
    }
};

struct OsiArgumentDesc
{
    OsiArgumentDesc* NextParam{ nullptr };
    OsiArgumentValue Value;

    OsiArgumentDesc()
        : NextParam(nullptr)
    {}

    ~OsiArgumentDesc()
    {
        if (NextParam != nullptr) {
            delete NextParam;
        }
    }

    static OsiArgumentDesc * Create(OsiArgumentValue const & v)
    {
        auto desc = new OsiArgumentDesc();
        desc->Value = v;
        return desc;
    }

    inline void Add(OsiArgumentValue const & v)
    {
        if (NextParam == nullptr)
        {
            NextParam = OsiArgumentDesc::Create(v);
        }
        else
        {
            NextParam->Add(v);
        }
    }

    inline uint32_t Count() const
    {
        uint32_t num = 0;
        auto next = this;
        while (next != nullptr) {
            num++;
            next = next->NextParam;
        }

        return num;
    }

    inline OsiArgumentValue const & Get(uint32_t index) const
    {
        auto next = this;
        while (index--) {
            next = next->NextParam;
        }

        return next->Value;
    }

    inline OsiArgumentValue & Get(uint32_t index)
    {
        auto next = this;
        while (index--) {
            next = next->NextParam;
        }

        return next->Value;
    }

    inline OsiArgumentValue const & operator [] (uint32_t index) const
    {
        auto next = this;
        while (index--) {
            next = next->NextParam;
        }

        return next->Value;
    }

    inline OsiArgumentValue & operator [] (uint32_t index)
    {
        auto next = this;
        while (index--) {
            next = next->NextParam;
        }

        return next->Value;
    }

    inline glm::vec3 GetVector(uint32_t index) const
    {
        auto next = this;
        while (index--) {
            next = next->NextParam;
        }

        glm::vec3 vec; 
        vec.x = next->Value.Float;
        vec.y = next->NextParam->Value.Float;
        vec.z = next->NextParam->NextParam->Value.Float;
        return vec;
    }

    inline void SetVector(uint32_t index, glm::vec3 const & vec)
    {
        auto next = this;
        while (index--) {
            next = next->NextParam;
        }

        next->Value.Float = vec.x;
        next->NextParam->Value.Float = vec.y;
        next->NextParam->NextParam->Value.Float = vec.z;
    }
};

enum class EoCFunctionArgumentType : uint32_t
{
    InParam = 1,
    OutParam = 2
};

struct EoCFunctionArgument
{
    char const * Name;
    ValueType Type;
    EoCFunctionArgumentType ArgType;
};

struct EoCCallParam
{
    EoCFunctionArgument * Argument;
    union {
        char * String;
        int32_t Int;
        float Float;
    } Value;
};

struct DivFunctions
{
    typedef bool (* CallProc)(uint32_t FunctionId, OsiArgumentDesc * Params);
    typedef void (* ErrorMessageProc)(char const * Message);
    typedef void (* AssertProc)(bool Successful, char const * Message, bool Unknown2);

    void * Unknown0;
    CallProc Call;
    CallProc Query;
    ErrorMessageProc ErrorMessage;
    AssertProc Assert;
};

struct BufferPool : public ProtectedGameObject<BufferPool>
{
    void * VMT;
    uint8_t Unknown;
    uint32_t MaxSize;
    uint32_t GrowCount;
    uint32_t Unknown3;
    uint32_t Capacity;
    void * PoolMemoryVMT;
    void * PoolMemory;
    uint32_t CurrentSize;
    uint32_t SomeCounter;
    uint32_t Unknown4;
    uint32_t CurrentPos;
    uint32_t Unknown5;
    uint32_t Unknown6;
    char const * Name;
};

template <typename T>
struct BaseArray
{
    void * VMT;
    T * Buffer;
    uint32_t Capacity;
    uint32_t Size;
    uint32_t Free;


};

enum class FunctionArgumentDirection : uint32_t
{
    In = 1,
    Out = 2
};

struct FunctionArgument
{
    char const * Name;
    ValueType Type;
    FunctionArgumentDirection Direction;
};

struct OsirisFunctionHandle
{
    uint32_t Handle;

    OsirisFunctionHandle() : Handle(0) {}
    OsirisFunctionHandle(uint32_t InHandle) : Handle(InHandle) {}
    OsirisFunctionHandle(uint32_t type, uint32_t Part2, uint32_t functionId, uint32_t Part4)
    {
        Handle = (type & 7) | (Part4 << 31);
        if (type < 4) {
            Handle |= (functionId & 0x1FFFFFF) << 3;
        } else {
            Handle |= ((functionId & 0x1FFFF) << 3) | ((Part2 & 0xFF) << 20);
        }
    }

    inline uint8_t GetTypeId() const
    {
        return Handle & 7;
    }

    inline uint32_t GetPart2() const
    {
        if (GetTypeId() < 4) {
            return (Handle >> 20) & 0xFF;
        } else {
            return 0;
        }
    }

    inline uint32_t GetFunctionId() const
    {
        if (GetTypeId() < 4) {
            return (Handle >> 3) & 0x1FFFFFF;
        } else {
            return (Handle >> 3) & 0x1FFFF;
        }
    }

    inline uint32_t GetPart4() const
    {
        return Handle >> 31;
    }
};

struct OsirisTypeInfo
{
    uint16_t TypeId;
    uint16_t Alias;
};

struct FixedStringMap
{
    uint32_t Capacity;
    uint32_t Unknown;
    void ** Buffer;
    uint32_t Size;
};

struct VariableItem
{
    void * ptr;
    uint64_t unused;
};

struct VariableItem2
{
    uint32_t a, b;
    void * strptr;
    uint64_t strpad, str1, str2;
};

struct VariableDb : public ProtectedGameObject<VariableDb>
{
    VariableItem vars[256];
    uint32_t NumVariables;
    uint8_t __Padding[4];
    uint16_t b;
    OsiVector<VariableItem2> Vars;
};

template <class T>
struct OsiSequentialArray
{
    uint32_t Size;
    OsiVector<T> Elements;
};

// PORT (linux): Osiris's lists are std::list, and this build's is libc++'s:
// a node is {Prev, Next, Item} (MSVC's, which upstream mirrored, is {Next, Prev, Item}) and
// the sentinel is embedded in the list object itself — {Prev, Next} then Size, 24 bytes —
// where MSVC's list holds a pointer to a heap sentinel. Head() is that embedded sentinel, so
// upstream's walks (`head = Head(); cur = head->Next; while (cur != head)`) read unchanged.
// Measured: the database's NumParams lands at +0x40 behind a 24-byte Facts, which is the
// byte CReteFact::Valid reads; the macOS port found the same list shape.
template <class T>
struct ListNode
{
    ListNode<T> * Prev{ nullptr }, * Next{ nullptr };
    T Item;

    ListNode() {}

    ListNode(T const & item)
        : Item(item)
    {}
};

template <class T>
struct List
{
    ListNode<T> * Prev{ nullptr };   // the sentinel's links: Prev = last, Next = first
    ListNode<T> * Next{ nullptr };
    uint64_t Size{ 0 };

    // The sentinel: this object's first two words, read as a node (Item is never touched)
    inline ListNode<T> * Head() { return reinterpret_cast<ListNode<T> *>(this); }
    inline ListNode<T> const * Head() const { return reinterpret_cast<ListNode<T> const *>(this); }

    void Init()
    {
        Prev = Next = Head();
        Size = 0;
    }

    // Upstream hands a pooled node in for the sentinel; libc++'s is embedded, so it is unused.
    void Init(ListNode<T> *)
    {
        Init();
    }

    void Insert(T const & value, ListNode<T> * item, ListNode<T> * prev)
    {
        item->Item = value;
        Insert(item, prev);
    }

    ListNode<T> * Insert(T const & value, ListNode<T> * prev)
    {
        auto item = new ListNode<T>(value);
        Insert(item, prev);
        return item;
    }

    void Insert(ListNode<T> * item, ListNode<T> * prev)
    {
        item->Prev = prev;
        item->Next = prev->Next;
        prev->Next->Prev = item;
        prev->Next = item;
        Size++;
    }

    ListNode<T> * Insert(ListNode<T> * prev)
    {
        auto item = new ListNode<T>();
        Insert(item, prev);
        return item;
    }
};

static_assert(sizeof(List<void*>) == 24, "libc++ std::list: embedded sentinel + size");
static_assert(offsetof(ListNode<void*>, Item) == 16, "libc++ list node: prev, next, value");

// PORT (linux): the maps inside Osiris's databases are libc++ std::map here, not MSVC's
// _Tree. A node is libc++'s __tree_node: {Left, Right, Parent, IsBlack}
// then the pair; the map object is {__begin_node_, root (the end node's left), size}, 24
// bytes. Leaves are null pointers, not a sentinel with an IsRoot flag, so the lookup is a
// plain lower-bound walk. Read-only: nothing here allocates or rebalances. Measured: the
// name index of the function manager is walked by COsiris::GetFunctionMappings at a stride
// of 0x18 with the size word at slot+0x10; the macOS port read key at node+0x20 and value at
// node+0x38 for a 24-byte string key, which is this layout.
template <typename TKey, typename TVal>
struct TMapNode
{
    struct KeyValuePair
    {
        TKey Key;
        TVal Value;
    };

    TMapNode<TKey, TVal> * Left;
    TMapNode<TKey, TVal> * Right;
    TMapNode<TKey, TVal> * Parent;
    bool IsBlack;
    KeyValuePair KV;
};

template <typename TKey, typename TVal, class Pred = std::less<TKey>>
struct TMap
{
    TMapNode<TKey, TVal> * Begin;    // leftmost node (the end node when empty)
    TMapNode<TKey, TVal> * Root;     // the end node's left child
    uint64_t Size;

    TVal * Find(TKey const & key)
    {
        TMapNode<TKey, TVal> * result = nullptr;
        auto node = Root;
        while (node != nullptr)
        {
            if (Pred()(node->KV.Key, key)) {
                node = node->Right;
            } else {
                result = node;
                node = node->Left;
            }
        }

        if (result == nullptr || Pred()(key, result->KV.Key))
            return nullptr;
        else
            return &result->KV.Value;
    }

    template <class Visitor>
    void Iterate(Visitor visitor)
    {
        Iterate(Root, visitor);
    }

    template <class Visitor>
    void Iterate(TMapNode<TKey, TVal> * node, Visitor visitor)
    {
        if (node != nullptr) {
            visitor(node->KV.Key, node->KV.Value);
            Iterate(node->Left, visitor);
            Iterate(node->Right, visitor);
        }
    }
};

static_assert(sizeof(TMap<uint32_t, uint32_t>) == 24, "libc++ std::map: begin, root, size");
using TMapNodeProbe = TMapNode<uint32_t, void*>;
static_assert(offsetof(TMapNodeProbe, KV) == 32, "libc++ tree node header is 32 bytes");


struct TypeDbLess
{
    bool operator ()(OsiString const & a, OsiString const & b) const
    {
        return _stricmp(a.c_str(), b.c_str()) < 0;
    }
};

template <class TValue>
struct TypeDb : public ProtectedGameObject<TypeDb<TValue>>
{
    struct HashSlot
    {
        TMap<OsiString, TValue, TypeDbLess> NodeMap;   // PORT (linux): the whole 24-byte slot; MSVC's map was 8 + an unknown word
    };

    TValue * Find(uint32_t hash, OsiString const & key)
    {
        auto & bucket = Hash[hash % 0x3FF];
        return bucket.NodeMap.Find(key);
    }

    template <class Visitor>
    void Iterate(Visitor visitor)
    {
        for (uint32_t i = 0; i < 0x3FF; i++) {
            HashSlot & bucket = Hash[i];
            bucket.NodeMap.Iterate(visitor);
        }
    }

    HashSlot Hash[1023];
    uint32_t NumItems;
};

struct EnumDb
{
    float field_0;
    TMap<OsiString, void*, TypeDbLess> Tree;
    __int64 field_10;
    Vector<void*> Enums;
    __int64 field_30;
    __int64 field_38;
};

struct SomeDbItem
{
    uint64_t Unknown;
    SomeDbItem * Next;
};

enum class OsiStringHandle : uint64_t {};

union ValueUnion
{
    int64_t Int64;
    float Float;
    int32_t Int32;
    OsiStringHandle StringHandle;
};

struct RawValue
{
    ValueUnion Val;
    uint32_t Unknown{ 0 };
    OsiString Str;
};

enum class ValueFlags : uint8_t
{
    NoneValue = 0,
    SimpleValue = 0x1,
    TypedValue = 0x2,
    Variable = 0x3,

    IsValid = 0x8,
    OutParam = 0x10,
    IsAType = 0x20,
    Unused = 0x40,
    Adapted = 0x80
};

class TypedValue
{
public:
    inline TypedValue()
        : Value{ .Int64 = 0 }
    {}

    ~TypedValue();
    
    TypedValue(TypedValue &);
    TypedValue(TypedValue &&) noexcept;

    TypedValue& operator = (TypedValue const&);
    TypedValue& operator = (TypedValue &&) noexcept;

    char const* GetString() const;
    void ClearValue();
    void SetValue(ValueType typeId, int32_t v);
    void SetValue(ValueType typeId, float v);
    void SetValue(ValueType typeId, int64_t v);
    void SetValue(ValueType typeId, OsiStringHandle s);
    void SetValue(ValueType typeId, char const* s);

    inline bool HasValue() const
    {
        return (Flags & ValueFlags::IsValid) == ValueFlags::IsValid;
    }

    ValueUnion Value;
    ValueType TypeId{ ValueType::None };
    int8_t Index{ -1 };
    ValueFlags Flags{ ValueFlags::TypedValue };

private:
    void ReleaseValue();
};

class TupleVec
{
public:
    TypedValue * Values{ nullptr };
    uint64_t Size{ 0 };
    // PORT (linux): Osiris's CTuple carries a third word here, zeroed by CTuple::Copy 0x7c250
    // and by the stack tuple CReteStartNode::Add builds; the list node CReteDBase::_insert
    // allocates is 0x28 = {Prev, Next} + this. Not read; here for the size.
    uint32_t Unknown{ 0 };
};

static_assert(sizeof(TupleVec) == 0x18, "CTuple: Values, Size, a u32");
static_assert(sizeof(ListNode<TupleVec>) == 0x28, "the fact list node CReteDBase::_insert allocates");

struct TuplePtrLL
{
    void* VMT{ nullptr };
    List<TypedValue*> Items;
};

class SmallTuple
{
public:
    static constexpr uint32_t InlineSize = 8;

    SmallTuple();
    ~SmallTuple();

    SmallTuple(SmallTuple const&) = delete;
    SmallTuple& operator =(SmallTuple const&) = delete;

    void Reserve(uint32_t size);
    TypedValue& Add();

    inline uint32_t Size() const
    {
        return size_;
    }

    inline TypedValue* Buf()
    {
        if (capacity_ <= InlineSize) {
            return inline_;
        } else {
            return values_;
        }
    }

    inline TypedValue const* Buf() const
    {
        if (capacity_ <= InlineSize) {
            return inline_;
        } else {
            return values_;
        }
    }

    TypedValue& operator [] (uint32_t index)
    {
        se_assert(index < size_);
        return Buf()[index];
    }

    TypedValue const& operator [] (uint32_t index) const
    {
        se_assert(index < size_);
        return Buf()[index];
    }

private:
    union
    {
        TypedValue inline_[InlineSize];
        TypedValue* values_;
    };
    uint32_t size_{ 0 };
    uint32_t capacity_{ 8 };

};

struct Database : public ProtectedGameObject<Database>
{
    uint32_t DatabaseId;
    void * FactsVMT;
    List<TupleVec> Facts;
    Vector<uint32_t> ParamTypes;
    uint8_t NumParams;
};

class RuleActionArguments
{
public:
    void * VMT;
    List<TypedValue *> Args;
};

class RuleActionNode
{
public:
    const char * FunctionName;
    RuleActionArguments * Arguments;
    bool Not;
    int32_t GoalIdOrDebugHook;
};

class RuleActionList
{
public:
    virtual ~RuleActionList() = 0;
    List<RuleActionNode *> Actions;
};

struct Goal : public ProtectedGameObject<Goal>
{
    RuleActionList * InitCalls;
    RuleActionList * ExitCalls;
    uint32_t Id;
    char const * Name;
    uint32_t SubGoalCombination;
    void* ParentGoalsVMT;
    OsiList<uint32_t> ParentGoals;
    void* SubGoalsVMT;
    OsiList<uint32_t> SubGoals;
    uint8_t Flags;
};

template <class T>
struct TypedDb
{
    OsiSequentialArray<T *> Db;
};

struct GoalDb : public TypeDb<Goal*>
{
    Vector<Goal*> Goals;
};

class Node;

struct Adapter : public ProtectedGameObject<Adapter>
{
    uint32_t Id;
    TMap<uint8_t, uint8_t> VarToColumnMaps;   // PORT (linux): 24 bytes; its Size is what upstream called VarToColumnMapCount
    Vector<int8_t> ColumnToVarMaps;
    SmallTuple Constants;
};
// CReteAdaptor::CReteAdaptor (libOsiris.so 0x9de60): 0xc0 bytes, map at +8, vector at +0x20,
// Constants at +0x38 with capacity 8 written at +0xbc
static_assert(sizeof(Adapter) == 0xc0, "CReteAdaptor");
static_assert(offsetof(Adapter, ColumnToVarMaps) == 0x20 && offsetof(Adapter, Constants) == 0x38, "CReteAdaptor fields");

using DatabaseDb = TypedDb<Database>;
using AdapterDb = TypedDb<Adapter>;
using NodeDb = TypedDb<Node>;

struct OsirisRef
{
    uint32_t Id{ 0 };
};

struct DatabaseRef : OsirisRef
{
    Database* Get() const;
};

struct AdapterRef : OsirisRef
{
    Adapter* Get() const;
};

struct NodeRef : OsirisRef
{
    Node* Get() const;
};

enum class EntryPoint : uint32_t
{
    None = 0,
    Left = 1,
    Right = 2
};

class NodeEntryRef
{
public:
    NodeRef Node;
    EntryPoint EntryPoint;
    uint32_t GoalId;
};

struct FunctionParamDesc
{
    uint16_t Type;
    uint32_t Unknown;
};

struct FunctionParamList
{
    void * VMT;
    List<FunctionParamDesc> Params;
};

inline int Popcnt8(uint8_t b)
{
    b = b - ((b >> 1) & 0x55);
    b = (b & 0x33) + ((b >> 2) & 0x33);
    return (((b + (b >> 4)) & 0x0F) * 0x01);
}

struct FuncSigOutParamList
{
    uint8_t * Params;
    // Number of param bytes
    uint32_t Count;

    inline uint32_t numOutParams() const
    {
        uint32_t numParams = 0;
        for (uint32_t i = 0; i < Count; i++) {
            numParams += Popcnt8(Params[i]);
        }

        return numParams;
    }

    inline bool isOutParam(unsigned i) const
    {
        se_assert(i < Count*8);
        return ((Params[i >> 3] << (i & 7)) & 0x80) == 0x80;
    }
};

struct FunctionSignature
{
    void * VMT;
    const char * Name;
    FunctionParamList * Params;
    FuncSigOutParamList OutParamList;
    uint32_t Unknown;
};

enum class FunctionType : uint32_t
{
    Unknown = 0,
    Event = 1,
    Query = 2,
    Call = 3,
    Database = 4,
    Proc = 5,
    SysQuery = 6,
    SysCall = 7,
    UserQuery = 8
};

struct OsiFunctionDef : public ProtectedGameObject<OsiFunctionDef>
{
    void * VMT;
    uint32_t Line;
    uint32_t Unknown1;
    uint32_t Unknown2;
    FunctionSignature * Signature;
    NodeRef Node;
    FunctionType Type;
    uint32_t Key[4];
    uint32_t OsiFunctionId;

    inline uint32_t GetHandle() const
    {
        return OsirisFunctionHandle(Key[0], Key[1], Key[2], Key[3]).Handle;
    }
};

// Osiris -> EoCApp function mapping info
struct MappingInfo
{
    const char * Name;
    uint32_t Id;
    uint32_t NumParams;
    uint8_t * ParamTypes;
};

struct NodeVMT
{
    using DestroyProc = void (*)(Node * self, bool free);
    using GetDatabaseRefProc = DatabaseRef * (*)(Node * self, DatabaseRef * ref);
    using IsDataNodeProc = bool (*)(Node * self);
    using IsValidProc = bool(*)(Node * self, SmallTuple* Values, uint32_t Adapter);
    using IsProcProc = bool(*)(Node * self);
    using IsPartOfAProcProc = bool(*)(Node * self);
    using GetParentProc = NodeRef * (*)(Node * self, NodeRef * ref);
    using SetNextNodeProc = void (*)(Node * self, NodeEntryRef * ref);
    using GetAdapterProc = Adapter * (*)(Node * self, EntryPoint which);
    using InsertTupleProc = void (*)(Node * self, TuplePtrLL * tuple);
    using PushDownTupleProc = void(*)(Node * self, SmallTuple* tuple, uint32_t adapter, EntryPoint entryPoint);
    using TriggerInsertEventProc = void (*)(Node * self, TupleVec * tuple);
    using GetLowDatabaseRefProc = NodeRef * (*)(Node * self, NodeRef * ref);
    using GetLowDatabaseProc = NodeEntryRef * (*)(Node * self, NodeEntryRef * ref);
    using GetLowDatabaseIndirectionProc = int (*)(Node * self);
    using SaveProc = bool (*)(Node * self, void * buf);
    using DebugDumpProc = char * (*)(Node * self, char * buf);
    using SetLineNumberProc = void (*)(Node * self, unsigned int line);
    using CallQueryProc = bool (*)(Node * self, OsiArgumentDesc * args);
    using GetQueryNameProc = const char * (*)(Node * self);

    DestroyProc Destroy; // 0   PORT (linux): the Itanium ABI has two destructor slots (complete,
    DestroyProc DestroyDeleting; // 8   deleting); every slot below is one further on than upstream's comment
    GetDatabaseRefProc GetDatabaseRef; // 8
    GetDatabaseRefProc GetDatabaseRef2; // 10
    IsDataNodeProc IsDataNode; // 18
    IsValidProc IsValid; // 20
    IsProcProc IsProc; // 28
    IsPartOfAProcProc IsPartOfAProc; // 30
    GetParentProc GetParent; // 38
    SetNextNodeProc SetNextNode; // 40
    GetAdapterProc GetAdapter; // 48
    // PORT (linux): the four tuple slots are in a different order than upstream's Windows
    // table. Read off every node vtable in libOsiris.so, slot for
    // slot against the macOS names: 11 = Add(COsiVarIDValuePairList*, PCReteNode,
    // TReteEntryPoint) (upstream's PushDownTuple), 12 = Del(same), 13 = Add(COsipParameterList*)
    // (InsertTuple: COsiris::Event and the story's Fire() call this slot), 14 = Del(same).
    // With upstream's order, Osi PROC/DB/event inserts hit slot 11, whose CReteStartNode
    // implementation is a bare `ret` -- which is why such inserts "returned ok and did nothing".
    PushDownTupleProc PushDownTuple; // 58
    PushDownTupleProc PushDownTupleDelete; // 60
    InsertTupleProc InsertTuple; // 68
    InsertTupleProc DeleteTuple; // 70
    TriggerInsertEventProc TriggerInsertEvent; // 78  (ForwardAddToken(CTuple const&) here)
    TriggerInsertEventProc TriggerDeleteEvent; // 80
    GetLowDatabaseRefProc GetLowDatabaseRef; // 88
    GetLowDatabaseProc GetLowDatabase; // 90
    GetLowDatabaseIndirectionProc GetLowDatabaseFlags; // 98
    SaveProc Save; // A0
    DebugDumpProc DebugDump; // A8
    DebugDumpProc DebugDump2; // B0
    SetLineNumberProc SetLineNumber; // B8
    union { // C0
        void * RelUnused;
        CallQueryProc CallQuery;
    };
    GetQueryNameProc GetQueryName; // C8
};

// PORT (linux): the slot layout above, once more as numbers the code can check against
static_assert(offsetof(NodeVMT, IsValid) == 5 * 8, "Valid is slot 5 on Linux");
static_assert(offsetof(NodeVMT, InsertTuple) == 13 * 8, "Add(COsipParameterList*) is slot 13 on Linux");
static_assert(offsetof(NodeVMT, DeleteTuple) == 14 * 8, "Del(COsipParameterList*) is slot 14 on Linux");
static_assert(offsetof(NodeVMT, SetLineNumber) == 23 * 8, "SetLinenoOfThen is slot 23 on Linux");
static_assert(offsetof(NodeVMT, CallQuery) == 24 * 8, "_ExecuteQuery is slot 24 on Linux");

class Node : public Noncopyable<Node>
{
public:
    virtual ~Node() = 0;
    virtual DatabaseRef * GetDatabaseRef(DatabaseRef * Db) = 0;
    virtual DatabaseRef * GetDatabaseRef2(DatabaseRef * Db) = 0;
    virtual bool IsDataNode() = 0;
    virtual bool IsValid(SmallTuple* Tuple, uint32_t Adapter) = 0;
    virtual bool IsProc() = 0;
    virtual bool IsPartOfAProc() = 0;
    virtual NodeRef * GetParent(NodeRef * Node) = 0;
    virtual void SetNextNode(NodeEntryRef * Node) = 0;
    virtual Adapter * GetAdapter(EntryPoint Which) = 0;
    // PORT (linux): declared in this build's slot order (see NodeVMT above)
    virtual void PushDownTuple(SmallTuple* Tuple, uint32_t Adapter, EntryPoint Which) = 0;
    virtual void PushDownTupleDelete(SmallTuple* Tuple, uint32_t Adapter, EntryPoint Which) = 0;
    virtual void InsertTuple(TuplePtrLL* Tuple) = 0;
    virtual void DeleteTuple(TuplePtrLL* Tuple) = 0;
    virtual void TriggerInsertEvent(TupleVec * Tuple) = 0;
    virtual void TriggerInsertEvent2(TupleVec * Tuple) = 0;
    virtual NodeRef * GetLowDatabaseRef(NodeRef * Node) = 0;
    virtual NodeEntryRef * GetLowDatabase(NodeEntryRef * Node) = 0;
    virtual uint8_t GetLowDatabaseIndirection() = 0;
    virtual bool Save(void * SmartBuf) = 0;
    virtual char * DebugDump(char * Buffer) = 0;
    virtual char * DebugDump2(char * Buffer) = 0;
    virtual void SetLineNumber(int Line) = 0;
    virtual bool CallQuery(OsiArgumentDesc* args) = 0;
    virtual const char* GetQueryName() = 0;


    uint32_t Id;
    OsiFunctionDef* Function;
    DatabaseRef Database;
};

class TreeNode : public Node
{
public:
    NodeEntryRef Next;
};

class RelNode : public TreeNode
{
public:
    NodeRef Parent;
    AdapterRef Adapter;
    NodeRef RelDatabaseRef;
    NodeEntryRef RelDatabase;
    uint8_t RelDatabaseIndirection;
};

class RelOpNode : public RelNode
{
public:
    uint8_t LeftValueIndex;
    uint8_t RightValueIndex;
    TypedValue LeftValue;
    TypedValue RightValue;
    uint32_t RelOp;

    static inline void HookVMTs(NodeVMT * vmt) {}
};

class RuleNode : public RelNode
{
public:
    uint32_t Line;
    bool IsQuery;
    void * Variables;
    RuleActionList * Calls;

    static inline void HookVMTs(NodeVMT * vmt) {}
};

class JoinNode : public TreeNode
{
public:
    NodeRef Left;
    NodeRef Right;
    AdapterRef LeftAdapter;
    AdapterRef RightAdapter;
    uint64_t Unknown;
    NodeRef LeftDatabaseRef;
    NodeEntryRef LeftDatabase;
    uint8_t LeftDatabaseIndirection;
    NodeRef RightDatabaseRef;
    NodeEntryRef RightDatabase;
    uint8_t RightDatabaseIndirection;
};

class AndNode : public JoinNode
{
public:

    static inline void HookVMTs(NodeVMT * vmt) {}
};

class NotAndNode : public JoinNode
{
public:

    static inline void HookVMTs(NodeVMT * vmt) {}
};

// PORT (linux): the game's list node is libc++'s __list_node<CReteConnection>, whose base is
// {__prev_, __next_} -- the opposite order to MSVC's {_Next, _Prev} that upstream's Head/Prev
// names come from.  The node is 0x20 bytes and its value is the 12-byte CReteConnection at
// 0x10, which is upstream's NodeEntryRef member for member.
struct DataNodeRef
{
    DataNodeRef * Prev;     // __prev_
    DataNodeRef * Next;     // __next_
    NodeEntryRef Reference; // __value_
};

// PORT (linux): the game's type is COsiList<CReteConnection>, i.e.
// std::list<CReteConnection, osi::StlAllocator<CReteConnection>> with a virtual destructor of
// its own, so: a vtable pointer, libc++'s sentinel node base, and __size_.  Upstream's
// two-word {Head, Count} is MSVC's _List_val and is 16 bytes short of this build's 0x20.
struct DataNodeReferenceList
{
    void * VMT;
    DataNodeRef * Last;  // __end_.__prev_
    DataNodeRef * First; // __end_.__next_
    uint64_t Count;      // __size_
};

class DataNode : public Node
{
public:
    // PORT (linux): the game's CReteStartNode has no UsedBy of any kind -- its ctor writes
    // nothing at 0x1c.  What sits here is the embedded child list, at 0x20.
    DataNodeReferenceList Children;
};

// PORT (linux): CReteEvent adds nothing to CReteStartNode -- its ctor is a call to the base's
// plus a vtable store, and the factory allocates 0x40, the base's own size.
class ProcNode : public DataNode
{
};

// PORT (linux): the same for CReteFact.
class DatabaseNode : public DataNode
{
};

#if defined(BG3SE_LINUX)
// PORT (linux): the Rete node hierarchy against libOsiris.so (build-id of the 24532579 snapshot)
// and its macOS twin libOsiris.dylib, which is the same source built the same way and *is*
// symbolised -- the Linux library keeps only COsiris:: and COsiArgumentDesc:: names, so the
// oracle is what supplies the class names and the Linux library is what supplies the offsets.
// The name map, read off the dylib's ctor signatures:
//
//   upstream          game                     how the name was fixed
//   Node              CReteNode                __ZN9CReteNodeC2EP12COsiSmartBuf 0x76040
//   TreeNode          CReteChildNode           its ctor 0x79530 tail-calls CReteNode's
//   RelNode           CReteUnaryNode           ctor 0x79800 takes (PCReteNode, PCReteAdaptor)
//   JoinNode          CReteBinaryNode          ctor 0x79830 takes two of each
//   RelOpNode         CReteRelCondition        ctor 0x74b10 takes (..., COsiParameter*, TOsiRelOp, COsiParameter*)
//   RuleNode          CReteRuleActionPart      ctor 0x750a0 takes (..., CpOsiFunctionParseDataList*)
//
// The base:  CReteNode's ctor writes `mov dword [rdi+0x18], 0` at macOS 0x76072, so Database is
// the dword at 0x18 and the class's non-virtual size is 0x1c.
//
// TreeNode:  CReteChildNode's ctor zeroes exactly twelve bytes at 0x1c -- `mov qword [rbx+0x1c], 0`
// then `mov dword [rbx+0x24], 0` -- which is sizeof(NodeEntryRef) starting in the base's tail
// padding.  This one IS a witness where a bare zero-fill would not be: clang can widen a zero
// store down into dead padding, but 0x18..0x1c is the base's live Database, so the store cannot
// have started lower than the member.  The same pair is in libOsiris.so at 0xac31d/0xac325 and
// 0xacfbd/0xacfc5, each right after a vtable install that follows a call to the shared base ctor
// 0xac000 -- the Linux CReteChildNode ctor and its inlining into CReteUnaryNode's.
//
// RelNode:   CReteUnaryNode::CReteUnaryNode(PCReteNode, PCReteAdaptor) macOS 0x79800 is 40 bytes
// whole: it zeroes 0x10..0x28 (`movups [rdi+0x10], xmm0` + `mov qword [rdi+0x20], 0`), installs
// the vtable, and then stores its own two arguments -- `mov dword [rdi+0x28], esi` (the parent
// node) and `mov dword [rdi+0x2c], edx` (the adaptor).  Argument stores, so Parent and Adapter
// are named by the ctor's own signature; and because the first of them is at 0x28, the TreeNode
// subobject ends at 0x28, which is only true if Next sits at 0x1c.  Under MSVC Next would be at
// 0x20, TreeNode would be 0x30 and Parent 0x30.
//
// RelOpNode: CReteRelCondition's ctor is the same prologue inlined, then
// `movzx eax, byte [rcx+0xa]` / `mov byte [rdi+0x41], al` and `movzx eax, byte [r9+0xa]` /
// `mov byte [rdi+0x42], al` -- the two value indices taken from the left and right
// COsiParameter arguments, so both are argument-typed -- `lea r14, [rdi+0x48]` and
// `lea r15, [rdi+0x58]` for the two TypedValues, and `mov dword [rdi+0x68], r8d` for the
// TOsiRelOp argument.  libOsiris.so has the whole sequence at 0xa9d26..0xa9d4c, register for
// register, and COsiris::_ReadReteNodes -- a *named* Linux symbol -- writes the same 0x41 from
// the save stream at 0x8bc5e.
//
// RuleNode:  CReteRuleActionPart's ctor writes `mov dword [rdi+0x44], 0xffffffff` (a non-zero
// value, so not a zero-fill), `mov byte [rdi+0x48], 0`, and `mov qword [rdi+0x58], rcx` -- its
// own CpOsiFunctionParseDataList* argument, which is upstream's Calls.  On Linux the ctor is
// inlined into COsiris::_ReadReteNodes: the vtable install and `mov byte [r14+0x48], 0` at
// 0x8b6ae/0x8b6b1, `mov qword [r14+0x58], rax` at 0x8b6eb, and the deserialiser's four-byte
// read into `lea r15, [rax+0x44]` at 0x8c109.  MSVC would put Line at 0x48 and IsQuery at 0x4c.
//
// No sizeof is asserted: nothing seen allocates one of these by a constant (the factory hands
// out COsiris::s_Allocator blocks by a size read from the stream).  The sizes below are the
// non-virtual extents the offsets above pin down, not allocation witnesses.
static_assert(offsetof(Node, Database) == 0x18);
static_assert(sizeof(Node) == 0x20);

static_assert(offsetof(TreeNode, Next) == 0x1c);
static_assert(sizeof(TreeNode) == 0x28);

static_assert(offsetof(RelNode, Parent) == 0x28);
static_assert(offsetof(RelNode, Adapter) == 0x2c);
// PORT (linux): the three members past Adapter, from CReteUnaryNode::Write macOS 0x66e40, which
// serialises the whole class with the game's own typed overloads after calling CReteNode::Write
// 0x638e0: write(unsigned int) of [this+0x1c], write(TReteEntryPoint) of [+0x20] and write(int)
// of [+0x24] (that is TreeNode::Next, re-witnessed), then write(unsigned int) of [+0x28] (Parent)
// and [+0x2c] (Adapter), then [+0x30] (RelDatabaseRef), then the NodeEntryRef at 0x34 as its
// three fields -- unsigned int [+0x34], TReteEntryPoint [+0x38], int [+0x3c] -- and finally
// write(unsigned char) of [+0x40].  Nothing after that, so 0x41 is the class's non-virtual
// extent, which is what RelOpNode::LeftValueIndex at 0x41 already said.
static_assert(offsetof(RelNode, RelDatabaseRef) == 0x30);
static_assert(offsetof(RelNode, RelDatabase) == 0x34);
static_assert(offsetof(RelNode, RelDatabaseIndirection) == 0x40);
static_assert(sizeof(RelNode) == 0x48);

static_assert(offsetof(RelOpNode, LeftValueIndex) == 0x41);
static_assert(offsetof(RelOpNode, RightValueIndex) == 0x42);
static_assert(offsetof(RelOpNode, LeftValue) == 0x48);
static_assert(offsetof(RelOpNode, RightValue) == 0x58);
static_assert(offsetof(RelOpNode, RelOp) == 0x68);

static_assert(offsetof(RuleNode, Line) == 0x44);
static_assert(offsetof(RuleNode, IsQuery) == 0x48);
static_assert(offsetof(RuleNode, Calls) == 0x58);

// PORT (linux): JoinNode is the game's CReteBinaryNode, AndNode its CReteAnd and NotAndNode its
// CReteNAnd.  The class is measured three ways and the mirror already matches all three, so
// nothing changes here -- only the asserts are new.
//
// (1) The construction ctor, macOS __ZN15CReteBinaryNodeC2E10PCReteNode13PCReteAdaptorS0_S1_
// 0x79830, is 28 instructions whole and stores all four of its own arguments: `mov [rdi+0x28],
// esi` (arg 1, PCReteNode) at 0x7984d, `mov [rdi+0x2c], ecx` (arg 3, PCReteNode) at 0x79850,
// `mov [rdi+0x30], edx` (arg 2, PCReteAdaptor) at 0x79853 and `mov [rdi+0x34], r8d` (arg 4,
// PCReteAdaptor) at 0x79856.  Argument stores, so the mangled signature names Left/Right as the
// nodes and LeftAdapter/RightAdapter as the adaptors, in the mirror's declared order.  The rest
// of the object is zero-filled in two blocks with a deliberate hole: 0x38..0x50 (`movups
// [rdi+0x38]` + `mov qword [rdi+0x48], 0`) and 0x54..0x64 (`movups [rdi+0x54]`).  The hole at
// 0x50..0x54 and the tail from 0x64 are the two uint8 indirection members, which the ctor leaves
// uninitialised -- a *hole* in a zero-fill is a positive witness where the fill itself is not.
//
// (2) CReteBinaryNode::Write macOS 0x670d0 serialises every member with a typed overload and in
// the mirror's order: [+0x1c] unsigned int / [+0x20] TReteEntryPoint / [+0x24] int (TreeNode's
// Next again), then unsigned int of [+0x28] Left, [+0x2c] Right, [+0x30] LeftAdapter, [+0x34]
// RightAdapter, then [+0x40] LeftDatabaseRef, then CReteConnection::Write 0x67280 on
// `lea rdi, [r14+0x44]` (LeftDatabase, the 12-byte NodeEntryRef), then write(unsigned char) of
// [+0x50], then [+0x54] RightDatabaseRef, CReteConnection::Write on [+0x58], and a tail-jmp to
// write(unsigned char) with [+0x64].  0x38 is skipped: that is the 8 unserialised bytes the
// mirror calls Unknown, and it is what puts LeftDatabaseRef at 0x40.
//
// (3) sizeof 0x68 is an allocation witness AND an ABI witness.  macOS
// CReteNodeFactory::CreateReteAnd(PCReteNode, PCReteAdaptor, PCReteNode, PCReteAdaptor) 0x74040
// and CreateReteNAnd 0x749b0 both do `mov esi, 0x68` into COsiris::s_Allocator, inline the ctor
// above instruction for instruction, and then store one vtable each (0x9a2e0 / 0x9a3b0) -- so
// neither subclass adds a member.  libOsiris.so says the same without the oracle: the two arms
// of COsiris::_ReadReteNodes at 0x8b1fe and 0x8b8e4 each `mov esi, 0x68`, call the shared
// deserialising ctor 0xac2f0, and store vtable 0x10ada0 and 0x10ae70 respectively.  Those two
// vtables are named on the Linux binary alone: slot 20 (Save) is 0xa6560 for the first, which
// writes the stream tag `mov byte [rax], 4` at 0xa6593, and 0xa9b70 for the second, which writes
// tag 5 at 0xa9ba3 -- the tags CReteAnd::Write macOS 0x69e00 and CReteNAnd::Write 0x6d220 emit.
//
// The Linux deserialising ctor 0xac2f0 re-reads the whole layout member by member: the
// CReteChildNode part (`mov [rbx+0x1c], eax` 0xac3d9, `lea rsi, [rbx+0x20]` 0xac467,
// `lea rsi, [rbx+0x24]` 0xac50b), then its own vtable and the same two zero-fill blocks with the
// same hole (0xac545..0xac555), then 0x28 (via `lea r12, [rbx+0x28]` 0xac53e), 0x2c (0xac6bb),
// 0x30 (0xac773), 0x34 (0xac82b), 0x40 (0xac8e3), 0x44/0x48/0x4c (0xac99f, 0xaca2d, 0xacad1),
// `mov byte [rbx+0x50], al` 0xacb99, and 0x54 onwards (`lea r12, [rbx+0x54]` 0xacbad).
//
// Every offset here discriminates the ABI.  TreeNode is 0x30 under MSVC (Next at 0x20), so MSVC
// would put Left at 0x30, Right 0x34, LeftAdapter 0x38, RightAdapter 0x3c, Unknown 0x40,
// LeftDatabaseRef 0x48, LeftDatabase 0x4c, LeftDatabaseIndirection 0x58, RightDatabaseRef 0x5c,
// RightDatabase 0x60, RightDatabaseIndirection 0x6c and sizeof 0x70.
static_assert(offsetof(JoinNode, Left) == 0x28);
static_assert(offsetof(JoinNode, Right) == 0x2c);
static_assert(offsetof(JoinNode, LeftAdapter) == 0x30);
static_assert(offsetof(JoinNode, RightAdapter) == 0x34);
static_assert(offsetof(JoinNode, Unknown) == 0x38);
static_assert(offsetof(JoinNode, LeftDatabaseRef) == 0x40);
static_assert(offsetof(JoinNode, LeftDatabase) == 0x44);
static_assert(offsetof(JoinNode, LeftDatabaseIndirection) == 0x50);
static_assert(offsetof(JoinNode, RightDatabaseRef) == 0x54);
static_assert(offsetof(JoinNode, RightDatabase) == 0x58);
static_assert(offsetof(JoinNode, RightDatabaseIndirection) == 0x64);
static_assert(sizeof(JoinNode) == 0x68);
static_assert(sizeof(AndNode) == 0x68);
static_assert(sizeof(NotAndNode) == 0x68);

// PORT (linux): the fourth Rete record, and the one place in this hierarchy where upstream is
// wrong rather than merely differently packed.  DataNode is the game's CReteStartNode,
// DatabaseNode its CReteFact and ProcNode its CReteEvent -- the last two named by vtable slot 6
// (upstream's IsProc, the game's ValidTestAlwaysFails): CReteEvent overrides it, CReteFact does
// not, which is what "proc" means here.  In libOsiris.so the three vtables are 0x10a6f0
// (CReteStartNode, slot 5 pure), 0x10aa30 (slot 6 = its own 0xa33d0, so CReteEvent) and
// 0x10a960 (slot 6 = the base's 0xa1b70, so CReteFact).
//
// There is no UsedBy anywhere in the branch.  CReteStartNode's ctor -- libOsiris.so 0xaba90,
// macOS __ZN14CReteStartNodeC2EP12COsiSmartBuf 0x75ae0, instruction for instruction the same --
// calls CReteNode's, installs its own vtable at 0x00, and then builds an empty libc++ list in
// place: the COsiList vtable at 0x20 (0xabace), `lea r8, [rbx+0x28]` stored to BOTH 0x28 and
// 0x30 (0xababe/0xabac2, the sentinel pointing at itself), and a zero qword at 0x38
// (0xabac6).  Nothing is written at 0x1c.  The type is named by the ctor's own landing pad,
// which destroys the sub-object at +0x20 by calling 0xa3240 -- the macOS twin's landing pad
// calls __ZN8COsiListI15CReteConnectionED1Ev 0x73c50 at the same spot.
//
// The two pointers are told apart by the read side, not by the ctor.  CReteStartNode's dtor
// (libOsiris.so 0xa1ac0, macOS 0x73be0) is libc++ list::clear(): it takes `first` from 0x30 and
// `last` from 0x28, reads first->[0] and last->[8] and relinks them, then walks `node = [node+8]`
// from 0x30 until it reaches &this->0x28 freeing as it goes -- so 0x00 is __prev_, 0x08 is
// __next_, 0x30 is __end_.__next_ (first) and 0x28 is __end_.__prev_ (last).  0x38 is __size_:
// the append at libOsiris.so 0xabdff `inc qword [rbx+0x38]`.
//
// The element is CReteConnection and it is upstream's NodeEntryRef.  Two witnesses, both value
// stores of an argument: CReteChildNode::SetChild(CReteConnection) macOS 0x63a90 copies
// `dword [rsi]` to 0x1c and `qword [rsi+4]` to 0x20 of a TreeNode, which is a 12-byte value
// written at TreeNode::Next; and CReteStartNode::Write macOS 0x66820 serialises each node with
// the game's own typed overloads -- write(unsigned int) of [node+0x10], write(TReteEntryPoint)
// of [node+0x14], write(int) of [node+0x18] -- naming NodeRef Node, EntryPoint EntryPoint and
// uint32_t GoalId in that order.  Write emits nothing else after the base's, so the class has
// no further serialised member.
//
// sizeof 0x40 is an allocation witness (completeness), NOT an ABI witness: the first derived
// member is the list's 8-aligned vtable pointer, so it does not move into the base's tail
// padding and MSVC would lay the same declaration out identically.  That is also why the three
// records drop out of the itanium worklist once this lands.  The size is read four times in the
// macOS oracle -- CReteNodeFactory::CreateReteFact(COsiSmartBuf*) 0x759f0,
// CreateReteFact(COsipParameterList*) 0x7393e, CreateReteFact(COsiValueTypeList*) 0x73d5e and
// CreateReteEvent() 0x73e68, plus CreateReteEvent(COsiSmartBuf*) 0x76470 -- and twice in
// libOsiris.so, where both factories are inlined into COsiris::_ReadReteNodes: `mov esi, 0x40`
// at 0x8b434 before the CReteEvent vtable store and at 0x8b7ce before the CReteFact one.
static_assert(offsetof(NodeEntryRef, EntryPoint) == 0x4);
static_assert(offsetof(NodeEntryRef, GoalId) == 0x8);
static_assert(sizeof(NodeEntryRef) == 0xc);

static_assert(offsetof(DataNodeRef, Next) == 0x8);
static_assert(offsetof(DataNodeRef, Reference) == 0x10);
static_assert(sizeof(DataNodeRef) == 0x20);

static_assert(offsetof(DataNodeReferenceList, Last) == 0x8);
static_assert(offsetof(DataNodeReferenceList, First) == 0x10);
static_assert(offsetof(DataNodeReferenceList, Count) == 0x18);

static_assert(offsetof(DataNode, Children) == 0x20);
static_assert(sizeof(DataNode) == 0x40);
static_assert(sizeof(ProcNode) == 0x40);
static_assert(sizeof(DatabaseNode) == 0x40);
#endif

class QueryNode : public Node
{
};

class UserQueryNode : public QueryNode
{
};

class DivQueryNode : public QueryNode
{
};

class InternalQueryNode : public QueryNode
{
};

// Used by COsiris::Event()
enum class ReturnCode
{
    EventProcessed = 0,
    FunctionUndefined = 1,
    EventIgnored = 2
};

enum class NodeType
{
    None = 0,
    Database = 1,
    Proc = 2,
    DivQuery = 3,
    And = 4,
    NotAnd = 5,
    RelOp = 6,
    Rule = 7,
    InternalQuery = 8,
    UserQuery = 9,
    Max = UserQuery
};

enum DebugFlag
{
    DF_FunctionList = 1 << 16,
    DF_NodeList = 1 << 17,
    DF_CompileTrace = 1 << 18,
    DF_DebugTrace = 1 << 19,
    DF_DebugFacts = 1 << 20,
    DF_LogRuleFailures = 1 << 21,
    DF_ListOrphans = 1 << 22,
    DF_SuppressInitLog = 1 << 23,
    DF_LogFactFailures = 1 << 24,
    DF_LogSuccessfulFacts = 1 << 25,
    DF_LogFailedFacts = 1 << 26,
    DF_SuppressRuleVariables = 1 << 27,
    DF_SuppressParentGoals = 1 << 28,
    DF_SuppressUnknown = 1 << 29,
    DF_SuppressQueryResults = 1 << 30,
    DF_DumpDatabases = 1 << 31
};

typedef void (* COsirisOpenLogFileProc)(void * Osiris, wchar_t const * Path, std::ios_base::openmode mode);
typedef void (* COsirisCloseLogFileProc)(void * Osiris);
typedef bool (* COsirisCompileProc)(void * Osiris, wchar_t const * path, wchar_t const * mode);
typedef int (* COsirisInitGameProc)(void * Osiris);
typedef int (* COsirisLoadProc)(void * Osiris, void * Buf);
typedef bool (* COsirisMergeProc)(void * Osiris, wchar_t * Src);
typedef bool(* COsirisGetFunctionMappingsProc)(void * Osiris, MappingInfo ** mappings, uint32_t * mappingCount);
typedef int (* COsirisDeleteAllDataProc)(void * Osiris, bool DeleteTypes);
typedef int (* COsirisReadHeaderProc)(void * Osiris, void * OsiSmartBuf, unsigned __int8 * MajorVersion, unsigned __int8 * MinorVersion, unsigned __int8 * BigEndian, unsigned __int8 * Unused, char * StoryFileVersion, unsigned int * DebugFlags);
typedef void (* RuleActionCallProc)(RuleActionNode * Action, void * a1, void * a2, void * a3, void * a4);

struct FunctionDb : public TypeDb<OsiFunctionDef*>
{
    struct FunctionIdHashSlot
    {
        TMap<uint32_t, OsiFunctionDef*> NodeMap;   // PORT (linux): 24-byte slot, as HashSlot
    };

    OsiFunctionDef** FindById(uint32_t id)
    {
        auto& bucket = FunctionIdHash[id % 0x3FF];
        return bucket.NodeMap.Find(id);
    }

    FunctionIdHashSlot FunctionIdHash[1023];
    TMap<uint32_t, uint32_t> AllFunctionIds;
    // PORT (linux): what follows in this build is an unordered_map (five words, load factor
    // 1.0f) and a counter at +0xc018; COsiris::GetFunctionMappings reads the counter.
    void* U1[5];
    uint32_t U2;
};

using ObjectDb = TypeDb<void*>; // Unknown type
// PORT (linux): an alias, not a measured shape -- the sizeof(TypeDb<void*>) assert below
// does not cover it.  OsiDIVObjectsMan is 0xbff0 in this build (two 1023-slot indexes
// plus a tree; manifest row osi__Objects), where this alias is 0x5ff0.  Nothing in the
// extender dereferences it, so it stays as upstream left it.

struct OsiTypeDb : public TypeDb<OsirisTypeInfo>
{
    struct AliasInfo
    {
        uint16_t TypeId;
        uint16_t AliasTypeId;
    };

    AliasInfo* Aliases;
    uint64_t c, d;

    ValueType ResolveAlias(uint16_t typeId)
    {
        // PORT (linux): Aliases/c/d are the alias vector's begin/end/capacity (+0x5ff0 in this
        // build); COsiDIVObjectTypesMan::GetRootType bounds the index by end - begin, so do we.
        auto numAliases = (uint64_t)((AliasInfo*)c - Aliases);
        while (typeId > (uint16_t)ValueType::GuidString && typeId != (uint16_t)ValueType::Undefined) {
            if (typeId >= numAliases) break;
            typeId = Aliases[typeId].AliasTypeId;
        }

        return (ValueType)typeId;
    }
};

struct COsiString
{
    const char* Str;
    uint64_t field_8;
    uint32_t SomeRef;
    int field_14;
    int RefCount;
    int field_1C;
};


struct COsiStringPool
{
    float field_0;
    std::unordered_map<char const*, uint32_t> Values;
    OsiVector<COsiString> Strings;
    OsiVector<uint32_t> FreeList;
};


struct COsiStringTable
{
    COsiStringPool GlobalPool;
    COsiStringPool UnknownPool;
    COsiStringPool GuidPool;
};

using COsiStringTableGetStringProc = char const* (COsiStringTable** a1, OsiStringHandle handle);
// PORT (linux): COsiStringTable::AddStr(char const*, bool) takes (this, str, isGuid) and returns
// the 8-byte handle in rax (read off 0x73f70 in libOsiris.so). Upstream's
// Windows shape had MSVC's hidden result pointer as a second argument; passed here it would
// have made the string argument the address of the handle.
using COsiStringTableAddStringProc = OsiStringHandle (COsiStringTable** a1, const char* str, bool isGuidString);
using COsiStringTableAddStringRefProc = void (COsiStringTable** a1, OsiStringHandle handle);
using COsiStringTableRemoveStringProc = void (COsiStringTable** a1, OsiStringHandle handle);

struct OsirisStaticGlobals
{
    COsiStringTable *** StringTable{ nullptr };
    VariableDb ** Variables{ nullptr };
    OsiTypeDb ** Types{ nullptr };
    EnumDb ** Enums{ nullptr };
    FunctionDb ** Functions{ nullptr };
    ObjectDb ** Objects{ nullptr };
    GoalDb ** Goals{ nullptr };
    AdapterDb ** Adapters{ nullptr };
    DatabaseDb ** Databases{ nullptr };
    NodeDb ** Nodes{ nullptr };
    DebugFlag * DebugFlags{ nullptr };

    COsiStringTableGetStringProc* GetString{ nullptr };
    COsiStringTableAddStringProc* AddString{ nullptr };
    COsiStringTableAddStringRefProc* AddStringRef{ nullptr };
    COsiStringTableRemoveStringProc* RemoveString{ nullptr };
};

struct OsirisDynamicGlobals
{
    void * OsirisObject{ nullptr };
};

// PORT (linux): the shapes measured off COsiris::COsiris in libOsiris.so
static_assert(sizeof(TypeDb<void*>) == 0x5ff0, "1023 slots of 24 + NumItems");
static_assert(sizeof(OsiTypeDb) == 0x6008, "TypeDb + the alias vector");
static_assert(sizeof(GoalDb) == 0x6008, "TypeDb + a vector");
static_assert(offsetof(FunctionDb, FunctionIdHash) == 0x5ff0 && sizeof(FunctionDb) == 0xc020, "COsiFunctionMan");
static_assert(sizeof(NodeDb) == 0x20 && offsetof(NodeDb, Db.Elements) == 8, "a factory: count, vector");
static_assert(offsetof(Database, NumParams) == 0x40, "CReteDBase::NumParams, the byte CReteFact::Valid reads");
static_assert(sizeof(TuplePtrLL) == 0x20, "VMT + a libc++ list");
static_assert(offsetof(Node, Id) == 8 && offsetof(Node, Database) == 0x18, "CReteNode: id at +8, database at +0x18");
static_assert(offsetof(NodeVMT, SetLineNumber) == 23 * 8, "SetLinenoOfThen is slot 23 in this build");

}
