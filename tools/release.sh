#!/bin/bash
# release.sh — build both objects from a clean tree, run every check that needs no game, and
# pack a release tarball. This is what .github/workflows/build.yml runs on a GitHub runner;
# it runs the same on a workstation.
#
#   tools/release.sh [VERSION]      ->  dist/bg3-se-linux-VERSION.tar.xz, its .sha256,
#                                       and dist/RELEASE-NOTES.md
#
# VERSION defaults to `git describe`. JOBS caps the parallelism (default: every core).
# Needs: zig 0.16 (the C/C++ compiler), cmake >= 3.20, ninja, protoc 36.1, python3 with numpy
# and capstone (the loader test audits a manifest with tools/manifest.py), curl, unzip, git,
# binutils (readelf, nm, objdump) and a host C compiler for tools/check-floor.sh --selftest.
#
# The tarball unpacks to bg3-se-linux-VERSION/ with the objects where the launch wrapper
# looks for them relative to itself (build/loader/, build/ext/), so the Steam launch option is
# the unpacked tools/bg3se-launch.sh by absolute path, exactly as for a source build. The
# NoesisGUI SDK headers the build fetches are proprietary and are not in the tarball: nothing
# under build/ext-external is copied, and the last step checks that.
set -eu
repo=$(realpath "$(dirname "$0")/..")
cd "$repo"
ver=${1:-$(git describe --tags --always --dirty 2> /dev/null || echo dev)}
jobs=${JOBS:-$(nproc)}
tc=$repo/loader/cmake/zig-glibc-2.31.cmake
name=bg3-se-linux-$ver
step() { echo; echo "==== $*"; }

step "toolchain"
zig version; cmake --version | head -1; ninja --version; protoc --version; python3 --version
python3 -c 'import numpy, capstone; print("numpy", numpy.__version__, "capstone", capstone.__version__)'

step "third-party sources (ext/fetch-externals.sh)"
ext/fetch-externals.sh

step "loader: libbg3se.so"
cmake -S loader -B build/loader --toolchain "$tc"
cmake --build build/loader -j "$jobs"

step "extender: libbg3se-ext.so (the first configure builds libc++ ABI v2, protobuf-lite and abseil)"
cmake -S ext -B build/ext -G Ninja --toolchain "$tc"
ninja -C build/ext -j "$jobs"
ninja -C build/ext -j "$jobs" proto-selftest

step "checks that need no game"
build/ext/proto-selftest
tools/check-floor.sh --selftest
tools/check-floor.sh build/loader/libbg3se.so
tools/check-floor.sh build/ext/libbg3se-ext.so
tools/check-floor.sh build/loader/manifest-check
python3 tools/modsettings.py --selftest
loader/test/run.sh build/loader     # the stand-in bg3; with the extender built it is dlopened too

step "package $name"
stage=build/dist/$name
rm -rf build/dist dist
mkdir -p "$stage/build/loader" "$stage/build/ext" "$stage/tools" dist
cp build/loader/libbg3se.so build/loader/manifest-check "$stage/build/loader/"
cp build/ext/libbg3se-ext.so "$stage/build/ext/"
cp tools/bg3se-launch.sh tools/bg3se-console tools/check-floor.sh "$stage/tools/"
cp README.md LICENSE "$stage/"
{
    echo "bg3-se-linux $ver"
    echo "built $(date -u '+%Y-%m-%d %H:%M UTC') from commit $(git rev-parse --verify -q HEAD || echo unknown)"
    echo "zig $(zig version), protoc $(protoc --version | cut -d' ' -f2)"
    echo
    echo "supported game builds (bin/bg3 ELF build-id):"
    for m in manifest/*.tsv; do
        id=$(basename "$m" .tsv)
        [ -f "typeids/$id.tsv" ] || continue      # the game binary's manifests have a TypeId table; libOsiris.so's do not
        echo "  $id  $(sed -n '1,8{/^#.*[Ss]team build/p}' "$m" | head -1 | sed 's/^# *//; s/; snapshot.*//')"
    done
} > "$stage/VERSION"
cat "$stage/VERSION"
tar -C build/dist --owner=0 --group=0 --numeric-owner -cf - "$name" | xz -T0 -6 > "dist/$name.tar.xz"
(cd dist && sha256sum "$name.tar.xz" > "$name.tar.xz.sha256")
{
    echo "Built by GitHub Actions from the tagged commit; see README.md for what works and what does not."
    echo
    echo '```'
    cat "$stage/VERSION"
    echo '```'
    echo
    echo "Install: unpack anywhere, then set the game's Steam launch option to the unpacked"
    echo '`tools/bg3se-launch.sh` by absolute path, followed by `%command%`. `build/loader/manifest-check <path to bin/bg3>`'
    echo "says whether your installed build is supported."
    echo
    echo "sha256: \`$(cut -d' ' -f1 "dist/$name.tar.xz.sha256")\`"
} > dist/RELEASE-NOTES.md

step "tarball check"
tar -tJf "dist/$name.tar.xz" > build/dist/listing
for want in build/loader/libbg3se.so build/loader/manifest-check build/ext/libbg3se-ext.so tools/bg3se-launch.sh tools/bg3se-console LICENSE README.md VERSION; do
    grep -qx "$name/$want" build/dist/listing || { echo "FAIL  $want is not in the tarball"; exit 1; }
done
if grep -q -i -E 'noesis|ext-external' build/dist/listing; then echo "FAIL  third-party sources in the tarball"; exit 1; fi
n=$(wc -l < build/dist/listing)
echo "ok    $n entries, nothing from build/ext-external"
ls -l dist/
