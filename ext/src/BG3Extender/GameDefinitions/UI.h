#pragma once

#include <CoreLib/Base/Base.h>

#include <NsCore/BaseComponent.h>
#include <NsCore/TypeClass.h>
#include <NsCore/Delegate.h>
#include <NsGui/GridLength.h>
#include <NsGui/BaseCommand.h>
#include <NsGui/DependencyData.h>
#include <NsGui/DependencyObjectValueData.h>
#include <NsGui/RoutedEvent.h>
#include <NsGui/UIElement.h>
#include <NsGui/UIElementData.h>
#include <NsGui/FrameworkElement.h>
#include <NsGui/Panel.h>
#include <NsGui/ObservableCollection.h>
#include <NsGui/UIElementCollection.h>
#include <NsGui/ContentControl.h>
#include <NsGui/VisualTreeHelper.h>
#include <NsGui/UserControl.h>
#include <NsGui/Uri.h>

BEGIN_BARE_NS(Noesis)

using namespace bg3se;

// Equivalent to Noesis GridLength
struct GridLengthHelper
{
    GridUnitType GridUnitType;
    float Value;
};

using LoadXamlProc = Ptr<BaseComponent>* (Ptr<BaseComponent>& ret, char const* path);
using Visual__PointConversionProc = Point (Visual const*, Point const& point);
using Visual__TreeHelperHitTestProc = HitTestResult (Visual* visual, Point const& point);

struct SymbolManagerInternals
{
    Vector<char const*> Strings;
    HashMap<char const*, uint32_t, 0, HashBucket_KHV<const char*, uint32_t, CaseCharArrayHashKeyInfo>> Keys;
};

struct ReflectionInternals
{
    Vector<Symbol> Names;
    HashMap<uint32_t, Type*> NameToType;
};

inline void* Alloc(SizeT size)
{
    return GameAllocRaw(size);
}

inline void* Realloc(void* ptr, SizeT size)
{
    if (ptr != nullptr) {
        auto newMem = Alloc(size);
        memcpy(newMem, ptr, size);
        Dealloc(ptr);
        return newMem;
    } else {
        return Alloc(size);
    }
}

inline void Dealloc(void* ptr)
{
    return GameFree(ptr);
}

inline char const* Type::GetName() const
{
    return mName.Str();
}

inline Symbol Type::GetTypeId() const
{
    return mName;
}

inline ReflectionInternals* GetReflection()
{
    return (ReflectionInternals *)((uintptr_t)GetStaticSymbols().Noesis__Reflection__Data - sizeof(Noesis::BaseVector_));
}

inline SymbolManagerInternals* GetSymbolManager()
{
    return (SymbolManagerInternals*)((uintptr_t)GetStaticSymbols().Noesis__SymbolManager__Data - sizeof(Noesis::BaseVector_));
}

inline char const* SymbolManager::GetString(uint32_t index)
{
    auto syms = GetSymbolManager();
    // Handle possibly corrupted symbol ptrs
    if (index < syms->Strings.Size()) {
        return syms->Strings[index];
    } else {
        return "";
    }
}

// PORT (linux): upstream takes Noesis's reflection lock in *shared* mode here. In bin/bg3 the
// lock is `gMutex`, a 4-byte pthread_spinlock_t with no shared mode (ext/linux/shim/windows.h
// has the witness), and the game guards it with a recursion protocol that this has to follow
// too or a re-entrant GetType from a thread already inside RegisterType self-deadlocks -- where
// the Win32 shared acquire would merely have blocked. Noesis::Reflection::RegisterType
// (bin/bg3 0x2fdfea0) is the protocol, verbatim:
//
//     tid = gettid()                                  // syscall 0xba at 0x2fdfeb0
//     if (gThreadID != tid) {                          // 0x2fdfec5, compared as 32 bits
//         pthread_spin_lock(&gMutex)                   // 0x2fdfed5
//         gThreadID = tid                              // 0x2fdfeda, *after* the acquire
//     }
//     saved = gRecursiveDepth                          // 0x2fdfee1
//     gRecursiveDepth = saved + 1                      // 0x2fdfef0
//     ... body ...
//     gRecursiveDepth = saved                          // 0x2fdff2d -- restored, not decremented
//     if (saved == 0) {                                // 0x2fdff33
//         gThreadID = -1                               // 0x2fdff3e, *before* the release
//         pthread_spin_unlock(&gMutex)                 // 0x2fdff48
//     }
//
// The two counters are plain loads and stores in the game, not atomics: the spinlock is what
// orders them, so relaxed is the faithful ordering as well as the cheap one. The tid is a
// gettid() tid -- never pthread_self(), which on glibc is a pointer -- and the shim's
// GetCurrentThreadId() is exactly syscall(SYS_gettid) (ext/linux/win32.cpp:112), so upstream's
// own spelling of it is the right one here.
// Never run in game; the whole file is compiled only with the Noesis fence down.
class ReflectionLock
{
public:
    inline ReflectionLock()
        : lock_(GetStaticSymbols().Noesis__Reflection__Lock),
          threadId_(GetStaticSymbols().Noesis__Reflection__LockedByThreadId),
          depth_(GetStaticSymbols().Noesis__Reflection__NumLocks)
    {
        auto tid = (uint32_t)GetCurrentThreadId();
        if (threadId_->load(std::memory_order_relaxed) != tid) {
            AcquireNoesisSpinLock(lock_);
            threadId_->store(tid, std::memory_order_relaxed);
        }
        saved_ = depth_->load(std::memory_order_relaxed);
        depth_->store(saved_ + 1, std::memory_order_relaxed);
    }

    inline ~ReflectionLock()
    {
        depth_->store(saved_, std::memory_order_relaxed);
        if (saved_ == 0) {
            threadId_->store(0xffffffff, std::memory_order_relaxed);
            ReleaseNoesisSpinLock(lock_);
        }
    }

    ReflectionLock(ReflectionLock const&) = delete;
    ReflectionLock& operator = (ReflectionLock const&) = delete;

private:
    NS_SPINLOCK* lock_;
    std::atomic<uint32_t>* threadId_;
    std::atomic<uint32_t>* depth_;
    uint32_t saved_;
};

inline const Type* Reflection::GetType(Symbol name)
{
    ReflectionLock lock;

    Type* val;
    auto it = GetReflection()->NameToType.Find((uint32_t)name);
    if (it != GetReflection()->NameToType.End()) {
        val = it->value;
    } else {
        val = nullptr;
    }

    return val;
}

inline TypeClass const* TypeClass::GetBase() const
{
    return mBase;
}

inline bool TypeClass::IsInterface() const
{
    return mIsInterface;
}

struct ObjectHelpers
{
public:
    static PropertyOperationResult FallbackGetProperty(lua_State* L, lua::LifetimeHandle lifetime, BaseObject const* object, bg3se::FixedString const& prop);
    static PropertyOperationResult FallbackSetProperty(lua_State* L, BaseObject* object, bg3se::FixedString const& prop, int index);
    static UserReturn GetNamedProperty(lua_State* L, BaseObject const* o, bg3se::FixedString const& name);
    static UserReturn GetProperty(lua_State* L, BaseObject const* o, TypeProperty const* prop);
    static void SetNamedProperty(lua_State* L, BaseObject* o, bg3se::FixedString const& name, lua::AnyRef value);
    static void SetProperty(lua_State* L, BaseObject* o, TypeProperty const* prop, lua::AnyRef value);
    static UserReturn GetAllProperties(lua_State* L, BaseObject const* o);
    static UserReturn GetDirectProperties(lua_State* L, BaseObject const* o);
    static UserReturn GetDependencyProperties(lua_State* L, BaseObject const* o);

    static TypeClass* GetClassType(BaseObject const* o);
    static Symbol GetClassTypeName(BaseObject const* o);
    static STDString ToString(BaseObject const* o);
    static uint32_t GetNumReferences(BaseRefCounted const* o);

private:
    static void DoGetDirectProperties(lua_State* L, BaseObject const* o);
    static void DoGetDependencyProperties(lua_State* L, BaseObject const* o);
};

struct RoutedEventHelpers
{
    static Symbol GetName(RoutedEvent const* o);
    static TypeClass* GetOwnerType(RoutedEvent const* o);
};

struct DependencyObjectHelpers
{
    static UserReturn GetProperty(lua_State* L, DependencyObject const* o, bg3se::FixedString const& name);
    static UserReturn GetDependencyProperty(lua_State* L, DependencyObject const* o, DependencyProperty const* prop);
    static void SetProperty(lua_State* L, DependencyObject* o, bg3se::FixedString const& name, lua::AnyRef value);
    static void SetDependencyProperty(lua_State* L, DependencyObject* o, DependencyProperty const* prop, lua::AnyRef value);
};

struct DependencyPropertyHelpers
{
    static Symbol GetName(DependencyProperty const* o);
    static Type* GetType(DependencyProperty const* o);
    static TypeClass* GetOwnerType(DependencyProperty const* o);
    static bool IsReadOnly(DependencyProperty const* o);
};

enum class StoredValueCopy {};

struct StoredValueHolder
{
    inline StoredValueHolder() {}

    template <class T>
    inline StoredValueHolder(T const& val, StoredValueCopy)
    {
        if constexpr (sizeof(T) <= sizeof(void*)) {
            Value = *reinterpret_cast<void* const*>(&val);
            IsIntegral = true;
            IsOwned = false;
        } else {
            Value = GameAlloc<T>(val);
            IsIntegral = false;
            IsOwned = true;
        }
    }

    inline StoredValueHolder(BaseObject* val) : Value(val), IsIntegral(true), IsOwned(false) {}

    StoredValueHolder(StoredValueHolder const&) = delete;
    StoredValueHolder & operator = (StoredValueHolder const&) = delete;

    inline StoredValueHolder(StoredValueHolder && o) noexcept
        : Value(o.Value), IsIntegral(o.IsIntegral), IsOwned(o.IsOwned)
    {
        o.IsOwned = false;
    }

    inline StoredValueHolder& operator = (StoredValueHolder&& o) noexcept
    {
        if (&o != this) {
            if (IsOwned) {
                GameFree(Value);
            }

            Value = o.Value;
            IsIntegral = o.IsIntegral;
            IsOwned = o.IsOwned;
            o.IsOwned = false;
        }

        return *this;
    }

    inline ~StoredValueHolder()
    {
        if (IsOwned) {
            GameFree(Value);
        }
    }

    void* Value{ nullptr };
    bool IsIntegral{ true };
    bool IsOwned{ false };
};

struct StoredValueHelpers
{
    static std::optional<int64_t> TryParseIntegralValue(lua_State* L, Type const* type, void* val);
    static void PushValue(lua_State* L, Type const* type, StoredValue const* o, Type const* objectType = nullptr, Symbol* propertyName = nullptr);
    static void PushValue(lua_State* L, Type const* type, void* val, Type const* objectType = nullptr, Symbol* propertyName = nullptr);
    static void PushProperty(lua_State* L, BaseObject const* obj, TypeClass const* objType, TypeProperty const* prop);
    static void PushValue(lua_State* L, TypeEnum const* type, uint64_t val);

    template <class T>
    static void PushRawValue(lua_State* L, Type const* type, void* val);
    template <class T>
    static void PushPtrValue(lua_State* L, Type const* type, void* val);

    template <class T>
    static StoredValueHolder GetRawValue(lua_State* L, Type const* type, lua::AnyRef value);

    static std::optional<StoredValueHolder> GetValue(lua_State* L, Type const* type, lua::AnyRef value);
};

struct CommandHelpers
{
    static bool CanExecute(lua_State* L, BaseCommand const* o, std::optional<BaseComponent*> arg);
    static void Execute(lua_State* L, BaseCommand const* o, std::optional<BaseComponent*> arg);
    static bool InterfaceCanExecute(lua_State* L, ICommand const* o, std::optional<BaseComponent*> arg);
    static void InterfaceExecute(lua_State* L, ICommand const* o, std::optional<BaseComponent*> arg);
};

struct VisualHelpers
{
    static Visual* GetVisualParent(Visual const* o);
    static uint32_t GetVisualChildrenCount(lua_State* L, Visual const* o);
    static Visual* GetVisualChild(Visual const* o, uint32_t index);
    static Point PointFromScreen(lua_State* L, Visual const* o, Point const& point);
    static Point PointToScreen(lua_State* L, Visual const* o, Point const& point);
    static Visual* HitTest(lua_State* L, Visual* o, Point const& point);
};

struct UIElementDataHelpers
{
    static RoutedEvent* GetEvent(UIElementData const* o, Symbol evt);
    static Array<RoutedEvent*> GetAllEvents(UIElementData const* o);
};

struct UIElementHelpers
{
    static uint64_t Subscribe(lua_State* L, UIElement* o, bg3se::FixedString const& evt, lua::FunctionRef func);
    static bool Unsubscribe(lua_State* L, UIElement* o, uint64_t index);
};

struct FrameworkElementHelpers
{
    static FrameworkElement* GetLogicalParent(FrameworkElement const* o);
    static uint32_t GetLogicalChildrenCount(FrameworkElement const* o);
    static BaseComponent* GetLogicalChild(FrameworkElement const* o, uint32_t index);
    static BaseComponent* FindNodeName(FrameworkElement const* o, char const* name);
    static BaseComponent* GetResource(FrameworkElement const* o, char const* key, std::optional<bool> fullElementSearch);
    static BaseObject* GetTreeParent(FrameworkElement const* o);
    static FrameworkElement* AttachXamlChild(FrameworkElement* o, char const* path);
    static FrameworkElement* SetXamlProperty(FrameworkElement* o, bg3se::FixedString const& prop, char const* path);
};

struct TypePropertyHelpers
{
    static Symbol GetName(TypeProperty const* o);
    static Type* GetContentType(TypeProperty const* o);
    static Vector<TypeMetaData*> const* GetMeta(TypeProperty const* o);
    // Variant of GetMeta() for Lua calls (Noesis::Vector is not exposed to Lua as a container type)
    static Array<TypeMetaData*> GetMetaLua(TypeProperty const* o);
    static bool IsReadOnly(TypeProperty const* o);
};

struct TypeHelpers
{
    static STDString GetName(Type const* o);
    static bool IsDescendantOf(Type const* type, TypeClass const* base);
    static bool IsDescendantOf(TypeClass const* type, TypeClass const* base);
    static TypeClass* GetBase(TypeClass const* o);
    static bool IsInterface(TypeClass const* o);
    static TypeClass::AncestorVector const* GetInterfaces(TypeClass const* o);

    static Array<TypeProperty const*> const* GetProperties(TypeClass const* o);
    static Array<DependencyProperty const*> const* GetDependencyProperties(TypeClass const* o);
    static Array<RoutedEvent const*> const* GetRoutedEvents(TypeClass const* o);

    static TypeProperty const* GetProperty(TypeClass const* o, bg3se::FixedString const& name);
    static DependencyProperty const* GetDependencyProperty(TypeClass const* o, bg3se::FixedString const& name);
    static RoutedEvent const* GetRoutedEvent(TypeClass const* o, bg3se::FixedString const& name);
    static TypeMetaData* FindMetaOrDescendant(TypeClass const* o, const TypeClass* metaDataType);

    static std::optional<uint64_t> StringToEnum(TypeEnum const* e, char const* value);
    static std::optional<uint64_t> StringToEnum(TypeEnum const* e, Symbol value);

    template <class Fun>
    static void ForEachMeta(TypeClass const* cls, const TypeClass* metaDataType, Fun fun);
};

class LuaDelegateCommand final : public BaseCommand
{
public:
    bool CanExecute(BaseComponent* param) const override;
    void Execute(BaseComponent* param) const override;
    void BindHandler(lua_State* L, lua::Ref const& handler);

    NS_DECLARE_REFLECTION(LuaDelegateCommand, BaseCommand)

private:
    lua::PersistentRegistryEntry handler_;
};

END_BARE_NS()


BEGIN_NS(ui)

using namespace Noesis;

struct UICanvas;
struct TooltipManager;
struct ContextMenuManager;
struct StateMachine;
struct DragAndDropManager;
struct MessageBoxManager;
struct BrushManager;
struct WindowManager;
// PORT (linux): upstream's forward-declaration list is missing these two, and both are used
// as pointer members hundreds of lines before their definitions (ViewModel at 686 and 844+,
// DCWidget at 959+; defined at 1269 and 1274). MSVC accepts a pointer to an undeclared class
// inside a class body by declaring it implicitly; clang does not, so the names go on the list
// upstream already keeps rather than anywhere new.
struct ViewModel;
struct DCWidget;

struct UIEvent
{
    uint32_t GameStateId{ 0 };
    uint8_t Type{ 0 };
    FixedString EventName;
    FixedString field_C;
    bool field_10{ false };
    bool field_11{ false };
    PlayerId PlayerId;
    Array<input::FireEventDesc>* InputEvents{ nullptr };
};


struct UIInitialSubstate : public Noesis::BaseComponent
{
    String Name;
    String MetaData;
};


struct UIStateEvent : public Noesis::BaseComponent
{
    String Name;
    String HandlesEvent;
    BaseObservableCollection* Actions;
};


struct UIStateWidget : public Noesis::BaseComponent
{
    String Filename;
    String Layer;
    String SoundLayer;
    Noesis::Uri Uri;
    bool IgnoreHitTest;
    bool BlockedLoading;
    bool AllowCreationDelay;
    BaseObservableCollection* Libraries;
};


struct UIWidgetMetadata
{
    __int64 field_0;
    Guid StateGuid;
    int field_18;
    int field_1C;
    uint8_t Flags;
};


struct UIState : public Noesis::BaseComponent
{
    String Name;
    String Layout;
    String Owner;
    String DisableStatesBelow;
    String HideStatesBelow;
    String IsModal;
    String IsModalCameraControlAllowed;
    String ForceSplitscreen;
    String ForceVisible;
    String DisableSoundLayersAbove;
    String TutorialLayer;
    String ModType;
    String IsModdable;
    BaseObservableCollection* InitialSubstates;
    BaseObservableCollection* Events;
    BaseObservableCollection* Widgets;
    String ActivateOnRestore;
    __int64 field_1A8_Events_M;
    __int64 field_1B0;
    __int64 field_1B8;
    __int64 field_1C0;
    __int64 field_1C8;
    __int64 field_1D0;
    __int64 field_1D8;
    __int64 field_1E0;
    uint8_t field_1E8[2048];
    LegacyMap<int, int> field_9E8;
};


struct UIWidget : public UserControl
{
    CRITICAL_SECTION CS_;
    Array<void*> field_2A0_InputChildInfo;
    Array<void*> field_2B0_InputChildChange;
    uint64_t qword2C0[2];
    ContextMenuManager* ContextMenuManager;
    void* qword2D8;
    TooltipManager* TooltipManager;
    WindowManager* WindowManager;
    StateMachine* StateMachine;
    UIStateWidget* WidgetData;
    input::InputManager* InputManager;
    Guid StateId;
    void* ParentCanvas;
    int LayerIndex;
    int IndexInLayer;
    STDString FileName;
    Noesis::BaseRefCounted* ContextMenu;
    Noesis::UIElement* FocusRefresh;
    Noesis::UIElement* GetRequestedFocusFallback;
    Noesis::BaseRefCounted* FocusVisual;
    Noesis::BaseRefCounted* qword360;
    int FocusDownEvent;
    int FocusLeftEvent;
    int FocusRightEvent;
    int FocusUpEvent;
    int UIPasteEvent;
    int UICopyEvent;
    int UICutEvent;
    int UISelectAllTextEvent;
    int UIContextMenuEvent;
    int MoveFocusInputState;
    uint8_t Flags;
    uint8_t Flags2;
    uint8_t Flags3;
    PlayerId PlayerId;
};



struct UIStateInstance : public ProtectedGameObject<UIStateInstance>
{
    Array<UIWidget*> Widgets;
    Array<UIWidgetMetadata*> WidgetMetadata;
#if defined(BG3SE_LINUX)
// PORT (linux): the native build's ui::State has no States array at 0x20 at all, and it holds a
// lock where upstream's header has none.  Measured end to end on bin/bg3 a187af94 from the
// class's own constructor (0x35ea780), its destructor (0x39a37a0) and the one allocation site
// (0x35ea210, ui::StateMachineComponent's "create a state from its template" path), all three of
// which agree.  What upstream calls State is at 0x20, not 0x30: the
// allocation site calls ui::StateMachineComponent::FindStateTemplate (0x35ea22a), refcount-
// increments the result (0x35ea291 lock inc [r15+8]) and stores it into [this+0x20] (0x35ea29f),
// and the destructor releases exactly that member (0x39a3814).  So the member order here is
// State, then two bytes, then a 48-byte ls::CriticalSection at 0x30 -- and everything from
// PlayerID on sits 0x28 further along than the Windows header puts it, which is exactly
// sizeof(CRITICAL_SECTION) - sizeof(SRWLOCK).  The States array upstream declares at 0x20 is the
// member that is simply absent; P(States) is compiled out of PropertyMaps/UI.inl on Linux for
// that reason, and it is what the validator refuses with upstream's layout -- an Array's
// validator runs on a refcounted pointer plus two bytes, fails, and disables proxying of the
// whole class ("Validation of property 'States' failed on ui::UIStateInstance" in extender.log).
//
// Two members have no name, only a size and a witness, so they get neither a P() nor a claim:
// field_28 is the two bytes the constructor zeroes at 0x28 (0x35ea792 zeroes [0x1a,0x2a)), and
// field_F0 is 16 bytes the constructor zeroes at 0xf0 (0x35ea871) and the destructor never
// touches -- so it owns nothing, and it is not a Guid either (the two real Guids at 0xd0 and
// 0xe0 are initialised from ls::Guid::s_Empty at 0x35ea84e/0x35ea85c, while 0xf0 gets a plain
// zero).  The Windows header does not model it.
    UIState* State;
    uint16_t field_28;
    CRITICAL_SECTION CriticalSection;
#else
    Array<Array<UIStateInstance*>*> States;
    UIState* State;
#endif
    PlayerId PlayerID;
    uint8_t Flags;
    uint8_t Flags2;
    uint8_t Layout;
    uint8_t Owner;
    uint8_t field_3E;
    uint8_t field_3F;
    FixedString field_40;
    int field_44;
    String field_48;
    String field_68;
    String TutorialLayer;
    Guid StateGuid;
    Guid field_B8;
#if defined(BG3SE_LINUX)
    uint64_t field_F0[2];
#endif
    UIElementCollection* StateWidgets;
};

#if defined(BG3SE_LINUX)
// PORT (linux): ui::State on bin/bg3 a187af94.  Three independent readings of the same object --
// the constructor at 0x35ea780, the destructor at 0x39a37a0 (which tears the members down in
// reverse declaration order) and the allocation site at 0x35ea210 -- and every offset below is
// taken from one of them.  See the block inside the struct for why State moved and what the two
// unnamed members are.
static_assert(sizeof(UIStateInstance) == 0x108,
    "bin/bg3 0x35ea260: mov edi, 0x108 / call operator new, immediately before the constructor "
    "call at 0x35ea270");
static_assert(offsetof(UIStateInstance, Widgets) == 0,
    "bin/bg3 0x39a3836: ~State tail-jumps to ls::DynamicArray<Noesis::Ptr<ui::Widget>>::"
    "~DynamicArray with `this` in rdi, so the widget array is at offset 0");
static_assert(offsetof(UIStateInstance, WidgetMetadata) == 0x10,
    "bin/bg3 0x39a381d/0x39a3823: ~State reads the size at [rbx+0x18] and frees the buffer at "
    "[rbx+0x10], the second 16-byte array");
static_assert(offsetof(UIStateInstance, State) == 0x20,
    "bin/bg3 0x35ea29f: the ui::StateMachineComponent::FindStateTemplate result (0x35ea22a), "
    "refcount-incremented at 0x35ea291, is stored into [this+0x20]; released there by ~State at "
    "0x39a3814");
static_assert(offsetof(UIStateInstance, CriticalSection) == 0x30,
    "bin/bg3 0x35ea78e + 0x35ea7dd: lea r14, [rdi+0x30] then pthread_mutex_init(r14, &attr) with "
    "PTHREAD_MUTEX_RECURSIVE; pthread_mutex_destroy([rbx+0x30]) in ~State at 0x39a380f");
static_assert(offsetof(UIStateInstance, PlayerID) == 0x60,
    "bin/bg3 0x35ea7fe..0x35ea81c: the constructor's first write past the lock is the 15-bit "
    "bitfield at word [rbx+0x60], then word [rbx+0x62] = 0x100 and byte [rbx+0x64] = 0");
static_assert(offsetof(UIStateInstance, field_40) == 0x68,
    "bin/bg3 0x35ea81f: mov dword ptr [rbx+0x68], 0xffffffff -- an ls::FixedString, which ~State "
    "releases through ls::gst::Map::Release from the same offset at 0x39a37e4");
static_assert(offsetof(UIStateInstance, field_48) == 0x70,
    "bin/bg3 0x35ea826: qword [rbx+0x70] = 0x0100001700000000 (mSize 0, mCapacity 23, mIsSmall 1) "
    "and byte [rbx+0x78] = 0; ~State's Noesis::Dealloc guard is [rbx+0x74] at 0x39a37d7");
static_assert(offsetof(UIStateInstance, field_68) == 0x90,
    "bin/bg3 0x35ea82d, same shape; ~State guard [rbx+0x94] at 0x39a37c7");
static_assert(offsetof(UIStateInstance, TutorialLayer) == 0xb0,
    "bin/bg3 0x35ea83a, same shape; ~State guard [rbx+0xb4] at 0x39a37b7");
static_assert(offsetof(UIStateInstance, StateGuid) == 0xd0,
    "bin/bg3 0x35ea84e: movups [rbx+0xd0], ls::Guid::s_Empty; and the allocation site assigns a "
    "freshly generated Guid there as rax:rdx at 0x35ea27a/0x35ea281");
static_assert(offsetof(UIStateInstance, field_B8) == 0xe0,
    "bin/bg3 0x35ea85c: movups [rbx+0xe0], ls::Guid::s_Empty");
static_assert(offsetof(UIStateInstance, StateWidgets) == 0x100,
    "bin/bg3 0x35ea866 + 0x35ea87d: qword [rbx+0x100] = 0, then operator new(0x78) / "
    "Noesis::BaseObservableCollection::BaseObservableCollection() lands there; ~State releases it "
    "through vtable +0x68 at 0x39a37a7");
#endif

struct UIStateMachine : public Noesis::BaseComponent
{
    UnknownSignal StateActivatedCallbacks;
    UnknownSignal StateDeactivatedCallbacks;
    UnknownSignal StateCallbacks;
    UnknownSignal field_58;
    UnknownSignal field_70;
    int field_88;
    bool CanProcessEvents;
    bool IsProcessingEvent;
    char field_8E;
    char field_8F;
    char field_90;
    UIStateInstance* State;
    void* field_A0;
    void* field_A8;
    HashMap<Guid, uint64_t> field_B0_MHM_Guid_pStateStack; // StateStack*
    HashMap<Guid, UIStateInstance*> field_F0_MHM_Guid_pState;
    HashMap<Guid, UIStateInstance*> field_130_MHM_Guid_pState;
    HashSet<UIStateInstance*> field_170_MHS_pState;
    HashSet<void*> field_1A0_pStateStack; // StateStack*
    HashSet<UIStateInstance*> field_1D0_pState;
// PORT (linux): these four are declared in a different ORDER in the native build -- the two
// hash containers come first, then the string and the collection, where upstream's Windows
// header has them the other way round.  The four together occupy 0x98 bytes either way
// (0x40 + 0x30 + 0x20 + 8), so nothing from field_298_short_Guid on moves and sizeof is
// unchanged; only these four sit elsewhere.  The two field_ names carry their Windows
// offsets (0x228, 0x268), not what they are here, which is why RootState and
// States carry asserts past the closing brace.
//
// The witness is bin/bg3 itself, in the Noesis reflection registration the class emits:
// _ZN6Noesis16TypeClassCreator4FillIN2ui21StateMachineComponentENS_13BaseComponentEEE
// (0x418d660) tail-calls two property builders, each of which news a TypeProperty, names it
// with SymbolManager::AddStaticString, installs a TypePropertyOffset<T> vtable and stores the
// member offset into TypeProperty's mOffset at +0x28:
//
//   0x418d6b4  lea rdi, "RootState"   ... 0x418d6d4 vtable TypePropertyOffset<Noesis::FixedString<24>>
//   0x418d6e1  mov dword ptr [rbx + 0x28], 0x270     <- RootState
//   0x418d724  lea rdi, "States"
//   0x418d751  mov dword ptr [rbx + 0x28], 0x290     <- States
//
// and the destructor (0x508a280) tears the region down in reverse declaration order, which
// shows the same four members and their sizes: a FixedString at 0x270 (its capacity bitfield
// read at 0x274 and its buffer union at 0x278, 0x508a921/0x508a92d), a 0x30-byte HashSet at
// 0x240 and a 0x40-byte HashMap at 0x200, between field_1D0_pState (ends 0x200) and
// field_298_short_Guid (0x298, 0x508a903).
//
// Why it matters: RootState is a P() property of this class, so with upstream's order its
// Noesis::String validator runs on whatever sits at 0x200 -- the HashMap's hash-key array --
// fails, and the proxy is disabled for the whole class, and Ext.UI.GetStateMachine() pushes nil
// without raising (extender.log: "Validation of property 'RootState' failed on
// ui::UIStateMachine", then "proxying of this class is disabled").
    HashMap<PlayerId, Array<Guid>> field_228_MHM_short_Guid;
    HashSet<PlayerId> field_268_MHS_short;
    String RootState;
    BaseObservableCollection* States;
    HashMap<short, Guid> field_298_short_Guid;
    __int64 field_2D8;
    HashMap<PlayerId, bool> field_2E0_short_bool;
    HashMap<PlayerId, Array<Guid>> field_320_short_Guid;
    HashSet<PlayerId> field_360_MHS_short;
    __int64 SignalConnections;
    __int64 field_398;
    __int64 field_3A0;
    __int64 field_3A8;
    __int64 field_3B0;
    __int64 field_3B8;
    __int64 field_3C0;
    __int64 field_3C8;
    HashSet<PlayerId> field_3D0_MHS_short;
    PlayerId PlayerID;
    CRITICAL_SECTION CriticalSection;

    struct EventArgs
    {
        uint32_t EventIndex{ 0 };
        int field_4{ 3 };
        FixedString StateEvent;
        FixedString SubState;
        bool RemoveState{ false };
        int16_t PlayerId{ 0 };
        uint64_t field_18{ 0 };
    };

    struct EventResult
    {
        __int64 field_0;
        int field_8;
        int field_C;
        int field_10;
        int field_14;
        int field_18;
        int field_1C;
        __int64 field_20;
        int field_28;
        int field_2C;
        uint16_t field_30;
        int field_34;
        BYTE field_38;
        char field_39;
    };

    struct ECSData
    {
        ecs::EntityWorld* EntityWorld{ nullptr };
        ecs::EntityWorld* EntityWorld2{ nullptr };
        ecs::EntityStorageContainer* Storage{ nullptr };
        ecs::QueryRegistry* QuerySystem{ nullptr };
        ecs::GroupAllocator* GroupAllocator{ nullptr };
    };

    struct EntityContext
    {
        ECSData const* ECS{ nullptr };
        int WorldView{ 0 };
        uint8_t field_C{ 0 };
    };

};

// PORT (linux): the native build's ui::StateMachineComponent, measured on bin/bg3 a187af94.
// The two offsets come out of the class's own Noesis reflection registration, where
// TypeClassCreator::Fill (0x418d660) stores each member offset into TypeProperty::mOffset
// (+0x28) right after installing a TypePropertyOffset<T> vtable; the string literal two
// instructions earlier names the property.  See the block above field_228_MHM_short_Guid for
// the declaration-order difference these two carry.
static_assert(offsetof(UIStateMachine, RootState) == 0x270,
    "bin/bg3 0x418d6e1: mov dword ptr [rbx + 0x28], 0x270, after 0x418d6b4 lea \"RootState\" "
    "and 0x418d6d4 vtable TypePropertyOffset<Noesis::FixedString<24>>");
static_assert(offsetof(UIStateMachine, States) == 0x290,
    "bin/bg3 0x418d751: mov dword ptr [rbx + 0x28], 0x290, after 0x418d724 lea \"States\"");
// This one is read off the destructor instead: ~StateMachineComponent (0x508a280) loads
// [rbx+0x98] into rsi (0x508a2e6), null-checks it, and passes it as argument 1 of
// ui::StateMachineComponent::RemoveChilds(ui::State*, ls::Guid const&) at 0x508a30d with
// ls::Guid::s_Empty in rdx -- so the member at 0x98 is the ui::State* upstream calls State.
static_assert(offsetof(UIStateMachine, State) == 0x98,
    "bin/bg3 0x508a2e6 + 0x508a30d: [rbx+0x98] is argument 1 of "
    "ui::StateMachineComponent::RemoveChilds(ui::State*, ls::Guid const&)");

template <class T>
struct alignas(64) UnsafeMessageQueue : public ProtectedGameObject<UnsafeMessageQueue<T>>
{
    T* Head;
    struct alignas(64)
    {
        T* Tail;
        T* BufferEnd;
        T* SomeBufferPtr;
        T* NextBuffer;
    };
};

struct GameUIParams
{
    void* RendererCommandBuffer;
    void* gap8;
    float oword10;
    float field_14;
    float field_18;
    int field_1C;
    void* ResourceManager;
    int CurHeight_M;
    int ScreenWidth;
    int ScreenHeight;
    BYTE IsNuWT;
    char field_35;
    char field_36;
    char field_37;
};

template <class T> requires requires(T v) { { v.QueueNext } -> std::same_as<T*&>; }
struct IntrusiveMPSCQueueSmall : public ProtectedGameObject<IntrusiveMPSCQueueSmall<T>>
{
    T* WriteStart{ nullptr };
    T* ReadStart{ nullptr };
    T Sentinel;
};

struct BaseCollectionCommand
{
    virtual ~BaseCollectionCommand() {}
    virtual void Run(Noesis::BaseCollection*) {}

    uint64_t ID;
    BaseCollectionCommand* QueueNext;
};

struct IBaseProperty
{
    virtual void SetFromUI(Noesis::BaseComponent* value) = 0;
    virtual void DeferChange(Noesis::BaseComponent* value) = 0;
};

template <class T>
struct DeferredProperty : public IBaseProperty
{
    // TODO - unimplemented
    void SetFromUI(Noesis::BaseComponent* value) override {}
    void DeferChange(Noesis::BaseComponent* value) override {}

    Noesis::BaseComponent* Object;
    T Value;
    Noesis::Symbol Name;
};

struct DeferredCollectionsManager : public ProtectedGameObject<DeferredCollectionsManager>
{
    uint64_t qword0;
    HashMap<uint64_t, Noesis::Ptr<Noesis::BaseObservableCollection>> Collections;
    IntrusiveMPSCQueueSmall<BaseCollectionCommand> Queue;
    CRITICAL_SECTION CS;
    uint8_t Flags;
};

struct DeferredPredicateRefreshParam
{
    void* Predicate; // FilterPredicate*
    DeferredPredicateRefreshParam* QueueNext;
};

struct DeferredPredicatesManager : public ProtectedGameObject<DeferredPredicatesManager>
{
    IntrusiveMPSCQueueSmall<DeferredPredicateRefreshParam> Queue;
    uint8_t Flags;
};

struct DeferredPropertyChangedParams
{
    ViewModel* ViewModel;
    Noesis::Symbol Name;
    DeferredPropertyChangedParams* QueueNext;
};

struct DeferredSetFromUIParams
{
    IBaseProperty* Property;
    ui::ViewModel* ViewModel;
    Noesis::BaseComponent* Value;
    DeferredSetFromUIParams* QueueNext;
};

struct DeferredListenersStorageParams
{
    IBaseProperty* Listener;
    DeferredListenersStorageParams* QueueNext;
};

struct DeferredPropertiesManager : public ProtectedGameObject<DeferredPropertiesManager>
{
    IntrusiveMPSCQueueSmall<DeferredPropertyChangedParams> PropertyChanges;
    IntrusiveMPSCQueueSmall<DeferredSetFromUIParams> SetFromUI;
    IntrusiveMPSCQueueSmall<DeferredListenersStorageParams> ListenerStorage;
    uint8_t Flags;
};


struct NoesisUIManager : public ProtectedGameObject<NoesisUIManager>
{
    GameUIParams Params;
    Noesis::BaseRefCounted* RenderContext;
    void* RenderDevice;
    Noesis::BaseRefCounted* XamlProvider;
    Noesis::BaseRefCounted* FontProvider;
    Noesis::BaseRefCounted* ElementCache;
    UICanvas* MainCanvas;
    Noesis::BaseRefCounted* field_68;
    void* View;
    Noesis::BaseRefCounted* DebugOverlay;
    Noesis::BaseRefCounted* ReloadingXaml;
    Noesis::BaseRefCounted* DirectlyOver;
    uint32_t field_90;
    DeferredCollectionsManager* DeferredCollectionsManager;
    DeferredPredicatesManager* DeferredPredicatesManager;
    DeferredPropertiesManager* DeferredPropertiesManager;
    BrushManager* BrushManager;
    ContextMenuManager* ContextMenuManager;
    ModManager* ModManager;
    TooltipManager* TooltipManager;
    uint32_t CurHeight_M;
    uint32_t ScreenWidth;
    uint32_t ScreenHeight;
    float ViewScale;
    double field_E0;
    int LastMouseX;
    int LastMouseY;
    __int64 field_F0;
    __int64 field_F8;
    UnsafeMessageQueue<void*> InputEvents;
    uint8_t MouseOverUI;
    uint8_t PointOverUI;
    uint8_t ControllerPointPosChanged;
    uint8_t gap183[5];
    float ControllerPointPos[2];
    float ViewMousePosition[2];
    CRITICAL_SECTION CS_;
    char Flags;
    uint8_t Flags2;
};

struct BrushManagerBase : public ProtectedGameObject<BrushManagerBase>
{
    void* VMT;
    void* TextureProvider;
    Noesis::BaseCollection* BrushesToRemove;
    CRITICAL_SECTION CS_;
};


struct BrushManager : public BrushManagerBase
{
    UnknownSignal qword40;
    UnknownSignal qword58;
    UnknownSignal qword70;
    void* TextureProvider2;
};

struct CommandProcessor : public ProtectedGameObject<CommandProcessor>
{
    CRITICAL_SECTION CS_;
    uint8_t Flags;
    Queue<void*> CommandQueue;
};

struct ModManager : public ProtectedGameObject<ModManager>
{
    void* RegisterCallback;
    Array<void*> Mods_Arr_uiModData;
};

struct WindowManager : public ProtectedGameObject<WindowManager>
{
    UnknownSignal FinishLoadWidgetSignal;
    UnknownSignal FinishLoadWidgetSignal2;
    UnknownSignal CancelOpenWidgetSignal;
    UnknownSignal CloseWidgetSignal;
    UnknownSignal field_60;
    CRITICAL_SECTION LoadXAMLCS;
    CRITICAL_SECTION LoadCS;
    Array<void*> PendingPreloads_pPreloadWidgetData;
    // TK only
    // Queue<void*> field_d8_pPreloadWidgetData;
    ModManager* ModManager;
    NoesisUIManager* NoesisUIManager;
    void* field_F8;
    TooltipManager* TooltipManager;
    ContextMenuManager* ContextMenuManager;
    StateMachine* StateMachine;
    Array<Noesis::Ptr<UIWidget>> Widgets;
    Array<Noesis::Ptr<UIWidget>> field_128;
    LegacyRefMap<FixedString, void*> field_138_Map_FS_Unk;
    uint8_t field_148;
    uint8_t field_149;
    void* field_150;
    input::InputManager* InputManager;
    CRITICAL_SECTION UnloadCS;
    HashMap<FixedString, void*> field_188_MHM_StateKey_DCAccum;
    Noesis::BaseCollection* field_1C8;
    Noesis::BaseCollection* field_1D0;
    Noesis::BaseCollection* field_1D8;
    Noesis::BaseCollection* field_1E0;
    Noesis::BaseCollection* field_1E8;
    Noesis::BaseCollection* field_1F0;
    Noesis::BaseCollection* DataContextStorage;
    HashMap<Guid, void*> field_200_Guid_StateWidgetsToRemove;
    HashMap<Guid, void*> field_240_Guid_WidgetToFinishLoadingData;
    UnsafeMessageQueue<void*> DelayedWidgetLoads_UnsafeMessageQueue;
};

struct StateMachine : public ProtectedGameObject<StateMachine>
{
    MessageBoxManager* MessageBoxManager;
    ModManager* ModManager;
    ContextMenuManager* ContextMenuManager;
    WindowManager* WindowManager;
    CRITICAL_SECTION CS_;
    Noesis::BaseRefCounted* qword48;
    UIStateMachine* StateMachineComponent;
    Noesis::BaseRefCounted* StoredStateMachineComponent;
    UnknownFunction ContextMenu_UpdateFromUI;
    Array<void*> LoadedMods_Arr_UIModData;
    bool ApplyModsRequested;
};

struct ViewModelProviderBase : public ProtectedGameObject<ViewModelProviderBase>
{
    void* VMT;
    ViewModel* VMGameData;
    HashMap<uint32_t, void*> Systems_MHM_u32_Ptr_uiSystem;
};

struct FlagHandler : public ProtectedGameObject<FlagHandler>
{
    HashMap<uint64_t, HashSet<EntityHandle>> DirtyHandles;
    HashMap<uint64_t, HashSet<EntityHandle>> NextFrameDirtyHandles;
};


struct ViewModelProvider : public ViewModelProviderBase
{
    void* field_50;
    void* ShroudEventListenerVMT;
    void* DefaultQuestListenerVMT;
    void* NetEventListenerVMT;
    void* CommunityAPIEventListenerVMT;
    char field_78[56];
    void* GameEventListenerVMT;
    UnknownSignal InitializeLocalPlayerSignal;
    UnknownSignal PlayerCombatNotificationSignal;
    UnknownSignal CharacterFTBEnteredSignal;
    UnknownSignal qword100;
    UnknownSignal SelectedCharacterChangedSignal;
    UnknownSignal qword130;
    UnknownSignal CharacterActivatedSignal;
    UnknownSignal CharacterDeactivatedSignal;
    UnknownSignal ActiveSlotChangedSignal;
    UnknownSignal qword190;
    UnknownSignal GameMenuOpenChangedSignal;
    UnknownSignal qword1C0;
    UnknownSignal InventoryOperationSignal;
    UnknownSignal qword1F0;
    UnknownSignal ItemRegisteredSignal;
    UnknownSignal UIInputMethodChangedSignal;
    UnknownSignal ConnectedSignal;
    uint16_t TargetGameObjectFlags;
    ecs::EntityRef TargetGameObject;
    void* TranslatedStringKeyManager;
    void* SpellPrototypeManager;
    void* ImmutableDataHeadmaster;
    void* ActionResourceTypes;
    input::InputManager* InputManager;
    void* ProgressionManager;
    void* ProgressionDescriptionManager;
    void* BoostPrototypeManager;
    void* ExternalCommunityAPI;
    void* PlayerManager;
    void* qword2C0;
    BrushManager* BrushManager;
    NoesisUIManager* NoesisUIManager;
    StateMachine* StateMachine;
    DragAndDropManager* DragAndDropManager;
    UnknownFunction field_2e8;
    HashMap<PlayerId, UnknownFunction> MHM_u16_SignalConnectionCollection;
    UnknownSignalSubscriber field_360[27];
    HashSet<EntityHandle> SummonsUpdated_MHS_EH;
    HashSet<EntityHandle> CharacterActivated_MHS_EH;
    HashSet<EntityHandle> field_C38_MHS_EH;
    HashSet<EntityHandle> ApprovalRatingChanged_MHS_EH;
    HashSet<EntityHandle> ShortRestAvailabilityChanged_MHS_EH;
    HashMap<EntityHandle, EntityHandle> UnregisterInventoryContainer_MHM_EH_EH;
    HashMap<PlayerId, EntityHandle> TargetGameObjects_MHM_short_EH;
    HashMap<EntityHandle, HashSet<ComponentHandle>> StatusUpdated_MHM_EH_HashSet_OH;
    HashMap<EntityHandle, HashSet<ComponentHandle>> StatusApplied_MHM_EH_HashSet_OH;
    HashMap<EntityHandle, HashSet<ComponentHandle>> StatusRemoved_MHM_EH_HashSet_OH;
    HashMap<EntityHandle, HashSet<ComponentHandle>> UpdatedStatusLifetimes_MHM_EH_HashSet_OH;
    HashMap<EntityHandle, HashSet<ComponentHandle>> UpdatedAddedStatuses_MHM_EH_HashSet_OH;
    HashMap<EntityHandle, HashSet<ComponentHandle>> UpdatedRemovedStatuses_MHM_EH_HashSet_OH;
    HashMap<PlayerId, void*> DelayDialogueRequestSwitchCharacter_MHM_short_CharacterSwapDelayData;
    FlagHandler DirtyFlags;
    uint64_t qwordF88;
    uint8_t Flags;
    uint8_t Flags2;
    HashMap<UserId, float> RemotePlayerFloats_MHM_u32_float;
    HashSet<UserId> RemotePlayerUserIds_MHS_u32;
    HashMap<PlayerId, uint8_t> DeferredSetContainerState_MHM_short_EContainerState;
    HashMap<EntityHandle, uint8_t> LevelLoad_MHM_EH_u8;
    HashMap<PlayerId, bool> GameMenuOpen_MHM_short_bool;
    HashMap<PlayerId, void*> CursorInfo_MHM_short_eclTaskInfo;
    HashSet<PlayerId> CursorInfo2_MHS_short;
    HashSet<PlayerId> PreviewTaskChanged_MHS_short;
    struct ecs::EntityWorld* EntityWorld;
    void* Thoth;
    Array<UnknownFunction> SystemUpdate_Arr_Function;
    HashSet<EntityHandle> OwnedCharacterActivated_MHS_EH;
    HashMap<EntityHandle, HashSet<SpellId>> field_11B8_MHM_EH_HashSet_SpellId;
    uint32_t GameStateID;
    void* ShroudTexture;
    int8_t RequestSetControllerType;
    HashMap<PlayerId, Array<EntityHandle>> DelayedShowActiveSearch_MHM_short_Arr_EH;
    Array<EntityHandle> DelayedInventoryRefresh;
    Array<void*> TimelineEvents_Arr_TTimelineEvent;
    HashMap<PlayerId, Array<EntityHandle>> ActiveRollOpen_MHM_short_MHS_EH;
    Array<EntityHandle> SetActiveDialogue;
    DeferredPredicatesManager* DeferredPredicatesManager;
    Array<PlayerId> ShroudReload;
};

struct ContextMenuManagerBase : public ProtectedGameObject<ContextMenuManagerBase>
{
    void* VMT;
    HashMap<PlayerId, void*> Menus_MHM_u16_PlayerContextMenu;
};

struct ContextMenuManager : public ContextMenuManagerBase
{
    TooltipManager* TooltipManager;
    MPMCQueueBounded<PlayerId> OPOCCircularQueue_short;
};

struct DataContextProviderBase : public ProtectedGameObject<DataContextProviderBase>
{
    void* VMT;
    DCWidget* GlobalDataContext;
    LegacyRefMap<uint16_t, HashMap<FixedString, Noesis::Ptr<DCWidget>*>> CustomDC_RefMap_u16_MHM_FS_Ptr_DCWidget;
    StateMachine* StateMachine;
    ViewModelProvider* ViewModelProvider;
    Noesis::BaseCollection* DataContextToInit;
    Noesis::BaseCollection* DataContextToInitFromUI;
    Noesis::BaseCollection* DataContextToRelease;
    Noesis::BaseCollection* qword48;
    CRITICAL_SECTION CS_;
};


struct DataContextProvider : public DataContextProviderBase
{
    DragAndDropManager* DragAndDropManager;
    DCWidget* GlobalDataContext2;
    BrushManager* BrushManager;
    ModManager* ModManager;
    HashMap<FixedString, Noesis::Ptr<Noesis::BaseCollection>> qword90_MHM_FS_Ptr_Collection_DCWidget;
    MPMCQueueBounded<FixedString> InitDataContexts_OPOCCircularQueue_FS;
    uint8_t byteF0;
};

struct DragAndDropManager : public ProtectedGameObject<DragAndDropManager>
{
    UnknownSignal qword0;
    NoesisUIManager* NoesisUIManager;
    ViewModelProvider* ViewModelProvider;
    void* eclDragDropManager;
    void* qword30;
    void* qword38;
    void* qword40;
    void* qword48;
    void* qword50;
    Array<PlayerId> qword58;
    Array<PlayerId> qword68;
    bool CanExecuteDragEvents;
    bool NeedsUpdate;
    uint8_t byte7A;
    MPMCQueueBounded<PlayerId> field_80_OPOCCircularQueue_i16;
    HashMap<PlayerId, uint64_t> field_a0_MHM_u16_u64;
    HashMap<PlayerId, uint64_t> field_e0_MHM_u16_u64;
    Array<void*> DelayedDragEvents_Arr_TDelayedDragEvent[4];
    MPMCQueueBounded<void*> field_160_Queue_TStartDraggingRequest;
};

struct TooltipManager : public ProtectedGameObject<TooltipManager>
{
    void* VMT;
    UnknownFunction SignalCollection;
    void* qword40;
    NoesisUIManager* NoesisUIManager;
    ViewModelProvider* ViewModelProvider;
    DragAndDropManager* DragAndDropManager;
    StateMachine* StateMachine;
    Array<void*> field_70_MHM_i16_pPlayerTooltipData;
    uint16_t wordB0;
    HashMap<PlayerId, void*> field_b8_MHM_i16_DelayedTooltipData;
    HashSet<PlayerId> field_f8_MHS_i16;
    HashSet<PlayerId> field_128_HashSet_u16;
    void* qword158;
    MPMCQueueBounded<std::pair<short, bool>> field_160_Queue_Pair_short_bool;
    MPMCQueueBounded<std::pair<short, bool>> field_180_Queue_Pair_short_bool;
    bool Initialized;
    bool field_1A1;
    bool byte1A2;
    MPMCQueueBounded<PlayerId> field_1A8_Queue_i16;
    MPMCQueueBounded<PlayerId> field_1c8_Queue_i16;
    MPMCQueueBounded<PlayerId> field_1e8_Queue_i16;
    MPMCQueueBounded<PlayerId> field_208_Queue_i16;
    MPMCQueueBounded<std::pair<short, bool>> field_228_Queue_Pair_short_bool;
};

struct MessageBoxManager : public ProtectedGameObject<MessageBoxManager>
{
    void* VMT;
    void* VMT2;
    BYTE word10;
    char field_11;
    TranslatedString field_14;
    int field_24;
    Noesis::BaseCollection* field_28;
    void* MessageBoxDC;
    Noesis::BaseComponent* MessageBox;
    Noesis::BaseRefCounted* field_40;
    uint8_t gap48[32];
    StateMachine* StateMachine;
    CommandProcessor* CommandProcessor;
    DataContextProvider* DataContextProvider;
    ViewModelProvider* ViewModelProvider;
    Array<void*> field_88_Arr_PendingMessageBox;
};

struct UINotificationManager : public ProtectedGameObject<UINotificationManager>
{
    void* VMT;
    UnknownFunction SignalCollection;
    void* SoundManager;
    void* BrushManager;
    ViewModelProvider* ViewModelProvider;
    void* PlayerManager;
    void* TutorialManager;
    void* TranslatedStringKeyManager;
    void* BackgroundManager;
    void* BackgroundGoals;
    void* ExperienceRewards;
    void* SpellPrototypeManager;
    HashSet<EntityHandle> field_98;
};

struct TwitchOverlayManager : public ProtectedGameObject<TwitchOverlayManager>
{
    uint64_t RequestTime;
    void* qword8;
    uint16_t word10;
    __int64 field_18;
    __int64 field_20;
    __int64 field_28;
    __int64 field_30;
    __int64 field_38;
    __int64 field_40;
    STDString ls__stdstring48;
    ViewModelProvider* ViewModelProvider;
    void* JsonConverter;
    HashSet<uint64_t> field_70_MHS_u64;
    void* qwordA0;
    uint64_t RequestTimeDeadline;
    STDString TwitchExtBaseURL;
    STDString TwitchExtToken;
    STDString TwitchExtSecret;
    CRITICAL_SECTION CS_;
    int net__defaulthttprequestmanager120;
    EntityHandle qword128;
    EntityHandle HttpRequest;
};

struct TutorialManager : public ProtectedGameObject<TutorialManager>
{
    ViewModelProvider* ViewModelProvider;
    StateMachine* StateMachine;
    UnknownFunction SignalCollection;
    Array<PlayerId> field_50_Arr_i16;
    Array<void*> field_60_Arr_HideRequest;
    Array<void*> field_70_Arr_ShowRequest;
    uint8_t byte80;
    Array<void*> field_88_Arr_ActiveTutorial;
};


struct GameUI : public ProtectedGameObject<GameUI>
{
    void* VMT;
    void* InputEventListenerVMT;
    void* PlayerEventListenerVMT;
// PORT (linux): upstream's `uint64_t field_18;` and `uint64_t ls__eventlistenerbase28;` are
// not in the native build -- two Win32 slots, the same kind ecl::TimelineSystem and
// esv::combat::System each carried one of. The
// macOS oracle's constructor, __ZN3gui6GameUIC2Ev (0x102a384e0, build af3f1c71), writes
// *five* vtable pointers -- [rdi], +8, +0x10, +0x18, +0x20 -- and only then starts zeroing
// data, at [r14+0x28]; the class's own thunks say the same from the symbol table alone
// (__ZThn8_, __ZThn16_, __ZThn24_ and __ZThn32_N3gui6GameUID{0,1}Ev: secondary bases at 8,
// 0x10, 0x18 and 0x20, contiguous, one vptr each). Itanium there and here alike, so the
// five listener VMTs are contiguous and the first data member is at 0x28.
//
// Upstream's header is therefore 0x10 too long and everything from SplitScreenChangedSignal on
// sits 0x10 high; the three static_asserts past the closing brace carry the reading.
    void* GameStateEventListenerVMT;
    void* GameEventManagerListenerVMT;
    UnknownSignal SplitScreenChangedSignal;
    GameUIParams GameUIParams;
    NoesisUIManager NoesisUIManager;
    BrushManager BrushManager;
    alignas(64) uint16_t field_380;
    MPMCQueueBounded<void*> CommandQueue;
    ModManager ModManager;
    __int64 field_3B0;
    __int64 field_3B8;
    WindowManager WindowManager;
    StateMachine StateMachine;
    ViewModelProvider ViewModelProvider;
    ContextMenuManager ContextMenuManager;
    DataContextProvider DataContextProvider;
    DragAndDropManager DragAndDropManager;
    TooltipManager TooltipManager;
    MessageBoxManager MessageBoxManager;
    UINotificationManager NotificationManager;
    TwitchOverlayManager TwitchOverlayManager;
    TutorialManager TutorialManager;
    DeferredPropertiesManager* DeferredPropertiesManager;
    __int64 field_22C0;
    void* SplitscreenRequestSystem;
    int field_22D0;
    int field_22D4;
    void* TextureProvider;
    __int64 field_22E0;
    __int64 field_22E8;
    __int64 field_22F0;
    __int64 field_22F8;
    Array<void*> field_2300_OpenTradeRequest;
    Array<void*> field_2310_OpenActiveRollRequest;
    Array<void*> field_2320_OpenRewardRequest;
    Array<void*> field_2330_OpenMakeCampRequest;
    HashMap<PlayerId, void*> field_2340_MHM_i16_ShowDialogueUIRequest;
    HashSet<PlayerId> field_2380_MHS_i16;
    Array<void*> field_23b0_TimelineTransitionRequest;
    Array<EntityHandle> field_23c0_CharacterCreationPlayers;
    HashMap<PlayerId, HashSet<EntityHandle>> field_23d0_MHM_u16_MHS_EH;
    HashSet<PlayerId> field_2410_i16;
    UnknownFunction field_2440_SignalConnectionCollection;
    int64_t SetControllerModeSubscriberId;
    int64_t OnWidgetFinishedLoadingSubscriberId;
    HashMap<PlayerId, uint64_t> field_2490_u16_u64;
    uint32_t DeviceType2;
    uint8_t ControllerType;
    uint8_t gap24D5[3];
    uint32_t DeviceType;
    uint8_t InitState;
    uint8_t Flags_byte24DD;
    uint8_t byte24DE;
    uint8_t dword24DF;
    uint8_t field_24E0;
    uint8_t field_24E1;
    uint8_t DisableUpdates;
    uint8_t byte24E3;
    void* GameControl;
    void* PlayerManager;
    uint32_t dword24F8;
    uint8_t ReloadState;
    HashSet<EntityHandle> field_2500_EH_Timelines;
    HashSet<uint32_t> field_2530_u32;
    void* NoesisGUI;
    uint64_t field_2E20;
    uint32_t field_2E28;
    uint32_t field_2E2C;
    const char* field_2E30;
    const char* field_2580;
    uint64_t field_2E40;
    uint32_t field_2E48;
    const char* field_2E50;
    const char* field_2E58;
    const char* field_2E60;
    uint64_t field_2E68;
    uint32_t field_2E70;
    const char* field_2e78;
    const char* field_2e80;
    const char* field_2e88;
    const char* field_2e90;
    const char* field_2e98;
    const char* field_2ea0;
    HashMap<uint32_t, void*> field_25f0_MHM_u32_pUiSystem[4];
    Array<UIEvent> QueuedEvents;
    Array<void*> field_2700_TDelayedOpenCombine;
    Array<input::FireEventDesc> QueuedFireEvents;
    Array<input::InputEvent> QueuedInputEvents;
    CRITICAL_SECTION EventsCS_;
    CRITICAL_SECTION UpdateCS_;
    CRITICAL_SECTION RenderCS_;
    CRITICAL_SECTION RenderOffscreenWithoutUpdatingCS_;
    Array<PlayerId> AddLocalPlayers;
    Array<PlayerId> InitializeLocalPlayers;
    HashMap<PlayerId, void*> field_27f0_MHM_short_OpenActiveRollWidgetEventDelayedData;
    UnsafeMessageQueue<void*> qword2840_UnsafeMessageQueue_WidgetClosedData;
    double GameTime;
    float ScaledTickDelta;
    float TickDelta;
    uint32_t Ticks;
    uint32_t gap28D4;
    uint32_t dword28D8;
    uint32_t gap28DC;
    Array<void*> qword28E0_Arr_EGameStateID;
    int RenderOffscreenRetries;
    int LastTicks;
};

// PORT (linux): `gui::GameUI` measured on the Linux binary.  Every
// witness below is one instruction of bin/bg3 0x2472930
// (`gui::GameUI::IsUIModal(ls::EInputPlayerIndex) const`, matched from the macOS oracle's
// __ZNK3gui6GameUI9IsUIModalEN2ls17EInputPlayerIndexE 0x1029a9f60, r3:2), where `this`
// arrives in rdi and is copied to r14 at 0x24729b8.  Absolute offsets transfer because the
// witness is the Linux build itself -- the "oracle's absolute offsets stop at the first
// lock" caveat does not apply, and it would bite hard here: WindowManager alone holds
// three 48-byte ls::CriticalSection.
//
// The middle assert is the one that matters and the reason these exist at all.  The two
// deleted Win32 slots moved `alignas(64) uint16_t field_380` from 0x380 to 0x340 -- a 0x40
// shift, not 0x10, because the alignment rounds up from where BrushManager *ends* (0x350
// rounds to 0x380; 0x340 is already aligned and stays).  The first reading predicted
// the opposite and could not check it.  Everything after that boundary therefore sits 0x40
// below the Windows offset its field name carries, and `Ext.UI.GetStateMachine()` reads
// exactly there (`Lua/Libs/ClientUI/Module.inl`, `UI->StateMachine.StateMachineComponent`).
// The game agrees with the shifted layout: 0x818, not 0x858.
static_assert(offsetof(GameUI, GameUIParams) + offsetof(GameUIParams, IsNuWT) == 0x74,
    "bin/bg3 0x2472951: cmp byte ptr [rdi + 0x74], 0");
static_assert(offsetof(GameUI, StateMachine) + offsetof(StateMachine, StateMachineComponent) == 0x818,
    "bin/bg3 0x24729df: mov rsi, qword ptr [r14 + 0x818]");
static_assert(offsetof(GameUI, ViewModelProvider) + offsetof(ViewModelProviderBase, VMGameData) == 0x888,
    "bin/bg3 0x24729be: mov rdi, qword ptr [rdi + 0x888]");


struct UICanvas : public Noesis::Panel
{
    __int64 field_2A0;
    __int64 field_2A8;
    __int64 field_2B0;
    __int64 field_2B8;
    __int64 field_2C0;
    __int64 field_2C8;
    __int64 field_2D0;
    NoesisUIManager* UIManager70;
    __int64 ElementCacheManager;
    DependencyObject* field_2E8;
    float field_2F0;
    float field_2F4;
};

struct DeferredCommand : public BaseCommand
{
    __int64 field_38;
    __int64 VMT3;
    __int64 field_48;
    __int64 field_50;
    __int64 field_58;
    __int64 field_60;
    __int64 field_68;
    __int64 field_70;
    __int64 field_78;
    __int64 VMT4;
};


struct ViewModel : public Noesis::BaseComponent, public INotifyPropertyChanged
{
    Noesis::Delegate<void ()> UnknownDelegate;
};

struct DCWidget : public ViewModel
{
    CommandProcessor CommandProcessor;
    DeferredProperty<Noesis::String> NameProperty;
    DeferredProperty<uint8_t> LayoutProperty;
    DeferredProperty<bool> IsInitializedProperty;
    UnknownSignal field_E8;
    StateMachine* StateMachine;
    PlayerId PlayerId;
    uint8_t Owner;
    DeferredCommand* DeferredCommand1;
    DeferredCommand* DeferredCommand2;
    Noesis::Delegate<void ()> NotifyWidgetClosing;
    ViewModelProvider* ViewModelProvider;
};

struct CustomPropertyDefn
{
    FixedString Name;
    FixedString Type;
    bool Notify{ true };
    lua::PersistentRegistryEntry WriteCallback;
};


// PORT (linux): UIWidget mirrors the game's `ui::Widget` -- upstream's name is invented, and the
// class's real one is what makes it measurable at all: the macOS oracle keeps
// `__ZN2ui6WidgetC1Ev` (0x1040764d0) and the matcher carries it onto **bin/bg3 0x3cc3400**
// (dumps/match/a187af943be2_af3f1c711ba2, s2:0.82). So this is the rare Noesis-derived mirror
// whose witness is the Linux binary itself and the offsets below are absolute, not
// lock-relative. The constructor, in order:
//   - calls Noesis::ContentControl::ContentControl() (0x36ad6c0) and then stores its own three
//     vptrs at +0x0, +0x10 and +0x1a0 -- +0x1a0 is Noesis::FrameworkElement's IUITreeNode
//     secondary base, so the base chain here is the SDK's own. UserControl adds no members over
//     ContentControl (both sizeof 0x270), so whether LTO ate a UserControl ctor is moot.
//   - `lea r14, [rbx+0x270]`, zeroes 0x270/0x280/0x290 (three movups, 0x270..0x2a0), then
//     pthread_mutexattr_init / _settype(RECURSIVE) / _setprotocol(PRIO_INHERIT) /
//     pthread_mutex_init(r14, attr) / _destroy, and finally `mov dword [rbx+0x29c], 0xfa0`.
//     That is ls::CriticalSection at 0x270 with its spin count (4000) at +0x2c, so the lock is
//     the game's 48 bytes and it ends at 0x2a0 -- i.e. CS_ sits at the base's end with no
//     tail-padding reuse (ContentControl's last member is a bool at 0x268, and an 8-aligned
//     member cannot go in the 7 bytes after it).
//   - zeroes 0x2a0 and 0x2b0 (the two Array<void*>), sets qword2C0 to `this+0x2d0` with a 1 in
//     the top byte of qword2C0[1], zeroes 0x2d0..0x300 (the six pointers from
//     ContextMenuManager to WidgetData) and 0x300 (InputManager).
//   - `movaps xmm0, [ls::Guid::s_Empty]` then `movups [rbx+0x308], xmm0`: StateId is a Guid at
//     0x308, which is the strongest single reading in the function.
//   - zeroes 0x318..0x368, `mov dword [rbx+0x384], 3`, `mov word [rbx+0x388], 0`,
//     `and byte [rbx+0x38a], 0xf0`, `mov word [rbx+0x38c], 0` -- MoveFocusInputState,
//     Flags/Flags2, Flags3, and PlayerId at 0x38c. The store is a *word*, which agrees with
//     `using PlayerId = int16_t` (ExposedTypes.h:7); that is a typedef, so there is no assert
//     on its width -- one would be vacuous.
//   - its unwind path releases 0x358, 0x350, 0x348, 0x340, 0x338 in that order (qword360,
//     FocusVisual, GetRequestedFocusFallback, FocusRefresh, ContextMenu -- reverse declaration
//     order), then tests `byte [rbx+0x337]` before freeing `qword [rbx+0x328]`, which is
//     libc++'s 16-byte short-string flag and data pointer: FileName is at 0x328.
// Every offset in this mirror is therefore what bin/bg3 has.
// Asserts only where the constructor names the offset; the members inside a zeroed run
// (ContextMenuManager..WidgetData, ParentCanvas..FocusLeftEvent) are witnessed as a region and
// get none of their own.
static_assert(offsetof(UIWidget, CS_) == 0x270,
    "pthread_mutex_init(this+0x270) in ui::Widget::Widget (bin/bg3 0x3cc3400)");
static_assert(offsetof(UIWidget, field_2A0_InputChildInfo) == 0x2a0,
    "the movups pair at 0x2a0/0x2b0 begins where the 48-byte lock ends (bin/bg3 0x3cc3400)");
static_assert(offsetof(UIWidget, field_2B0_InputChildChange) == 0x2b0,
    "the second movups of the pair at 0x2a0/0x2b0 (bin/bg3 0x3cc3400)");
static_assert(offsetof(UIWidget, qword2C0) == 0x2c0,
    "qword2C0 = this+0x2d0 (bin/bg3 0x3cc3400)");
static_assert(offsetof(UIWidget, StateId) == 0x308,
    "movups [this+0x308], ls::Guid::s_Empty (bin/bg3 0x3cc3400)");
static_assert(offsetof(UIWidget, FileName) == 0x328,
    "the dtor path tests byte [this+0x337] and frees qword [this+0x328] (bin/bg3 0x3cc3400)");
static_assert(offsetof(UIWidget, ContextMenu) == 0x338,
    "the last of the five releases at 0x358/0x350/0x348/0x340/0x338 (bin/bg3 0x3cc3400)");
static_assert(offsetof(UIWidget, MoveFocusInputState) == 0x384,
    "mov dword [this+0x384], 3 (bin/bg3 0x3cc3400)");
static_assert(offsetof(UIWidget, Flags) == 0x388,
    "mov word [this+0x388], 0 (bin/bg3 0x3cc3400)");
static_assert(offsetof(UIWidget, Flags3) == 0x38a,
    "and byte [this+0x38a], 0xf0 (bin/bg3 0x3cc3400)");
static_assert(offsetof(UIWidget, PlayerId) == 0x38c,
    "mov word [this+0x38c], 0 (bin/bg3 0x3cc3400)");

END_NS()


BEGIN_NS(lua)

#define FOR_EACH_NOESIS_TYPE() \
    FOR_NOESIS_TYPE(Noesis::BaseObject) \
    FOR_NOESIS_TYPE(Noesis::BaseRefCounted) \
    FOR_NOESIS_TYPE(Noesis::BaseComponent) \
    FOR_NOESIS_TYPE(Noesis::Type) \
    FOR_NOESIS_TYPE(Noesis::TypeMeta) \
    FOR_NOESIS_TYPE(Noesis::TypeMetaData) \
    FOR_NOESIS_TYPE(Noesis::TypeClass) \
    FOR_NOESIS_TYPE(Noesis::BaseCommand) \
    FOR_NOESIS_TYPE(Noesis::LuaDelegateCommand) \
    FOR_NOESIS_TYPE(Noesis::DispatcherObject) \
    FOR_NOESIS_TYPE(Noesis::DependencyObject) \
    FOR_NOESIS_TYPE(Noesis::DependencyData) \
    FOR_NOESIS_TYPE(Noesis::Visual) \
    FOR_NOESIS_TYPE(Noesis::UIElement) \
    FOR_NOESIS_TYPE(Noesis::UIElementData) \
    FOR_NOESIS_TYPE(Noesis::FrameworkElement) \
    FOR_NOESIS_TYPE(Noesis::Panel) \
    FOR_NOESIS_TYPE(ui::UIInitialSubstate) \
    FOR_NOESIS_TYPE(ui::UIStateEvent) \
    FOR_NOESIS_TYPE(ui::UIStateWidget) \
    FOR_NOESIS_TYPE(ui::UIState) \
    FOR_NOESIS_TYPE(ui::UIWidget) \
    FOR_NOESIS_TYPE(ui::DCWidget)

#define FOR_NOESIS_TYPE(c) LUA_POLYMORPHIC(c)
FOR_EACH_NOESIS_TYPE()
#undef FOR_NOESIS_TYPE

LUA_POLYMORPHIC(Noesis::RoutedEventArgs)

END_NS()
