// The game's side of Osiris in the stand-in bg3: registers its callbacks, asks for the
// function mappings, raises one event. Returns 0 when every answer came back intact
// through the interposers.
#include <cstdio>
#include <cstdlib>

#include "osiris_abi.h"

static int g_calls, g_queries, g_errors;

static bool game_call(uint32_t id, COsiArgumentDesc*) { g_calls++; return id == 303; }
static bool game_query(uint32_t id, COsiArgumentDesc* a)
{
    g_queries++;
    if (a && a->next) a->next->int32 = 7;  // out-parameter
    return id == 202;
}
static void game_error(const char*) { g_errors++; }

extern "C" int run_osiris(void)
{
    COsiris* osi = (COsiris*)calloc(1, 64);
    TOsirisInitFunction div{nullptr, game_call, game_query, game_error, nullptr};
    TOsirisInitFunction before = div;
    osi->RegisterDIVFunctions(&div);
    bool untouched = div.call == before.call && div.query == before.query && div.error == before.error;

    MappingInfo* maps = nullptr;
    unsigned n = 0;
    osi->GetFunctionMappings(&maps, &n);
    free(maps);

    bool init = osi->InitGame();
    bool merged = osi->Merge(L"/story/gôal.div");
    COsiArgumentDesc arg;
    arg.type = 5;
    arg.string = "S_Player_Karlach_2c76687d-93a2-477b-8b18-8a14b549304c";
    int rc = osi->Event(101, &arg);
    int unnamed = osi->Event(999, &arg);

    printf("fake bg3: osiris n=%u init=%d merged=%d rc=%d unnamed=%d calls=%d queries=%d errors=%d untouched=%d\n",
           n, init, merged, rc, unnamed, g_calls, g_queries, g_errors, untouched);
    return n == 3 && init && !merged && rc == 108 && unnamed == 1006 && g_calls == 2 && g_queries == 2 &&
                   g_errors == 2 && untouched
               ? 0 : 1;
}
