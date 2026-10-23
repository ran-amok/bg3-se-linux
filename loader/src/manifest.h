// manifest.h — the addresses in bin/bg3 and libOsiris.so the extender may touch, per build
// .
//
// manifest/<build-id>.tsv is embedded at build time (manifest_blob.S); BG3SE_MANIFEST=path
// replaces the embedded set with one file, for the tests and for trying a new build's
// manifest without a rebuild. A build with no manifest is unknown: the loader stays
// passive there unless BG3SE_ALLOW_UNKNOWN is set, and even then every lookup below
// comes back empty. tools/manifest.py writes and audits the files offline.
//
// load() checks every row against the running image before any of it is used: a
// function must lie in an executable segment and begin with the bytes the manifest
// recorded, and hook::probe decides whether it may be hooked; a global must lie in a
// writable segment; a read-only object (rodata) must lie in a non-writable segment and
// hold exactly the bytes recorded. A row that fails is off, and so is every feature
// that requires it.
#pragma once

#include <link.h>

#include <cstdint>
#include <initializer_list>

namespace bg3se::manifest {

// Is there a manifest for this build? No allocation and no lasting state beyond a
// static buffer: safe in the constructor, before the game forks.
bool known(const char* build_id);

// Parse this build's rows and verify them in memory (base = the PIE load base).
// Once, from deferred_init, before any hook goes in. Logs one line per failed row
// and a summary.
void load(const char* build_id, uintptr_t base);
// The same over another image's program headers (test/manifest_check.cpp maps bin/bg3
// and runs the embedded manifest against it, without the game). One load per build-id;
// rows from every module share one table, looked up by symbol.
void load(const char* build_id, uintptr_t base, const ElfW(Phdr)* phdr, size_t phnum);
// A shared library already mapped in the process (libOsiris.so: manifest/b5dc….tsv),
// found by soname through dl_iterate_phdr; its build-id picks its section of the blob.
// After load() of the game's own rows, from deferred_init.
void load_library(const char* soname);
// The GNU build-id note of a mapped image, as hex into out; false if it has none.
bool build_id_of(uintptr_t base, const ElfW(Phdr)* phdr, size_t phnum, char* out, size_t n);

// The runtime address of a row that verified; nullptr if the row is absent or off.
void* get(const char* symbol);
// ... and whether hook::install() may take it (fn rows that probed ok).
bool hookable(const char* symbol);

// True if every symbol verified (and, for `hook`, probed ok). Otherwise logs the
// feature as off with what it lacks. A feature calls this once and keeps the answer.
bool require(const char* feature, std::initializer_list<const char*> symbols);
bool require_hooks(const char* feature, std::initializer_list<const char*> symbols);

}  // namespace bg3se::manifest
