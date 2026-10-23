// manifest-check BIN: the loader's own manifest verification (src/manifest.cpp, the
// embedded blob) run against a game binary on disk instead of the running game. BIN's
// PT_LOAD segments are mapped read-only at one base, as ld.so would place them, and
// manifest::load() is given that base and those program headers. What it logs is what
// the game's log will say at deferred init. Exit 0 when every row verifies.
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include "loader.h"
#include "manifest.h"

namespace bg3se {
bool g_is_game = false;
bool g_active = false;
int g_verified = -1, g_rows = -1;
void log_line(const char* fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    printf("%s\n", buf);
    sscanf(buf, "manifest: build %*s %*s %d rows, %d verified", &g_rows, &g_verified);
}
const char* log_path() { return "stdout"; }
}  // namespace bg3se

int main(int argc, char** argv)
{
    if (argc != 2) { fprintf(stderr, "usage: manifest-check BIN\n"); return 2; }
    int fd = open(argv[1], O_RDONLY);
    struct stat st;
    if (fd < 0 || fstat(fd, &st)) { perror(argv[1]); return 2; }
    auto* file = (const uint8_t*)mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file == MAP_FAILED) { perror("mmap"); return 2; }
    auto* eh = (const ElfW(Ehdr)*)file;
    if (memcmp(eh->e_ident, ELFMAG, SELFMAG) || eh->e_type != ET_DYN) {
        fprintf(stderr, "%s: not a PIE\n", argv[1]);
        return 2;
    }
    auto* ph = (const ElfW(Phdr)*)(file + eh->e_phoff);
    uintptr_t top = 0;
    std::string build;
    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type == PT_LOAD && ph[i].p_vaddr + ph[i].p_memsz > top) top = ph[i].p_vaddr + ph[i].p_memsz;
        if (ph[i].p_type != PT_NOTE) continue;
        for (size_t o = ph[i].p_offset; o + sizeof(ElfW(Nhdr)) <= ph[i].p_offset + ph[i].p_filesz;) {
            auto* nh = (const ElfW(Nhdr)*)(file + o);
            const uint8_t* desc = file + o + sizeof *nh + ((nh->n_namesz + 3) & ~3u);
            if (nh->n_type == NT_GNU_BUILD_ID && nh->n_namesz == 4)
                for (uint32_t k = 0; k < nh->n_descsz; k++) {
                    char h[3];
                    snprintf(h, 3, "%02x", desc[k]);
                    build += h;
                }
            o += sizeof *nh + ((nh->n_namesz + 3) & ~3u) + ((nh->n_descsz + 3) & ~3u);
        }
    }
    // reserve the whole image, then map each segment's file bytes over it
    long pg = sysconf(_SC_PAGESIZE);
    auto base = (uintptr_t)mmap(nullptr, top + pg, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD || !ph[i].p_filesz) continue;
        uintptr_t va = ph[i].p_vaddr & ~(uintptr_t)(pg - 1);
        size_t skew = ph[i].p_vaddr - va;
        if (mmap((void*)(base + va), ph[i].p_filesz + skew, PROT_READ, MAP_PRIVATE | MAP_FIXED, fd,
                 ph[i].p_offset - skew) == MAP_FAILED) {
            perror("mmap segment");
            return 2;
        }
    }
    printf("%s: build-id %s, mapped at 0x%lx\n", argv[1], build.c_str(), (unsigned long)base);
    bg3se::manifest::load(build.c_str(), base, ph, eh->e_phnum);
    return bg3se::g_rows > 0 && bg3se::g_verified == bg3se::g_rows ? 0 : 1;
}
