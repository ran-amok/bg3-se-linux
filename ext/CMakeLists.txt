cmake_minimum_required(VERSION 3.20)
project(bg3se-linux-ext CXX C)
enable_language(ASM)

# The ported extender: upstream's tree (ext/src, MIT + Commons Clause) plus the Linux
# platform layer (ext/linux), built as libbg3se-ext.so, which the loader (loader/)
# dlopens at deferred init on a known build. Configure with the loader's toolchain:
#   cmake -S ext -B build/ext -G Ninja --toolchain loader/cmake/zig-glibc-2.31.cmake
# Prerequisites, all checked here: ext/fetch-externals.sh (pinned third-party
# sources), ext/toolchain/libcxx-overlay.sh (the patched libc++ headers, made at configure
# time) and ext/toolchain/build-libcxx-v2.sh (libc++ in the game's ABI, built on demand at
# configure time, against that same overlay).

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
set(REPO "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(LINUX "${CMAKE_CURRENT_SOURCE_DIR}/linux")
set(B "${SRC}/BG3Extender")

# ---- externals (ext/fetch-externals.sh), checked by content
set(EXT_EXTERNAL "${REPO}/build/ext-external" CACHE PATH "where fetch-externals.sh put the sources")
foreach(w "glm/glm/glm.hpp" "rapidjson/include/rapidjson/document.h" "tinycrypt/lib/include/tinycrypt/sha256.h"
          "lua/src/lstate.h" "optick/src/optick.h" "SDL/include/SDL_events.h" "imgui/imgui.h" "Vulkan/include/vulkan/vulkan.h"
          "protobuf/src/google/protobuf/message_lite.h" "abseil/absl/base/config.h")
    if(NOT EXISTS "${EXT_EXTERNAL}/${w}")
        message(FATAL_ERROR "missing ${EXT_EXTERNAL}/${w}: run ext/fetch-externals.sh first")
    endif()
endforeach()

# ---- the libc++ sfinae-tag overlay: ext/toolchain/libcxx-overlay.sh (PORT (linux))
# zig's <variant>, <optional> and __tuple/sfinae_helpers.h with toolchain/libcxx-sfinae-tag.patch
# applied, so a std::variant whose alternative is or starts with a variant or an optional has no
# 8-byte hole at the front -- the game's ls::Variant has none, and the two spell_cast mirrors
# only reach their 160/960 bytes this way.  The
# directory's name is a hash of the patch and the stock headers, so a changed patch or a new
# zig is a new command line (zig's cache keys on it); a patch that no longer applies fails the
# configure.  It is -I (an -isystem overlay loses to zig's own), it goes in here, before the
# abseil/protobuf add_subdirectory, so every C++ TU in the tree inherits it, and libc++v2.a
# below is built against the same directory: one definition of the two templates everywhere.
#   ext/toolchain/libcxx-overlay.sh --check build/ext    every C++ compile line carries it
set(LIBCXX_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/toolchain/libcxx-sfinae-tag.patch")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${LIBCXX_PATCH}")
execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/toolchain/libcxx-overlay.sh" "${REPO}/build/libcxx-overlay" "${LIBCXX_PATCH}"
                OUTPUT_VARIABLE LIBCXX_OVERLAY OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE rc)
if(NOT rc EQUAL 0 OR NOT IS_DIRECTORY "${LIBCXX_OVERLAY}" OR NOT EXISTS "${LIBCXX_OVERLAY}/variant")
    message(FATAL_ERROR "libc++ overlay failed (ext/toolchain/libcxx-overlay.sh, rc ${rc}): see above")
endif()
get_filename_component(LIBCXX_OVERLAY_ID "${LIBCXX_OVERLAY}" NAME)
message(STATUS "libc++ overlay ${LIBCXX_OVERLAY_ID}: ${LIBCXX_OVERLAY}")

# ---- libc++ in ABI version 2 (std::__2, the game's): ext/toolchain/build-libcxx-v2.sh
# Rebuilt when the archive is missing or was built against another overlay (its .overlay
# stamp names the one it saw).
set(LIBCXX_V2 "${REPO}/build/libcxx-v2/libc++v2.a" CACHE FILEPATH "libc++ built with _LIBCPP_ABI_VERSION=2")
set(_libcxx_have "")
if(EXISTS "${LIBCXX_V2}" AND EXISTS "${LIBCXX_V2}.overlay")
    file(READ "${LIBCXX_V2}.overlay" _libcxx_have)
    string(STRIP "${_libcxx_have}" _libcxx_have)
endif()
if(NOT EXISTS "${LIBCXX_V2}" OR NOT _libcxx_have STREQUAL LIBCXX_OVERLAY_ID)
    if(EXISTS "${LIBCXX_V2}")
        message(STATUS "rebuilding libc++ ABI v2: archive saw overlay '${_libcxx_have}', this tree uses ${LIBCXX_OVERLAY_ID}")
    else()
        message(STATUS "building libc++ ABI v2 (once)")
    endif()
    get_filename_component(libcxx_dir "${LIBCXX_V2}" DIRECTORY)
    execute_process(COMMAND ${CMAKE_COMMAND} -E env "LIBCXX_OVERLAY=${LIBCXX_OVERLAY}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/toolchain/build-libcxx-v2.sh" "${libcxx_dir}"
                    RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0 OR NOT EXISTS "${LIBCXX_V2}")
        message(FATAL_ERROR "libc++ v2 build failed")
    endif()
endif()
# every C++ TU in this project sees the v2 headers' configuration, and the overlay
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-D_LIBCPP_ABI_VERSION=2>
                    $<$<COMPILE_LANGUAGE:CXX>:-D_LIBCPP_ABI_NAMESPACE=__2>
                    $<$<COMPILE_LANGUAGE:CXX>:-Wno-macro-redefined>
                    "$<$<COMPILE_LANGUAGE:CXX>:-I${LIBCXX_OVERLAY}>")

# ---- upstream's generators, on the vendored tree (they write into it, as on Windows)
set(GEN "${B}/GameDefinitions/Generated")
# PORT (linux): the generators read every header under GameDefinitions/, so the dependency
# glob has to as well: a subdirectory missing from it means a member added or removed there
# does not re-run make_property_map.py -- silently stale on an addition, and a confusing
# PropertyMaps.inl compile error on a removal.
file(GLOB GEN_INPUTS CONFIGURE_DEPENDS "${B}/GameDefinitions/PropertyMaps/*.inl" "${B}/GameDefinitions/Enumerations/*.inl"
     "${B}/GameDefinitions/ExternalEnumerations/*.inl"
     "${B}/GameDefinitions/Base/*.h" "${B}/GameDefinitions/Components/*.h"
     "${B}/GameDefinitions/Controllers/*.h" "${B}/GameDefinitions/Stats/*.h"
     "${B}/GameDefinitions/*.h")
set(GEN_OUTPUTS "${GEN}/Enumerations.inl" "${GEN}/ExternalEnumerations.inl" "${GEN}/EnumerationMeta.h"
                "${GEN}/PropertyMaps.inl" "${GEN}/PropertyMapNames.inl" "${GEN}/PropertyMapMeta.h" "${GEN}/ComponentTypes.inl")
add_custom_command(OUTPUT ${GEN_OUTPUTS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GEN}"
    COMMAND python3 make_enumerations.py
    COMMAND python3 make_property_map.py
    WORKING_DIRECTORY "${B}"
    DEPENDS "${B}/make_enumerations.py" "${B}/make_property_map.py" ${GEN_INPUTS}
    COMMENT "upstream generators: enumerations and property maps")
add_custom_target(generated DEPENDS ${GEN_OUTPUTS})

# ---- the builtin Lua scripts, packed and linked in
# (the packer also writes the .S that .incbin's the bundle, with the bundle's hash in it,
# because zig's compile cache does not see what .incbin pulled in)
set(LUA_BUNDLE "${CMAKE_CURRENT_BINARY_DIR}/lua.bundle")
set(LUA_BUNDLE_S "${CMAKE_CURRENT_BINARY_DIR}/lua_bundle.S")
file(GLOB_RECURSE LUA_SCRIPTS CONFIGURE_DEPENDS "${B}/LuaScripts/*.lua")
add_custom_command(OUTPUT "${LUA_BUNDLE}" "${LUA_BUNDLE_S}"
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tools/bundle-lua.py" "${B}/LuaScripts" "${LUA_BUNDLE}" "${LUA_BUNDLE_S}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/bundle-lua.py" ${LUA_SCRIPTS}
    COMMENT "packing LuaScripts/")

# ---- the ECS TypeId tables (typeids/<build-id>.tsv, tools/typeids.py), embedded whole, read by
# ext/linux/typeids.cpp: the loader's manifest pattern. Editing one re-runs the configure step,
# which rewrites the blob and the .S that .incbin's it; the blob's name and the .S carry its hash,
# because zig's compile cache is keyed on source text and command line, not on what .incbin
# pulled in.
file(GLOB TYPEID_TABLES CONFIGURE_DEPENDS "${REPO}/typeids/*.tsv")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${TYPEID_TABLES})
set(blob "")
foreach(t IN LISTS TYPEID_TABLES)
    file(READ "${t}" text)
    string(APPEND blob "${text}")
endforeach()
string(SHA1 TYPEIDS_BLOB_SHA1 "${blob}")
set(TYPEIDS_BLOB "${CMAKE_CURRENT_BINARY_DIR}/typeids-${TYPEIDS_BLOB_SHA1}.txt")
if(NOT EXISTS "${TYPEIDS_BLOB}")
    file(WRITE "${TYPEIDS_BLOB}" "${blob}")
endif()
configure_file("${LINUX}/typeids_blob.S.in" "${CMAKE_CURRENT_BINARY_DIR}/typeids_blob.S" @ONLY)
set(TYPEIDS_BLOB_S "${CMAKE_CURRENT_BINARY_DIR}/typeids_blob.S")

# ---- Lua (Norbyte's fork), as upstream's LuaLib: a static library, compiled as C++
# (LuaLib.vcxproj: CompileAsCpp; the fork's headers need it, and the extender includes
# them without extern "C")
file(GLOB LUA_SRCS "${EXT_EXTERNAL}/lua/src/*.c")
list(FILTER LUA_SRCS EXCLUDE REGEX "/(lua|luac)\\.c$")
set_source_files_properties(${LUA_SRCS} PROPERTIES LANGUAGE CXX)
add_library(lua STATIC ${LUA_SRCS})
target_compile_options(lua PRIVATE -x c++ -O2 -fPIC -w -include cstdint)
target_compile_definitions(lua PRIVATE LUA_USE_LINUX)

# ---- Optick, the profiler upstream links even when profiling is off (its RegisterThread
# calls are unconditional); the GPU backends are left out
set(OPTICK_SRCS ${EXT_EXTERNAL}/optick/src/optick_capi.cpp ${EXT_EXTERNAL}/optick/src/optick_core.cpp
    ${EXT_EXTERNAL}/optick/src/optick_message.cpp ${EXT_EXTERNAL}/optick/src/optick_miniz.cpp
    ${EXT_EXTERNAL}/optick/src/optick_serialization.cpp ${EXT_EXTERNAL}/optick/src/optick_server.cpp)
add_library(optick STATIC ${OPTICK_SRCS})
target_compile_options(optick PRIVATE -O2 -fPIC -w -fvisibility=hidden)
target_compile_definitions(optick PRIVATE USE_OPTICK=1 OPTICK_ENABLE_GPU=0 OPTICK_LIB=1)

# ---- the extender
set(UPSTREAM_TUS
    ${SRC}/CoreLib/Base/Base.cpp ${SRC}/CoreLib/Console.cpp ${SRC}/CoreLib/Crypto.cpp
    ${SRC}/CoreLib/MurmurHash3.cpp ${SRC}/CoreLib/Utils.cpp
    ${B}/Extender/ScriptExtender.cpp
    ${B}/Extender/Server/ScriptExtenderServer.cpp
    ${B}/Extender/Client/ScriptExtenderClient.cpp ${B}/Extender/Client/ExtensionStateClient.cpp
    ${B}/Extender/Client/VisualHelpers.cpp ${B}/Extender/Client/SDL.cpp
    ${B}/Extender/Shared/Console.cpp ${B}/Extender/Shared/ExtensionState.cpp ${B}/Extender/Shared/Hooks.cpp
    # PORT (linux): the wire.  These three are upstream's own, with no Linux stand-in in front
    # of them: ExtenderNet.cpp carries BaseNetworkManager::Update, OnResetExtensionState and
    # ProcessLocalMessages, and the two NetworkManager.cpp files carry HandleLocalMessage.  So
    # the in-process path single-player uses is upstream's: Lua pushes a LocalMessage
    # (ClientNet.inl / ServerNet.inl) and the queue is drained by Update() -- the server's from
    # esv::ScriptExtender::PreUpdate/PostUpdate, the client's from ecl::ScriptExtender::OnECSTick
    # (ScriptExtenderClient.cpp; ecl::GameStateMachine::Update is inlined on this build).
    ${B}/Extender/Shared/ExtenderNet.cpp
    ${B}/Extender/Server/ServerNetworking.cpp
    ${B}/Extender/Client/ClientNetworking.cpp
    ${B}/Extender/Shared/ScriptHelpers.cpp ${B}/Extender/Shared/tinyxml2.cpp ${B}/Extender/Shared/Utils.cpp
    ${B}/Extender/Client/IMGUI/IMGUI.cpp    # object model + Vulkan.inl (the backend; DX11.inl stays out)
    ${B}/GameDefinitions/Base/Base.cpp ${B}/GameDefinitions/EntitySystem.cpp ${B}/GameDefinitions/Enumerations.cpp
    ${B}/GameDefinitions/GameDefinitionHelpers.cpp ${B}/GameDefinitions/GameHelpers.cpp
    ${B}/GameDefinitions/GlobalFixedStrings.cpp ${B}/GameDefinitions/Stats/Stats.cpp
    ${B}/GameDefinitions/Stats/StatsObject.inl
    ${B}/GameHooks/DataLibraries.cpp ${B}/GameHooks/EngineHooks.cpp ${B}/GameHooks/OsirisWrappers.cpp
    ${B}/Lua/Client/LuaClient.cpp ${B}/Lua/Libs/LuaSharedLibs.cpp ${B}/Lua/LuaBinding.cpp
    ${B}/Lua/LuaSerializers.cpp ${B}/Lua/Osiris/LuaOsirisBinding.cpp ${B}/Lua/Server/LuaServer.cpp
    ${B}/Lua/Shared/LuaBundle.cpp ${B}/Lua/Shared/LuaInternalHelpers.cpp ${B}/Lua/Shared/LuaStats.cpp
    ${B}/Lua/Shared/Proxies/LuaObjectProxies.cpp ${B}/Lua/Shared/Proxies/LuaTypeInformation.cpp
    ${B}/Osiris/Functions/FunctionLibrary.cpp ${B}/Osiris/Functions/FunctionUtilities.cpp
    ${B}/Osiris/Functions/LuaFunctions.cpp ${B}/Osiris/OsirisExtender.cpp
    ${B}/Osiris/Shared/CustomFunctions.cpp ${B}/Osiris/Shared/NodeHooks.cpp ${B}/Osiris/Shared/OsirisHelpers.cpp)
# StatsObject.inl is a translation unit of its own upstream (a ClCompile item)
set_source_files_properties(${B}/GameDefinitions/Stats/StatsObject.inl PROPERTIES LANGUAGE CXX)
# imgui itself, and its Vulkan and SDL2 backends (upstream's; the DX11 one stays out)
set(IMGUI_SRCS ${EXT_EXTERNAL}/imgui/imgui.cpp ${EXT_EXTERNAL}/imgui/imgui_draw.cpp
    ${EXT_EXTERNAL}/imgui/imgui_tables.cpp ${EXT_EXTERNAL}/imgui/imgui_widgets.cpp ${EXT_EXTERNAL}/imgui/imgui_demo.cpp
    ${EXT_EXTERNAL}/imgui/backends/imgui_impl_vulkan.cpp ${EXT_EXTERNAL}/imgui/backends/imgui_impl_sdl2.cpp)
# SDL is the game's own bin/libSDL2.so, bound at dlopen through weak references
# (ext/linux/sdl_weak.h) -- no NEEDED beyond glibc (tools/check-floor.sh), and the
# stand-in test loads the extender with none of them resolved
set_source_files_properties(${EXT_EXTERNAL}/imgui/backends/imgui_impl_sdl2.cpp ${B}/Extender/Client/SDL.cpp
    ${B}/Lua/Libs/LuaSharedLibs.cpp
    PROPERTIES COMPILE_OPTIONS "-include;${LINUX}/sdl_weak.h")
# Vulkan is the loader the game dlopens, by pointer (ext/linux/vulkan_fns.h): no prototypes in
# the two TUs that touch it, nothing linked
set_source_files_properties(${EXT_EXTERNAL}/imgui/backends/imgui_impl_vulkan.cpp ${B}/Extender/Client/IMGUI/IMGUI.cpp
    PROPERTIES COMPILE_DEFINITIONS "VK_NO_PROTOTYPES;IMGUI_IMPL_VULKAN_NO_PROTOTYPES")
set(LINUX_SRCS
    ${LINUX}/win32.cpp ${LINUX}/entry.cpp ${LINUX}/setup.cpp ${LINUX}/stubs.cpp ${LINUX}/typeids.cpp
    ${LINUX}/vulkan.cpp
    "${LUA_BUNDLE_S}" "${TYPEIDS_BLOB_S}"
    ${REPO}/loader/src/hook.cpp)

# ---- NoesisGUI
# The pinned SDK's Include/ goes in front of ext/linux/shim, and everything in the tree that
# touches NoesisGUI is compiled.  The SDK is headers only here -- upstream reaches Noesis
# through GameDefinitions/Symbols.h's proc pointers rather than by linking against it -- so
# nothing is added to the link line and nothing needs a manifest row.
# The pin is 3.1.7 and the game's Noesis is 3.1.6; tools/noesis-version.py proves the two
# ABI-identical, which is what makes the pinned headers usable at all.
# There is no option to build without the SDK: the tree compiles against the pinned headers,
# links with 0 undefined Noesis symbols and passes check-floor.sh, and no second state is kept
# buildable.
# ---- protobuf (PORT (linux))
# upstream's wire half of Ext.Net is protobuf end to end: Extender/Shared/ExtenderProtocol.proto
# -> MessageWrapper, carried by ExtenderMessage over the game's net::Message.  The .proto says
# `optimize_for = LITE_RUNTIME`, so only libprotobuf-lite is built here -- the full runtime, the
# upb runtime, libprotoc and the protoc binary are all off, and so is zlib (a host library, and
# the lite runtime does not need it).  Both subdirectories are EXCLUDE_FROM_ALL, so `ninja` builds
# only what libbg3se-ext.so actually links.  Everything inherits this directory's compile options,
# which is what matters: protobuf's API is full of std::string, so the runtime has to be built in
# the game's libc++ ABI v2 like the rest of the tree, and that rules out the host's libprotobuf.
#
# The .pb pair is generated into the build tree and never committed, as on Windows
# (generate-proto.bat writes it into the source tree; here it would be an untracked file inside
# ext/src).  The generator is the host's protoc, which must be the pinned runtime's own version:
# the generated code's PROTOBUF_VERSION assert would catch a mismatch at compile time, but a
# configure-time check says so in one line instead of a hundred.
set(PROTOBUF_SRC "${EXT_EXTERNAL}/protobuf")
file(READ "${PROTOBUF_SRC}/version.json" _pb_version_json)
string(JSON _pb_major MEMBER "${_pb_version_json}" 0)   # the one key, "36.x"
string(JSON _pb_protoc_version GET "${_pb_version_json}" "${_pb_major}" "protoc_version")
find_program(PROTOC_EXE NAMES protoc)
if(NOT PROTOC_EXE)
    message(FATAL_ERROR "no protoc on PATH: the pinned protobuf is ${_pb_protoc_version}, install that")
endif()
execute_process(COMMAND "${PROTOC_EXE}" --version OUTPUT_VARIABLE _protoc_out OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _protoc_out MATCHES "^libprotoc ${_pb_protoc_version}$")
    message(FATAL_ERROR "protoc is '${_protoc_out}', the pinned protobuf runtime is ${_pb_protoc_version}: "
                        "the generated .pb.cc and the runtime must come from the same version")
endif()
message(STATUS "protobuf ${_pb_protoc_version}: ${PROTOC_EXE}, lite runtime from ${PROTOBUF_SRC}")

set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_ENABLE_INSTALL OFF)
set(ABSL_BUILD_TESTING OFF)
set(protobuf_INSTALL OFF)
set(protobuf_BUILD_TESTS OFF)
set(protobuf_BUILD_CONFORMANCE OFF)
set(protobuf_BUILD_EXAMPLES OFF)
set(protobuf_BUILD_LIBPROTOBUF OFF)     # the lite runtime is what the .proto asks for
set(protobuf_BUILD_LIBPROTOC OFF)
set(protobuf_BUILD_PROTOC_BINARIES OFF) # the host's protoc, checked above
set(protobuf_BUILD_LIBUPB OFF)
set(protobuf_BUILD_SHARED_LIBS OFF)
set(protobuf_WITH_ZLIB OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # both go into a .so
add_subdirectory("${EXT_EXTERNAL}/abseil" "${CMAKE_CURRENT_BINARY_DIR}/abseil" EXCLUDE_FROM_ALL)
add_subdirectory("${PROTOBUF_SRC}" "${CMAKE_CURRENT_BINARY_DIR}/protobuf" EXCLUDE_FROM_ALL)
unset(CMAKE_POSITION_INDEPENDENT_CODE)  # bg3se-ext passes -fPIC itself; leaving this set would
                                        # add a second one and rebuild all 60-odd TUs for nothing

# ExtenderNet.h includes it as <Extender/Shared/ExtenderProtocol.pb.h>, so the output tree mirrors
# the source tree and its root goes on the include path.
set(PROTO_OUT "${CMAKE_CURRENT_BINARY_DIR}/proto")
set(PROTO_PB_H "${PROTO_OUT}/Extender/Shared/ExtenderProtocol.pb.h")
set(PROTO_PB_CC "${PROTO_OUT}/Extender/Shared/ExtenderProtocol.pb.cc")
add_custom_command(OUTPUT "${PROTO_PB_H}" "${PROTO_PB_CC}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROTO_OUT}"
    COMMAND "${PROTOC_EXE}" -I "${B}" --cpp_out "${PROTO_OUT}" Extender/Shared/ExtenderProtocol.proto
    WORKING_DIRECTORY "${B}"
    DEPENDS "${B}/Extender/Shared/ExtenderProtocol.proto"
    COMMENT "protoc: ExtenderProtocol.proto")
set_source_files_properties("${PROTO_PB_CC}" PROPERTIES GENERATED TRUE)

# Is the runtime usable, and not merely linkable?  ext/test/proto-selftest.cpp round-trips a
# MessageWrapper; EXCLUDE_FROM_ALL, so it costs a normal build nothing:
#   ninja -C build/ext proto-selftest && build/ext/proto-selftest
add_executable(proto-selftest EXCLUDE_FROM_ALL "${CMAKE_CURRENT_SOURCE_DIR}/test/proto-selftest.cpp")
target_include_directories(proto-selftest PRIVATE "${PROTO_OUT}" "${PROTOBUF_SRC}/third_party/utf8_range")
target_sources(proto-selftest PRIVATE "${PROTO_PB_CC}")
target_link_libraries(proto-selftest PRIVATE "${LIBCXX_V2}" libprotobuf-lite pthread)
target_compile_options(proto-selftest PRIVATE -fPIC -Wall -Wextra)

set(NOESIS_SDK "${EXT_EXTERNAL}/NoesisGUI-NativeSDK-win-3.1.7-Indie/Include")
if(NOT EXISTS "${NOESIS_SDK}")
    message(FATAL_ERROR "the NoesisGUI SDK is required at ${NOESIS_SDK}: run ext/fetch-externals.sh")
endif()

add_library(bg3se-ext SHARED ${UPSTREAM_TUS} ${IMGUI_SRCS} ${LINUX_SRCS} "${PROTO_PB_CC}")
add_dependencies(bg3se-ext generated)
# parse_context.h includes <utf8_validity.h>, which lives in protobuf's vendored utf8_range and
# which libprotobuf-lite only puts on its *own* directory's include path (cmake/utf8_range.cmake
# calls include_directories()), so every TU of ours that includes a .pb.h needs it too.
target_include_directories(bg3se-ext PRIVATE
    "${NOESIS_SDK}" "${PROTO_OUT}" "${PROTOBUF_SRC}/third_party/utf8_range"
    "${LINUX}/shim" "${LINUX}" "${B}" "${SRC}" "${REPO}/loader/src"
    "${EXT_EXTERNAL}/glm" "${EXT_EXTERNAL}/lua/src" "${EXT_EXTERNAL}/rapidjson/include"
    "${EXT_EXTERNAL}/tinycrypt/lib/include" "${EXT_EXTERNAL}/optick/src" "${EXT_EXTERNAL}/SDL/include"
    "${EXT_EXTERNAL}/imgui" "${EXT_EXTERNAL}/Vulkan/include")
target_compile_definitions(bg3se-ext PRIVATE
    BG3SE_LINUX=1
    # BG3Extender.vcxproj, Release|x64 (IMGUI and UI headers are needed by the root header; their code is phase 7)
    ENABLE_SDL ENABLE_IMGUI ENABLE_UI NS_STATIC_LIBRARY IMGUI_DISABLE_OBSOLETE_FUNCTIONS NDEBUG _WINDOWS _USRDLL OSI_EOCAPP OSIINTERFACE_EXPORTS OSI_EXTENSION_BUILD
    _ITERATOR_DEBUG_LEVEL=0 GLM_FORCE_ALIGNED_GENTYPES GLM_FORCE_INTRINSICS RAPIDJSON_HAS_STDSTRING=1
    USE_OPTICK=0)   # Extender/Shared/Optick.h sets it too; TUs that include optick.h directly must agree
target_compile_options(bg3se-ext PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-fms-extensions> $<$<COMPILE_LANGUAGE:CXX>:-fdeclspec>
    -fPIC -fvisibility=hidden -fno-omit-frame-pointer -msse4.2 -mpopcnt -mcx16
    $<$<COMPILE_LANGUAGE:CXX>:-w>)
# our own files get the warnings upstream's would drown
set_source_files_properties(${LINUX}/win32.cpp ${LINUX}/entry.cpp ${LINUX}/stubs.cpp ${LINUX}/vulkan.cpp
    ${REPO}/loader/src/hook.cpp
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra")
target_link_libraries(bg3se-ext PRIVATE "${LIBCXX_V2}" lua optick libprotobuf-lite dl pthread)
target_link_options(bg3se-ext PRIVATE
    "-Wl,--version-script=${LINUX}/exports.map" -Wl,-z,nodelete -Wl,--no-undefined)
set_target_properties(bg3se-ext PROPERTIES LINK_DEPENDS "${LINUX}/exports.map")
