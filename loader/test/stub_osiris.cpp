// Stub libOsiris.so for the smoke test: the "real" COsiris behind the interposers.
// Event() calls back into the game through the registered table, as Osiris does, so
// the test sees whether the shim sits in that path and the game's own callback
// is still reached behind it.
#include <cstdlib>
#include <cstring>

#include "osiris_abi.h"

#define EXPORT __attribute__((visibility("default")))

static TOsirisInitFunction g_div;

EXPORT bool COsiArgumentDesc::IsStringType() const { return type == 4 || type == 5 || type == 9; }

EXPORT void COsiris::RegisterDIVFunctions(TOsirisInitFunction* f) { memcpy(&g_div, f, sizeof g_div); }

EXPORT void COsiris::GetFunctionMappings(MappingInfo** out, unsigned int* count)
{
    auto* m = (MappingInfo*)calloc(3, sizeof(MappingInfo));
    m[0] = {"CombatStarted", 101, 1, nullptr};
    m[1] = {"GetHitpoints", 202, 2, nullptr};
    m[2] = {"ApplyStatus", 303, 3, nullptr};
    *out = m;
    *count = 3;
}

EXPORT bool COsiris::InitGame() { return true; }
EXPORT bool COsiris::Load(COsiSmartBuf&) { return true; }
EXPORT bool COsiris::Merge(wchar_t const*) { return false; }

EXPORT int COsiris::Event(unsigned int id, COsiArgumentDesc* args)
{
    COsiArgumentDesc hp;
    hp.type = 1;
    COsiArgumentDesc who;
    who.type = 9;  // a story alias type, string underneath
    who.string = args ? args->string : "?";
    who.next = &hp;
    bool ok = g_div.query(202, &who);
    g_div.call(303, &who);
    g_div.error("stub story error");
    return ok ? (int)id + hp.int32 : -1;
}
