#include <GameDefinitions/Components/ServerData.h>
#include <GameDefinitions/Surface.h>

BEGIN_NS(esv)

SurfaceAction* SurfaceManager::CreateAction(SurfaceActionType type)
{
    auto factory = GetStaticSymbols().esv__gSurfaceActionFactory;
#if defined(BG3SE_LINUX)
    // PORT (linux): bin/bg3 0x38a5fd0 takes five arguments, not upstream's three -- the 32-bit
    // TStoryActionID and the ClassDescriptions reference sit between the type and the handle, and
    // the body stores them at SurfaceAction +0x10 and +0x50, which are StoryActionID and the
    // ClassDescriptionMgr assigned by hand below (Symbols.h). So the
    // resource bank is fetched before the call and passed in, and the story action id is the zero
    // the game's own call site passes (0x7183924 `xor edx, edx`).
    auto ctor = GetStaticSymbols().esv__SurfaceActionFactory__DoCreateActionLinux;
#else
    auto ctor = GetStaticSymbols().esv__SurfaceActionFactory__DoCreateAction;
#endif

    if (!ctor || !factory || !*factory) {
        return nullptr;
    }

    auto helpers = GetCurrentExtensionState()->GetLua()->GetEntitySystemHelpers();
    auto classDescs = helpers->GetResourceManager<resource::ClassDescription>();

#if defined(BG3SE_LINUX)
    auto action = ctor(*factory, type, 0,
        classDescs ? (resource::GuidResourceBankBase*)*classDescs : nullptr,
        ComponentHandle::NullHandle);
#else
    auto action = ctor(*factory, type, ComponentHandle::NullHandle);
#endif

    if (action && classDescs) {
        action->ClassDescriptionMgr = *classDescs;
    }

    return action;
}

void SurfaceManager::AddAction(SurfaceAction* action)
{
    if (action->Level) {
        ERR("Surface action is already activated!");
        return;
    }

    action->Level = Level;

    if (action->GetTypeId() == SurfaceActionType::TransformSurface) {
        auto initProc = GetStaticSymbols().esv__TransformSurfaceAction__Init;
#if defined(BG3SE_LINUX)
        // PORT (linux): esv__TransformSurfaceAction__Init is rowed on this build
        // (bg3+0x2682850), so this guard does not fire here. It stays for a build where the
        // row is not found: the member would be null and the call would jump to address 0.
        // Refuse the action rather than leaving it half-activated.
        if (!initProc) {
            ERR("Cannot activate a TransformSurface action: esv__TransformSurfaceAction__Init is not in the manifest");
            action->Level = nullptr;
            return;
        }
#endif
        auto transform = static_cast<TransformSurfaceAction*>(action);
        initProc(transform, transform->SurfaceTransformAction, transform->SurfaceLayer, transform->OriginSurface);
    }

    action->Enter();
    Actions.push_back(action);
}

END_NS()

BEGIN_NS(esv::surface)

Surface* SurfaceComponent::GetSurface(lua_State* L) const
{
    auto level = lua::State::FromLua(L)->GetExtensionState().GetLevelManager()->CurrentLevel;
    if (!level) return nullptr;

    return ((Level*)level)->SurfaceManager->Get(SurfaceHandle);
}

END_NS()

BEGIN_NS(lua)

void MakePolymorphicRef(lua_State* L, esv::SurfaceAction* v, LifetimeHandle lifetime)
{
#define V(type) case esv::type::Type: \
            MakeDirectObjectRef(L, static_cast<esv::type*>(v), lifetime); break;

    switch (v->GetTypeId()) {
        V(CreateSurfaceAction)
        V(CreatePuddleAction)
        V(ForceCreateSurfaceAction)
        V(CapsuleSurfaceAction)
        V(RemoveSurfaceAction)
        V(ZoneAction)
        V(TransformSurfaceAction)
        V(ChangeSurfaceOnPathAction)
        V(RectangleSurfaceAction)
        V(PolygonSurfaceAction)

    default:
        MakeDirectObjectRef(L, v, lifetime);
        break;
    }

#undef V
}

END_NS()
