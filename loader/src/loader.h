// loader.h — what preload.cpp shares with the hook files.
#pragma once

namespace bg3se {

// Set once by the constructor, before any hook can run.
extern bool g_is_game;  // this process is bin/bg3
extern bool g_active;   // ... and a build we know (or BG3SE_ALLOW_UNKNOWN is set)

void log_line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
const char* log_path();

// Per-thread line-formatting scratch, mmap'd on first use on that thread and never
// freed (one mapping per thread that logs, tens of them in the game). A `char buf[8192]`
// on the *caller's* stack would be an 8488-byte frame, which is more than some of the
// game's threads have left (the Osiris thread overflows inside log_line). Nothing that
// formats a log line may put its buffer on a game thread's stack.
//
// The slots are distinct because the users nest: osiris' log_function formats the
// arguments and then calls osi_line, and the crash handler copies the faulting stack
// while it logs. Within one slot a signal handler on the same thread reuses its own
// interrupted buffer, which loses that half-formatted line and nothing else.
enum { kScratchLog = 0, kScratchOsi, kScratchOsiArgs, kScratchCrash, kScratchSlots };
constexpr unsigned kScratchSize = 8192;
char* scratch(int slot);

// Idempotent; every interposer calls it before doing anything of its own.
void deferred_init(const char* via);

}  // namespace bg3se
