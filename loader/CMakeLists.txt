cmake_minimum_required(VERSION 3.20)
project(bg3se-linux-loader CXX)
enable_language(ASM)

# Configure with:
#   cmake -S loader -B build/loader --toolchain loader/cmake/zig-glibc-2.31.cmake
# and check the result with tools/check-floor.sh before it goes near the game.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# -mcx16: hook.cpp patches with a 16-byte cmpxchg where an 8-byte store cannot cover
add_library(hook OBJECT src/hook.cpp)
target_compile_options(hook PRIVATE -Wall -Wextra -fvisibility=hidden -fPIC -mcx16)
# The offset manifests (manifest/<build-id>.tsv), embedded whole. Editing one
# re-runs the configure step, which rewrites the blob and the .S that embeds it.
file(GLOB MANIFESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../manifest/*.tsv")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${MANIFESTS})
set(blob "")
foreach(m IN LISTS MANIFESTS)
    file(READ "${m}" text)
    string(APPEND blob "${text}")
endforeach()
# The blob's file name carries its hash, and so does the generated .S that .incbin's it:
# zig's compile cache is keyed on the source text and command line, not on included
# binaries, so an unchanged .S with a changed blob would come back stale, and the
# built object would ship rows missing from the embedded set.
string(SHA1 MANIFEST_BLOB_SHA1 "${blob}")
set(MANIFEST_BLOB "${CMAKE_CURRENT_BINARY_DIR}/manifests-${MANIFEST_BLOB_SHA1}.txt")
if(NOT EXISTS "${MANIFEST_BLOB}")
    file(WRITE "${MANIFEST_BLOB}" "${blob}")
endif()
configure_file(src/manifest_blob.S.in "${CMAKE_CURRENT_BINARY_DIR}/manifest_blob.S" @ONLY)
set(MANIFEST_BLOB_S "${CMAKE_CURRENT_BINARY_DIR}/manifest_blob.S")

add_library(bg3se SHARED src/preload.cpp src/sdl_trace.cpp src/osiris.cpp src/manifest.cpp src/crash.cpp
    "${MANIFEST_BLOB_S}"
    $<TARGET_OBJECTS:hook>)
target_compile_options(bg3se PRIVATE -Wall -Wextra -fvisibility=hidden -fno-omit-frame-pointer)
target_link_options(bg3se PRIVATE
    # `local: *` in the map is what keeps the static C++ runtime's symbols inside
    # (zig's linker driver rejects --exclude-libs).
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.map"
    -Wl,-z,nodelete)
target_link_libraries(bg3se PRIVATE dl pthread)
set_target_properties(bg3se PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/exports.map")

# Smoke test without the game: a stand-in named "bg3" that forks without exec, calls
# SDL_Init from a stub libSDL2.so, drives a stub libOsiris.so, and exits. See test/run.sh.
enable_language(C)
add_library(SDL2 SHARED test/stub_sdl.c)
add_library(Osiris SHARED test/stub_osiris.cpp)
target_compile_options(Osiris PRIVATE -fvisibility=hidden)
add_executable(fake-bg3 test/fake_bg3.c test/fake_osiris_client.cpp)
set_target_properties(fake-bg3 PROPERTIES OUTPUT_NAME bg3 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fake")
set_target_properties(SDL2 Osiris PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fake")
target_link_libraries(fake-bg3 PRIVATE SDL2 Osiris pthread)
target_link_options(fake-bg3 PRIVATE -Wl,--build-id=sha1 "-Wl,-rpath,$ORIGIN")

# The hooking engine on its own: hook-test relocates and patches hand-written
# prologues; decode-dump feeds tools/hook-decode-check.py (the decoder against capstone).
add_executable(hook-test test/hook_test.cpp test/hook_test_fns.S $<TARGET_OBJECTS:hook>)
target_include_directories(hook-test PRIVATE src)
target_link_libraries(hook-test PRIVATE pthread)
add_executable(decode-dump test/decode_dump.cpp $<TARGET_OBJECTS:hook>)
target_include_directories(decode-dump PRIVATE src)

# The loader's manifest verification against a game binary on disk (test/manifest_check.cpp)
add_executable(manifest-check test/manifest_check.cpp src/manifest.cpp "${MANIFEST_BLOB_S}"
    $<TARGET_OBJECTS:hook>)
target_include_directories(manifest-check PRIVATE src)
