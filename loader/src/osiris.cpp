// osiris.cpp — Phase 3: libOsiris.so interposed by symbol.
//
// bg3 imports COsiris's entry points as ordinary dynamic symbols and calls them
// through the PLT. A preloaded object precedes libOsiris.so
// in the lookup order, so defining the same mangled names here takes the calls, and
// dlsym(RTLD_NEXT) reaches the originals. No byte is patched.
//
// libOsiris calls some of its own exports through its PLT as well (DeleteAllData,
// the _Read*/_Write* family), so interposing one of those also takes the calls that
// start inside Osiris. None of the six hooked here is called that way: each is one
// JUMP_SLOT in bg3 and nothing else.
//
// Layouts below were read off libOsiris.so b5dc7357395776f8;
// they equal upstream's Windows ones. Upstream's counterpart is
// BG3Extender/GameHooks/OsirisWrappers.cpp (MIT + Commons Clause, (c) Norbyte); the
// names of the DivFunctions slots and of MappingInfo's fields are taken from there.

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

#include "crash.h"
#include "loader.h"

using namespace bg3se;

namespace {

// COsiArgumentDesc: ctor zeroes 0x00..0x13; accessors read the value at +0x08 and
// compare the uint16 at +0x10; the dtor follows +0x00 as a chain.
struct OsiArgumentDesc {
    OsiArgumentDesc* next;
    union {
        const char* string;
        int32_t int32;
        int64_t int64;
        float real;
    };
    uint16_t type;  // 1 int, 2 int64, 3 real, 4 string, 5 guidstring, >= 6 story aliases
    bool flag;
};
static_assert(sizeof(OsiArgumentDesc) == 0x18);

// COsiris::GetFunctionMappings fills these at a 0x18 stride. Only functions the game
// implements are listed (events, queries, calls), and `id` is the number
// COsiris::Event and the DivFunctions callbacks are handed.
struct MappingInfo {
    const char* name;
    uint32_t id;
    uint32_t num_params;
    uint8_t* param_types;
};
static_assert(sizeof(MappingInfo) == 0x18);

// TOsirisInitFunction: RegisterDIVFunctions copies exactly these 0x28 bytes into
// libOsiris's .bss, so handing it a copy of ours leaves the game's struct untouched.
struct DivFunctions {
    using HandleProc = uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t);
    using CallProc = bool (*)(uint32_t function_id, OsiArgumentDesc* params);
    using ErrorProc = void (*)(const char* message);
    using AssertProc = void (*)(bool ok, const char* message, bool unknown);
    HandleProc get_handle;
    CallProc call;
    CallProc query;
    ErrorProc error;
    AssertProc assert_;
};
static_assert(sizeof(DivFunctions) == 0x28);

// ---- what to log ------------------------------------------------------------------
// BG3SE_OSI is a comma list of: events, calls, queries, errors, none.
// Default "events,errors": calls and queries run at a rate that wants a filter first.

bool g_log_events = true, g_log_calls = false, g_log_queries = false, g_log_errors = true;
std::once_flag g_setup_once;
int g_fd = -1;

void setup()
{
    if (const char* v = getenv("BG3SE_OSI"); v && *v) {
        g_log_events = strstr(v, "events");
        g_log_calls = strstr(v, "calls");
        g_log_queries = strstr(v, "queries");
        g_log_errors = strstr(v, "errors");
    }
    char path[4200];
    const char* p = getenv("BG3SE_OSI_LOG");
    if (p && *p) {
        snprintf(path, sizeof path, "%s", p);
    } else {
        snprintf(path, sizeof path, "%s", log_path());
        char* slash = strrchr(path, '/');
        if (!slash) return;
        snprintf(slash + 1, sizeof path - (slash + 1 - path), "osiris.log");
    }
    // Kept open, unlike bg3se.log: by now we are in the game process proper, and an
    // event line per open/close would be three syscalls where one will do.
    g_fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
    log_line("osiris: logging to %s (events=%d calls=%d queries=%d errors=%d)%s", path, g_log_events,
             g_log_calls, g_log_queries, g_log_errors, g_fd < 0 ? " - OPEN FAILED" : "");
}

void osi_line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void osi_line(const char* fmt, ...)
{
    if (g_fd < 0) return;
    // Per-thread scratch, not a local: this runs on the game's Osiris thread, which has
    // not got 4 kB of frame to spare (loader.h). And this is
    // where the crash reader first sees that thread when only osiris logging is on.
    crash::ensure_alt_stack();
    char* buf = scratch(kScratchOsi);
    if (!buf) return;
    const size_t bufsz = kScratchSize;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    size_t n = strftime(buf, bufsz, "%H:%M:%S", &tm);
    n += snprintf(buf + n, bufsz - n, ".%03ld [%ld] ", ts.tv_nsec / 1000000, (long)syscall(SYS_gettid));
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(buf + n, bufsz - n - 1, fmt, ap);
    va_end(ap);
    if (m > 0) n += (size_t)m < bufsz - n - 1 ? (size_t)m : bufsz - n - 2;
    buf[n++] = '\n';
    (void)!write(g_fd, buf, n);
}

void enter(const char* via)
{
    deferred_init(via);
    std::call_once(g_setup_once, setup);
}

// ---- function names ---------------------------------------------------------------
// Copied out of the array the game asks for (it frees the array afterwards). Readers
// take the current table through an atomic pointer; a replaced table is leaked on
// purpose — there is one per story load.

struct FunctionInfo {
    std::string name;
    uint32_t num_params;
};
using FunctionTable = std::unordered_map<uint32_t, FunctionInfo>;
std::atomic<const FunctionTable*> g_functions{nullptr};

const FunctionInfo* find_function(uint32_t id)
{
    const FunctionTable* t = g_functions.load(std::memory_order_acquire);
    if (!t) return nullptr;
    auto it = t->find(id);
    return it == t->end() ? nullptr : &it->second;
}

// ---- formatting -------------------------------------------------------------------

template <class T>
T* next_sym(const char* mangled)
{
    return (T*)dlsym(RTLD_NEXT, mangled);
}

// Story types (>= 6) alias a base type through Osiris's type table; the exported
// COsiArgumentDesc::IsStringType walks it for us and only reads.
bool is_string(const OsiArgumentDesc* a)
{
    if (a->type == 4 || a->type == 5) return true;
    if (a->type < 6 || a->type == 0x7f) return false;
    static auto* real = next_sym<bool(const OsiArgumentDesc*)>("_ZNK16COsiArgumentDesc12IsStringTypeEv");
    return real && real(a);
}

size_t format_args(char* out, size_t cap, const OsiArgumentDesc* a)
{
    size_t n = 0;
    out[0] = 0;
    for (int i = 0; a && i < 32 && n + 1 < cap; a = a->next, i++) {
        const char* sep = i ? ", " : "";
        int m;
        if (is_string(a)) m = snprintf(out + n, cap - n, "%s\"%.200s\"", sep, a->string ? a->string : "(null)");
        else if (a->type == 1) m = snprintf(out + n, cap - n, "%s%d", sep, a->int32);
        else if (a->type == 2) m = snprintf(out + n, cap - n, "%s%lldL", sep, (long long)a->int64);
        else if (a->type == 3) m = snprintf(out + n, cap - n, "%s%g", sep, (double)a->real);
        else m = snprintf(out + n, cap - n, "%s<type %u: 0x%llx>", sep, a->type, (unsigned long long)a->int64);
        if (m < 0) break;
        n += (size_t)m < cap - n ? (size_t)m : cap - n - 1;
    }
    return n;
}

void log_function(const char* kind, uint32_t id, const OsiArgumentDesc* args, const char* suffix)
{
    char* buf = scratch(kScratchOsiArgs);   // its own slot: osi_line below uses another
    if (!buf) return;
    format_args(buf, kScratchSize, args);
    if (const FunctionInfo* f = find_function(id)) osi_line("%s %s(%s)%s", kind, f->name.c_str(), buf, suffix);
    else osi_line("%s #%u(%s)%s", kind, id, buf, suffix);
}

// wchar_t is 4 bytes here; no locale involved.
std::string narrow(const wchar_t* w)
{
    std::string s;
    if (!w) return "(null)";
    for (size_t i = 0; w[i] && i < 1024; i++) {
        uint32_t c = (uint32_t)w[i];
        if (c < 0x80) s += (char)c;
        else if (c < 0x800) s += (char)(0xc0 | c >> 6), s += (char)(0x80 | (c & 0x3f));
        else if (c < 0x10000) s += (char)(0xe0 | c >> 12), s += (char)(0x80 | (c >> 6 & 0x3f)), s += (char)(0x80 | (c & 0x3f));
        else s += (char)(0xf0 | (c >> 18 & 7)), s += (char)(0x80 | (c >> 12 & 0x3f)), s += (char)(0x80 | (c >> 6 & 0x3f)), s += (char)(0x80 | (c & 0x3f));
    }
    return s;
}

// ---- the DivFunctions shim --------------------------------------------------------

DivFunctions g_game_div{};

bool call_wrapper(uint32_t id, OsiArgumentDesc* params)
{
    if (!g_log_calls) return g_game_div.call(id, params);
    log_function("call ", id, params, "");
    return g_game_div.call(id, params);
}

bool query_wrapper(uint32_t id, OsiArgumentDesc* params)
{
    bool ok = g_game_div.query(id, params);
    // after the call, so that out-parameters show what the game answered
    if (g_log_queries) log_function("query", id, params, ok ? " = true" : " = false");
    return ok;
}

void error_wrapper(const char* message)
{
    if (g_log_errors) osi_line("error %.1000s", message ? message : "(null)");
    g_game_div.error(message);
}

std::atomic<uint64_t> g_events{0};

}  // namespace

// The interposers. Return values travel as the full rax: bool or enum, the caller
// reads the width it expects and we never look at it except to print it.
#define OSI_HOOK(ret, name, mangled, ...) \
    extern "C" __attribute__((visibility("default"))) ret name(__VA_ARGS__) asm(mangled); \
    ret name(__VA_ARGS__)

OSI_HOOK(void, bg3se_COsiris_RegisterDIVFunctions, "_ZN7COsiris20RegisterDIVFunctionsEP19TOsirisInitFunction",
         void* self, DivFunctions* functions)
{
    static auto* real = next_sym<void(void*, DivFunctions*)>("_ZN7COsiris20RegisterDIVFunctionsEP19TOsirisInitFunction");
    if (!real) return;
    if (!g_active || !functions) return real(self, functions);
    enter("COsiris::RegisterDIVFunctions");
    g_game_div = *functions;
    DivFunctions shim = *functions;
    if (shim.call) shim.call = call_wrapper;
    if (shim.query) shim.query = query_wrapper;
    if (shim.error) shim.error = error_wrapper;
    log_line("osiris: RegisterDIVFunctions(self=%p) get_handle=%p call=%p query=%p error=%p assert=%p", self,
             (void*)functions->get_handle, (void*)functions->call, (void*)functions->query,
             (void*)functions->error, (void*)functions->assert_);
    real(self, &shim);
}

OSI_HOOK(void, bg3se_COsiris_GetFunctionMappings, "_ZN7COsiris19GetFunctionMappingsEPP11MappingInfoPj",
         void* self, MappingInfo** mappings, uint32_t* count)
{
    static auto* real = next_sym<void(void*, MappingInfo**, uint32_t*)>("_ZN7COsiris19GetFunctionMappingsEPP11MappingInfoPj");
    if (!real) return;
    real(self, mappings, count);
    if (!g_active || !mappings || !count || !*mappings) return;
    enter("COsiris::GetFunctionMappings");
    auto* table = new FunctionTable;
    table->reserve(*count);
    for (uint32_t i = 0; i < *count; i++) {
        const MappingInfo& m = (*mappings)[i];
        if (m.name) (*table)[m.id] = FunctionInfo{m.name, m.num_params};
    }
    log_line("osiris: GetFunctionMappings: %u functions, %zu distinct ids", *count, table->size());
    g_functions.store(table, std::memory_order_release);
}

OSI_HOOK(uint64_t, bg3se_COsiris_InitGame, "_ZN7COsiris8InitGameEv", void* self)
{
    static auto* real = next_sym<uint64_t(void*)>("_ZN7COsiris8InitGameEv");
    if (!real) return 0;
    if (!g_active) return real(self);
    enter("COsiris::InitGame");
    uint64_t rc = real(self);
    log_line("osiris: InitGame(self=%p) = %u", self, (unsigned)(uint8_t)rc);
    return rc;
}

OSI_HOOK(uint64_t, bg3se_COsiris_Load, "_ZN7COsiris4LoadER12COsiSmartBuf", void* self, void* buf)
{
    static auto* real = next_sym<uint64_t(void*, void*)>("_ZN7COsiris4LoadER12COsiSmartBuf");
    if (!real) return 0;
    if (!g_active) return real(self, buf);
    enter("COsiris::Load");
    uint64_t rc = real(self, buf);
    log_line("osiris: Load(self=%p, buf=%p) = %u", self, buf, (unsigned)(uint8_t)rc);
    return rc;
}

OSI_HOOK(uint64_t, bg3se_COsiris_Merge, "_ZN7COsiris5MergeEPKw", void* self, const wchar_t* path)
{
    static auto* real = next_sym<uint64_t(void*, const wchar_t*)>("_ZN7COsiris5MergeEPKw");
    if (!real) return 0;
    if (!g_active) return real(self, path);
    enter("COsiris::Merge");
    uint64_t rc = real(self, path);
    log_line("osiris: Merge(self=%p, \"%s\") = %u", self, narrow(path).c_str(), (unsigned)(uint8_t)rc);
    return rc;
}

OSI_HOOK(uint64_t, bg3se_COsiris_Event, "_ZN7COsiris5EventEjP16COsiArgumentDesc",
         void* self, uint32_t function_id, OsiArgumentDesc* args)
{
    static auto* real = next_sym<uint64_t(void*, uint32_t, OsiArgumentDesc*)>("_ZN7COsiris5EventEjP16COsiArgumentDesc");
    if (!real) return 0;
    if (!g_active) return real(self, function_id, args);
    enter("COsiris::Event");
    if (g_events.fetch_add(1) == 0) log_line("osiris: first Event (id %u)", function_id);
    if (g_log_events) log_function("event", function_id, args, "");
    return real(self, function_id, args);
}
