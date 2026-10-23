#pragma once

BEGIN_SE()

struct StatsExpressionInternal
{
    using Variant2 = std::variant<StatsExpressionVariableData, AbilityId, SkillId, stats::StatusGroup, STDString>;
    using Param = std::variant<StatsExpressionType, Variant2, StatsExpressionVariableDataType, StatsExpressionVariableDataModifier, RollDefinition, ResourceRollDefinition, StatsContextType, int32_t, bool>;

    Array<Param> Params;
    STDString Code;

    static Guid Hash(StringView code);
};

// bin/bg3 0x352bc90, the copy constructor of ls::DynamicArray<Param, TaggedAllocator<int>>
// (matched r3:2 to the macOS oracle's _ZN2ls12DynamicArrayINS_7VariantIJN3eoc20EStatsExpres...),
// reads the whole shape of one element: it sizes the buffer n<<5 (0x352bcb8) and strides the
// copy loop by 0x20 (0x352bd1a), takes the *outer* discriminant from +0x18 and range-checks it
// against 8 -- Param's nine alternatives (0x352bced / 0x352bcf8), takes the *inner* one from
// +0x10 and range-checks it against 4 -- Variant2's five (0x352bd42 / 0x352bd4d), and reads the
// inner string arm's long flag at +0x0f with its data at +0x00 and size at +0x08 before calling
// ls::STDString's copy constructor (0x352bd5d, 0x352bd73, 0x352bd78). So both unions start at
// +0x00 and neither carries libc++'s leading hole. libc++ puts a variant's one-byte index at
// round_up(max sizeof, max alignof) past the union, so the sizes below pin the two
// discriminants where the binary has them.  PORT (linux): both unions are at +0 only because
// the sfinae-tag overlay (ext/toolchain/) stops the inner variant's empty bases colliding with
// the outer's; stock libc++ puts the outer union at +8 and Param at 40.
static_assert(sizeof(StatsExpressionInternal::Variant2) == 0x18, "inner index at +0x10 of +0x10, bin/bg3 0x352bd42");
static_assert(sizeof(StatsExpressionInternal::Param) == 0x20, "outer index at +0x18, and the n<<5 / 0x20 stride, bin/bg3 0x352bcb8, 0x352bced, 0x352bd1a");
static_assert(sizeof(STDString) == 0x10, "the inner string arm, bin/bg3 0x352bd5d and 0x352bd73");
static_assert(offsetof(StatsExpressionInternal, Params) == 0, "the array the copy constructor walks, bin/bg3 0x352bc9b");

struct StatsExpressionPooled : public StatsExpressionInternal
{
    [[bg3::hidden]] std::atomic<int32_t> RefCount;

    // Lua property helpers
    //# P_GETTER_SETTER(Params, GetParams, SetParams)
    //# P_GETTER_SETTER(Code, GetCode, SetCode)
    //# P_GETTER(RefCount, GetRefCount)
    Array<Param>* GetParams() const;
    void SetParams(lua_State* L, Array<Param> params);

    StringView GetCode() const;
    void SetCode(lua_State* L, StringView code);

    int32_t GetRefCount() const;
};

class [[bg3::hidden]] StatsExpressionRef
{
public:
    inline StatsExpressionRef()
        : expression_{ nullptr }
    {}
    
    explicit inline StatsExpressionRef(StatsExpressionPooled* expr)
        : expression_{ expr }
    {
        if (expression_) {
            expression_->RefCount++;
        }
    }
    
    inline ~StatsExpressionRef()
    {
        if (expression_ && --expression_->RefCount == 0) {
            GameDelete(expression_);
        }
    }
    
    inline StatsExpressionRef(StatsExpressionRef const& expr)
        : expression_{ expr.expression_ }
    {
        if (expression_) {
            expression_->RefCount++;
        }
    }
    
    inline StatsExpressionRef& operator = (StatsExpressionRef const& expr)
    {
        expression_ = expr.expression_;
        if (expression_) {
            expression_->RefCount++;
        }
        return *this;
    }

    inline StatsExpressionPooled* operator -> () const
    {
        return expression_;
    }

    inline StatsExpressionPooled* operator * () const
    {
        return expression_;
    }

    inline StatsExpressionPooled* Get() const
    {
        return expression_;
    }
    
private:
    StatsExpressionPooled* expression_;
};

MARK_BY_VALUE_TYPE(StatsExpressionRef);

struct StatsExpressionWithMetadata : public StatsExpressionInternal
{
    BoostType BoostType;
    BoostSource Cause;
    TranslatedString BoostSourceName;
    bool CriticalHit;
};

struct StaticStatsExpressionManager : public ProtectedGameObject<StaticStatsExpressionManager>
{
    [[bg3::hidden]] void* VMT;
    LegacyRefMap<Guid, StatsExpressionInternal*> Expressions;
    uint8_t field_18;
};

struct DynamicStatsExpressionManager : public ProtectedGameObject<DynamicStatsExpressionManager>
{
    virtual ~DynamicStatsExpressionManager() = 0;
    virtual StatsExpressionRef CreateStatsExpression(Guid const& id, StringView const& expr) = 0;
};

END_SE()
