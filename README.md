# bg3-se-linux

A port of [Norbyte's Baldur's Gate 3 Script Extender](https://github.com/Norbyte/bg3se) to the
**native Linux build** of Baldur's Gate 3 (Steam app 1086940, `bin/bg3`), with no Proton or Wine.

**If you want to play with Script Extender mods on native Linux, use
[bg3le](https://github.com/lenonk/bg3le)** ([Nexus](https://www.nexusmods.com/baldursgate3/mods/25431)),
not this. bg3le was started independently around the same time as this project. It is
maintained and released, has an installer, and is built to survive game updates. This repository
is **not maintained**. It is published as it stood on 2026-10-22, as source and reference
material: a second, independent reading of the same binary, and the tools used to make it.
Issues and discussions are off on purpose: nobody is answering them.

## How this was made: AI involvement

Nearly all of the code, tooling and reverse engineering here was written by Claude. 
Here is what was checked rather than assumed:

- Every game address the loader touches is in a per-build **offset manifest**
  (`manifest/<build-id>.tsv`). Each row has a witness check, and the loader re-verifies it
  against the running binary before using it. On a build with no manifest the extender stays
  passive instead of guessing.
- Inline hooks go through a fail-closed x86-64 decoder (`loader/src/hook.cpp`). Its length
  decoding is calibrated against capstone over every function in the game binary
  (`tools/hook-decode-check.py`).
- Upstream's game-structure mirrors were written for MSVC. Here they are checked against the
  game's Itanium layout: `static_assert`s carry a binary witness each, and component sizes are
  checked in game against the engine's own registry (`ValidateMappedComponentSizes`, which
  reported no mismatches when last run).
- `tools/osi-sweep.sh` runs a scripted probe of the Lua/Osiris surface over the extender console.
  On the last runs it passed 186/186 with nine mods loaded (MCM among them), plus `Ext.Net`
  messages in both directions between two game instances on one machine, hosting and joining a
  LAN lobby. It was never tested against a Windows bg3se peer.
- Most tools have a `--selftest`: a calibration run on known input, with planted errors that
  must be caught.

## What works

On the builds below, upstream's extender runs nearly unchanged (every Linux edit is marked
`PORT (linux)`):

- Script Extender mods load from their paks: bootstraps, `Ext.Require`, `PersistentVars`.
- Osiris: calls, queries, events, `RegisterListener`.
- `Ext.Entity`, with ECS names bound from the binary's own `TypeId` table (`typeids/`).
- Stats, static data, templates, `Ext.IO`, `Ext.Vars`.
- `Ext.IMGUI`. Mod Configuration Menu's window opens on Insert.
- `Ext.UI` over the game's Noesis.
- `Ext.Net`, over the real wire as well as locally.
- A joiner without the extender is refused with the game's own "mods do not match" dialog (the
  `RefusePeersWithoutExtender` setting).

Supported game builds (ELF build-id), and nothing else:

| Steam build | Game version | build-id |
|---|---|---|
| 24532579 | 4.1.1.7398727 | `c8da62b1…` |
| 25605617 | 4.1.1.7631656 | `a187af94…` |

Any later patch needs its own manifest and TypeId table. `tools/rebase.py` carries both across a
small patch: it pairs the two builds' functions one for one, and refuses whatever it cannot
prove.

## Layout

- `loader/`: `libbg3se.so`, the `LD_PRELOAD` object. It holds the hook engine, the manifest
  checks, the crash reader and the Osiris tap, plus its tests (`loader/test/run.sh`, against a
  stand-in `bg3`; no game needed).
- `ext/`: the extender proper, `libbg3se-ext.so`, which the loader dlopens. `ext/src` is
  upstream's tree, vendored at the commit in `ext/src/VENDORED`. `ext/linux` is the Win32 shim,
  entry and setup; `ext/toolchain` holds the libc++ ABI-v2 build and the `std::variant` layout
  overlay.
- `manifest/`, `typeids/`: per-build data, embedded into the two objects at build time.
- `tools/`: launch wrapper (`bg3se-launch.sh`), console client (`bg3se-console`), the glibc floor
  check, the probe sweep, and the per-build pipeline. `harvest.py` → `typeids.py`; `features.py` +
  `match.py` → `targets.py` → `manifest.py` (name transfer from the macOS build, which keeps
  symbols the Linux one lost to LTO); `rebase.py` for patches. Also `disasm.py`/`macdis.py`,
  `lspk.py` (LSPK paks) and `modsettings.py`.

  The tools default to Steam's standard install paths. A few selftests calibrate on one particular
  game build, and they look for it under `$BG3_SPECIMENS/<Steam build>_<ELF build-id>/bin/bg3` (the
  headers of `manifest/` and `typeids/` name those directories). When the variable is unset, those
  checks are skipped and the output says so. `install-test-mod.sh` packs upstream's SampleMod and
  needs `UPSTREAM` pointed at a checkout of upstream.

## Building

Needs zig 0.16 (the C/C++ compiler, targeting glibc 2.31, the floor of Steam's "sniper" runtime
the game runs in), CMake, Ninja, curl, unzip, Python 3.14, and `protoc` 36.1 (configure refuses
any other version). The Python tools also want `numpy` and `capstone`.

    ext/fetch-externals.sh
    cmake -S loader -B build/loader --toolchain $PWD/loader/cmake/zig-glibc-2.31.cmake
    cmake --build build/loader
    cmake -S ext -B build/ext -G Ninja --toolchain $PWD/loader/cmake/zig-glibc-2.31.cmake
    ninja -C build/ext
    loader/test/run.sh
    tools/check-floor.sh build/loader/libbg3se.so

The first configure of `build/ext` builds libc++ (ABI v2, which the game uses), protobuf-lite and
abseil from pinned sources. `fetch-externals.sh` downloads the NoesisGUI 3.1.7 SDK headers from
upstream's mirror. They are proprietary and never vendored.

`tools/release.sh` does all of the above from a clean tree, runs every check that needs no game,
and packs the two objects, the launch wrapper and the console client into
`dist/bg3-se-linux-<version>.tar.xz`. GitHub Actions runs it on every push
(`.github/workflows/build.yml`) and publishes that tarball as a release on a `v*` tag. The
tarball unpacks to the same layout the wrapper expects, so the launch option is the unpacked
`tools/bg3se-launch.sh` (next section), with no build step.

## Running

Set the game's Steam launch option to the wrapper, by **absolute path** (Steam changes into the
game's `bin/` first):

    /path/to/bg3-se-linux/tools/bg3se-launch.sh %command%

Logs go to `~/.local/state/bg3-se-linux/` (`bg3se.log`, `extender.log`, `osiris.log`).
`tools/bg3se-console` connects to the running game's console (next section).
`build/loader/manifest-check <path to bin/bg3>` checks whether your installed build is supported,
without starting the game.

## The console

Upstream's debug console opens a Windows console window. Here it is a Unix socket instead,
`console.sock` in the same state directory as the logs (`$XDG_STATE_HOME/bg3-se-linux/`, by
default `~/.local/state/bg3-se-linux/`). It is always on while the extender runs. The socket
appears once the game has started; the client tells you if it cannot find one.
`tools/bg3se-console` is the client. It needs `socat`, or an `nc` that supports `-U`:

    tools/bg3se-console                          # interactive session
    tools/bg3se-console '_P(Ext.Utils.Version())'   # one command, then disconnect

Interactive mode uses socat's readline support when socat was built with it (line editing, with
history in `~/.bg3se_console_history`). Otherwise input is plain line mode. One-shot mode sends
the command, waits half a second, and disconnects, so it suits quick commands. Run anything
slower in an interactive session. Set `BG3SE_CONSOLE_SOCK` to use a socket somewhere else.

The commands are upstream's:

- A line that is not one of the commands below runs as Lua in the current context. `_P` prints a
  value, `_D` dumps it, and `_C()` is the host character's entity, e.g. `_D(_C().Health)`.
- `server` and `client` switch context. The console starts in the server context, and the context
  carries over from one session to the next. The prompt shows which context you are in: `S >> `
  or `C >> `. The server context has Osiris; the client context has `Ext.IMGUI` and `Ext.UI`.
  The server context takes commands only while a game is loaded, running or paused. The client
  context also takes them at the main menu and in the lobby. In any other state, commands are
  refused with an error.
- To enter multi-line Lua, type `--[[` on a line by itself, then the code, then `]]--` on a line by
  itself. The prompt changes to `S -->> ` (or `C -->> `) while a block is open.
- `!name arg …` runs a command that a mod registered with `Ext.RegisterConsoleCommand`.
- `reset`, `reset server` and `reset client` reload the Lua state. `help` lists the commands, and
  `exit` ends the session. `clear` and `silence on|off` are accepted, but they act on the game's
  stderr, not on the socket.

Each command runs on the game thread for its context, and the console waits for it to finish.
Everything the extender prints while you are connected is echoed to you. Nothing is buffered for
a client that connects later; `extender.log` is the complete record. The socket serves one
session at a time, and a second client waits until the first one leaves. A client that stops
reading loses output. The extender never blocks a game thread on the socket, so it drops those
lines and tells you how many it dropped when you read again.
