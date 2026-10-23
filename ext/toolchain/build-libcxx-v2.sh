#!/bin/bash
# Builds libc++ in ABI version 2 (namespace std::__2) from the sources zig ships, as a
# static archive for the extender. The game is a libc++ unstable-ABI build (std::__2):
# with the same ABI, upstream's STDString (basic_string with a 32-bit size_type
# allocator) has the game's 16-byte layout — data at 0, size at 8, capacity at 12 with
# the long flag in its top bit (Phase 5) — and so does every other
# libc++ container a game struct holds by value. zig's own libc++ is ABI 1.
#
#   ext/toolchain/build-libcxx-v2.sh [OUT_DIR]     default build/libcxx-v2
#
# libc++abi and libunwind are zig's (their exported symbols are unversioned), so only
# libc++ proper is rebuilt. Checked by content: the archive must define std::__2::cout.
#
# The archive is compiled against the sfinae-tag overlay (libcxx-overlay.sh, the patched
# <variant>/<optional>) like every TU of the extender, so there is one definition of those
# templates everywhere; $LIBCXX_OVERLAY names the directory (ext/CMakeLists.txt passes it),
# or it is made here. OUT/libc++v2.a.overlay records which one, and the configure rebuilds
# the archive when the tree's overlay differs.
set -eu
HERE=$(dirname "$(realpath "$0")")
ZIG=${ZIG:-zig}
LIB=$(dirname "$(realpath "$(command -v $ZIG)")")/lib
[ -d "$LIB/libcxx/src" ] || { echo "no libcxx sources under $LIB" >&2; exit 2; }
OUT=$(realpath -m "${1:-$HERE/../../build/libcxx-v2}")
OVERLAY=${LIBCXX_OVERLAY:-$("$HERE/libcxx-overlay.sh" "$(dirname "$OUT")/libcxx-overlay")}
[ -f "$OVERLAY/variant" ] || { echo "no overlay at $OVERLAY" >&2; exit 2; }
mkdir -p "$OUT/obj"
TARGET=${TARGET:-x86_64-linux-gnu.2.31}
# zig's own configuration of this libc++ (zig c++ -dM -E), with the ABI version changed
FLAGS=(
  -target "$TARGET" -std=c++23 -O2 -fPIC -fno-omit-frame-pointer
  -Wno-macro-redefined -Wno-user-defined-literals -Wno-covered-switch-default -Wno-suggest-override
  -D_LIBCPP_ABI_VERSION=2 -D_LIBCPP_ABI_NAMESPACE=__2
  -D_LIBCPP_BUILDING_LIBRARY -DLIBCXX_BUILDING_LIBCXXABI
  -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS -D_LIBCXXABI_DISABLE_VISIBILITY_ANNOTATIONS
  -D_LIBCPP_HAS_NO_VENDOR_AVAILABILITY_ANNOTATIONS -D_LIBCPP_HAS_VENDOR_AVAILABILITY_ANNOTATIONS=0
  -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_NONE -D_LIBCPP_PSTL_BACKEND_SERIAL=1
  -D_LIBCPP_HAS_MUSL_LIBC=0 -D_LIBCPP_HAS_THREADS=1 -D_LIBCPP_HAS_MONOTONIC_CLOCK=1
  -D_LIBCPP_HAS_FILESYSTEM=1 -D_LIBCPP_HAS_LOCALIZATION=1 -D_LIBCPP_HAS_UNICODE=1
  -D_LIBCPP_HAS_WIDE_CHARACTERS=1 -D_LIBCPP_HAS_RANDOM_DEVICE=1 -D_LIBCPP_HAS_TERMINAL=1
  -D_LIBCPP_HAS_TIME_ZONE_DATABASE=1 -D_LIBCPP_HAS_NO_STD_MODULES=1
  -DNDEBUG
  -I"$OVERLAY" -I"$LIB/libcxx/src" -I"$LIB/libcxx/libc" -I"$LIB/libcxxabi/include"
)
SRCS=("$LIB"/libcxx/src/*.cpp "$LIB"/libcxx/src/filesystem/*.cpp "$LIB"/libcxx/src/ryu/*.cpp)
jobs=${JOBS:-$(nproc)}
running=0; fail=0
for s in "${SRCS[@]}"; do
    o="$OUT/obj/$(basename "${s%.cpp}").o"
    ( "$ZIG" c++ "${FLAGS[@]}" -c "$s" -o "$o" 2> "$o.log" || { echo "FAILED: $s (see $o.log)"; exit 1; } ) &
    running=$((running+1)); if [ "$running" -ge "$jobs" ]; then wait -n || fail=1; running=$((running-1)); fi
done
while [ "$running" -gt 0 ]; do wait -n || fail=1; running=$((running-1)); done
[ "$fail" = 0 ] || { echo "libc++ v2: compile failures" >&2; exit 1; }
rm -f "$OUT/libc++v2.a" "$OUT/libc++v2.a.overlay"
"$ZIG" ar rcs "$OUT/libc++v2.a" "$OUT"/obj/*.o
"$ZIG" ranlib "$OUT/libc++v2.a"
# content check, not exit code: the archive must carry the versioned namespace
nm "$OUT/libc++v2.a" 2>/dev/null | grep -q '_ZNSt3__24coutE' || { echo "libc++ v2: no std::__2::cout in the archive" >&2; exit 1; }
basename "$OVERLAY" > "$OUT/libc++v2.a.overlay"
n=$(nm "$OUT/libc++v2.a" | grep -c ' T _ZNSt3__2')
echo "libc++ v2: $OUT/libc++v2.a, $n std::__2 text symbols, overlay $(basename "$OVERLAY")"
