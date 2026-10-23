#!/bin/bash
# fetch-oracles.sh — fetch the Phase 1b oracle binaries with DepotDownloader.
#
#   tools/fetch-oracles.sh <steam-user> [linux] [macos] [windows] [toolkit]   (default: all)
#
# Interactive: DepotDownloader asks for the password and Steam Guard code, so a
# person runs this, not an agent. -remember-password leaves a login token beside
# DepotDownloader — treat it like config.vdf, never dump or copy it.
#
# Manifests are pinned per game build. Build 24532579 (ELF c8da62b1): macOS
# 7960684881345890822, Windows 7005275183208636774, from -manifest-only runs,
# fetched and snapshotted.
# Build 25605617 (ELF a187af94): the public manifest ids below, read anonymously
# off the product info, SHA1s from the depot manifests
# of the first fetch. On a new build: read the ids the
# same way, pin the SHA1s "-" (printed, not judged), fetch, then pin them from
# tools/depot-manifest.py list <dir>/.DepotDownloader/<depot>_<gid>.manifest --
# no -manifest-only login needed.
# The Toolkit did not move. Only binaries are fetched — the macOS depot is
# 156 GB, of which we want two files. `linux` is depot 2330359, bin/ entire
# (334 MB): a new build's ELF without updating the live install.
#
# Output: $BG3_ORACLE_DIR/<name>/ (default dumps/depot-fetch/), staging only. Each
# fetch is checked by content (SHA1 from the manifest, then file(1)), not by exit
# code; snapshots then go under $BG3_SPECIMENS by hand.

dd=${DEPOTDOWNLOADER:-DepotDownloader}
out=${BG3_ORACLE_DIR:-$(dirname "$(realpath "$0")")/../dumps/depot-fetch}
toolkit_app=${TOOLKIT_APP:-2934770}

user=$1
[ -n "$user" ] || { echo "usage: $0 <steam-user> [macos] [windows] [toolkit]" >&2; exit 2; }
shift
[ $# -gt 0 ] || set -- linux macos windows toolkit

# name  app  depot  manifest  os
oracles="
linux   1086940       2330359 1591643763935820945 linux
macos   1086940       1419660 3067062821888772426 macos
windows 1086940       1419652 1625295449847783523 windows
toolkit $toolkit_app  2934771 5300403907239633818 windows
"

# name  sha1  path — the files a fetch is judged by
expect="
linux   952c9b89f09aad55850dd36b100ef71c308c8348 bin/bg3
linux   1385f11a3f7d97c7f43cff5e6b306f67cd8aa345 bin/libOsiris.so
macos   85568b355a80efaf67474c837284d682a449c6ba Baldur's Gate 3.app/Contents/MacOS/Baldur's Gate 3
macos   da45c3de1d070427b58ee9422c4a9da27c12022a Baldur's Gate 3.app/Contents/Frameworks/libOsiris.dylib
windows 44524316e5a594d8af5ddce1177a6e92c74148a7 bin/bg3.exe
windows f85b1927193b1b113e97850b1f9afb0ccb3db69e bin/Osiris.dll
toolkit 0282c8c278b1eaecc11fd93efff6958786b2d023 EoCServer.dll
toolkit fa11f74090954b37cac74963077aadc351b482c0 EoCClient.dll
toolkit 6de3f8430d7bcef5e887b90629ce3d3ce5eb1de2 EoCShared.dll
toolkit 6e8ee13fabf3cdec52e946f617ee86f56bbfceba GameEngine.dll
toolkit b267dbdbce8cc8bb8bc7b256331a5be6e1ae8b00 CoreLib.dll
"

mkdir -p "$out" || exit 1
rc=0

for want in "$@"; do
    row=$(awk -v n="$want" '$1 == n' <<<"$oracles")
    [ -n "$row" ] || { echo "unknown oracle: $want" >&2; rc=2; continue; }
    read -r name app depot manifest os <<<"$row"
    dir="$out/$name"
    mkdir -p "$dir"

    args=(-app "$app" -depot "$depot" -manifest "$manifest" -os "$os" -osarch 64
          -username "$user" -remember-password -dir "$dir")
    if [ "$name" = macos ]; then
        # the two Mach-O files we read, plus the bundle's Info.plist for the version
        cat >"$dir/filelist.txt" <<'EOF'
regex:Contents[\\/]MacOS[\\/]
regex:Contents[\\/]Frameworks[\\/]libOsiris\.dylib$
regex:\.app[\\/]Contents[\\/]Info\.plist$
EOF
        args+=(-filelist "$dir/filelist.txt")
    fi

    echo "=== $name: app $app depot $depot manifest $manifest"
    "$dd" "${args[@]}"

    while read -r n sha path; do
        [ "$n" = "$name" ] || continue
        if [ ! -f "$dir/$path" ]; then
            echo "MISSING  $path"; rc=1; continue
        fi
        got=$(sha1sum "$dir/$path" | cut -d' ' -f1)
        if [ "$sha" = - ]; then
            echo "UNPINNED $path ($got)"
            echo "         $(file -b "$dir/$path")"
        elif [ "$got" = "$sha" ]; then
            echo "ok       $path"
            echo "         $(file -b "$dir/$path")"
        else
            echo "BAD SHA1 $path ($got, want $sha)"; rc=1
        fi
    done <<<"$expect"
done

exit $rc
