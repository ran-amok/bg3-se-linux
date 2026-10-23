#include <stdafx.h>
#include <Extender/ScriptExtender.h>
#include <Extender/Client/VisualHelpers.h>

BEGIN_NS(ecl)

void VisualHelpers::Setup()
{
    gExtender->GetEngineHooks().ls__Visual__UpdateBlendshapeWeightsFromSkeleton
        .SetPostHook(&VisualHelpers::OnVisualUpdateBlendshapeWeightsFromSkeleton, this);
}

bool VisualHelpers::SetWeight(RenderableObject* obj, FixedString const& driver, std::optional<float> weight)
{
#if defined(BG3SE_LINUX)
    // PORT (linux): ls::Visual::UpdateBlendshapeWeightsFromSkeleton has no out-of-line body in this
    // build -- LTO inlined it into ls::Visual::NotifySkeletonUpdated(bool) 0x24a6af0, on macOS too
    // (tools/targets-hand.tsv). So OnVisualUpdateBlendshapeWeightsFromSkeleton is
    // never called and an override recorded here would never be applied. Upstream has no guard of its
    // own on this path -- SetWeight does not look at the hook -- so without this one the Lua call
    // returns true and silently does nothing, which is the one thing sAbsentOnLinux's comment says a
    // Lost name must not do. Refuse by name instead, the way step 1's unguarded calls do.
    if (GetStaticSymbols().ls__Visual__UpdateBlendshapeWeightsFromSkeleton == nullptr) {
        ERR("Cannot override a blendshape weight: ls__Visual__UpdateBlendshapeWeightsFromSkeleton has "
            "no out-of-line body in this build (inlined into ls::Visual::NotifySkeletonUpdated), so "
            "there is nothing to post-hook and the weight would be recomputed from the skeleton");
        return false;
    }
#endif

    if (obj->BlendShape == nullptr) return false;

    auto const& drivers = static_cast<TransformingVertexModel*>(obj->Model)->BlendShapeModelData->Data->MorphTargetDrivers;
    auto driverIt = drivers.find(driver);
    if (driverIt == drivers.end()) return false;
    auto driverIdx = (uint32_t)(driverIt - drivers.begin());

    auto overrides = weightOverrides_.get_or_add(obj);
    while (driverIdx >= overrides->size()) {
        overrides->push_back({});
    }

    (*overrides)[driverIdx] = weight;
    return true;
}

bool VisualHelpers::ClearWeights(RenderableObject* obj)
{
    if (obj->BlendShape == nullptr) return false;

    weightOverrides_.remove(obj);
    return true;
}

void VisualHelpers::OnVisualUpdateBlendshapeWeightsFromSkeleton(Visual* obj)
{
    if (obj->BlendShapeBase == 0xffff) return;

    for (auto const& child : obj->ObjectDescs) {
        if (child.Renderable->BlendShape != nullptr) {
            auto overrides = weightOverrides_.try_get(child.Renderable);

            if (overrides != nullptr) {
                auto blendShape = child.Renderable->LuaGetBlendShape();

                for (uint32_t i = 0; i < std::min(overrides->size(), blendShape->Weights.size()); i++) {
                    if ((*overrides)[i]) {
                        blendShape->Weights[i] = *(*overrides)[i];
                    }
                }
            }
        }
    }
}

END_NS()
