// manifest.cpp — see manifest.h. The file format is tools/manifest.py's; only the first
// five columns (symbol, kind, addr, size, bytes) matter here.

#include "manifest.h"

#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <sys/auxv.h>
#include <unistd.h>
#include <cstdio>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "hook.h"
#include "loader.h"

// manifest_blob.S: every manifest/*.tsv, concatenated at configure time
extern "C" const char bg3se_manifests_begin[];
extern "C" const char bg3se_manifests_end[];

namespace bg3se::manifest {
namespace {

struct Row {
    std::string symbol;
    bool fn = false;
    bool ro = false;            // rodata: a read-only object whose bytes are the witness
    uint64_t vaddr = 0;
    size_t size = 0;
    std::vector<uint8_t> bytes;
    void* addr = nullptr;       // set only if the row verified
    bool hookable = false;
};

std::vector<Row> g_rows;                 // every module's rows, looked up by symbol
std::vector<std::string> g_builds;       // build-ids already loaded
std::atomic<bool> g_loaded{false};       // at least one load() done: find() may answer

// BG3SE_MANIFEST's contents, read in the constructor without allocating
char g_file[1 << 20];
size_t g_file_len = 0;
const char* g_file_path = nullptr;

std::string_view source()
{
    if (g_file_path) return {g_file, g_file_len};
    return {bg3se_manifests_begin, size_t(bg3se_manifests_end - bg3se_manifests_begin)};
}

void read_override()
{
    static bool done = false;
    if (done) return;
    done = true;
    const char* p = getenv("BG3SE_MANIFEST");
    if (!p || !*p) return;
    g_file_path = p;
    int fd = open(p, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return;
    ssize_t n;
    while (g_file_len < sizeof g_file &&
           (n = read(fd, g_file + g_file_len, sizeof g_file - g_file_len)) > 0)
        g_file_len += size_t(n);
    close(fd);
    if (g_file_len == sizeof g_file) g_file_len = 0;   // too large: treat as empty
}

// This build's rows: the text from its "# build-id" line to the next one.
std::string_view section(std::string_view text, const char* build_id)
{
    const std::string_view tag = "# build-id ";
    size_t start = std::string_view::npos, pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string_view::npos) eol = text.size();
        std::string_view line = text.substr(pos, eol - pos);
        if (line.substr(0, tag.size()) == tag) {
            if (start != std::string_view::npos) return text.substr(start, pos - start);
            std::string_view id = line.substr(tag.size());
            while (!id.empty() && (id.back() == ' ' || id.back() == '\r')) id.remove_suffix(1);
            if (id == build_id) start = eol;
        }
        pos = eol + 1;
    }
    return start == std::string_view::npos ? std::string_view{} : text.substr(start);
}

std::vector<std::string_view> split(std::string_view s, char c)
{
    std::vector<std::string_view> out;
    size_t p = 0;
    for (size_t q; (q = s.find(c, p)) != std::string_view::npos; p = q + 1) out.push_back(s.substr(p, q - p));
    out.push_back(s.substr(p));
    return out;
}

bool parse_row(std::string_view line, Row* r)
{
    auto f = split(line, '\t');
    if (f.size() < 5 || f[0].empty()) return false;
    r->symbol = std::string(f[0]);
    if (f[1] == "fn") r->fn = true;
    else if (f[1] == "rodata") r->ro = true;
    else if (f[1] != "data") return false;
    std::string num(f[2]);
    char* end;
    r->vaddr = strtoull(num.c_str(), &end, 16);
    if (*end || num.empty()) return false;
    if (r->fn || r->ro) {
        num = std::string(f[3]);
        r->size = strtoull(num.c_str(), &end, 10);
        if (*end || !r->size) return false;
        if (f[4].size() % 2 || f[4].empty()) return false;
        for (size_t i = 0; i < f[4].size(); i += 2) {
            std::string h(f[4].substr(i, 2));
            r->bytes.push_back(uint8_t(strtoul(h.c_str(), &end, 16)));
            if (*end) return false;
        }
        if (r->bytes.size() > r->size) return false;
    }
    return true;
}

const ElfW(Phdr)* g_phdr = nullptr;
size_t g_phnum = 0;

// Does [a, a + n) lie in one PT_LOAD of the image with all of `flags`?
bool in_segment(uintptr_t base, uintptr_t a, size_t n, uint32_t flags)
{
    const ElfW(Phdr)* phdr = g_phdr;
    for (size_t i = 0; phdr && i < g_phnum; i++) {
        const auto& p = phdr[i];
        if (p.p_type != PT_LOAD || (p.p_flags & flags) != flags) continue;
        uintptr_t lo = base + p.p_vaddr, hi = lo + p.p_memsz;
        if (a >= lo && a + n <= hi && a + n >= a) return true;
    }
    return false;
}

const Row* find(const char* symbol)
{
    if (!g_loaded.load(std::memory_order_acquire)) return nullptr;
    for (const Row& r : g_rows)
        if (r.symbol == symbol) return &r;
    return nullptr;
}

bool require_impl(const char* feature, std::initializer_list<const char*> symbols, bool hooks)
{
    std::string missing;
    for (const char* s : symbols) {
        const Row* r = find(s);
        const char* why = !r ? "not in this build's manifest"
                        : !r->addr ? "failed verification"
                        : hooks && !r->hookable ? "not hookable" : nullptr;
        if (why) missing += std::string(missing.empty() ? "" : ", ") + s + " (" + why + ")";
    }
    if (missing.empty()) return true;
    log_line("feature off: %s — %s", feature, missing.c_str());
    return false;
}

}  // namespace

bool known(const char* build_id)
{
    read_override();
    return !section(source(), build_id).empty();
}

void load(const char* build_id, uintptr_t base)
{
    load(build_id, base, (const ElfW(Phdr)*)getauxval(AT_PHDR), getauxval(AT_PHNUM));
}

void load(const char* build_id, uintptr_t base, const ElfW(Phdr)* phdr, size_t phnum)
{
    for (const std::string& b : g_builds)
        if (b == build_id) return;                      // one load per module
    g_builds.push_back(build_id);
    g_phdr = phdr;                                      // in_segment() checks against this image
    g_phnum = phnum;
    read_override();
    const char* from = g_file_path ? g_file_path : "embedded";
    std::string_view text = section(source(), build_id);
    if (text.empty()) {
        log_line("manifest: none for build %s (%s): every feature that needs an address is off",
                 build_id, from);
        g_loaded.store(true, std::memory_order_release);
        return;
    }
    int rows = 0, fns = 0, ok = 0, hookable = 0, bad_lines = 0;
    for (std::string_view line : split(text, '\n')) {
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty() || line[0] == '#' || line.substr(0, 7) == "symbol\t") continue;
        Row r;
        if (!parse_row(line, &r)) {
            bad_lines++;
            log_line("manifest: unreadable row: %.*s", int(line.size() < 120 ? line.size() : 120), line.data());
            continue;
        }
        rows++;
        fns += r.fn;
        bool dup = false;
        for (const Row& o : g_rows) dup |= o.symbol == r.symbol;
        if (dup) {
            log_line("manifest: %s: duplicate row, ignored", r.symbol.c_str());
            continue;
        }
        uintptr_t a = base + r.vaddr;
        const char* why = nullptr;
        if (r.fn) {
            if (!in_segment(base, a, r.size, PF_R | PF_X)) why = "not in an executable segment";
            else if (memcmp((const void*)a, r.bytes.data(), r.bytes.size()) != 0) why = "bytes differ";
            else {
                hook::Status st = hook::probe((const uint8_t*)a, r.size, a);
                r.hookable = st == hook::Status::ok;
                if (!r.hookable) log_line("manifest: %s: callable, not hookable (%s)", r.symbol.c_str(), hook::describe(st));
            }
        } else if (r.ro) {
            if (!in_segment(base, a, r.size, PF_R) || in_segment(base, a, r.size, PF_W)) why = "not in a read-only segment";
            else if (memcmp((const void*)a, r.bytes.data(), r.bytes.size()) != 0) why = "bytes differ";
        } else if (!in_segment(base, a, 8, PF_R | PF_W)) {
            why = "not in a writable segment";
        }
        if (why) {
            log_line("manifest: %s %s 0x%lx: %s — off", r.symbol.c_str(), r.fn ? "fn" : r.ro ? "rodata" : "data",
                     (unsigned long)r.vaddr, why);
        } else {
            r.addr = (void*)a;
            ok++;
            hookable += r.hookable;
        }
        g_rows.push_back(std::move(r));
    }
    log_line("manifest: build %s (%s): %d rows, %d verified, %d off; %d of %d functions hookable%s",
             build_id, from, rows, ok, rows - ok, hookable, fns,
             bad_lines ? ", unreadable rows skipped" : "");
    g_loaded.store(true, std::memory_order_release);
}

// The GNU build-id note of an image already mapped: from its program headers, no file I/O.
bool build_id_of(uintptr_t base, const ElfW(Phdr)* phdr, size_t phnum, char* out, size_t n)
{
    for (size_t i = 0; i < phnum; i++) {
        if (phdr[i].p_type != PT_NOTE) continue;
        auto* p = (const uint8_t*)(base + phdr[i].p_vaddr);
        auto* end = p + phdr[i].p_memsz;
        while (p + sizeof(ElfW(Nhdr)) <= end) {
            auto* nh = (const ElfW(Nhdr)*)p;
            const uint8_t* name = p + sizeof *nh;
            const uint8_t* desc = name + ((nh->n_namesz + 3) & ~3u);
            const uint8_t* next = desc + ((nh->n_descsz + 3) & ~3u);
            if (next > end) break;
            if (nh->n_type == NT_GNU_BUILD_ID && nh->n_namesz == 4 && !memcmp(name, "GNU", 4) &&
                2 * nh->n_descsz + 1 <= n) {
                for (uint32_t k = 0; k < nh->n_descsz; k++) snprintf(out + 2 * k, 3, "%02x", desc[k]);
                return true;
            }
            p = next;
        }
    }
    return false;
}

namespace {
struct LibSearch {
    const char* soname;
    bool found = false;
};

int on_object(struct dl_phdr_info* info, size_t, void* arg)
{
    auto* s = (LibSearch*)arg;
    const char* name = info->dlpi_name ? info->dlpi_name : "";
    const char* slash = strrchr(name, '/');
    if (strcmp(slash ? slash + 1 : name, s->soname) != 0) return 0;
    s->found = true;
    char build[2 * 64 + 1] = "";
    if (!build_id_of(info->dlpi_addr, info->dlpi_phdr, info->dlpi_phnum, build, sizeof build)) {
        log_line("manifest: %s at 0x%lx has no build-id note: its addresses stay off", s->soname,
                 (unsigned long)info->dlpi_addr);
        return 1;
    }
    log_line("manifest: %s at 0x%lx, build %s", s->soname, (unsigned long)info->dlpi_addr, build);
    load(build, info->dlpi_addr, info->dlpi_phdr, info->dlpi_phnum);
    return 1;
}
}  // namespace

void load_library(const char* soname)
{
    LibSearch s{soname};
    dl_iterate_phdr(on_object, &s);
    if (!s.found) log_line("manifest: %s is not mapped in this process: its addresses stay off", soname);
}

void* get(const char* symbol)
{
    const Row* r = find(symbol);
    return r ? r->addr : nullptr;
}

bool hookable(const char* symbol)
{
    const Row* r = find(symbol);
    return r && r->addr && r->hookable;
}

bool require(const char* feature, std::initializer_list<const char*> symbols)
{
    return require_impl(feature, symbols, false);
}

bool require_hooks(const char* feature, std::initializer_list<const char*> symbols)
{
    return require_impl(feature, symbols, true);
}

}  // namespace bg3se::manifest
