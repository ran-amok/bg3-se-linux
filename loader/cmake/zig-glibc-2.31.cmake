# Toolchain: zig's bundled clang/lld against glibc 2.31 headers and stubs — the
# sniper runtime's glibc, which is the floor (PLAN, Phase 2). zig links libc++,
# libc++abi and libunwind statically, so nothing depends on the host's libstdc++.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER "${CMAKE_CURRENT_LIST_DIR}/zig-cc")
set(CMAKE_CXX_COMPILER "${CMAKE_CURRENT_LIST_DIR}/zig-c++")
# zig 0.16.0's linker driver segfaults on `--dependency-file`, which CMake >= 3.27
# passes by itself once it sees an lld-compatible linker.
set(CMAKE_LINK_DEPENDS_USE_LINKER OFF)
