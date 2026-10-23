#!/bin/bash
# The libc++ sfinae-tag overlay: zig's own <variant>, <optional> and __tuple/sfinae_helpers.h
# with ext/toolchain/libcxx-sfinae-tag.patch applied, in a directory every C++ TU of the
# extender -- and libc++v2.a itself -- gets as `-I`.  Stock libc++ gives a std::variant whose
# alternative is (or starts with) a std::variant or a std::optional an 8-byte hole at the
# front: the outer variant's two empty sfinae bases collide with the inner's, and Itanium
# forbids two subobjects of one type at one address, so clang moves the union to +8.  The
# game's ls::Variant has no hole.  The patch gives the two
# base templates a tag parameter that variant<Ts...> and optional<T> fill with themselves, so
# the bases never collide and nothing else changes.
#
#   ext/toolchain/libcxx-overlay.sh ROOT [PATCH]     prints the overlay directory
#   ext/toolchain/libcxx-overlay.sh --check BUILDDIR  every C++ compile line carries the -I
#
# The directory is ROOT/<hash>, where the hash covers the patch *and* the three stock headers:
# a changed patch or a zig upgrade is a new directory and so a new command line (zig's
# compile cache keys on the command line and the source text, not on what a path pulled in --
# the .incbin trap).  A patch that does not apply cleanly is the zig-upgrade
# signal and fails here, loudly, before anything is compiled against half an overlay.  It
# must be `-I`: zig puts its own libc++ include directory first among the -isystem ones, so an
# -isystem overlay loses.
set -eu
HERE=$(dirname "$(realpath "$0")")
ZIG=${ZIG:-zig}

if [ "${1:-}" = "--check" ]; then
    # Every compile line for a C++ source must name a libcxx-overlay directory; .c and .S
    # sources are exempt (they include no libc++).  Prints the offenders, exits 1 if any.
    build=${2:?build dir}
    bad=$(ninja -C "$build" -t commands 2>/dev/null \
        | grep -E -- '(^| )-c ' | grep -E '\.(cpp|cc|cxx|inl)( |$)' | grep -v -- '-I[^ ]*/libcxx-overlay/' || true)
    n=$(ninja -C "$build" -t commands 2>/dev/null | grep -E -- '(^| )-c ' | grep -cE '\.(cpp|cc|cxx|inl)( |$)' || true)
    if [ -n "$bad" ]; then
        echo "C++ compile lines without the overlay -I (of $n):" >&2
        echo "$bad" | sed 's/ .*-o \([^ ]*\).*/  \1/' >&2
        exit 1
    fi
    echo "libc++ overlay: all $n C++ compile lines in $build carry it"
    exit 0
fi

ROOT=$(realpath -m "${1:?overlay root dir}")
PATCH=$(realpath "${2:-$HERE/libcxx-sfinae-tag.patch}")
LIB=$(dirname "$(realpath "$(command -v "$ZIG")")")/lib/libcxx/include
FILES=(__tuple/sfinae_helpers.h variant optional)
for f in "${FILES[@]}"; do
    [ -f "$LIB/$f" ] || { echo "libc++ overlay: no $LIB/$f" >&2; exit 2; }
done
HASH=$(cat "$PATCH" "${FILES[@]/#/$LIB/}" | sha1sum | cut -c1-12)
OUT="$ROOT/$HASH"
if [ -f "$OUT/.complete" ]; then
    echo "$OUT"
    exit 0
fi

tmp=$(mktemp -d "$ROOT/.tmp.XXXXXX" 2>/dev/null || { mkdir -p "$ROOT" && mktemp -d "$ROOT/.tmp.XXXXXX"; })
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/__tuple"
for f in "${FILES[@]}"; do cp "$LIB/$f" "$tmp/$f"; done
# -F0: no fuzz.  An offset is tolerated (reported below); a rejected hunk is a failure.
if ! out=$(patch -d "$tmp" -p1 -F0 --no-backup-if-mismatch -r - < "$PATCH" 2>&1); then
    echo "libc++ overlay: $PATCH does not apply to $LIB -- a zig upgrade?  Re-derive the patch." >&2
    echo "$out" >&2
    exit 1
fi
echo "$out" | grep -i 'offset\|fuzz' | sed 's/^/libc++ overlay: note: /' >&2 || true
# by content, not by exit code: the bases must take the tag and both templates must pass it
grep -q 'class _Tag = void' "$tmp/__tuple/sfinae_helpers.h" || { echo "libc++ overlay: sfinae_helpers.h carries no _Tag parameter" >&2; exit 1; }
grep -q 'variant<_Types\.\.\.>>' "$tmp/variant" || { echo "libc++ overlay: variant does not pass itself as the tag" >&2; exit 1; }
grep -q 'optional<_Tp> >' "$tmp/optional"       || { echo "libc++ overlay: optional does not pass itself as the tag" >&2; exit 1; }
{
    echo "zig:   $(realpath "$(command -v "$ZIG")")"
    echo "lib:   $LIB"
    echo "patch: $PATCH"
    echo "hash:  $HASH (sha1 of patch + the three stock headers, first 12)"
    echo "made:  $(date '+%Y-%m-%d %H:%M')"
} > "$tmp/SOURCE"
touch "$tmp/.complete"
rm -rf "$OUT"
mv "$tmp" "$OUT"
trap - EXIT
echo "$OUT"
