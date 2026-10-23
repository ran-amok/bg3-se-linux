#include <GameDefinitions/UI.h>
#include <NsGui/UIElementEvents.h>
#include <NsGui/Uri.h>
#include <NsDrawing/Color.h>
#include <NsMath/Vector.h>
#include <Lua/Libs/ClientUI/ClassCache.h>
#include <Lua/Libs/ClientUI/Symbols.inl>
#include <Lua/Libs/ClientUI/ClassCache.inl>

BEGIN_BARE_NS(Noesis)

namespace Boxing
{
    void* BoxingAllocate(size_t size)
    {
        return bg3se::GameAllocRaw(size);
    }

    void BoxingDeallocate(void* ptr, size_t size)
    {
        bg3se::GameFree(ptr);
    }

    Boxed<bool>* gTrueBoxed{ nullptr };
    Boxed<bool>* gFalseBoxed{ nullptr };

    BoxedValue* TrueBoxed()
    {
        if (gTrueBoxed == nullptr) {
            gTrueBoxed = new Boxed<bool>(true);
        }

        return gTrueBoxed;
    }

    BoxedValue* FalseBoxed()
    {
        if (gFalseBoxed == nullptr) {
            gFalseBoxed = new Boxed<bool>(false);
        }

        return gFalseBoxed;
    }
}

namespace Impl
{
    String ToString(bool value)
    {
        return value ? "True" : "False";
    }
    
    String ToString(int8_t value)
    {
        return std::to_string(value).c_str();
    }
    
    String ToString(uint8_t value)
    {
        return std::to_string(value).c_str();
    }
    
    String ToString(int16_t value)
    {
        return std::to_string(value).c_str();
    }
    
    
    String ToString(uint16_t value)
    {
        return std::to_string(value).c_str();
    }
    
    String ToString(int32_t value)
    {
        return std::to_string(value).c_str();
    }
    
    String ToString(uint32_t value)
    {
        return std::to_string(value).c_str();
    }

    String ToString(int64_t value)
    {
        return std::to_string(value).c_str();
    }

    String ToString(uint64_t value)
    {
        return std::to_string(value).c_str();
    }

    String ToString(float value)
    {
        return std::to_string(value).c_str();
    }

    String ToString(double value)
    {
        return std::to_string(value).c_str();
    }

    String ToString(String const& value)
    {
        return value;
    }
}

String Vector2::ToString() const
{
    char s[100];
    sprintf_s(s, "(%f,%f)", x, y);
    return s;
}

String Vector3::ToString() const
{
    char s[100];
    sprintf_s(s, "(%f,%f,%f)", x, y, z);
    return s;
}

String Point::ToString() const
{
    char s[100];
    sprintf_s(s, "%g,%g", x, y);
    return s;
}

String Color::ToString() const
{
    auto col = ((uint32_t)(r * 255.0f + 0.5f) << 24)
        | ((uint32_t)(g * 255.0f + 0.5f) << 16)
        | ((uint32_t)(b * 255.0f + 0.5f) << 8)
        | ((uint32_t)(a * 255.0f + 0.5f) << 0);

    char s[100];
    sprintf_s(s, "#%08x", col);
    return s;
}

String Rect::ToString() const
{
    char s[100];
    sprintf_s(s, "%g,%g,%g,%g", x, y, width, height);
    return s;
}

void* Cast(const TypeClass* type, BaseObject* obj)
{
    if (obj == nullptr) {
        return nullptr;
    }

    auto cls = obj->GetClassType();
    if (cls == type) {
        return obj;
    }

    const TypeClass::AncestorInfo* ancestor{ nullptr };
    if (cls->IsDescendantOf(type, &ancestor)) {
        return (void*)((uintptr_t)obj + ancestor->offset);
    }

    return nullptr;
}

void* BaseObject::operator new(unsigned __int64 sz)
{
    return GameAllocRaw(sz);
}

void* BaseObject::operator new(unsigned __int64 sz, void* ptr)
{
    return ptr;
}

void BaseObject::operator delete(void* ptr)
{
    GameFree(ptr);
}

void BaseObject::operator delete(void* ptr, void* place)
{
}

BaseObject::BaseObject() {}
BaseObject::~BaseObject() {}

const TypeClass* BaseObject::GetClassType() const
{
    return gStaticSymbols.TypeClasses.BaseObject.Type;
}

String BaseObject::ToString() const
{
    return GetClassType()->GetName();
}

bool BaseObject::Equals(const BaseObject* left, const BaseObject* right)
{
    return left == right
        || left->Equals(right);
}

bool BaseObject::Equals(const BaseObject* o) const
{
    return this == o;
}

BaseComponent::BaseComponent() {}
BaseComponent::~BaseComponent() {}

const TypeClass* BaseComponent::StaticGetClassType(TypeTag<BaseComponent>*)
{
    return gStaticSymbols.TypeClasses.BaseComponent.Type;
}

const TypeClass* BaseComponent::GetClassType() const
{
    return gStaticSymbols.TypeClasses.BaseComponent.Type;
}

const TypeClass* BaseRefCounted::GetClassType() const
{
    return gStaticSymbols.TypeClasses.BaseRefCounted.Type;
}


Type::Type(Symbol name)
    : mName(name)
{}

const TypeClass* Type::GetClassType() const
{
    return gStaticSymbols.TypeClasses.Type.Type;
}

bool Type::IsAssignableFrom(const Type* type) const
{
    return type == this;
}


TypeMeta::TypeMeta(Symbol name)
    : Type(name)
{}

TypeMeta::~TypeMeta()
{}


TypePtr::TypePtr(Symbol name)
    : TypeMeta(name)
{}

const TypeClass* TypePtr::GetClassType() const
{
    return gStaticSymbols.TypeClasses.TypePtr.Type;
}

void TypePtr::SetStaticContentType(Type const* t)
{
    mContentType = t;
}


TypePointer::TypePointer(Symbol name)
    : TypeMeta(name)
{}

const TypeClass* TypePointer::GetClassType() const
{
    return gStaticSymbols.TypeClasses.TypePointer.Type;
}

void TypePointer::SetStaticContentType(Type const* t)
{
    mContentType = t;
}


// PORT (linux): this is a mirror of a *game* structure -- Noesis's private
// TypeClass::Ancestors -- and the game locks it, so the lock has to be the game's kind and
// has to start out unlocked.  Both witnesses are in bin/bg3 a187af94:
//
//   Noesis::TypeClass::TypeClass (0x3e87a90) builds it:  operator new(0x20) at 0x3e87aea,
//   byte [p] = 0 (Collapsed), [p+8] = p+0x18 and [p+0x10] = 0x0100000000000000 (the
//   Vector's mBegin / mSize+mCapacity+mIsSmall), then pthread_spin_init(p+0x18, 0) at
//   0x3e87b03 (plt 0x77e9240), and stores p into TypeClass+0x60.
//
//   Noesis::TypeClass::CollapseAncestors (0x5319fe0) takes it:  early-out on mBase
//   (TypeClass+0x28) and mInterfaces.mSize (+0x38), then `cmp byte ptr [mAncestors], 0`
//   at 0x531a005, then pthread_spin_lock(mAncestors + 0x18) at 0x531a020 (plt 0x77e9250).
//
// So the lock is a 4-byte pthread_spinlock_t at +0x18, where upstream has an 8-byte Win32
// SRWLOCK -- same offset, and the sizes agree once the tail padding is counted, which is why
// nothing about this mirror ever looked wrong.  What is wrong is the *value*: unlocked is 1
// on glibc, not 0 (shim/windows.h has that measurement), and InitializeSRWLock here zeroes
// its 8 bytes.  Every type the extender registered therefore handed the game a permanently
// held spinlock, and the first game-side Cast of one of our objects spun forever on the
// GameThread -- which is a UI mod hanging the game at the main menu.
struct TypeClassAncestors
{
    TypeClassAncestors()
    {
#if defined(BG3SE_LINUX)
        InitializeNoesisSpinLock(&Lock);
#else
        InitializeSRWLock(&Lock);
#endif
    }

    bool Collapsed{ false };
    Vector<TypeClass::AncestorInfo> Ancestors;
#if defined(BG3SE_LINUX)
    NS_SPINLOCK Lock;
#else
    SRWLOCK Lock;
#endif
};

#if defined(BG3SE_LINUX)
// pthread_spin_init at bin/bg3 0x3e87b03 on mAncestors + 0x18, pthread_spin_lock at
// 0x531a020 on the same word; the block is operator new(0x20).
static_assert(offsetof(TypeClassAncestors, Lock) == 0x18,
    "Noesis's TypeClass::Ancestors has its spinlock at +0x18 (bin/bg3 0x3e87b03, 0x531a020)");
static_assert(sizeof(TypeClassAncestors) == 0x20,
    "Noesis's TypeClass::Ancestors is 32 bytes (bin/bg3 0x3e87aea, operator new(0x20))");
#endif

const TypeClass* TypeClass::StaticGetClassType(TypeTag<TypeClass>*)
{
    return gStaticSymbols.TypeClasses.TypeClass.Type;
}

const TypeClass* TypeMeta::GetClassType() const
{
    return gStaticSymbols.TypeClasses.TypeMeta.Type;
}

TypeMetaData* TypeMeta::FindMeta(const TypeClass* metaDataType) const
{
    for (auto const& meta : mMetaData) {
        auto m = meta->GetClassType();
        if (m == metaDataType) {
            return meta;
        }
    }

    return nullptr;
}


TypeProperty::TypeProperty(Symbol name, const Type* type)
    : mName(name),
    mType(type)
{}

TypeProperty::~TypeProperty()
{}

bool TypeProperty::IsReadOnly() const
{
    return true;
}

Ptr<BaseComponent> TypeProperty::GetComponent(const void* ptr) const
{
    throw std::runtime_error("Not implemented");
}

void TypeProperty::SetComponent(void* ptr, BaseComponent* value) const
{
    throw std::runtime_error("Not implemented");
}

void TypeProperty::GetCopy(const void* ptr, void* dest) const
{
    throw std::runtime_error("Not implemented");
}

const void* TypeProperty::Get(const void* ptr) const
{
    throw std::runtime_error("Not implemented");
}

void TypeProperty::Set(void* ptr, const void* value) const
{
    throw std::runtime_error("Not implemented");
}


TypeClass::TypeClass(Symbol name, bool isInterface)
    : TypeMeta(name),
    mIsInterface(isInterface),
    mBase(nullptr),
    mAncestors(nullptr)
{
    mAncestors = (TypeClass::Ancestors*)GameAlloc<TypeClassAncestors>();
}

TypeClass::~TypeClass()
{}

const TypeClass* TypeClass::GetClassType() const
{
    return gStaticSymbols.TypeClasses.TypeClass.Type;
}

bool TypeClass::IsAssignableFrom(const Type* type) const
{
    if (type == this) return true;

    auto cls = DynamicCast<TypeClass const*>(type);
    return cls != nullptr
        && cls->IsDescendantOf(this, nullptr);
}

bool TypeClass::IsDescendantOf(const TypeClass* ancestorType,
    const AncestorInfo** ancestorInfo) const
{
    auto ancestors = (TypeClassAncestors*)mAncestors;
    if (ancestors->Collapsed) {
        for (auto& ancestor : ancestors->Ancestors) {
            if (ancestor.type == ancestorType) {
                if (ancestorInfo) {
                    *ancestorInfo = &ancestor;
                }

                return true;
            }
        }
    }

    return false;
}

void TypeClassBuilder::AddBase(const TypeClass* baseType)
{
    mBase = baseType;
}

void TypeClassBuilder::AddInterface(const TypeClass* interfaceType, uint32_t offset)
{
    mInterfaces.PushBack(AncestorInfo(interfaceType, offset));
}

void TypeClassBuilder::AddProperty(TypeProperty* prop)
{
    auto insertAfter = mProperties.Begin();
    while (insertAfter != mProperties.End() && (*insertAfter)->mName < prop->mName) insertAfter++;

    mProperties.Insert(insertAfter, prop);
}


BaseCollection::BaseCollection()
{}

BaseCollection::~BaseCollection()
{}

void BaseCollection::InsertItem(uint32_t index, BaseComponent* item)
{
    mItems.Insert(mItems.Data() + index, Ptr<BaseComponent>(item));
}

void BaseCollection::SetItem(uint32_t index, BaseComponent* item)
{
    if (mItems[index] != item) {
        mItems[index].Reset(item);
    }
}

void BaseCollection::RemoveItem(uint32_t index)
{
    mItems.Erase(mItems.Data() + index);
}

void BaseCollection::ClearItems()
{
    mItems.Clear();
}

const TypeClass* BaseCollection::GetClassType() const
{
    return gStaticSymbols.TypeClasses.BaseCollection.Type;
}

const TypeClass* BaseCollection::GetItemType() const
{
    return BaseComponent::StaticGetClassType((TypeTag<BaseComponent>*)nullptr);
}

bool BaseCollection::CheckType(BaseComponent* item) const
{
    auto ty = GetItemType();
    auto itemTy = item->GetClassType();

    if (ty->IsAssignableFrom(itemTy)) {
        return true;
    }

    // TODO boxed types etc.

    ERR("Tried to add object of type '%s' to a collection of type '%s'", itemTy->GetName(), ty->GetName());
    return false;
}

void BaseCollection::Set(uint32_t index, BaseComponent* item)
{
    if (index < mItems.Size()) {
        if (mItems[index].GetPtr() != item) {
            SetItem(index, item);
        }
    } else {
        ERR("BaseCollection::Set(%d) out of bounds (size is %d)", index, mItems.Size());
    }
}

Ptr<BaseComponent> BaseCollection::GetComponent(uint32_t index) const
{
    if (index < mItems.Size()) {
        return mItems[index];
    } else {
        ERR("BaseCollection::GetComponent(%d) out of bounds (size is %d)", index, mItems.Size());
        return {};
    }
}

void BaseCollection::SetComponent(uint32_t index, BaseComponent* item)
{
    if (CheckType(item)) {
        Set(index, item);
    }
}

int BaseCollection::AddComponent(BaseComponent* item)
{
    if (CheckType(item)) {
        auto idx = mItems.Size();
        InsertItem(idx, item);
        return idx;
    } else {
        return -1;
    }
}

int BaseCollection::IndexOfComponent(const BaseComponent* item) const
{
    for (uint32_t i = 0; i < mItems.Size(); i++) {
        if (mItems[i].GetPtr() == item) {
            return (int32_t)i;
        }
    }

    return -1;
}

int BaseCollection::Count() const
{
    return (int)mItems.Size();
}


const TypeClass* BaseObservableCollection::StaticGetClassType(TypeTag<BaseObservableCollection>*)
{
    return gStaticSymbols.TypeClasses.BaseObservableCollection.Type;
}


BaseObservableCollection::BaseObservableCollection()
{}

BaseObservableCollection::~BaseObservableCollection()
{}

const TypeClass* BaseObservableCollection::GetClassType() const
{
    return gStaticSymbols.TypeClasses.BaseObservableCollection.Type;
}

NotifyCollectionChangedEventHandler& BaseObservableCollection::CollectionChanged()
{
    return mCollectionChanged;
}

PropertyChangedEventHandler& BaseObservableCollection::PropertyChanged()
{
    return mPropertyChanged;
}

void BaseObservableCollection::MoveItem(int oldIndex, int newIndex)
{
    auto oldItem = mItems[oldIndex];
    BaseCollection::RemoveItem(oldIndex);
    BaseCollection::InsertItem(newIndex, oldItem.GetPtr());
    // FIXME - NOTIFY Item[]
    mCollectionChanged(this, NotifyCollectionChangedEventArgs{
        .action = NotifyCollectionChangedAction_Move,
        .oldIndex = (int32_t)oldIndex,
        .newIndex = (int32_t)newIndex,
        .oldValue = oldItem.GetPtr(),
        .newValue = oldItem.GetPtr()
    });
}

void BaseObservableCollection::InsertItem(uint32_t index, BaseComponent* item)
{
    BaseCollection::InsertItem(index, item);
    // FIXME - NOTIFY Count, Item[]
    mCollectionChanged(this, NotifyCollectionChangedEventArgs{
        .action = NotifyCollectionChangedAction_Add,
        .oldIndex = -1,
        .newIndex = (int32_t)index,
        .oldValue = nullptr,
        .newValue = item
    });
}

void BaseObservableCollection::SetItem(uint32_t index, BaseComponent* item)
{
    auto oldItem = mItems[index];
    BaseCollection::SetItem(index, item);
    // FIXME - NOTIFY Item[]
    mCollectionChanged(this, NotifyCollectionChangedEventArgs{
        .action = NotifyCollectionChangedAction_Replace,
        .oldIndex = (int32_t)index,
        .newIndex = (int32_t)index,
        .oldValue = oldItem.GetPtr(),
        .newValue = item
    });
}

void BaseObservableCollection::RemoveItem(uint32_t index)
{
    auto item = mItems[index];
    BaseCollection::RemoveItem(index);
    // FIXME - NOTIFY Count, Item[]
    mCollectionChanged(this, NotifyCollectionChangedEventArgs{
        .action = NotifyCollectionChangedAction_Remove,
        .oldIndex = (int32_t)index,
        .newIndex = -1,
        .oldValue = item.GetPtr(),
        .newValue = nullptr
    });
}

void BaseObservableCollection::ClearItems()
{
    mCollectionChanged(this, NotifyCollectionChangedEventArgs{
        .action = NotifyCollectionChangedAction_PreReset,
        .oldIndex = -1,
        .newIndex = -1,
        .oldValue = nullptr,
        .newValue = nullptr
    });

    BaseCollection::ClearItems();

    /*
    mPropertyChanged(this, PropertyChangedEventArgs{
        .propertyName = SymCount
    });
    // FIXME - NOTIFY Count, Item[]
    */

    mCollectionChanged(this, NotifyCollectionChangedEventArgs{
        .action = NotifyCollectionChangedAction_Reset,
        .oldIndex = -1,
        .newIndex = -1,
        .oldValue = nullptr,
        .newValue = nullptr
    });
}


BaseCommand::BaseCommand()
{}

BaseCommand::~BaseCommand()
{}

const TypeClass* BaseCommand::GetClassType() const
{
    return gStaticSymbols.TypeClasses.BaseCommand.Type;
}


const TypeClass* BaseCommand::StaticGetClassType(TypeTag<BaseCommand>*)
{
    return gStaticSymbols.TypeClasses.BaseCommand.Type;
}

EventHandler& BaseCommand::CanExecuteChanged()
{
    return mCanExecuteChanged;
}

bool BaseCommand::CanExecute(BaseComponent* param) const
{
    return false;
}

void BaseCommand::Execute(BaseComponent* param) const
{}


bool LuaDelegateCommand::CanExecute(BaseComponent* param) const
{
    return (bool)handler_;
}

void LuaDelegateCommand::Execute(BaseComponent* param) const
{
    ContextGuardAnyThread ctx(ContextType::Client);
    ecl::LuaClientPin pin(ecl::ExtensionState::Get());
    if (pin && handler_.IsValid(pin->GetState())) {
        pin->GetDeferredUIEvents().OnCommand(handler_, const_cast<LuaDelegateCommand*>(this), param);
    }
}

void LuaDelegateCommand::BindHandler(lua_State* L, lua::Ref const& handler)
{
    handler_.Bind(L, handler);
    CanExecuteChanged().Invoke(this, EventArgs{});
}

NS_IMPLEMENT_REFLECTION(LuaDelegateCommand)
{
}


const TypeClass* BoxedValue::GetClassType() const
{
    return gStaticSymbols.TypeClasses.BoxedValue.Type;
}

const TypeClass* BoxedValue::StaticGetClassType(TypeTag<BoxedValue>*)
{
    return gStaticSymbols.TypeClasses.BoxedValue.Type;
}


NS_CORE_KERNEL_API Type* Reflection::RegisterType(const char* name, CreatorFn creator, FillerFn filler)
{
    return RegisterType(Symbol(name), creator, filler);
}

// Hack to allow re-registering types in developer mode
unsigned gAllowTypeReregistration{ 0 };

NS_CORE_KERNEL_API Type* Reflection::RegisterType(Symbol name, CreatorFn creator, FillerFn filler)
{
    // PORT (linux): upstream open-codes the recursion protocol here; it is now the
    // ReflectionLock guard in GameDefinitions/UI.h, which has the witness written out. The two
    // are the same algorithm -- upstream's `locks = ++*numLocks ... --*numLocks; if (locks ==
    // 1)` is the game's `saved = depth; depth = saved + 1 ... depth = saved; if (saved == 0)`
    // -- so this loses nothing and there is one place left to be wrong. What does change is the
    // lock: an exclusive pthread_spin_lock, not AcquireSRWLockExclusive on the shim's
    // pthread_rwlock_t*, which would have written 8 bytes over a 4-byte lock.
    ReflectionLock lock;

    auto reflection = GetReflection();
    Type* cls{ nullptr };

    auto currentTy = reflection->NameToType.Find((uint32_t)name);
    if (currentTy != reflection->NameToType.End() && gAllowTypeReregistration == 0) {
        WARN("Registering type '%s' when it already exists in the registry? Might be problematic", name.Str());
        cls = currentTy->value;
    } else {
        cls = creator(name);
        if (filler) {
            filler(cls);
        }

        reflection->NameToType.Insert((uint32_t)name, cls);
        reflection->Names.PushBack(name);
    }

    return cls;
}

uint32_t SymFindStringInternal(char const* k)
{
    auto const& keys = GetSymbolManager()->Keys;
    auto it = keys.Find(k);
    if (it != keys.End()) {
        return it->value;
    } else {
        return 0;
    }
}

// PORT (linux): exclusive, not shared. The SymbolManager's lock is a 4-byte
// pthread_spinlock_t in bin/bg3 (0x7b66060 on a187af94, `lea rdi` at 0x3155c0f in
// Noesis::SymbolManager::AddString 0x3155c00, tools/targets-hand.tsv) and a spinlock has no
// shared mode to ask for; ext/linux/shim/windows.h carries the type and the rest of the
// witness. Unlike Reflection's gMutex this one has no recursion protocol in the game -- there
// are no thread-id or depth globals beside it -- so a plain exclusive spin is the whole of it,
// and a re-entrant FindString would deadlock here exactly as it does in the game.
// Never run in game.
uint32_t SymbolManager::FindString(char const* k)
{
    auto lock = GetStaticSymbols().Noesis__SymbolManager__Lock;
    AcquireNoesisSpinLock(lock);

    auto idx = SymFindStringInternal(k);

    ReleaseNoesisSpinLock(lock);

    return idx;
}

uint32_t SymbolManager::AddStaticString(char const* k)
{
    return AddString(k);
}

uint32_t SymbolManager::AddString(char const* k)
{
    auto lock = GetStaticSymbols().Noesis__SymbolManager__Lock;
    AcquireNoesisSpinLock(lock);   // PORT (linux): see FindString above

    uint32_t idx;
    auto sym = GetSymbolManager();

    auto it = sym->Keys.Find(k);
    if (it != sym->Keys.End()) {
        idx = it->value;
    } else {
        idx = sym->Strings.Size();
        auto keyCopy = _strdup(k);
        sym->Strings.PushBack(keyCopy);
        sym->Keys.Insert(keyCopy, idx);
    }

    ReleaseNoesisSpinLock(lock);
    return idx;
}

using AddVisualChildProc = void (Visual*, Visual*);

void Visual::AddVisualChild(Visual* child)
{
    auto addProc = (AddVisualChildProc*)GetStaticSymbols().Noesis__Visual__AddVisualChild;
    if (addProc) addProc(this, child);
}

void Visual::RemoveVisualChild(Visual* child)
{
    auto removeProc = (AddVisualChildProc*)GetStaticSymbols().Noesis__Visual__RemoveVisualChild;
    if (removeProc) removeProc(this, child);
}

END_BARE_NS()
