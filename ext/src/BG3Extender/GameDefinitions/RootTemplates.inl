#pragma once

#include <GameDefinitions/Level.h>
#include <GameDefinitions/RootTemplates.h>
#include <Extender/ScriptExtender.h>

BEGIN_SE()

ConstructionFillingTemplate::~ConstructionFillingTemplate()
{}

ConstructionTileTemplate::~ConstructionTileTemplate()
{}

FixedString GameObjectTemplate::GetTemplateType() const
{
    return *GetType();
}

TemplateType GameObjectTemplate::GetTemplateStorageType() const
{
    return TemplateHandle.GetType();
}

IActionData* ItemTemplate::AddUseAction(ActionDataType type)
{
    auto action = gExtender->GetUseActionHelpers().Create(type);
    if (action) {
        OnUsePeaceActions.Value.push_back(action);
        return action;
    } else {
        return nullptr;
    }
}

void ItemTemplate::RemoveUseAction(int32_t index)
{
    if (index >= 1 && index <= (int)OnUsePeaceActions.Value.size()) {
        OnUsePeaceActions.Value.remove_at(index - 1);
    }
}

GameObjectTemplate* CacheTemplateManagerBase::CacheTemplate(GameObjectTemplate* tmpl, FixedString const& levelName, FixedString const& templateId)
{
    TemplateHandle newHandle(GetNewIndex(), TemplateManagerType);
    auto newTmpl = tmpl->Clone();

    // The game requires cache templates to have a root template (roots are not supposed to be in the cache)
    if (!newTmpl->TemplateName) {
        newTmpl->TemplateName = newTmpl->Id;
        newTmpl->ParentTemplateId = FixedString{};
    }

    newTmpl->Id = templateId;
    newTmpl->LevelName = levelName;

    RegisterTemplate(newHandle, newTmpl);

    if (EnableTemplateSync) {
        Lock.WriteLock();
        AddedTemplates.push_back(newTmpl);
        Lock.WriteUnlock();
    }

    return newTmpl;
}

void CacheTemplateManagerBase::RegisterTemplate(TemplateHandle handle, GameObjectTemplate* tmpl)
{
    Lock.WriteLock();
    TemplatesByHandle.set(handle.GetHandle(), tmpl);
    tmpl->TemplateHandle = handle;
    Templates.set(tmpl->Id, tmpl);
    RefCountsByHandle.set(handle.GetHandle(), 0);
    Lock.WriteUnlock();
}

void CacheTemplateManagerBase::IncTemplateRef(TemplateHandle handle)
{
    Lock.ReadLock();
    auto refCount = RefCountsByHandle.try_get(handle.GetHandle());
    ++(*refCount);
    Lock.ReadUnlock();
}

void CacheTemplateManagerBase::DecTemplateRef(TemplateHandle handle)
{
    Lock.ReadLock();
    auto refCount = RefCountsByHandle.try_get(handle.GetHandle());
    auto refs = --(*refCount);
    Lock.ReadUnlock();

    if (refs == 0) {
        Lock.WriteLock();
        auto tmpl = TemplatesByHandle.get_or_default(handle.GetHandle());
        RefCountsByHandle.remove(handle.GetHandle());
        TemplatesByHandle.remove(handle.GetHandle());
        Templates.remove(tmpl->Id);
        if (EnableTemplateSync) {
            auto addedIt = AddedTemplates.find(tmpl);
            if (addedIt != AddedTemplates.end()) {
                AddedTemplates.erase(addedIt);
            }
            RemovedTemplates.push_back(tmpl);
        }
        Lock.WriteUnlock();
    }
}

// PORT (linux): Lock is the game's own pthread_rwlock_t here, so the shared acquire is the
// game's algorithm rather than the shim's storage -- see LS_SRWLOCK in ext/linux/shim/windows.h.
#if defined(BG3SE_LINUX)
#define LOCAL_TEMPLATE_LOCK_SHARED()    AcquireGameSRWLockShared(&Lock)
#define LOCAL_TEMPLATE_UNLOCK_SHARED()  ReleaseGameSRWLockShared(&Lock)
#else
#define LOCAL_TEMPLATE_LOCK_SHARED()    AcquireSRWLockShared(&Lock)
#define LOCAL_TEMPLATE_UNLOCK_SHARED()  ReleaseSRWLockShared(&Lock)
#endif

void LocalTemplateManager::IncTemplateRef(TemplateHandle handle)
{
    LOCAL_TEMPLATE_LOCK_SHARED();
    auto tmpl = TemplatesByHandle.try_get(handle.GetHandle());
    ++tmpl->RefCount;
    LOCAL_TEMPLATE_UNLOCK_SHARED();
}

void LocalTemplateManager::DecTemplateRef(TemplateHandle handle)
{
    LOCAL_TEMPLATE_LOCK_SHARED();
    auto tmplData = TemplatesByHandle.try_get(handle.GetHandle());
    auto tmpl = tmplData->Template;
    auto refs = tmplData->RefCount.fetch_sub(1);
    LOCAL_TEMPLATE_UNLOCK_SHARED();

    if (refs == 0) {
        ERR("Deleting local template with RefCount==0 not yet implemented!");
    }
}

END_SE()

BEGIN_NS(esv)

void IncTemplateRef(GameObjectTemplate* tmpl)
{
    switch (tmpl->TemplateHandle.GetType()) {
    case TemplateType::RootTemplate:
    case TemplateType::LevelTemplate: // These are not refcounted
        break;

    case TemplateType::LocalTemplate:
        GetStaticSymbols().GetServerLevelManager()->CurrentLevel->LocalTemplateManager->IncTemplateRef(tmpl->TemplateHandle);
        break;

    case TemplateType::CacheTemplate:
        (*GetStaticSymbols().esv__CacheTemplateManager)->IncTemplateRef(tmpl->TemplateHandle);
        break;

    case TemplateType::LevelCacheTemplate:
        (static_cast<Level*>(GetStaticSymbols().GetServerLevelManager()->CurrentLevel))->CacheTemplateManager->IncTemplateRef(tmpl->TemplateHandle);
        break;
    }
}

void DecTemplateRef(GameObjectTemplate* tmpl)
{
    switch (tmpl->TemplateHandle.GetType()) {
    case TemplateType::RootTemplate:
    case TemplateType::LevelTemplate: // These are not refcounted
        break;

    case TemplateType::LocalTemplate:
        GetStaticSymbols().GetServerLevelManager()->CurrentLevel->LocalTemplateManager->DecTemplateRef(tmpl->TemplateHandle);
        break;

    case TemplateType::CacheTemplate:
        (*GetStaticSymbols().esv__CacheTemplateManager)->DecTemplateRef(tmpl->TemplateHandle);
        break;

    case TemplateType::LevelCacheTemplate:
        (static_cast<Level*>(GetStaticSymbols().GetServerLevelManager()->CurrentLevel))->CacheTemplateManager->DecTemplateRef(tmpl->TemplateHandle);
        break;
    }
}

END_NS()
