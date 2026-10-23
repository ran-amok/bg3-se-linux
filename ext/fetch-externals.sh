#!/bin/bash
# Fetches the third-party sources the extender build needs, pinned by commit, into
# build/ext-external/ by default. Each fetch is checked by content: a witness file must
# exist afterwards. Not needed here: Detours (replaced), and protobuf's full runtime
# (the debugger protocols stay out; only the lite runtime is built).
#
# PORT (linux): protobuf and abseil are needed because the wire half of Ext.Net is protobuf
# end to end (ExtenderProtocol.proto -> MessageWrapper), so the pinned pair is fetched and
# ext/CMakeLists.txt builds libprotobuf-lite out of it (the .proto is optimize_for =
# LITE_RUNTIME).  The protoc that generates the .pb pair is the
# host's and has to be the pinned runtime's own version; ext/CMakeLists.txt checks that.
#
#   ext/fetch-externals.sh [DIR]
set -eu
E=$(realpath -m "${1:-$(dirname "$(realpath "$0")")/../build/ext-external}")
mkdir -p "$E"
die() { echo "$*" >&2; exit 2; }
fetch() {   # $1 = name, $2 = url, $3 = commit, $4 = witness file
    local d=$E/$1
    if [ "$(git -C "$d" rev-parse HEAD 2> /dev/null)" != "$3" ]; then
        rm -rf "$d"; mkdir -p "$d"
        git -C "$d" init -q && git -C "$d" fetch -q --depth 1 "$2" "$3" &&
            git -C "$d" checkout -q FETCH_HEAD || die "could not fetch $1 @ $3"
    fi
    [ "$(git -C "$d" rev-parse HEAD)" = "$3" ] || die "$1 is not at $3"
    [ -s "$d/$4" ] || die "$1 @ $3 has no $4"
    echo "$1 @ ${3:0:12} ok"
}
fetch glm       https://github.com/g-truc/glm                8d1fd52e5ab5590e2c81768ace50c72bae28f2ed glm/glm.hpp
fetch rapidjson https://github.com/Tencent/rapidjson         24b5e7a8b27f42fa16b96fc70aade9106cf7102f include/rapidjson/document.h
fetch tinycrypt https://github.com/intel/tinycrypt           cf24c907a61c01bc2c4e1ee0ae24457c27a840a2 lib/include/tinycrypt/sha256.h
fetch lua       https://github.com/Norbyte/lua-dos           4fff4e19954f927a0478055306dadaf71bba02b2 src/lstate.h
fetch optick    https://github.com/Norbyte/optick            a84a9ac6b23a4cf4ab21ac3d98e05aa475dab34c src/optick.h
fetch SDL       https://github.com/libsdl-org/SDL            ba2f78a0069118a6c583f1fbf1420144ffa35bad include/SDL_events.h
# headers only: IMGUI itself is phase 7, but Extender/Client/IMGUI/IMGUI.h sits in the root header
fetch imgui     https://github.com/Norbyte/imgui             f0e177b80ed7525cf60fdbd552cebb7c58f8320f imgui.h
fetch Vulkan    https://github.com/KhronosGroup/Vulkan-Headers e3b1eec08173d6b825cd3ac88c885a63b621504a include/vulkan/vulkan.h
# protobuf 36.1 and the abseil it needs (pinned with the rest):
# the lite runtime is linked into libbg3se-ext.so and carries ExtenderProtocol.pb.cc.
fetch protobuf  https://github.com/protocolbuffers/protobuf  f377bfefc5e2cfab68b816903c25b23e091c439d src/google/protobuf/message_lite.h
fetch abseil    https://github.com/abseil/abseil-cpp         76bb24329e8bf5f39704eb10d21b9a80befa7c81 absl/base/config.h

# NoesisGUI 3.1.7 headers: upstream's pull-externals.bat fetches this zip from Norbyte's mirror
# (Noesis gives Indie licensees the current version only). Proprietary (Noesis EULA): never
# vendored. Pinned by sha256; a local copy named by NOESIS_SDK_ZIP is used when set. Required:
# ext/CMakeLists.txt puts Include/ on the include path and the build fails without it.
# 3.1.7 is ABI-identical to the game's 3.1.6 (tools/noesis-version.py).
fetch_zip() {   # $1 = name, $2 = url, $3 = sha256, $4 = local copy, $5 = witness file
    local d=$E/$1 z=$E/$1.zip
    if [ ! -s "$d/$5" ] || [ "$(cat "$d/.sha256" 2> /dev/null)" != "$3" ]; then
        rm -rf "$d" "$z"
        if [ -s "$4" ]; then cp "$4" "$z"; else curl -fsSL -o "$z" "$2" || die "could not fetch $1"; fi
        [ "$(sha256sum < "$z" | cut -d' ' -f1)" = "$3" ] || die "$1: $z is not sha256 $3"
        (cd "$E" && unzip -q "$z") || die "could not unpack $1"
        echo "$3" > "$d/.sha256"; rm -f "$z"
    fi
    [ -s "$d/$5" ] || die "$1 has no $5"
    echo "$1 @ ${3:0:12} ok"
}
fetch_zip NoesisGUI-NativeSDK-win-3.1.7-Indie \
    http://bg3se-updates.norbyte.dev/Stuff/NoesisGUI-NativeSDK-win-3.1.7-Indie.zip \
    0a23696db368dd8e9fe9ccacc628d4b168c76060ba503cf4e0ec1c0de9528c2a \
    "${NOESIS_SDK_ZIP:-}" \
    Include/NsGui/FrameworkElement.h

# PORT (linux): the first of two places where the 3.1.7 SDK and bin/bg3 disagree (the second is
# the slot order below).  This one is hit by every TU that includes GameDefinitions/Base.h.
# NsCore/TypePropertyImpl.h declares `void Get(const void*, void*) const override` on
# TypePropertyFunction<C,T>, but its base TypeProperty has no such virtual -- the one it means is
# GetCopy, which is also what the out-of-line body in TypePropertyImpl.inl calls. MSVC never
# notices, because the class is a template and it defers the `override` check to an instantiation
# that never happens; clang checks eagerly, since the base is not dependent. Two words, in place:
# the zip stays pinned and unvendored (Noesis EULA), so this is a sed and not a patch file, and it
# refuses rather than guesses if either line is not exactly the pinned text.
noesis_fix() {  # $1 = file, $2 = line, $3 = text before, $4 = text after
    local f=$E/NoesisGUI-NativeSDK-win-3.1.7-Indie/Include/NsCore/$1
    local now; now=$(sed -n "$2p" "$f" | tr -d '\r')   # the zip is the Windows SDK: CRLF throughout
    if [ "$now" = "$4" ]; then return 0; fi
    [ "$now" = "$3" ] || die "noesis SDK $1:$2 is neither the pinned text nor the fix: $now"
    sed -i "$2s/.*/$(printf '%s' "$4" | sed 's/[&/\]/\\&/g')\\r/" "$f"
    [ "$(sed -n "$2p" "$f" | tr -d '\r')" = "$4" ] || die "noesis SDK $1:$2 did not take the fix"
}
noesis_fix TypePropertyImpl.h 105 \
    '    void Get(const void* ptr, void* dest) const override;' \
    '    void GetCopy(const void* ptr, void* dest) const override;'
noesis_fix TypePropertyImpl.inl 178 \
    'void TypePropertyFunction<C,T>::Get(const void* ptr, void* dest) const' \
    'void TypePropertyFunction<C,T>::GetCopy(const void* ptr, void* dest) const'
echo "NoesisGUI-NativeSDK-win-3.1.7-Indie TypePropertyFunction::Get -> GetCopy ok"

# PORT (linux): the 3.1.7 header declares `Noesis::TypeProperty`'s two value getters in the
# opposite order to the one `bin/bg3` was built with, so every virtual call through either of
# them picks the other's slot.  Witness (offline, re-runnable -- `tools/noesis-version.py vorder
# BIN TypeProperty`): on a187af94 the concrete vtables
# `_ZTVN6Noesis18TypePropertyOffsetINS_11FixedStringILj24EEEEE` (0x7a0eb40) and
# `…TypePropertyOffsetIiE` (0x7a16228) agree slot for slot and read
#   6  Get(void const*)        7  Get(void const*, void*)
# where the header declares `GetCopy(ptr, dest)` at 6 and `Get(ptr)` at 7.  The game spells the
# two-argument overload `Get`; the header spells it `GetCopy` (hence the rename above) and
# declares it *first*.  Swapping the two declarations puts both call sites and the extender's own
# `TypeProperty` subclasses (`Lua/Libs/ClientUI/CustomProperties.inl`, whose vtables the game's
# Noesis calls into) on the game's slots at once.
noesis_fix TypeProperty.h 68 \
    '    /// Copies the value of the pointer to the given address' \
    '    /// Gets a pointer to the value of the property'
noesis_fix TypeProperty.h 69 \
    '    virtual void GetCopy(const void* ptr, void* dest) const;' \
    '    virtual const void* Get(const void* ptr) const;'
noesis_fix TypeProperty.h 71 \
    '    /// Gets a pointer to the value of the property' \
    '    /// Copies the value of the pointer to the given address'
noesis_fix TypeProperty.h 72 \
    '    virtual const void* Get(const void* ptr) const;' \
    '    virtual void GetCopy(const void* ptr, void* dest) const;'
echo "NoesisGUI-NativeSDK-win-3.1.7-Indie TypeProperty::Get/GetCopy slot order ok"
