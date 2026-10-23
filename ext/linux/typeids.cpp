// typeids.cpp — PORT (linux): fills StaticSymbols::IndexSymbolToNameMaps and
// IndexSymbolToContextMaps from the embedded ECS TypeId table, where Windows fills them from
// the pattern-scan callbacks in GameHooks/DataLibrariesBG3Game.cpp (BindECSIndex & co.),
// which are not built here.
//
// The table is typeids/<build-id>.tsv (tools/typeids.py; the rows are bin/bg3's .symtab
// addresses of ls::TypeId<T, Context>::m_TypeIndex and ls::TypeContext<Context>::m_State,
// each re-established from the binary by `typeids.py verify`), every build's file
// concatenated into one blob at configure time (ext/CMakeLists.txt, typeids_blob.S.in).
// Only the section for the running build is used: the loader's build-id of bin/bg3 comes
// over host_api, and the PIE base with it. A build with no section -- a game update, or a
// stripped bin/bg3 that harvest.py had nothing to read from -- leaves both maps empty, which
// UpdateComponentMappings() already handles (every Map*Index() lookup misses and warns, as
// every Linux launch did before this existed). Never a refusal to
// start, never a dereference: nothing here reads game memory. The int32 behind each index
// address is read by UpdateComponentMappings() later, the same way it is on Windows.
//
// What upstream's `context` pointer is: the registrant list head the game pushes onto
// (BinaryMappings.xml, ecs::StaticIDRegistrant). On Linux that head is the qword at
// ls::TypeContext<C>::m_State + 8 (read off the registrants;
// tools/typeids-hand.tsv), so that is the key used for both maps.
#include "stdafx.h"
#include <GameDefinitions/Symbols.h>
#include "platform.h"

#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <string_view>

extern "C" {
extern const char bg3se_typeids_begin[];
extern const char bg3se_typeids_end[];
}

namespace bg3se::platform {

namespace {

constexpr uintptr_t kHeadOffset = 8;   // the registrant list head inside m_State

// The maps hold char const*; the blob's cells are tab-terminated, so the names live here.
// A deque never moves what it holds.
std::deque<std::string> g_names;

char const* keep(std::string_view s)
{
    g_names.emplace_back(s);
    return g_names.back().c_str();
}

bool split4(std::string_view line, std::string_view (&cell)[4])
{
    for (int i = 0; i < 4; i++) {
        auto tab = line.find('\t');
        if (i < 3) {
            if (tab == std::string_view::npos) return false;
            cell[i] = line.substr(0, tab);
            line.remove_prefix(tab + 1);
        } else {
            if (tab != std::string_view::npos) return false;
            cell[i] = line;
        }
    }
    return true;
}

bool parse_addr(std::string_view s, uintptr_t& out)
{
    if (s.size() < 3 || s.size() > 18 || s[0] != '0' || (s[1] != 'x' && s[1] != 'X')) return false;
    char buf[19];
    memcpy(buf, s.data(), s.size());
    buf[s.size()] = 0;
    char* end = nullptr;
    unsigned long long v = strtoull(buf, &end, 16);
    if (!end || *end) return false;
    out = (uintptr_t)v;
    return true;
}

}  // namespace

void BindTypeIdTable()
{
    auto const& host = platform::host();
    if (!host.build_id || !host.build_id[0] || !host.base) {
        log_line("typeids: no build-id or base from the loader; ECS names stay unbound");
        return;
    }
    std::string_view want{ host.build_id };
    std::string_view blob{ bg3se_typeids_begin, (size_t)(bg3se_typeids_end - bg3se_typeids_begin) };

    auto& sym = GetStaticSymbols();
    std::map<std::string, int32_t*, std::less<>> heads;     // context name -> list head, this build
    std::map<std::string, unsigned, std::less<>> per_context;
    unsigned builds = 0, ids = 0, orphans = 0, bad = 0;
    bool in_build = false;

    while (!blob.empty()) {
        auto nl = blob.find('\n');
        auto line = blob.substr(0, nl);
        blob.remove_prefix(nl == std::string_view::npos ? blob.size() : nl + 1);
        if (line.empty()) continue;
        if (line[0] == '#') {
            constexpr std::string_view tag{ "# build-id " };
            if (line.substr(0, tag.size()) == tag) {
                builds++;
                in_build = line.substr(tag.size()) == want;
            }
            continue;
        }
        if (!in_build) continue;
        std::string_view cell[4];
        if (!split4(line, cell)) { bad++; continue; }
        if (cell[0] == "kind") continue;                  // the header row
        uintptr_t addr;
        if (!parse_addr(cell[1], addr)) { bad++; continue; }
        if (cell[0] == "ctx") {
            auto head = (int32_t*)(host.base + addr + kHeadOffset);
            auto name = keep(cell[2]);
            heads.emplace(std::string(cell[2]), head);
            sym.IndexSymbolToContextMaps.insert(std::make_pair(head, name));
        } else if (cell[0] == "id") {
            auto it = heads.find(cell[2]);
            if (it == heads.end()) { orphans++; continue; }
            auto index = (int32_t*)(host.base + addr);
            sym.IndexSymbolToNameMaps.insert(std::make_pair(index, IndexSymbolInfo{ keep(cell[3]), it->second }));
            per_context[it->first]++;
            ids++;
        } else {
            bad++;
        }
    }

    if (heads.empty()) {
        log_line("typeids: no table for build %s (%u build(s) embedded); ECS names stay unbound", host.build_id, builds);
        INFO("TypeId table: none for build %s (%u embedded); ECS component/system names unavailable on this build", host.build_id, builds);
        return;
    }
    std::string detail;
    for (auto const& [ctx, n] : per_context) {
        if (!detail.empty()) detail += ", ";
        detail += ctx + " " + std::to_string(n);
    }
    log_line("typeids: build %s: %zu contexts, %u names (%s); %u orphan, %u bad rows", host.build_id,
             heads.size(), ids, detail.c_str(), orphans, bad);
    INFO("TypeId table: build %s, %zu contexts, %u names: %s", host.build_id, heads.size(), ids, detail.c_str());
    if (orphans || bad) {
        WARN("TypeId table: %u rows with no ctx row, %u malformed -- the table needs `tools/typeids.py verify`", orphans, bad);
    }
}

// PORT (linux): see platform.h. getenv once, not per frame. It lives here because this
// is the only TU in the Linux layer that already opens namespace bg3se::platform and is
// not the Win32 shim.
bool NoImgui()
{
    static bool const no = [] {
        char const* v = getenv("BG3SE_NO_IMGUI");
        return v != nullptr && *v != '\0' && *v != '0';
    }();
    return no;
}

}  // namespace bg3se::platform
